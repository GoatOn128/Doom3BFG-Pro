#include "jxrlibencoderbridge.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

// Keep the reference codec declarations isolated from hdpcodec.h: both codecs
// deliberately retain their original public enum names.
extern "C" {
#include "windowsmediaphoto.h"
}

bool JxrLibCompressBareYCoCg( const std::uint8_t * rgba,
		std::uint8_t * output, int outputCapacity, int width, int height,
		int inputByteStride, int quality, int * outputBytes ) {
	static const int FULL_IMAGE_HEADER_BYTES = 16;
	if ( outputBytes != nullptr ) {
		*outputBytes = 0;
	}
	if ( rgba == nullptr || output == nullptr || outputBytes == nullptr ||
		outputCapacity <= 0 || width <= 0 || height <= 0 ||
		inputByteStride < width * 4 ) {
		return false;
	}

	// The reference stream API accepts packed 24-bit RGB. Drop source alpha;
	// Tech 5's YCoCg target stores transformed color as Co,Cg,0,Y.
	std::vector< std::uint8_t > rgb(
		static_cast< std::size_t >( width ) * height * 3 );
	for ( int y = 0; y < height; ++y ) {
		const std::uint8_t * source = rgba + y * inputByteStride;
		std::uint8_t * target = rgb.data() +
			static_cast< std::size_t >( y ) * width * 3;
		for ( int x = 0; x < width; ++x ) {
			target[x * 3 + 0] = source[x * 4 + 0];
			target[x * 3 + 1] = source[x * 4 + 1];
			target[x * 3 + 2] = source[x * 4 + 2];
		}
	}

	// For this fixed profile the standard JPEG XR image header is 16 bytes:
	// signature, flags, and abbreviated 16-bit dimensions. The image-plane
	// header immediately following it is the first byte Tech 5 stores.
	std::vector< std::uint8_t > fullStream(
		static_cast< std::size_t >( outputCapacity ) + FULL_IMAGE_HEADER_BYTES );
	WMPStream * stream = nullptr;
	if ( CreateWS_Memory( &stream, fullStream.data(), fullStream.size() ) != WMP_errSuccess ) {
		return false;
	}

	CWMImageInfo imageInfo{};
	imageInfo.cWidth = static_cast< std::size_t >( width );
	imageInfo.cHeight = static_cast< std::size_t >( height );
	imageInfo.cfColorFormat = CF_RGB;
	imageInfo.bdBitDepth = BD_8;
	imageInfo.cBitsPerUnit = 24;
	imageInfo.bRGB = TRUE;
	imageInfo.oOrientation = O_NONE;

	CWMIStrCodecParam codecParms{};
	codecParms.uiDefaultQPIndex = static_cast< U8 >(
		std::max( 1, std::min( 255, quality ) ) );
	codecParms.cfColorFormat = YUV_444;
	codecParms.bdBitDepth = BD_LONG;
	codecParms.olOverlap = OL_ONE;
	codecParms.bfBitstreamFormat = SPATIAL;
	codecParms.cChannel = 3;
	codecParms.uAlphaMode = 0;
	codecParms.sbSubband = SB_NO_FLEXBITS;
	codecParms.pWStream = stream;

	CTXSTRCODEC codec = nullptr;
	bool success = ImageStrEncInit( &imageInfo, &codecParms, &codec ) == ICERR_OK;
	for ( int y = 0; success && y < height; y += 16 ) {
		CWMImageBufferInfo bufferInfo{};
		bufferInfo.pv = rgb.data() + static_cast< std::size_t >( y ) * width * 3;
		bufferInfo.cLine = static_cast< std::size_t >( std::min( 16, height - y ) );
		bufferInfo.cbStride = static_cast< std::size_t >( width ) * 3;
		success = ImageStrEncEncode( codec, &bufferInfo ) == ICERR_OK;
	}
	if ( codec != nullptr ) {
		success = ImageStrEncTerm( codec ) == ICERR_OK && success;
	}

	std::size_t fullBytes = 0;
	if ( success ) {
		success = stream->GetPos( stream, &fullBytes ) == WMP_errSuccess &&
			fullBytes > FULL_IMAGE_HEADER_BYTES &&
			fullBytes - FULL_IMAGE_HEADER_BYTES <= static_cast< std::size_t >( outputCapacity );
	}
	if ( success ) {
		*outputBytes = static_cast< int >( fullBytes - FULL_IMAGE_HEADER_BYTES );
		memcpy( output, fullStream.data() + FULL_IMAGE_HEADER_BYTES,
			static_cast< std::size_t >( *outputBytes ) );
	}
	stream->Close( &stream );
	return success;
}

