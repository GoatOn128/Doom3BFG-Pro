#include "../idlib/precompiled.h"
#pragma hdrstop

#include "virtualtexture.h"
#include "../framework/File.h"

namespace {

void ClearPageFileHeader( pageFileHeader_t & header ) {
	memset( &header, 0, sizeof( header ) );
}

int FloorLog2( int value ) {
	int result = 0;
	while ( value > 1 ) {
		value >>= 1;
		++result;
	}
	return result;
}

struct pageValidationRef_t {
	int64		offset;
	int			length;
	uint16		x;
	uint16		y;
	uint16		level;
};

}

void pageHeader_t::Swap() {
	idSwap::Little( magic );
	idSwap::Little( pageCompression );
	idSwap::LittleArray( finerDiskOffset, 4 );
	idSwap::LittleArray( finerDiskLength, 4 );
	idSwap::Little( x );
	idSwap::Little( y );
	idSwap::Little( level );
	idSwap::Little( reserved );
}

void pageHeader_t::CopySwap( pageHeader_t * destination ) const {
	*destination = *this;
	destination->Swap();
}

void pageFileHeader_t::Swap() {
	idSwap::Little( magic );
	idSwap::Little( pagesWide );
	idSwap::Little( installedMipAndOffset );
	idSwap::Little( numLevels );
	idSwap::Little( layoutVersion );
	idSwap::Little( totalPages );
	idSwap::Little( diskOffsetScale );
	idSwap::Little( totalFileSize );
	subRoot.Swap();
}

idPhysicalPageHash::idPhysicalPageHash() : hashMask( 0 ), pageHashTable( NULL ), pageHashIndex( NULL ) {}
idPhysicalPageHash::~idPhysicalPageHash() {
	Mem_Free( pageHashTable );
	Mem_Free( pageHashIndex );
}

void idPhysicalPageHash::Init( int hashSize, int indexSize ) {
	Mem_Free( pageHashTable );
	Mem_Free( pageHashIndex );
	hashSize = Max( 1, hashSize );
	indexSize = Max( 1, indexSize );
	pageHashTable = static_cast< int16 * >( Mem_Alloc( hashSize * sizeof( *pageHashTable ), TAG_RENDER ) );
	pageHashIndex = static_cast< pageHashEntry_t * >( Mem_ClearedAlloc( indexSize * sizeof( *pageHashIndex ), TAG_RENDER ) );
	memset( pageHashTable, 0xFF, hashSize * sizeof( *pageHashTable ) );
	hashMask = hashSize - 1;
}

void idPhysicalPageHash::RemovePage( texturePage_t * ) {}

idPhysicalPageList::idPhysicalPageList()
	: firstPageIndex( 0 ), lastPageIndex( -1 ), indexMask( 0 ), physicalPageNums( NULL ) {}
idPhysicalPageList::~idPhysicalPageList() { Mem_Free( physicalPageNums ); }

void idPhysicalPageList::Init( int size ) {
	Mem_Free( physicalPageNums );
	size = Max( 1, size );
	physicalPageNums = static_cast< int64 * >( Mem_Alloc( size * sizeof( *physicalPageNums ), TAG_RENDER ) );
	for ( int i = 0; i < size; ++i ) {
		physicalPageNums[i] = i;
	}
	firstPageIndex = 0;
	lastPageIndex = size - 1;
	indexMask = size - 1;
}

void idPhysicalPageList::Remove( int physicalPageNum ) {
	for ( int i = firstPageIndex; i <= lastPageIndex; ++i ) {
		if ( physicalPageNums[i] == physicalPageNum ) {
			physicalPageNums[i] = physicalPageNums[lastPageIndex--];
			return;
		}
	}
}

idPhysicalPages::idPhysicalPages()
	: physicalPagesPool( PHYSICAL_PAGES_POOL_VMTR ), pages( NULL ),
	  physicalPagesImage0( NULL ), physicalPagesImage1( NULL ),
	  physicalPagesImage2( NULL ), physicalMappingsImage( NULL ) {}
idPhysicalPages::~idPhysicalPages() { Clear(); }
void idPhysicalPages::Clear() {
	Mem_Free( pages );
	pages = NULL;
}
void idPhysicalPages::EmptyCache() {}

idVirtualTexture::idVirtualTexture()
	: flavor( ID_VTF_UNKNOWN ), vmtrData( NULL ), pageSource( PAGESOURCE_INVALID ),
	  layoutVersion( 0 ), storedNumLevels( 0 ), pageFile( NULL ),
	  pageFileInstalled( NULL ), minMipLevelInstalled( 16 ),
	  VIRTUAL_PAGES_WIDE_LOG2( 0 ), VIRTUAL_TEXELS_WIDE( 0 ),
	  VIRTUAL_MAPPING_PAGES_WIDE( 0 ), physicalPages( NULL ),
	  rootTexturePage( NULL ), pageTableImage( NULL ), pageMinLodImage( NULL ),
	  pageTableImageShadow( NULL ), pageFileIsLocal( false ), skipFlush( false ),
	  forcedPageCompression( COMP_INVALID ), isMapVmtr( false ) {
	ClearPageFileHeader( header );
	memset( mipLevelPagesWide, 0, sizeof( mipLevelPagesWide ) );
	memset( levelResidency, 0, sizeof( levelResidency ) );
}

