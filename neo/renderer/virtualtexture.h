#pragma once

#include "BufferObject.h"
#include "jobs/feedbackanalysis/feedbackanalysis.h"
#include "jobs/pageminlod/pageminlod.h"
#include "jobs/pagesort/pagesort.h"

class idFile;
class idImage;
class idMaterial;
class idCmdArgs;
class idParallelJobList;
class idRenderDestination;
class idVirtualTexturePageWorker;
class idVirtualTextureArchiveWorker;
struct vtFeedbackJob_t;

static const int PAGE_FILE_MAGIC = 1999870212;
static const int PAGE_SUBROOT_MAGIC = -893391612;

enum physicalPagesPool_t {
	PHYSICAL_PAGES_POOL_VMTR = 0,
	PHYSICAL_PAGES_POOL_UNIQUE = 1,
	PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY = 2,
	PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2 = 3,
	PHYSICAL_PAGES_POOL_SHARED_COUNT = 4,
	PHYSICAL_PAGES_POOL_PRT = 5,
	PHYSICAL_PAGES_POOL_MAX = 6
};

enum virtualTextureFlavor_t {
	ID_VTF_UNKNOWN = 0,
	ID_VTF_BAKE = 1,
	ID_VTF_VMTR = 2
};

// Disk ABI.  Do not add runtime pointers or platform-sized fields here.
struct pageHeader_t {
	int				magic;
	pageCompression_t pageCompression;
	uint32			finerDiskOffset[4];
	uint16			finerDiskLength[4];
	uint16			x;
	uint16			y;
	uint16			level;
	uint16			reserved;

	void				Swap();
	void				CopySwap( pageHeader_t * destination ) const;
};

struct pageFileHeader_t {
	int				magic;
	int				pagesWide;
	int				installedMipAndOffset;
	int16			numLevels;
	int16			layoutVersion;
	int				totalPages;
	int				diskOffsetScale;
	int64			totalFileSize;
	pageHeader_t	subRoot;

	void				Swap();
};

static_assert( sizeof( pageHeader_t ) == 40, "pageHeader_t disk ABI drift" );
static_assert( offsetof( pageHeader_t, finerDiskOffset ) == 8, "pageHeader_t offset ABI drift" );
static_assert( offsetof( pageHeader_t, x ) == 32, "pageHeader_t coordinate ABI drift" );
static_assert( sizeof( pageFileHeader_t ) == 72, "pageFileHeader_t disk ABI drift" );
static_assert( offsetof( pageFileHeader_t, subRoot ) == 32, "pageFileHeader_t root ABI drift" );

class idPhysicalPageHash {
public:
	struct pageHashEntry_t {
		uint32	pageID;
		uint16	mapped;
		uint16	next;
	};

					idPhysicalPageHash();
					~idPhysicalPageHash();
	void			Init( int hashSize, int indexSize );
	void			RemovePage( texturePage_t * page );

	int				hashMask;
	int16 *		pageHashTable;
	pageHashEntry_t * pageHashIndex;
};

class idPhysicalPageList {
public:
					idPhysicalPageList();
					~idPhysicalPageList();
	void			Init( int size );
	void			Remove( int physicalPageNum );

	int				firstPageIndex;
	int				lastPageIndex;
	int				indexMask;
	int64 *		physicalPageNums;
};

class idPhysicalPages {
public:
					idPhysicalPages();
					~idPhysicalPages();
	void			Clear();
	void			EmptyCache();

	physicalPagesPool_t physicalPagesPool;
	texturePage_t *	pages;
	idPhysicalPageHash pageHash;
	idPhysicalPageList freePageList;
	idPhysicalPageList unlockedPageList;
	idImage *		physicalPagesImage0;
	idImage *		physicalPagesImage1;
	idImage *		physicalPagesImage2;
	idImage *		physicalMappingsImage;
	idPixelUnpackBuffer physicalMappingsImagePBO;
};

