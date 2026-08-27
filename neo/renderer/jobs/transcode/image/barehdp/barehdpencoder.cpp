#include "barehdp.h"
#include "jxrlibencoderbridge.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

class HdpBitWriter {
public:
	HdpBitWriter( std::uint8_t * output, int capacity )
		: output_( output ), capacity_( capacity ), bytes_( 0 ), current_( 0 ), bits_( 0 ), failed_( false ) {
	}

	void WriteBits( std::uint32_t value, unsigned count ) {
		if ( count < 32 ) {
			value &= count == 0 ? 0 : ( ( 1U << count ) - 1U );
		}
		for ( int bit = static_cast< int >( count ) - 1; bit >= 0; --bit ) {
			const std::uint32_t valueBit = bit < 32 ? ( ( value >> bit ) & 1U ) : 0U;
			current_ = static_cast< std::uint8_t >( ( current_ << 1 ) | valueBit );
			if ( ++bits_ == 8 ) {
				PutByte( current_ );
				current_ = 0;
				bits_ = 0;
			}
		}
	}

	void AlignByte() {
		if ( bits_ != 0 ) {
			WriteBits( 0, 8U - bits_ );
		}
	}

	void WriteByte( std::uint8_t value ) {
		AlignByte();
		PutByte( value );
	}

	int Finish() {
		AlignByte();
		return failed_ ? 0 : bytes_;
	}

private:
	void PutByte( std::uint8_t value ) {
		if ( bytes_ >= capacity_ ) {
			failed_ = true;
			return;
		}
		output_[bytes_++] = value;
	}

	std::uint8_t * output_;
	int capacity_;
	int bytes_;
	std::uint8_t current_;
	unsigned bits_;
	bool failed_;
};

void WriteHuffmanSymbol( HdpBitWriter & writer,
		hdpAdaptiveHuffman_t & huffman, int symbol ) {
	const int * table = huffman.encTable;
	writer.WriteBits( static_cast< std::uint32_t >( table[1 + 2 * symbol] ),
		static_cast< unsigned >( table[2 + 2 * symbol] ) );
}

void UpdateHuffmanSymbol( hdpAdaptiveHuffman_t & huffman, int symbol ) {
	if ( huffman.deltaTable != nullptr ) {
		huffman.discriminant += huffman.deltaTable[symbol];
	}
}

void UpdateHuffmanSymbolSecond( hdpAdaptiveHuffman_t & huffman, int symbol ) {
	UpdateHuffmanSymbol( huffman, symbol );
	if ( huffman.deltaTable1 != nullptr ) {
		huffman.discriminant1 += huffman.deltaTable1[symbol];
	}
}

void InitHuffmanArray( hdpCodecParms_t & parms,
		hdpAdaptiveHuffman_t * huffman, const int * alphabets, int count ) {
	for ( int index = 0; index < count; ++index ) {
		InitAdaptiveHuffman( &parms, &huffman[index], alphabets[index] );
		AdaptiveHuffmanDiscriminant( &huffman[index] );
	}
}

void AdaptHuffmanArray( hdpAdaptiveHuffman_t * huffman, int count ) {
	for ( int index = 0; index < count; ++index ) {
		AdaptiveHuffmanDiscriminant( &huffman[index] );
	}
}