bool JxrLibCompressBareMono( const std::uint8_t * gray,
		std::uint8_t * output, int outputCapacity, int width, int height,
		int inputByteStride, int quality, int * outputBytes ) {
	static const int FULL_IMAGE_HEADER_BYTES = 16;
	if ( outputBytes != nullptr ) {
		*outputBytes = 0;
	}
	if ( gray == nullptr || output == nullptr || outputBytes == nullptr ||
		outputCapacity <= 0 || width <= 0 || height <= 0 || inputByteStride < width ) {
		return false;
	}

	std::vector< std::uint8_t > fullStream(
		static_cast< std::size_t >( outputCapacity ) + FULL_IMAGE_HEADER_BYTES );
	WMPStream * stream = nullptr;
	if ( CreateWS_Memory( &stream, fullStream.data(), fullStream.size() ) != WMP_errSuccess ) {
		return false;
	}

	CWMImageInfo imageInfo{};
	imageInfo.cWidth = static_cast< std::size_t >( width );
	imageInfo.cHeight = static_cast< std::size_t >( height );
	imageInfo.cfColorFormat = Y_ONLY;
	imageInfo.bdBitDepth = BD_8;
	imageInfo.cBitsPerUnit = 8;
	imageInfo.oOrientation = O_NONE;

	CWMIStrCodecParam codecParms{};
	codecParms.uiDefaultQPIndex = static_cast< U8 >(
		std::max( 1, std::min( 255, quality ) ) );
	codecParms.cfColorFormat = Y_ONLY;
	codecParms.bdBitDepth = BD_LONG;
	codecParms.olOverlap = OL_ONE;
	codecParms.bfBitstreamFormat = SPATIAL;
	codecParms.cChannel = 1;
	codecParms.sbSubband = SB_NO_FLEXBITS;
	codecParms.pWStream = stream;

	CTXSTRCODEC codec = nullptr;
	bool success = ImageStrEncInit( &imageInfo, &codecParms, &codec ) == ICERR_OK;
	for ( int y = 0; success && y < height; y += 16 ) {
		CWMImageBufferInfo bufferInfo{};
		bufferInfo.pv = const_cast< std::uint8_t * >( gray + y * inputByteStride );
		bufferInfo.cLine = static_cast< std::size_t >( std::min( 16, height - y ) );
		bufferInfo.cbStride = static_cast< std::size_t >( inputByteStride );
		success = ImageStrEncEncode( codec, &bufferInfo ) == ICERR_OK;
	}
	if ( codec != nullptr ) {
		success = ImageStrEncTerm( codec ) == ICERR_OK && success;
	}
	std::size_t fullBytes = 0;
	if ( success ) {
		success = stream->GetPos( stream, &fullBytes ) == WMP_errSuccess &&
			fullBytes > FULL_IMAGE_HEADER_BYTES &&
			fullBytes - FULL_IMAGE_HEADER_BYTES <= static_cast< std::size_t >( outputCapacity );
	}
	if ( success ) {
		*outputBytes = static_cast< int >( fullBytes - FULL_IMAGE_HEADER_BYTES );
		memcpy( output, fullStream.data() + FULL_IMAGE_HEADER_BYTES,
			static_cast< std::size_t >( *outputBytes ) );
	}
	stream->Close( &stream );
	return success;
}

