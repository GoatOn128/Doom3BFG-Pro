#include "../idlib/precompiled.h"
#pragma hdrstop

#include "virtualtextureencoder.h"
#include "virtualtexture.h"
#include "Image.h"
#include "jobs/transcode/image/baredct/baredct.h"
#include "jobs/transcode/image/barehdp/barehdp.h"
#include "jobs/transcode/image/barehdp/jxrlibencoderbridge.h"
#include "../framework/File.h"
#include "../framework/FileSystem.h"

namespace {

static int AlignUp( int value, int alignment ) {
	return ( value + alignment - 1 ) & ~( alignment - 1 );
}

static void MakeCodecPattern( byte * rgba ) {
	for ( int y = 0; y < idVirtualTextureEncoder::PAGE_TEXELS; ++y ) {
		for ( int x = 0; x < idVirtualTextureEncoder::PAGE_TEXELS; ++x ) {
			byte * pixel = rgba + 4 * ( y * idVirtualTextureEncoder::PAGE_TEXELS + x );
			pixel[0] = static_cast< byte >( ( x * 255 ) / 127 );
			pixel[1] = static_cast< byte >( ( y * 255 ) / 127 );
			pixel[2] = static_cast< byte >( ( ( x ^ y ) * 255 ) / 127 );
			pixel[3] = 255;
		}
	}
}

static bool WriteExact( idFile * file, const void * data, int bytes,
		const char * description, idStr & error ) {
	if ( bytes <= 0 ) {
		return true;
	}
	const int written = file->Write( data, bytes );
	if ( written != bytes ) {
		error.Format( "short write for %s: %i of %i bytes", description, written, bytes );
		return false;
	}
	return true;
}

static bool WriteZeroes( idFile * file, int bytes, idStr & error ) {
	static const byte zeroes[4096] = { 0 };
	while ( bytes > 0 ) {
		const int count = Min( bytes, static_cast< int >( sizeof( zeroes ) ) );
		if ( !WriteExact( file, zeroes, count, "alignment padding", error ) ) {
			return false;
		}
		bytes -= count;
	}
	return true;
}

static void MakeSolidPage( idList< byte > & page, byte r, byte g, byte b, byte a ) {
	page.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
	for ( int pixel = 0; pixel < idVirtualTextureEncoder::PAGE_TEXELS *
			idVirtualTextureEncoder::PAGE_TEXELS; ++pixel ) {
		page[pixel * 4 + 0] = r;
		page[pixel * 4 + 1] = g;
		page[pixel * 4 + 2] = b;
		page[pixel * 4 + 3] = a;
	}
}

struct decodedVTImage_t {
	idList< byte > rgba;
	int width;
	int height;
	bool loaded;
};

static void LoadVTImage( const char * generatedName, decodedVTImage_t & image ) {
	image.width = 0;
	image.height = 0;
	image.loaded = generatedName != NULL && generatedName[0] != '\0' &&
		idImage::LoadRGBAForVirtualTextureGenerated( generatedName, image.rgba,
			image.width, image.height );
}

static bool ResamplePage( const decodedVTImage_t & image, byte defaultR,
		byte defaultG, byte defaultB, int tileX, int tileY,
		int tilesWide, int tilesHigh, idList< byte > & page ) {
	if ( !image.loaded ) {
		MakeSolidPage( page, defaultR, defaultG, defaultB, 255 );
		return false;
	}
	const int width = image.width;
	const int height = image.height;
	const byte * source = image.rgba.Ptr();

	page.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
	const int pageBorder = 4;
	const int pageInterior = idVirtualTextureEncoder::PAGE_TEXELS - pageBorder * 2;
	// Physical pages are 128x128, with a four-texel border around the 120x120
	// virtual interior. For multi-page materials the border samples across the
	// neighboring tile and clamps only at the outer material edge.
	for ( int y = 0; y < idVirtualTextureEncoder::PAGE_TEXELS; ++y ) {
		const int virtualY = idMath::ClampInt( 0, tilesHigh * pageInterior - 1,
			tileY * pageInterior + y - pageBorder );
		const float sourceY = ( ( virtualY + 0.5f ) * height /
			static_cast< float >( tilesHigh * pageInterior ) ) - 0.5f;
		const int y0 = idMath::ClampInt( 0, height - 1, idMath::Ftoi( idMath::Floor( sourceY ) ) );
		const int y1 = Min( y0 + 1, height - 1 );
		const float fy = idMath::ClampFloat( 0.0f, 1.0f, sourceY - idMath::Floor( sourceY ) );
		for ( int x = 0; x < idVirtualTextureEncoder::PAGE_TEXELS; ++x ) {
			const int virtualX = idMath::ClampInt( 0, tilesWide * pageInterior - 1,
				tileX * pageInterior + x - pageBorder );
			const float sourceX = ( ( virtualX + 0.5f ) * width /
				static_cast< float >( tilesWide * pageInterior ) ) - 0.5f;
			const int x0 = idMath::ClampInt( 0, width - 1, idMath::Ftoi( idMath::Floor( sourceX ) ) );
			const int x1 = Min( x0 + 1, width - 1 );
			const float fx = idMath::ClampFloat( 0.0f, 1.0f, sourceX - idMath::Floor( sourceX ) );
			for ( int channel = 0; channel < 4; ++channel ) {
				const float top = source[( y0 * width + x0 ) * 4 + channel] * ( 1.0f - fx ) +
					source[( y0 * width + x1 ) * 4 + channel] * fx;
				const float bottom = source[( y1 * width + x0 ) * 4 + channel] * ( 1.0f - fx ) +
					source[( y1 * width + x1 ) * 4 + channel] * fx;
				page[( y * idVirtualTextureEncoder::PAGE_TEXELS + x ) * 4 + channel] =
					static_cast< byte >( idMath::ClampInt( 0, 255,
						idMath::Ftoi( top * ( 1.0f - fy ) + bottom * fy + 0.5f ) ) );
			}
		}
	}
	return true;
}

static void BuildMaterialPlanePages( const char * generatedName, byte defaultR,
		byte defaultG, byte defaultB, int tilesWide, int tilesHigh,
		idList< byte > & materialPages ) {
	decodedVTImage_t source;
	LoadVTImage( generatedName, source );
	const int tileCount = tilesWide * tilesHigh;
	materialPages.SetNum( tileCount * idVirtualTextureEncoder::PAGE_RGBA_BYTES );
	idList< byte > page;
	for ( int tileY = 0; tileY < tilesHigh; ++tileY ) {
		for ( int tileX = 0; tileX < tilesWide; ++tileX ) {
			ResamplePage( source, defaultR, defaultG, defaultB, tileX, tileY,
				tilesWide, tilesHigh, page );
			const int tileIndex = tileY * tilesWide + tileX;
			memcpy( materialPages.Ptr() + tileIndex * idVirtualTextureEncoder::PAGE_RGBA_BYTES,
				page.Ptr(), idVirtualTextureEncoder::PAGE_RGBA_BYTES );
		}
	}
}

static bool EncodeHDPPagePayload( const byte * diffuseRGBA,
		const byte * normalRGBA, const byte * specularRGBA, int quality,
		idList< byte > & payload, idStr & error ) {
	idList< byte > diffuseStream;
	idList< byte > normalStream;
	idList< byte > specularStream;
	idList< byte > powerStream;
	idList< byte > powerSource;
	diffuseStream.SetNum( 65535 );
	normalStream.SetNum( 65535 );
	specularStream.SetNum( 65535 );
	powerStream.SetNum( 65535 );
	powerSource.SetNum( 32 * 32 );

	for ( int blockY = 0; blockY < 32; ++blockY ) {
		for ( int blockX = 0; blockX < 32; ++blockX ) {
			int sum = 0;
			for ( int y = 0; y < 4; ++y ) {
				for ( int x = 0; x < 4; ++x ) {
					const int sourceX = blockX * 4 + x;
					const int sourceY = blockY * 4 + y;
					sum += normalRGBA[( sourceY * idVirtualTextureEncoder::PAGE_TEXELS + sourceX ) * 4];
				}
			}
			powerSource[blockY * 32 + blockX] = static_cast< byte >( ( sum + 8 ) >> 4 );
		}
	}

	idBareHDPEncoder encoder;
	encoder.quality = quality;
	int diffuseBytes = 0;
	int normalBytes = 0;
	int specularBytes = 0;
	int powerBytes = 0;
	if ( !encoder.CompressImageYCoCg( diffuseRGBA, diffuseStream.Ptr(),
			diffuseStream.Num(), idVirtualTextureEncoder::PAGE_TEXELS,
			idVirtualTextureEncoder::PAGE_TEXELS,
			idVirtualTextureEncoder::PAGE_TEXELS * 4, &diffuseBytes ) ||
			!encoder.CompressImageYCoCg( normalRGBA, normalStream.Ptr(),
			normalStream.Num(), idVirtualTextureEncoder::PAGE_TEXELS,
			idVirtualTextureEncoder::PAGE_TEXELS,
			idVirtualTextureEncoder::PAGE_TEXELS * 4, &normalBytes ) ||
			!encoder.CompressImageYCoCg( specularRGBA, specularStream.Ptr(),
			specularStream.Num(), idVirtualTextureEncoder::PAGE_TEXELS,
			idVirtualTextureEncoder::PAGE_TEXELS,
			idVirtualTextureEncoder::PAGE_TEXELS * 4, &specularBytes ) ||
			!encoder.CompressImageMono( powerSource.Ptr(), powerStream.Ptr(),
			powerStream.Num(), 32, 32, 32, &powerBytes ) ) {
		error = "failed to encode one or more HDP page planes";
		return false;
	}
	if ( diffuseBytes <= 0 || normalBytes <= 0 || specularBytes <= 0 ||
			powerBytes <= 0 || diffuseBytes > 65535 || normalBytes > 65535 ||
			specularBytes > 65535 || powerBytes > 65535 ) {
		error = "HDP stream size does not fit the Tech 5 page header";
		return false;
	}

	HDPHeader_t header;
	memset( &header, 0, sizeof( header ) );
	header.qualityDiffuse = static_cast< uint8 >( quality );
	header.qualityNormal = static_cast< uint8 >( quality );
	header.qualitySpecular = static_cast< uint8 >( quality );
	header.qualityPower = static_cast< uint8 >( quality );
	header.diffuseSize = static_cast< uint16 >( BigShort( static_cast< short >( diffuseBytes ) ) );
	header.normalSize = static_cast< uint16 >( BigShort( static_cast< short >( normalBytes ) ) );
	header.specularSize = static_cast< uint16 >( BigShort( static_cast< short >( specularBytes ) ) );
	header.powerSize = static_cast< uint16 >( BigShort( static_cast< short >( powerBytes ) ) );

	payload.SetNum( sizeof( header ) + diffuseBytes + normalBytes + specularBytes + powerBytes );
	byte * cursor = payload.Ptr();
	memcpy( cursor, &header, sizeof( header ) );
	cursor += sizeof( header );
	memcpy( cursor, diffuseStream.Ptr(), diffuseBytes );
	cursor += diffuseBytes;
	memcpy( cursor, normalStream.Ptr(), normalBytes );
	cursor += normalBytes;
	memcpy( cursor, specularStream.Ptr(), specularBytes );
	cursor += specularBytes;
	memcpy( cursor, powerStream.Ptr(), powerBytes );
	return true;
}

struct archivePageRef_t {
	uint32 offset;
	uint16 length;
};

} // namespace