struct vtPageValidation_t {
	int			pagesChecked;
	int			hdpPages;
	int			badPages;
	int			queuedPages;
	int64		fileBytes;
};

class idVMTRData;

class idVirtualTexture {
public:
	static const int PAGEFILE_MAX_LOD_CLAMP = 12;

					idVirtualTexture();
					~idVirtualTexture();
	bool			SetSource( const char * baseName, const char * pageFilePath,
						physicalPagesPool_t physicalPagesPool,
						virtualTextureFlavor_t flavor );
	void			ReleaseSource();
	void			SetEmpty( const char * pageFile, int pagesWide, int diskOffsetScale );
	idVec4			GetFeedbackVec4() const;
	bool			UsesPartiallyResidentTextures() const;
	physicalPagesPool_t GetPhysicalPagesPool() const;
	void			FreeData();
	void			EmptyCache();
	bool			ValidatePageTable();
	bool			ValidatePageFile( int maxPages, vtPageValidation_t & result ) const;
	void			Print() const;
	const char *	GetBaseName() const;
	void			SetMapName( const char * mapName );

	virtualTextureFlavor_t flavor;
	idVMTRData *	vmtrData;
	pageFileHeader_t header;
	idStrStatic< 256 > pageFilePath;
	idStrStatic< 256 > baseName;
	pageSource_t	pageSource;
	int				layoutVersion;
	int				storedNumLevels;
	idFile *		pageFile;
	idFile *		pageFileInstalled;
	int				minMipLevelInstalled;
	int				VIRTUAL_PAGES_WIDE_LOG2;
	int				VIRTUAL_TEXELS_WIDE;
	int				VIRTUAL_MAPPING_PAGES_WIDE;
	uint16			mipLevelPagesWide[PAGESOURCE_TOTAL];
	idStrStatic< 128 > mapName;
	idPhysicalPages * physicalPages;
	texturePage_t *	rootTexturePage;
	idImage *		pageTableImage;
	idImage *		pageMinLodImage;
	void *			pageTableImageShadow;
	idPixelUnpackBuffer pageTableImagePBO;
	idPixelUnpackBuffer pageMinLodPBO[2];
	int				levelResidency[12];
	idSysMutex		headerMutex;
	bool			pageFileIsLocal;
	bool			skipFlush;
	pageCompression_t forcedPageCompression;
	bool			isMapVmtr;
};

class idVirtualMaterial {
public:
	idStr			materialName;
	uint32			materialChecksum;
	uint32			cpuVmtrTimeStamp;
	int				allocX;
	int				allocY;
	int				allocWidth;
	int				allocHeight;
	int				flags;
	const idMaterial * material;
	idVec4			scaleAndBias;
	idVMTRData *	vmtrData;
};

class idBackgroundVmtrUpdater;

struct vtRuntimeDiskPage_t {
	int64			offset;
	int			length;
};

class idVMTRData {
public:
	static const int VMTR_FILE_VERSION = 26;
	explicit		idVMTRData( idVirtualTexture * virtualTexture );
					~idVMTRData();
	void			ReadVmtrFile();
	void			SetupVirtualMaterial( idVirtualMaterial * vmtr );
	idVirtualMaterial * VirtualMaterialForMaterial( const idMaterial * material );

	idStrStatic< 256 > vmtrHeaderPath;
	idVirtualTexture * vt;
	idList< idVirtualMaterial * > vmtrs;
	idHashIndex		vmtrHash;
	idBackgroundVmtrUpdater * backgroundUpdater;
};

class idBackgroundVmtrUpdater {
public:
	explicit		idBackgroundVmtrUpdater( idVMTRData * vmtrData );
	idList< idVirtualMaterial * > updateList;
	idVMTRData *	vmtrData;
	bool			isDoingWork;
};