idVirtualTexture::~idVirtualTexture() { FreeData(); }

bool idVirtualTexture::SetSource( const char * sourceBaseName,
		const char * sourcePageFilePath, physicalPagesPool_t pool,
		virtualTextureFlavor_t sourceFlavor ) {
	ReleaseSource();
	if ( sourcePageFilePath == NULL || sourcePageFilePath[0] == '\0' ) {
		return false;
	}

	pageFile = fileSystem->OpenExplicitFileRead( sourcePageFilePath );
	if ( pageFile == NULL ) {
		idLib::Warning( "idVirtualTexture::SetSource: cannot open '%s'", sourcePageFilePath );
		return false;
	}

	if ( pageFile->ReadAt( 0, &header, sizeof( header ) ) != sizeof( header ) ) {
		idLib::Warning( "idVirtualTexture::SetSource: short header in '%s'", sourcePageFilePath );
		ReleaseSource();
		return false;
	}
	header.Swap();
	if ( header.magic != PAGE_FILE_MAGIC || header.subRoot.magic != PAGE_SUBROOT_MAGIC ||
			header.pagesWide <= 0 || header.numLevels <= 0 ||
			header.diskOffsetScale <= 0 ) {
		idLib::Warning( "idVirtualTexture::SetSource: invalid .pages header in '%s'", sourcePageFilePath );
		ReleaseSource();
		return false;
	}
	if ( header.totalFileSize > pageFile->Length64() ) {
		idLib::Warning( "idVirtualTexture::SetSource: declared size %lld exceeds file size %lld in '%s'",
			header.totalFileSize, pageFile->Length64(), sourcePageFilePath );
		ReleaseSource();
		return false;
	}

	baseName = sourceBaseName != NULL ? sourceBaseName : sourcePageFilePath;
	pageFilePath = sourcePageFilePath;
	flavor = sourceFlavor;
	layoutVersion = header.layoutVersion;
	storedNumLevels = header.numLevels;
	VIRTUAL_MAPPING_PAGES_WIDE = header.pagesWide;
	VIRTUAL_PAGES_WIDE_LOG2 = FloorLog2( header.pagesWide );
	VIRTUAL_TEXELS_WIDE = 120 * header.pagesWide;
	for ( int level = 0; level < PAGESOURCE_TOTAL; ++level ) {
		mipLevelPagesWide[level] = static_cast< uint16 >( Max( 1, header.pagesWide >> level ) );
	}
	physicalPages = pool >= 0 && pool < PHYSICAL_PAGES_POOL_SHARED_COUNT
		? &virtualTextureSystem.physicalPagesPool[pool] : NULL;
	pageFileIsLocal = true;
	if ( flavor == ID_VTF_VMTR ) {
		vmtrData = new ( TAG_RENDER ) idVMTRData( this );
		vmtrData->backgroundUpdater = new ( TAG_RENDER ) idBackgroundVmtrUpdater( vmtrData );
		vmtrData->vmtrHeaderPath = pageFilePath;
		vmtrData->vmtrHeaderPath.StripFileExtension();
		vmtrData->vmtrHeaderPath += ".vmtr";
		vmtrData->ReadVmtrFile();
	}
	return true;
}

void idVirtualTexture::ReleaseSource() {
	FreeData();
	baseName.Clear();
	pageFilePath.Clear();
	ClearPageFileHeader( header );
	flavor = ID_VTF_UNKNOWN;
	storedNumLevels = 0;
	VIRTUAL_PAGES_WIDE_LOG2 = 0;
	VIRTUAL_TEXELS_WIDE = 0;
	VIRTUAL_MAPPING_PAGES_WIDE = 0;
}

void idVirtualTexture::FreeData() {
	delete vmtrData;
	vmtrData = NULL;
	if ( pageFile != NULL ) {
		delete pageFile;
		pageFile = NULL;
	}
	if ( pageFileInstalled != NULL ) {
		delete pageFileInstalled;
		pageFileInstalled = NULL;
	}
	pageTableImagePBO.FreeBufferObject();
	pageMinLodPBO[0].FreeBufferObject();
	pageMinLodPBO[1].FreeBufferObject();
	physicalPages = NULL;
	rootTexturePage = NULL;
}