bool idVirtualTextureEncoder::WriteUncompressedPageFile( const char * osPath,
		const byte * diffuseRGBA, const byte * normalRGBA,
		const byte * specularRGBA, idStr & error ) const {
	error.Clear();
	if ( osPath == NULL || osPath[0] == '\0' || diffuseRGBA == NULL ||
			normalRGBA == NULL || specularRGBA == NULL ) {
		error = "invalid output path or page planes";
		return false;
	}

	const int diskOffsetScale = 64;
	const int pageOffset = AlignUp( sizeof( pageFileHeader_t ), diskOffsetScale );
	const int pageBytes = sizeof( pageHeader_t ) + 3 * PAGE_RGBA_BYTES;
	const int storedPageBytes = AlignUp( pageBytes, diskOffsetScale );
	const int totalBytes = pageOffset + storedPageBytes;

	pageFileHeader_t header;
	memset( &header, 0, sizeof( header ) );
	header.magic = PAGE_FILE_MAGIC;
	header.pagesWide = 1;
	header.numLevels = 1;
	header.layoutVersion = 0;
	header.totalPages = 1;
	header.diskOffsetScale = diskOffsetScale;
	header.totalFileSize = totalBytes;
	header.subRoot.magic = PAGE_SUBROOT_MAGIC;
	header.subRoot.pageCompression = COMP_NONE;
	header.subRoot.finerDiskOffset[0] = pageOffset / diskOffsetScale;
	header.subRoot.finerDiskLength[0] = storedPageBytes / diskOffsetScale;

	pageHeader_t page;
	memset( &page, 0, sizeof( page ) );
	page.magic = PAGE_SUBROOT_MAGIC;
	page.pageCompression = COMP_NONE;
	page.x = 0;
	page.y = 0;
	page.level = 0;

	idList< byte > fileData;
	fileData.SetNum( totalBytes );
	memset( fileData.Ptr(), 0, fileData.Num() );

	pageFileHeader_t diskHeader = header;
	diskHeader.Swap();
	pageHeader_t diskPage = page;
	diskPage.Swap();
	memcpy( fileData.Ptr(), &diskHeader, sizeof( diskHeader ) );
	memcpy( fileData.Ptr() + pageOffset, &diskPage, sizeof( diskPage ) );
	byte * payload = fileData.Ptr() + pageOffset + sizeof( diskPage );
	memcpy( payload, diffuseRGBA, PAGE_RGBA_BYTES );
	memcpy( payload + PAGE_RGBA_BYTES, normalRGBA, PAGE_RGBA_BYTES );
	memcpy( payload + 2 * PAGE_RGBA_BYTES, specularRGBA, PAGE_RGBA_BYTES );

	idFile * output = fileSystem->OpenExplicitFileWrite( osPath );
	if ( output == NULL ) {
		error.Format( "could not open '%s' for writing", osPath );
		return false;
	}
	const int written = output->Write( fileData.Ptr(), fileData.Num() );
	delete output;
	if ( written != fileData.Num() ) {
		error.Format( "short write to '%s': %i of %i bytes", osPath, written, fileData.Num() );
		return false;
	}
	return true;
}