bool CompressNeutral( const idBareHDPEncoder & encoder,
		std::uint8_t * output, int outputCapacity, int width, int height,
		int * outputBytes, bool fullBands, bool diagnosticHighPass ) {
	if ( outputBytes != nullptr ) {
		*outputBytes = 0;
	}
	if ( output == nullptr || outputBytes == nullptr || outputCapacity <= 0 ||
		width <= 0 || height <= 0 || ( width & 15 ) != 0 || ( height & 15 ) != 0 ) {
		return false;
	}

	HdpBitWriter writer( output, outputCapacity );
	writer.WriteBits( CF_YUV_444, 3 );
	writer.WriteBits( 0, 1 );
	writer.WriteBits( fullBands ? SB_NO_FLEXBITS : SB_NO_LOWPASS, 4 );
	writer.WriteBits( 0, 4 );
	writer.WriteBits( 0, 4 );
	writer.WriteBits( 1, 1 );
	writer.WriteBits( 0, 2 );
	writer.WriteBits( static_cast< std::uint32_t >(
		std::max( 1, std::min( 255, encoder.quality ) ) ), 8 );
	if ( fullBands ) {
		const std::uint32_t quantizer = static_cast< std::uint32_t >(
			std::max( 1, std::min( 255, encoder.quality ) ) );
		writer.WriteBits( 0, 1 ); // independent LP quantizer
		writer.WriteBits( 1, 1 ); // uniform LP quantizer is in this header
		writer.WriteBits( 0, 2 ); // shared by all channels
		writer.WriteBits( quantizer, 8 );
		writer.WriteBits( 0, 1 ); // independent HP quantizer
		writer.WriteBits( 1, 1 ); // uniform HP quantizer is in this header
		writer.WriteBits( 0, 2 ); // shared by all channels
		writer.WriteBits( quantizer, 8 );
	}
	writer.AlignByte();
	writer.WriteByte( 0 );
	writer.WriteByte( 0 );
	writer.WriteByte( 0 );
	writer.WriteByte( 0 );
	writer.WriteByte( 1 );
	writer.WriteByte( 0 );

	hdpCodecParms_t parms{};
	const int dcAlphabets[3] = { 8, 7, 7 };
	const int bandAlphabets[8] = { 12, 6, 6, 12, 6, 6, 7, 7 };
	const int cbpAlphabets[3] = { 9, 5, 4 };
	hdpAdaptiveHuffman_t dcHuffman[3]{};
	hdpAdaptiveHuffman_t lpHuffman[8]{};
	hdpAdaptiveHuffman_t hpHuffman[8]{};
	hdpAdaptiveHuffman_t cbpHuffman[3]{};
	InitHuffmanArray( parms, dcHuffman, dcAlphabets, 3 );
	InitHuffmanArray( parms, lpHuffman, bandAlphabets, 8 );
	InitHuffmanArray( parms, hpHuffman, bandAlphabets, 8 );
	InitHuffmanArray( parms, cbpHuffman, cbpAlphabets, 3 );

	hdpAdaptiveCNModel_t dcModel{};
	hdpAdaptiveCNModel_t lpModel{};
	hdpAdaptiveCNModel_t hpModel{};
	dcModel.band = BAND_DC;
	lpModel.band = BAND_LP;
	hpModel.band = BAND_HP;
	for ( int channel = 0; channel < 3; ++channel ) {
		dcModel.fixedLengthCodeBits[channel] = 8;
		lpModel.fixedLengthCodeBits[channel] = 4;
	}
	int lpMaxCount = 1;
	int lpZeroCount = 1;
	const int widthMB = width >> 4;
	const int heightMB = height >> 4;
	for ( int y = 0; y < heightMB; ++y ) {
		for ( int x = 0; x < widthMB; ++x ) {
			WriteHuffmanSymbol( writer, dcHuffman[0], 0 );
			for ( int channel = 0; channel < 3; ++channel ) {
				writer.WriteBits( 0, static_cast< unsigned >(
					dcModel.fixedLengthCodeBits[channel] ) );
			}
			int laplacianMean[16]{};
			UpdateAdaptiveCoefficientNormalization( CF_YUV_444, 3,
				laplacianMean, &dcModel );

			if ( fullBands ) {
				// LP CBP zero. The adaptive representation starts as a raw
				// three-bit mask and quickly switches to its one-bit zero code.
				if ( lpZeroCount <= 0 || lpMaxCount < 0 ) {
					writer.WriteBits( 0, 1 );
				} else {
					writer.WriteBits( 0, 3 );
				}
				lpMaxCount = std::max( -8, std::min( 7, lpMaxCount + 1 ) );
				lpZeroCount = std::max( -8, std::min( 7, lpZeroCount - 3 ) );
				for ( int channel = 0; channel < 3; ++channel ) {
					writer.WriteBits( 0, static_cast< unsigned >(
						15 * lpModel.fixedLengthCodeBits[channel] ) );
				}
				memset( laplacianMean, 0, sizeof( laplacianMean ) );
				UpdateAdaptiveCoefficientNormalization( CF_YUV_444, 3,
					laplacianMean, &lpModel );

				// HP actual CBP is zero. At the upper-left tile macroblock the
				// predictor is all ones, so its residual is bit zero in every
				// channel. Later zero blocks have a zero residual.
				if ( x == 0 && y == 0 ) {
					WriteHuffmanSymbol( writer, cbpHuffman[1], 1 );
					UpdateHuffmanSymbol( cbpHuffman[1], 1 );
					writer.WriteBits( 0, 2 ); // group zero
					const int blockSymbol = diagnosticHighPass ? 8 : 6;
					WriteHuffmanSymbol( writer, cbpHuffman[0], blockSymbol );
					UpdateHuffmanSymbol( cbpHuffman[0], blockSymbol );
					writer.WriteBits( 0, 2 ); // U and V are both present
					if ( diagnosticHighPass ) {
						writer.WriteBits( 1, 1 ); // retain CBP value nine
						writer.WriteBits( 1, 2 ); // residual Y pattern six
					} else {
						writer.WriteBits( 0, 2 ); // residual Y bit zero
					}
					for ( int chroma = 0; chroma < 2; ++chroma ) {
						WriteHuffmanSymbol( writer, cbpHuffman[2], 0 );
						writer.WriteBits( 0, 2 );
					}
					if ( diagnosticHighPass ) {
						// One positive unit high-pass coefficient at scan location one.
						WriteHuffmanSymbol( writer, hpHuffman[0], 1 );
						UpdateHuffmanSymbolSecond( hpHuffman[0], 1 );
						writer.WriteBits( 0, 1 ); // positive sign
					}
				} else {
					WriteHuffmanSymbol( writer, cbpHuffman[1], 0 );
					UpdateHuffmanSymbol( cbpHuffman[1], 0 );
				}
				memset( laplacianMean, 0, sizeof( laplacianMean ) );
				UpdateAdaptiveCoefficientNormalization( CF_YUV_444, 3,
					laplacianMean, &hpModel );
			}

			if ( x == 0 || x + 1 == widthMB ) {
				AdaptHuffmanArray( dcHuffman, 3 );
				if ( fullBands ) {
					AdaptHuffmanArray( lpHuffman, 8 );
					AdaptHuffmanArray( hpHuffman, 8 );
					AdaptHuffmanArray( cbpHuffman, 3 );
				}
			}
		}
	}

	*outputBytes = writer.Finish();
	return *outputBytes > 0;
}

} // namespace