void idVirtualTexture::SetEmpty( const char *, int pagesWide, int diskOffsetScale ) {
	ReleaseSource();
	header.magic = PAGE_FILE_MAGIC;
	header.pagesWide = pagesWide;
	header.numLevels = static_cast< int16 >( FloorLog2( pagesWide ) + 1 );
	header.diskOffsetScale = diskOffsetScale;
	header.totalFileSize = sizeof( header );
	header.subRoot.magic = PAGE_SUBROOT_MAGIC;
	storedNumLevels = header.numLevels;
}

idVec4 idVirtualTexture::GetFeedbackVec4() const {
	return pageFile != NULL
		? idVec4( static_cast< float >( pageSource ),
			static_cast< float >( VIRTUAL_MAPPING_PAGES_WIDE ),
			static_cast< float >( storedNumLevels ), 1.0f )
		: idVec4( 0.0f, 0.0f, 0.0f, 0.0f );
}
bool idVirtualTexture::UsesPartiallyResidentTextures() const {
	return physicalPages != NULL && physicalPages->physicalPagesPool == PHYSICAL_PAGES_POOL_PRT;
}
physicalPagesPool_t idVirtualTexture::GetPhysicalPagesPool() const {
	return physicalPages != NULL ? physicalPages->physicalPagesPool : PHYSICAL_PAGES_POOL_MAX;
}
void idVirtualTexture::EmptyCache() { if ( physicalPages != NULL ) { physicalPages->EmptyCache(); } }
bool idVirtualTexture::ValidatePageTable() { return pageFile != NULL && header.magic == PAGE_FILE_MAGIC; }

bool idVirtualTexture::ValidatePageFile( int maxPages, vtPageValidation_t & result ) const {
	memset( &result, 0, sizeof( result ) );
	if ( pageFile == NULL || header.magic != PAGE_FILE_MAGIC ) {
		return false;
	}
	result.fileBytes = pageFile->Length64();
	maxPages = Max( 1, maxPages );

	idList< pageValidationRef_t > queue;
	pageValidationRef_t root;
	root.offset = static_cast< int64 >( header.diskOffsetScale ) * header.subRoot.finerDiskOffset[0];
	root.length = header.diskOffsetScale * header.subRoot.finerDiskLength[0];
	root.x = 0;
	root.y = 0;
	root.level = static_cast< uint16 >( header.numLevels - 1 );
	if ( root.length <= 0 ) {
		return false;
	}
	queue.Append( root );
	result.queuedPages = 1;

	for ( int cursor = 0; cursor < queue.Num() && result.pagesChecked < maxPages; ++cursor ) {
		const pageValidationRef_t ref = queue[cursor];
		if ( ref.length < static_cast< int >( sizeof( pageHeader_t ) ) || ref.offset < 0 ||
				ref.offset > result.fileBytes - ref.length ) {
			++result.badPages;
			continue;
		}
		pageHeader_t diskPage;
		if ( pageFile->ReadAt( ref.offset, &diskPage, sizeof( diskPage ) ) != sizeof( diskPage ) ) {
			++result.badPages;
			continue;
		}
		diskPage.Swap();
		++result.pagesChecked;
		if ( diskPage.magic != PAGE_SUBROOT_MAGIC || diskPage.x != ref.x ||
				diskPage.y != ref.y || diskPage.level != ref.level ||
				diskPage.pageCompression <= COMP_INVALID || diskPage.pageCompression >= COMP_MAX_COMPRESSIONS ) {
			++result.badPages;
			continue;
		}
		if ( diskPage.pageCompression == COMP_HDP ) {
			++result.hdpPages;
		}
		if ( ref.level == 0 ) {
			continue;
		}
		for ( int quadrant = 0; quadrant < 4; ++quadrant ) {
			if ( diskPage.finerDiskLength[quadrant] == 0 ) {
				continue;
			}
			pageValidationRef_t child;
			child.offset = static_cast< int64 >( header.diskOffsetScale ) * diskPage.finerDiskOffset[quadrant];
			child.length = header.diskOffsetScale * diskPage.finerDiskLength[quadrant];
			child.x = static_cast< uint16 >( diskPage.x * 2 + ( quadrant & 1 ) );
			child.y = static_cast< uint16 >( diskPage.y * 2 + ( ( quadrant >> 1 ) & 1 ) );
			child.level = static_cast< uint16 >( diskPage.level - 1 );
			queue.Append( child );
			++result.queuedPages;
		}
	}
	return result.badPages == 0 && result.pagesChecked > 0;
}

void idVirtualTexture::Print() const {
	idLib::Printf( "%s: %i pages wide, %i levels, %i total pages, scale %i, %lld bytes\n",
		baseName.c_str(), header.pagesWide, header.numLevels, header.totalPages,
		header.diskOffsetScale, header.totalFileSize );
}

const char * idVirtualTexture::GetBaseName() const { return baseName.c_str(); }
void idVirtualTexture::SetMapName( const char * name ) { mapName = name != NULL ? name : ""; }