bool idVirtualTextureEncoder::WriteHDPPageFile( const char * osPath,
		const byte * diffuseRGBA, const byte * normalRGBA,
		const byte * specularRGBA, int quality, idStr & error ) const {
	error.Clear();
	if ( osPath == NULL || osPath[0] == '\0' || diffuseRGBA == NULL ||
			normalRGBA == NULL || specularRGBA == NULL ) {
		error = "invalid output path or page planes";
		return false;
	}
	quality = idMath::ClampInt( 1, 255, quality );

	idList< byte > diffuseStream;
	idList< byte > normalStream;
	idList< byte > specularStream;
	idList< byte > powerStream;
	idList< byte > powerSource;
	diffuseStream.SetNum( 65535 );
	normalStream.SetNum( 65535 );
	specularStream.SetNum( 65535 );
	powerStream.SetNum( 65535 );
	powerSource.SetNum( 32 * 32 );

	// The power plane supplies one normal-X sample for each 4x4 texel block.
	for ( int blockY = 0; blockY < 32; ++blockY ) {
		for ( int blockX = 0; blockX < 32; ++blockX ) {
			int sum = 0;
			for ( int y = 0; y < 4; ++y ) {
				for ( int x = 0; x < 4; ++x ) {
					const int sourceX = blockX * 4 + x;
					const int sourceY = blockY * 4 + y;
					sum += normalRGBA[( sourceY * PAGE_TEXELS + sourceX ) * 4 + 0];
				}
			}
			powerSource[blockY * 32 + blockX] = static_cast< byte >( ( sum + 8 ) >> 4 );
		}
	}

	idBareHDPEncoder encoder;
	encoder.quality = quality;
	int diffuseBytes = 0;
	int normalBytes = 0;
	int specularBytes = 0;
	int powerBytes = 0;
	if ( !encoder.CompressImageYCoCg( diffuseRGBA, diffuseStream.Ptr(),
			diffuseStream.Num(), PAGE_TEXELS, PAGE_TEXELS, PAGE_TEXELS * 4,
			&diffuseBytes ) ||
			!encoder.CompressImageYCoCg( normalRGBA, normalStream.Ptr(),
			normalStream.Num(), PAGE_TEXELS, PAGE_TEXELS, PAGE_TEXELS * 4,
			&normalBytes ) ||
			!encoder.CompressImageYCoCg( specularRGBA, specularStream.Ptr(),
			specularStream.Num(), PAGE_TEXELS, PAGE_TEXELS, PAGE_TEXELS * 4,
			&specularBytes ) ||
			!encoder.CompressImageMono( powerSource.Ptr(), powerStream.Ptr(),
			powerStream.Num(), 32, 32, 32, &powerBytes ) ) {
		error = "failed to encode one or more HDP page planes";
		return false;
	}
	if ( diffuseBytes <= 0 || normalBytes <= 0 || specularBytes <= 0 ||
			powerBytes <= 0 || diffuseBytes > 65535 || normalBytes > 65535 ||
			specularBytes > 65535 || powerBytes > 65535 ) {
		error = "HDP stream size does not fit the Tech 5 page header";
		return false;
	}

	HDPHeader_t hdpHeader;
	memset( &hdpHeader, 0, sizeof( hdpHeader ) );
	hdpHeader.qualityDiffuse = static_cast< uint8 >( quality );
	hdpHeader.qualityNormal = static_cast< uint8 >( quality );
	hdpHeader.qualitySpecular = static_cast< uint8 >( quality );
	hdpHeader.qualityPower = static_cast< uint8 >( quality );
	// Tech 5 stores these five lengths in network order inside an otherwise
	// little-endian page container.
	hdpHeader.diffuseSize = static_cast< uint16 >( BigShort( static_cast< short >( diffuseBytes ) ) );
	hdpHeader.normalSize = static_cast< uint16 >( BigShort( static_cast< short >( normalBytes ) ) );
	hdpHeader.specularSize = static_cast< uint16 >( BigShort( static_cast< short >( specularBytes ) ) );
	hdpHeader.powerSize = static_cast< uint16 >( BigShort( static_cast< short >( powerBytes ) ) );

	const int diskOffsetScale = 64;
	const int pageOffset = AlignUp( sizeof( pageFileHeader_t ), diskOffsetScale );
	const int payloadBytes = diffuseBytes + normalBytes + specularBytes + powerBytes;
	const int pageBytes = sizeof( pageHeader_t ) + sizeof( HDPHeader_t ) + payloadBytes;
	const int storedPageBytes = AlignUp( pageBytes, diskOffsetScale );
	const int totalBytes = pageOffset + storedPageBytes;

	pageFileHeader_t header;
	memset( &header, 0, sizeof( header ) );
	header.magic = PAGE_FILE_MAGIC;
	header.pagesWide = 1;
	header.numLevels = 1;
	header.totalPages = 1;
	header.diskOffsetScale = diskOffsetScale;
	header.totalFileSize = totalBytes;
	header.subRoot.magic = PAGE_SUBROOT_MAGIC;
	header.subRoot.pageCompression = COMP_HDP;
	header.subRoot.finerDiskOffset[0] = pageOffset / diskOffsetScale;
	header.subRoot.finerDiskLength[0] = storedPageBytes / diskOffsetScale;

	pageHeader_t page;
	memset( &page, 0, sizeof( page ) );
	page.magic = PAGE_SUBROOT_MAGIC;
	page.pageCompression = COMP_HDP;

	idList< byte > fileData;
	fileData.SetNum( totalBytes );
	memset( fileData.Ptr(), 0, fileData.Num() );
	pageFileHeader_t diskHeader = header;
	diskHeader.Swap();
	pageHeader_t diskPage = page;
	diskPage.Swap();
	memcpy( fileData.Ptr(), &diskHeader, sizeof( diskHeader ) );
	byte * cursor = fileData.Ptr() + pageOffset;
	memcpy( cursor, &diskPage, sizeof( diskPage ) );
	cursor += sizeof( diskPage );
	memcpy( cursor, &hdpHeader, sizeof( hdpHeader ) );
	cursor += sizeof( hdpHeader );
	memcpy( cursor, diffuseStream.Ptr(), diffuseBytes );
	cursor += diffuseBytes;
	memcpy( cursor, normalStream.Ptr(), normalBytes );
	cursor += normalBytes;
	memcpy( cursor, specularStream.Ptr(), specularBytes );
	cursor += specularBytes;
	memcpy( cursor, powerStream.Ptr(), powerBytes );

	idFile * output = fileSystem->OpenExplicitFileWrite( osPath );
	if ( output == NULL ) {
		error.Format( "could not open '%s' for writing", osPath );
		return false;
	}
	const int written = output->Write( fileData.Ptr(), fileData.Num() );
	delete output;
	if ( written != fileData.Num() ) {
		error.Format( "short write to '%s': %i of %i bytes", osPath, written, fileData.Num() );
		return false;
	}
	return true;
}