idBareHDPEncoder::idBareHDPEncoder() : quality( 30 ) {
}

bool idBareHDPEncoder::CompressImageYCoCg( const std::uint8_t * rgba,
		std::uint8_t * output, int outputCapacity, int width, int height,
		int inputByteStride, int * outputBytes ) const {
	return JxrLibCompressBareYCoCg( rgba, output, outputCapacity, width,
		height, inputByteStride, quality, outputBytes );
}

bool idBareHDPEncoder::CompressImageMono( const std::uint8_t * gray,
		std::uint8_t * output, int outputCapacity, int width, int height,
		int inputByteStride, int * outputBytes ) const {
	return JxrLibCompressBareMono( gray, output, outputCapacity, width,
		height, inputByteStride, quality, outputBytes );
}

bool idBareHDPEncoder::CompressNeutralYCoCg( std::uint8_t * output,
		int outputCapacity, int width, int height, int * outputBytes ) const {
	return CompressNeutral( *this, output, outputCapacity, width, height,
		outputBytes, false, false );
}

bool idBareHDPEncoder::CompressNeutralYCoCgFullBands( std::uint8_t * output,
		int outputCapacity, int width, int height, int * outputBytes ) const {
	return CompressNeutral( *this, output, outputCapacity, width, height,
		outputBytes, true, false );
}

bool idBareHDPEncoder::CompressDiagnosticYCoCg( std::uint8_t * output,
		int outputCapacity, int width, int height, int * outputBytes ) const {
	return CompressNeutral( *this, output, outputCapacity, width, height,
		outputBytes, true, true );
}
