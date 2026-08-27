#pragma once

class idStr;
class idImage;

struct vtHDPPageSource_t {
	const char * materialName;
	const idImage * diffuseImage;
	const idImage * normalImage;
	const idImage * specularImage;
};

// Development-side encoder for the Tech 5 .pages disk ABI.  Runtime code
// remains read-only with respect to source page containers.
class idVirtualTextureEncoder {
public:
	static const int PAGE_TEXELS = 128;
	static const int PAGE_RGBA_BYTES = PAGE_TEXELS * PAGE_TEXELS * 4;

	bool		WriteUncompressedPageFile( const char * osPath,
					const byte * diffuseRGBA, const byte * normalRGBA,
					const byte * specularRGBA, idStr & error ) const;
	bool		WriteHDPPageFile( const char * osPath,
					const byte * diffuseRGBA, const byte * normalRGBA,
					const byte * specularRGBA, int quality, idStr & error ) const;
	bool		WriteHDPPageArchive( const char * pagesOSPath,
					const char * vmtrOSPath, const vtHDPPageSource_t * sources,
					int sourceCount, int quality, idStr & error ) const;
	bool		TestDCTRoundTrip( int quality, int & encodedBytes,
					float & meanAbsoluteError, int & maximumError,
					idStr & error ) const;
	bool		TestHDPRoundTrip( int quality, int & encodedBytes,
					unsigned int & decodedChecksum, idStr & error ) const;
};