bool idVirtualTextureEncoder::WriteHDPPageArchive( const char * pagesOSPath,
		const char * vmtrOSPath, const vtHDPPageSource_t * sources,
		int sourceCount, int quality, idStr & error ) const {
	error.Clear();
	if ( pagesOSPath == NULL || pagesOSPath[0] == '\0' || vmtrOSPath == NULL ||
			vmtrOSPath[0] == '\0' || sources == NULL || sourceCount <= 0 ) {
		error = "invalid VMTR archive paths or source list";
		return false;
	}
	quality = idMath::ClampInt( 1, 255, quality );

	struct materialAllocation_t {
		int x, y;
		int pagesWide, pagesHigh;
	};
	static const int maxMaterialPagesPerAxis = 4;
	idList< materialAllocation_t > allocations;
	idList< int > allocationOrder;
	allocations.SetNum( sourceCount );
	allocationOrder.SetNum( sourceCount );
	for ( int index = 0; index < sourceCount; ++index ) {
		const int imageWidth = Max( 1, sources[index].width );
		const int imageHeight = Max( 1, sources[index].height );
		allocations[index].x = allocations[index].y = 0;
		allocations[index].pagesWide = idMath::ClampInt( 1, maxMaterialPagesPerAxis,
			( imageWidth + 119 ) / 120 );
		allocations[index].pagesHigh = idMath::ClampInt( 1, maxMaterialPagesPerAxis,
			( imageHeight + 119 ) / 120 );
		allocationOrder[index] = index;
	}
	std::sort( allocationOrder.Ptr(), allocationOrder.Ptr() + allocationOrder.Num(),
		[&allocations]( int a, int b ) {
			if ( allocations[a].pagesHigh != allocations[b].pagesHigh ) {
				return allocations[a].pagesHigh > allocations[b].pagesHigh;
			}
			return allocations[a].pagesWide > allocations[b].pagesWide;
		} );

	int pagesWide = 1;
	for ( ;; pagesWide <<= 1 ) {
		int cursorX = 0;
		int cursorY = 0;
		int rowHeight = 0;
		bool fits = true;
		for ( int orderIndex = 0; orderIndex < allocationOrder.Num(); ++orderIndex ) {
			materialAllocation_t & allocation = allocations[allocationOrder[orderIndex]];
			if ( cursorX + allocation.pagesWide > pagesWide ) {
				cursorX = 0;
				cursorY += rowHeight;
				rowHeight = 0;
			}
			if ( cursorY + allocation.pagesHigh > pagesWide ) {
				fits = false;
				break;
			}
			allocation.x = cursorX;
			allocation.y = cursorY;
			cursorX += allocation.pagesWide;
			rowHeight = Max( rowHeight, allocation.pagesHigh );
		}
		if ( fits ) { break; }
	}
	idList< int > leafOwner;
	idList< byte > leafTileX;
	idList< byte > leafTileY;
	leafOwner.SetNum( pagesWide * pagesWide );
	leafTileX.SetNum( pagesWide * pagesWide );
	leafTileY.SetNum( pagesWide * pagesWide );
	for ( int i = 0; i < leafOwner.Num(); ++i ) { leafOwner[i] = -1; }
	for ( int materialIndex = 0; materialIndex < sourceCount; ++materialIndex ) {
		const materialAllocation_t & allocation = allocations[materialIndex];
		for ( int tileY = 0; tileY < allocation.pagesHigh; ++tileY ) {
			for ( int tileX = 0; tileX < allocation.pagesWide; ++tileX ) {
				const int page = ( allocation.y + tileY ) * pagesWide + allocation.x + tileX;
				leafOwner[page] = materialIndex;
				leafTileX[page] = static_cast< byte >( tileX );
				leafTileY[page] = static_cast< byte >( tileY );
			}
		}
	}
	int numLevels = 1;
	for ( int width = pagesWide; width > 1; width >>= 1 ) {
		++numLevels;
	}

	int levelStarts[32];
	int levelWidths[32];
	int totalPages = 0;
	for ( int level = 0, width = pagesWide; level < numLevels; ++level, width >>= 1 ) {
		levelStarts[level] = totalPages;
		levelWidths[level] = width;
		totalPages += width * width;
	}
	idList< archivePageRef_t > pageRefs;
	pageRefs.SetNum( totalPages );
	memset( pageRefs.Ptr(), 0, pageRefs.Num() * sizeof( pageRefs[0] ) );

	idList< byte > neutralDiffuse;
	idList< byte > neutralNormal;
	idList< byte > neutralSpecular;
	idList< byte > neutralPayload;
	MakeSolidPage( neutralDiffuse, 255, 255, 255, 255 );
	MakeSolidPage( neutralNormal, 128, 128, 255, 255 );
	MakeSolidPage( neutralSpecular, 0, 0, 0, 255 );
	if ( !EncodeHDPPagePayload( neutralDiffuse.Ptr(), neutralNormal.Ptr(),
			neutralSpecular.Ptr(), quality, neutralPayload, error ) ) {
		return false;
	}

	fileSystem->CreateOSPath( pagesOSPath );
	idFile * output = fileSystem->OpenExplicitFileWrite( pagesOSPath );
	if ( output == NULL ) {
		error.Format( "could not open '%s' for writing", pagesOSPath );
		return false;
	}
	const int diskOffsetScale = 64;
	const int firstPageOffset = AlignUp( sizeof( pageFileHeader_t ), diskOffsetScale );
	if ( !WriteZeroes( output, firstPageOffset, error ) ) {
		delete output;
		return false;
	}

	// Page offsets, rather than physical record order, define the tree.  Write
	// all leaves belonging to one material together so each generated image is
	// decoded exactly once instead of once per 128x128 tile.  A 4x4 material used
	// to perform 48 full image reads/decompressions here.
	auto WritePage = [&]( int x, int y, int level, const idList< byte > & payload,
			const char * description ) -> bool {
		const int width = levelWidths[level];
		const int nodeIndex = levelStarts[level] + y * width + x;
		const int64 pageOffset = output->Tell64();
		if ( ( pageOffset & ( diskOffsetScale - 1 ) ) != 0 ||
				pageOffset / diskOffsetScale > 0xFFFFFFFFLL ) {
			error = "VMTR archive offset exceeds the page-file ABI";
			return false;
		}

		pageHeader_t page;
		memset( &page, 0, sizeof( page ) );
		page.magic = PAGE_SUBROOT_MAGIC;
		page.pageCompression = COMP_HDP;
		page.x = static_cast< uint16 >( x );
		page.y = static_cast< uint16 >( y );
		page.level = static_cast< uint16 >( level );
		if ( level > 0 ) {
			const int childWidth = levelWidths[level - 1];
			for ( int quadrant = 0; quadrant < 4; ++quadrant ) {
				const int childX = x * 2 + ( quadrant & 1 );
				const int childY = y * 2 + ( ( quadrant >> 1 ) & 1 );
				const archivePageRef_t & child = pageRefs[levelStarts[level - 1] +
					childY * childWidth + childX];
				page.finerDiskOffset[quadrant] = child.offset;
				page.finerDiskLength[quadrant] = child.length;
			}
		}

		const int pageBytes = sizeof( pageHeader_t ) + payload.Num();
		const int storedBytes = AlignUp( pageBytes, diskOffsetScale );
		if ( storedBytes / diskOffsetScale > 0xFFFF ) {
			error.Format( "encoded page for '%s' is too large for the page-file ABI",
				description );
			return false;
		}
		pageRefs[nodeIndex].offset = static_cast< uint32 >( pageOffset / diskOffsetScale );
		pageRefs[nodeIndex].length = static_cast< uint16 >( storedBytes / diskOffsetScale );
		pageHeader_t diskPage = page;
		diskPage.Swap();
		return WriteExact( output, &diskPage, sizeof( diskPage ), "page header", error ) &&
			WriteExact( output, payload.Ptr(), payload.Num(), "HDP payload", error ) &&
			WriteZeroes( output, storedBytes - pageBytes, error );
	};

	idList< byte > diffusePages;
	idList< byte > normalPages;
	idList< byte > specularPages;
	idList< byte > encodedPayload;
	idLib::Printf( "Building VMTR pages: 0/%i materials (0%%)\n", sourceCount );
	const int progressStep = Max( 1, sourceCount / 20 );
	for ( int materialIndex = 0; materialIndex < sourceCount; ++materialIndex ) {
		const materialAllocation_t & allocation = allocations[materialIndex];
		// Only one potentially large generated source is alive at a time.  The
		// resampled material planes are bounded to 16 physical pages each.
		BuildMaterialPlanePages( sources[materialIndex].diffuseGeneratedName,
			255, 255, 255, allocation.pagesWide, allocation.pagesHigh, diffusePages );
		BuildMaterialPlanePages( sources[materialIndex].normalGeneratedName,
			128, 128, 255, allocation.pagesWide, allocation.pagesHigh, normalPages );
		BuildMaterialPlanePages( sources[materialIndex].specularGeneratedName,
			0, 0, 0, allocation.pagesWide, allocation.pagesHigh, specularPages );
		for ( int tileY = 0; tileY < allocation.pagesHigh; ++tileY ) {
			for ( int tileX = 0; tileX < allocation.pagesWide; ++tileX ) {
				const int tileIndex = tileY * allocation.pagesWide + tileX;
				const int tileOffset = tileIndex * PAGE_RGBA_BYTES;
				if ( !EncodeHDPPagePayload( diffusePages.Ptr() + tileOffset,
						normalPages.Ptr() + tileOffset, specularPages.Ptr() + tileOffset,
						quality, encodedPayload, error ) ||
						!WritePage( allocation.x + tileX, allocation.y + tileY, 0,
							encodedPayload, sources[materialIndex].materialName ) ) {
					error.Append( va( " while encoding material '%s'",
						sources[materialIndex].materialName ) );
					delete output;
					return false;
				}
			}
		}
		common->UpdateLevelLoadPacifier();
		if ( materialIndex + 1 == sourceCount ||
				( materialIndex + 1 ) % progressStep == 0 ) {
			idLib::Printf( "Building VMTR pages: %i/%i materials (%i%%)\n",
				materialIndex + 1, sourceCount,
				( materialIndex + 1 ) * 100 / sourceCount );
		}
	}

	// Fill unused leaves after all owned leaves have their final offsets.
	for ( int y = 0; y < pagesWide; ++y ) {
		for ( int x = 0; x < pagesWide; ++x ) {
			if ( leafOwner[y * pagesWide + x] < 0 &&
					!WritePage( x, y, 0, neutralPayload, "neutral leaf" ) ) {
				delete output;
				return false;
			}
		}
	}
	// Parent pages are neutral but contain the offsets of the four finer pages.
	for ( int level = 1; level < numLevels; ++level ) {
		const int width = levelWidths[level];
		for ( int y = 0; y < width; ++y ) {
			for ( int x = 0; x < width; ++x ) {
				if ( !WritePage( x, y, level, neutralPayload, "neutral mip" ) ) {
					delete output;
					return false;
				}
			}
		}
	}

	pageFileHeader_t header;
	memset( &header, 0, sizeof( header ) );
	header.magic = PAGE_FILE_MAGIC;
	header.pagesWide = pagesWide;
	header.numLevels = static_cast< int16 >( numLevels );
	header.totalPages = totalPages;
	header.diskOffsetScale = diskOffsetScale;
	header.totalFileSize = output->Tell64();
	header.subRoot.magic = PAGE_SUBROOT_MAGIC;
	header.subRoot.pageCompression = COMP_HDP;
	const archivePageRef_t & root = pageRefs[levelStarts[numLevels - 1]];
	header.subRoot.finerDiskOffset[0] = root.offset;
	header.subRoot.finerDiskLength[0] = root.length;
	pageFileHeader_t diskHeader = header;
	diskHeader.Swap();
	if ( output->Seek64( 0, FS_SEEK_SET ) != 0 ||
			!WriteExact( output, &diskHeader, sizeof( diskHeader ), "archive header", error ) ) {
		delete output;
		return false;
	}
	delete output;

	fileSystem->CreateOSPath( vmtrOSPath );
	idFile * metadata = fileSystem->OpenExplicitFileWrite( vmtrOSPath );
	if ( metadata == NULL ) {
		error.Format( "could not open '%s' for writing", vmtrOSPath );
		return false;
	}
	metadata->Printf( "%i\n%i\n", idVMTRData::VMTR_FILE_VERSION, sourceCount );
	for ( int index = 0; index < sourceCount; ++index ) {
		const materialAllocation_t & allocation = allocations[index];
		metadata->Printf( "%i %i %i %i 0 0 %i \"%s\"\n",
			allocation.x * 120, allocation.y * 120,
			allocation.pagesWide * 120, allocation.pagesHigh * 120,
			idStr::Hash( sources[index].materialName ),
			sources[index].materialName );
	}
	delete metadata;
	return true;
}