bool JxrLibDecompressBareRGB( const std::uint8_t * input, int inputBytes,
		std::uint8_t * rgba, int width, int height, int outputByteStride ) {
	static const int FULL_IMAGE_HEADER_BYTES = 16;
	if ( input == nullptr || rgba == nullptr || inputBytes <= 0 ||
		width <= 0 || height <= 0 || width > 65536 || height > 65536 ||
		outputByteStride < width * 4 ) {
		return false;
	}

	std::vector< std::uint8_t > fullStream(
		static_cast< std::size_t >( inputBytes ) + FULL_IMAGE_HEADER_BYTES );
	std::uint8_t * header = fullStream.data();
	const char signature[8] = { 'W', 'M', 'P', 'H', 'O', 'T', 'O', '\0' };
	memcpy( header, signature, sizeof( signature ) );
	header[8] = 0x11; // codec version 1, soft-tile scaling subversion 1
	header[9] = 0x01; // spatial, no index/tiles/window, overlap level one
	header[10] = 0xC0; // abbreviated dimensions and long-word lengths
	header[11] = 0x71; // source RGB, 8-bit unsigned
	header[12] = static_cast< std::uint8_t >( ( width - 1 ) >> 8 );
	header[13] = static_cast< std::uint8_t >( width - 1 );
	header[14] = static_cast< std::uint8_t >( ( height - 1 ) >> 8 );
	header[15] = static_cast< std::uint8_t >( height - 1 );
	memcpy( header + FULL_IMAGE_HEADER_BYTES, input,
		static_cast< std::size_t >( inputBytes ) );

	WMPStream * stream = nullptr;
	if ( CreateWS_Memory( &stream, fullStream.data(), fullStream.size() ) != WMP_errSuccess ) {
		return false;
	}

	CWMImageInfo imageInfo{};
	imageInfo.cWidth = static_cast< std::size_t >( width );
	imageInfo.cHeight = static_cast< std::size_t >( height );
	imageInfo.cThumbnailWidth = imageInfo.cWidth;
	imageInfo.cThumbnailHeight = imageInfo.cHeight;
	imageInfo.cfColorFormat = CF_RGB;
	imageInfo.bdBitDepth = BD_8;
	imageInfo.cBitsPerUnit = 24;
	imageInfo.bRGB = TRUE;
	imageInfo.oOrientation = O_NONE;

	CWMIStrCodecParam codecParms{};
	codecParms.cfColorFormat = YUV_444;
	codecParms.bdBitDepth = BD_LONG;
	codecParms.pWStream = stream;
	codecParms.cbStream = fullStream.size();

	CTXSTRCODEC codec = nullptr;
	bool success = ImageStrDecInit( &imageInfo, &codecParms, &codec ) == ICERR_OK;
	std::vector< std::uint8_t > rgb(
		static_cast< std::size_t >( width ) * height * 3 );
	if ( success ) {
		CWMImageBufferInfo bufferInfo{};
		bufferInfo.pv = rgb.data();
		bufferInfo.cLine = static_cast< std::size_t >( height );
		bufferInfo.cbStride = static_cast< std::size_t >( width ) * 3;
		std::size_t totalDecodedLines = 0;
		const unsigned int macroblockRows =
			static_cast< unsigned int >( ( height + 15 ) / 16 );
		// REENTRANT_MODE has one transform-row of latency. Decode the real
		// macroblock rows plus the synthetic bottom row that flushes the last.
		for ( unsigned int row = 0; success && row <= macroblockRows; ++row ) {
			bufferInfo.uiFirstMBRow = row;
			bufferInfo.uiLastMBRow = row;
			std::size_t decodedLines = 0;
			success = ImageStrDecDecode( codec, &bufferInfo, &decodedLines ) == ICERR_OK;
			totalDecodedLines += decodedLines;
		}
		success = success && totalDecodedLines == static_cast< std::size_t >( height );
	}
	if ( codec != nullptr ) {
		success = ImageStrDecTerm( codec ) == ICERR_OK && success;
	}
	if ( success ) {
		for ( int y = 0; y < height; ++y ) {
			const std::uint8_t * source = rgb.data() +
				static_cast< std::size_t >( y ) * width * 3;
			std::uint8_t * target = rgba + y * outputByteStride;
			for ( int x = 0; x < width; ++x ) {
				target[x * 4 + 0] = source[x * 3 + 0];
				target[x * 4 + 1] = source[x * 3 + 1];
				target[x * 4 + 2] = source[x * 3 + 2];
				target[x * 4 + 3] = 255;
			}
		}
	}
	stream->Close( &stream );
	return success;
}