class idVirtualTextureSystem {
public:
					idVirtualTextureSystem();
					~idVirtualTextureSystem();
	void			Init();
	void			Shutdown();
	bool			Restart();
	void			SetContainerPath( const char * path );
	bool			BuildLoadedVmtrCache( bool forceRebuild = false );
	void			SetMapName( const char * mapName );
	void			EmptyCache( bool skipVmtrCache );
	idVirtualMaterial * VirtualMaterialForMaterial( const idMaterial * material );
	bool			EnsureRuntimeResources();
	void			UpdateRuntime();
	bool			BindVirtualMaterial( const idMaterial * material );
	bool			BeginFeedbackPass();
	void			EndFeedbackPass();
	bool			FeedbackUsesSceneDepth() const { return feedbackUsesSceneDepth; }
	idVec4			FeedbackParms( const idVirtualMaterial * material ) const;
	void			InvalidateRuntime();
	void			PrintRuntimeStats() const;
	idFile *		OpenCachedPagesFile( const char * path );
	void			CloseCachedPagesFile( idFile * file );

	static void		ValidateCache_f( const idCmdArgs & args );
	static void		TestEncoder_f( const idCmdArgs & args );
	static void		TestHDPWriter_f( const idCmdArgs & args );
	static void		DecodeHDP_f( const idCmdArgs & args );
	static void		SetSource_f( const idCmdArgs & args );
	static void		EmptyCache_f( const idCmdArgs & args );
	static void		ListVirtualTextures_f( const idCmdArgs & args );
	static void		BuildVmtrCache_f( const idCmdArgs & args );
	static void		RuntimeStats_f( const idCmdArgs & args );

	idVirtualTexture vts[PAGESOURCE_TOTAL];
	idVirtualTexture * vtPtrs[PAGESOURCE_TOTAL];
	idPhysicalPages physicalPagesPool[PHYSICAL_PAGES_POOL_SHARED_COUNT];
	idStrStatic< 256 > containerPath;
	idStrStatic< 128 > currentMapName;
	bool			initialized;

	// Backend-owned runtime data.  The idImages remain registered with the
	// image manager and are explicitly pinned across level-load purges.
	static const int VT_PAGE_TEXELS = 128;
	static const int VT_PAGE_BORDER = 4;
	static const int VT_INTERIOR_TEXELS = 120;
	static const int VT_CACHE_PAGES_WIDE = 16;
	static const int VT_FEEDBACK_WIDTH = 160;
	static const int VT_FEEDBACK_HEIGHT = 120;
	bool			runtimeReady;
	idRenderDestination * feedbackDestination;
	idPixelPackBuffer feedbackPBO[2];
	bool			feedbackPBOIssued[2];
	int			feedbackWriteIndex;
	int			feedbackPreviousFBO;
	int			feedbackPreviousViewport[4];
	int			feedbackPreviousScissor[4];
	bool			feedbackPreviousScissorEnabled;
	bool			feedbackUsesSceneDepth;
	idList< vtRuntimeDiskPage_t > runtimeLeafPages;
	idList< int >	runtimePageToSlot;
	idList< int >	runtimePagePending;
	idList< int >	runtimeSlotToPage;
	idList< int >	runtimeSlotLastUsed;
	idList< byte > runtimePageTableShadow;
	int			runtimeFrame;
	int			runtimeRequests;
	int			runtimeUploads;
	int			runtimeEvictions;
	int			runtimeFeedbackAnalyses;
	int			runtimeFeedbackPages;
	bool			runtimeFeedbackPending;
	int			runtimeDebugMode;
	idVirtualTexturePageWorker * runtimePageWorker;
	idVirtualTextureArchiveWorker * archiveWorker;

private:
	void			PollArchiveBuild();
	bool			BuildLeafPageIndex();
	bool			MakePageResident( int pageX, int pageY );
	void			QueueFeedbackPixels( const byte * pixels );
	void			ProcessFeedbackPages( vtFeedbackJob_t * job );
	void			UploadPageTable();
};

extern idVirtualTextureSystem virtualTextureSystem;
extern idCVar r_virtualTextureFeedbackInterval;
extern idCVar vt_showFeedback;