bool idVirtualTextureEncoder::TestDCTRoundTrip( int quality,
		int & encodedBytes, float & meanAbsoluteError, int & maximumError,
		idStr & error ) const {
	error.Clear();
	encodedBytes = 0;
	meanAbsoluteError = 0.0f;
	maximumError = 0;
	quality = idMath::ClampInt( 1, 100, quality );

	idList< byte > source;
	idList< byte > decoded;
	idList< byte > encoded;
	source.SetNum( PAGE_RGBA_BYTES );
	decoded.SetNum( PAGE_RGBA_BYTES );
	// The reconstructed encoder has no capacity parameter. Eight bytes per
	// source pixel plus headers safely bounds this 128x128 development test.
	encoded.SetNum( PAGE_RGBA_BYTES * 2 + 4096 );
	MakeCodecPattern( source.Ptr() );
	memset( decoded.Ptr(), 0, decoded.Num() );
	memset( encoded.Ptr(), 0, encoded.Num() );

	idBareDctEncoder encoder;
	encoder.SetQuality_SSE2( quality, quality, quality );
	if ( !encoder.CompressImageRGB_JPEG( source.Ptr(), encoded.Ptr(),
			PAGE_TEXELS, PAGE_TEXELS, &encodedBytes, false ) ||
			encodedBytes <= 607 || encodedBytes > encoded.Num() ) {
		error = "idBareDctEncoder failed or returned an invalid byte count";
		return false;
	}

	idBareDCTHuffmanTable ydc( HUFFMAN_TYPE_Y_DC, false );
	idBareDCTHuffmanTable yac( HUFFMAN_TYPE_Y_AC, false );
	idBareDCTHuffmanTable cocgdc( HUFFMAN_TYPE_COCG_DC, false );
	idBareDCTHuffmanTable cocgac( HUFFMAN_TYPE_COCG_AC, false );
	idBareDctDecoder decoder;
	decoder.SetHuffmanTables( &ydc, &yac, &cocgdc, &cocgac, NULL, NULL );
	if ( !decoder.DecompressImageRGB_JPEG( encoded.Ptr(), decoded.Ptr(),
			PAGE_TEXELS, PAGE_TEXELS, encodedBytes ) ) {
		error = "idBareDctDecoder rejected the encoded stream";
		return false;
	}

	uint64 totalError = 0;
	for ( int y = 0; y < PAGE_TEXELS; ++y ) {
		// The Tech 5 JPEG helper deliberately writes source rows bottom-up.
		const byte * expected = source.Ptr() + ( PAGE_TEXELS - 1 - y ) * PAGE_TEXELS * 4;
		const byte * actual = decoded.Ptr() + y * PAGE_TEXELS * 4;
		for ( int x = 0; x < PAGE_TEXELS; ++x ) {
			for ( int channel = 0; channel < 3; ++channel ) {
				const int difference = idMath::Abs( static_cast< int >( expected[x * 4 + channel] ) -
					static_cast< int >( actual[x * 4 + channel] ) );
				totalError += difference;
				maximumError = Max( maximumError, difference );
			}
		}
	}
	meanAbsoluteError = static_cast< float >( totalError ) /
		static_cast< float >( PAGE_TEXELS * PAGE_TEXELS * 3 );
	return true;
}

bool idVirtualTextureEncoder::TestHDPRoundTrip( int quality,
		int & encodedBytes, unsigned int & decodedChecksum, idStr & error ) const {
	error.Clear();
	encodedBytes = 0;
	decodedChecksum = 0;

	idList< byte > encoded;
	idList< byte > decoded;
	idList< byte > source;
	encoded.SetNum( 256 * 1024 );
	decoded.SetNum( PAGE_RGBA_BYTES );
	source.SetNum( PAGE_RGBA_BYTES );
	memset( encoded.Ptr(), 0, encoded.Num() );
	memset( decoded.Ptr(), 0xCD, decoded.Num() );
	for ( int y = 0; y < PAGE_TEXELS; ++y ) {
		for ( int x = 0; x < PAGE_TEXELS; ++x ) {
			const int offset = ( y * PAGE_TEXELS + x ) * 4;
			// Gradients plus sharp macroblock-scale edges exercise DC, LP, HP,
			// prediction, adaptive CBP, and entropy coding in one compact page.
			source[offset + 0] = static_cast< byte >( ( x * 2 + ( y & 31 ) * 5 ) & 255 );
			source[offset + 1] = static_cast< byte >( ( y * 2 + ( x & 15 ) * 11 ) & 255 );
			source[offset + 2] = static_cast< byte >( ( ( x ^ y ) * 7 ) & 255 );
			source[offset + 3] = 255;
		}
	}

	idBareHDPEncoder encoder;
	encoder.quality = idMath::ClampInt( 1, 255, quality );
	if ( !encoder.CompressImageYCoCg( source.Ptr(), encoded.Ptr(), encoded.Num(),
			PAGE_TEXELS, PAGE_TEXELS, PAGE_TEXELS * 4, &encodedBytes ) ) {
		error = "idBareHDPEncoder failed to encode the patterned RGBA page";
		return false;
	}

	if ( !JxrLibDecompressBareRGB( encoded.Ptr(), encodedBytes, decoded.Ptr(),
			PAGE_TEXELS, PAGE_TEXELS, PAGE_TEXELS * 4 ) ) {
		error.Format( "JPEG XR reference decoder rejected the generated bare stream "
			"(bytes %02x %02x %02x %02x %02x %02x %02x %02x)",
			encoded[0], encoded[1], encoded[2], encoded[3], encoded[4], encoded[5],
			encoded[6], encoded[7] );
		return false;
	}

	bool spatialVariation = false;
	uint64 totalError = 0;
	for ( int index = 0; index < decoded.Num(); ++index ) {
		decodedChecksum = decodedChecksum * 16777619U ^ decoded[index];
		if ( index >= 4 && decoded[index] != decoded[index & 3] ) {
			spatialVariation = true;
		}
		if ( ( index & 3 ) != 3 ) {
			totalError += idMath::Abs( static_cast< int >( decoded[index] ) -
				static_cast< int >( source[index] ) );
		}
	}
	if ( !spatialVariation ) {
		error = "decoded reference stream is spatially constant";
		return false;
	}
	const float meanError = static_cast< float >( totalError ) /
		static_cast< float >( PAGE_TEXELS * PAGE_TEXELS * 3 );
	if ( meanError > 80.0f ) {
		error.Format( "reference round trip has implausible mean RGB error %.2f", meanError );
		return false;
	}
	return true;
}
