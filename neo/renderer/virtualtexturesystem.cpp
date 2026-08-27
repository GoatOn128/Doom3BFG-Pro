#include "../idlib/precompiled.h"
#pragma hdrstop

#include "tr_local.h"
#include "virtualtexture.h"
#include "virtualtextureencoder.h"
#include "renderdestination.h"
#include "jobs/transcode/image/barehdp/barehdp.h"
#include "jobs/transcode/image/barehdp/jxrlibencoderbridge.h"
#include "../framework/File.h"
#include "../framework/FileSystem.h"
#include "../framework/CmdSystem.h"

idVirtualTextureSystem virtualTextureSystem;

idCVar r_virtualTexture( "r_virtualTexture", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL,
	"render VMTR materials through the virtual texture page cache" );
idCVar r_virtualTextureFeedback( "r_virtualTextureFeedback", "1", CVAR_RENDERER | CVAR_ARCHIVE | CVAR_BOOL,
	"render and analyze the virtual texture feedback buffer" );
idCVar r_virtualTextureUploadBudget( "r_virtualTextureUploadBudget", "4", CVAR_RENDERER | CVAR_INTEGER,
	"maximum completed virtual-texture pages uploaded per frame", 1, 32 );
idCVar r_virtualTextureFeedbackInterval( "r_virtualTextureFeedbackInterval", "4",
	CVAR_RENDERER | CVAR_ARCHIVE | CVAR_INTEGER,
	"render VT feedback once every N frames", 1, 16 );

namespace {

struct runtimePageRef_t {
	int64 offset;
	int length;
	int x;
	int y;
	int level;
};

void InvalidateTextureBindingCache() {
	for ( int unit = 0; unit < MAX_PROG_TEXTURE_PARMS; ++unit ) {
		backEnd.glState.tmu[unit].current2DMap = 0xFFFFFFFFu;
	}
}

}

struct vtRuntimePageJob_t {
	int pageIndex;
	int pageX;
	int pageY;
	vtRuntimeDiskPage_t diskRef;
	idFile * pageFile;
	idList< byte > planes;
	bool decoded;
};

struct vtFeedbackJob_t {
	int frame;
	int pagesWide;
	int numLevels;
	idList< byte > pixels;
	idList< byte > pageStorage;
	int pageCount;
};

static bool DecodeRuntimePage( vtRuntimePageJob_t & job ) {
	const int pageTexels = idVirtualTextureSystem::VT_PAGE_TEXELS;
	if ( job.pageFile == NULL || job.diskRef.length < static_cast< int >( sizeof( pageHeader_t ) ) ||
			job.diskRef.length > 1024 * 1024 ) {
		return false;
	}
	idList< byte > diskData;
	diskData.SetNum( job.diskRef.length );
	if ( job.pageFile->ReadAt( job.diskRef.offset, diskData.Ptr(), job.diskRef.length ) != job.diskRef.length ) {
		return false;
	}
	pageHeader_t page;
	memcpy( &page, diskData.Ptr(), sizeof( page ) );
	page.Swap();
	if ( page.magic != PAGE_SUBROOT_MAGIC || page.pageCompression != COMP_HDP ||
			job.diskRef.length < static_cast< int >( sizeof( page ) + sizeof( HDPHeader_t ) ) ) {
		return false;
	}
	HDPHeader_t hdp;
	memcpy( &hdp, diskData.Ptr() + sizeof( page ), sizeof( hdp ) );
	hdp.diffuseSize = static_cast< uint16 >( BigShort( static_cast< short >( hdp.diffuseSize ) ) );
	hdp.normalSize = static_cast< uint16 >( BigShort( static_cast< short >( hdp.normalSize ) ) );
	hdp.specularSize = static_cast< uint16 >( BigShort( static_cast< short >( hdp.specularSize ) ) );
	hdp.powerSize = static_cast< uint16 >( BigShort( static_cast< short >( hdp.powerSize ) ) );
	hdp.alphaSize = static_cast< uint16 >( BigShort( static_cast< short >( hdp.alphaSize ) ) );
	const int streamsSize = hdp.diffuseSize + hdp.normalSize + hdp.specularSize +
		hdp.powerSize + hdp.alphaSize + ( ( hdp.flags & 8 ) != 0 ? 2048 : 0 );
	if ( hdp.diffuseSize == 0 || hdp.normalSize == 0 || hdp.specularSize == 0 ||
			streamsSize > job.diskRef.length - sizeof( page ) - sizeof( hdp ) ) {
		return false;
	}
	const byte * stream = diskData.Ptr() + sizeof( page ) + sizeof( hdp );
	job.planes.SetNum( pageTexels * pageTexels * 4 * 3 );
	byte * diffuse = job.planes.Ptr();
	byte * normal = diffuse + pageTexels * pageTexels * 4;
	byte * specular = normal + pageTexels * pageTexels * 4;
	return JxrLibDecompressBareRGB( stream, hdp.diffuseSize, diffuse,
			pageTexels, pageTexels, pageTexels * 4 ) &&
		JxrLibDecompressBareRGB( stream + hdp.diffuseSize, hdp.normalSize, normal,
			pageTexels, pageTexels, pageTexels * 4 ) &&
		JxrLibDecompressBareRGB( stream + hdp.diffuseSize + hdp.normalSize,
			hdp.specularSize, specular, pageTexels, pageTexels, pageTexels * 4 );
}

static void AnalyzeRuntimeFeedback( vtFeedbackJob_t & job ) {
	const int storageBytes = static_cast< int >( offsetof( pageIndices_t, indices ) +
		8192 * sizeof( uint64 ) );
	job.pageStorage.SetNum( storageBytes );
	memset( job.pageStorage.Ptr(), 0, job.pageStorage.Num() );
	pageIndices_t * pageIndices = reinterpret_cast< pageIndices_t * >( job.pageStorage.Ptr() );
	virtualTextureInfo_t infos[PAGESOURCE_TOTAL];
	memset( infos, 0, sizeof( infos ) );
	infos[PAGESOURCE_VMTR].numLevels = job.numLevels;
	infos[PAGESOURCE_VMTR].baseLevel = 0;
	infos[PAGESOURCE_VMTR].pagesWide = job.pagesWide;
	infos[PAGESOURCE_VMTR].physicalPagesPool = PHYSICAL_PAGES_POOL_VMTR;
	infos[PAGESOURCE_VMTR].priorityBias = 8;
	analyzeParms_t parms;
	memset( &parms, 0, sizeof( parms ) );
	parms.analyzeFrame = job.frame;
	parms.pageIndices = pageIndices;
	parms.vtInfo = infos;
	parms.data = job.pixels.Ptr();
	parms.width = idVirtualTextureSystem::VT_FEEDBACK_WIDTH;
	parms.height = idVirtualTextureSystem::VT_FEEDBACK_HEIGHT;
	parms.bytePitch = idVirtualTextureSystem::VT_FEEDBACK_WIDTH * 4;
	FeedbackAnalysisJob( &parms );
	job.pageCount = Min( static_cast< int >( pageIndices->totalNumPages ), 8192 );
}

class idVirtualTexturePageWorker : public idSysThread {
public:
	idVirtualTexturePageWorker() {}
	~idVirtualTexturePageWorker() {
		StopThread( true );
		for ( int i = 0; i < pending.Num(); ++i ) { delete pending[i]; }
		for ( int i = 0; i < completed.Num(); ++i ) { delete completed[i]; }
		for ( int i = 0; i < pendingFeedback.Num(); ++i ) { delete pendingFeedback[i]; }
		for ( int i = 0; i < completedFeedback.Num(); ++i ) { delete completedFeedback[i]; }
	}
	void Queue( vtRuntimePageJob_t * job ) {
		{
			idScopedCriticalSection lock( mutex );
			pending.Append( job );
		}
		SignalWork();
	}
	vtRuntimePageJob_t * PopCompleted() {
		idScopedCriticalSection lock( mutex );
		if ( completed.Num() == 0 ) { return NULL; }
		vtRuntimePageJob_t * job = completed[0];
		completed.RemoveIndex( 0 );
		return job;
	}
	void QueueFeedback( vtFeedbackJob_t * job ) {
		{
			idScopedCriticalSection lock( mutex );
			pendingFeedback.Append( job );
		}
		SignalWork();
	}
	vtFeedbackJob_t * PopCompletedFeedback() {
		idScopedCriticalSection lock( mutex );
		if ( completedFeedback.Num() == 0 ) { return NULL; }
		vtFeedbackJob_t * job = completedFeedback[0];
		completedFeedback.RemoveIndex( 0 );
		return job;
	}
	virtual int Run() {
		for ( ;; ) {
			vtRuntimePageJob_t * job = NULL;
			vtFeedbackJob_t * feedbackJob = NULL;
			{
				idScopedCriticalSection lock( mutex );
				if ( pendingFeedback.Num() > 0 ) {
					feedbackJob = pendingFeedback[0];
					pendingFeedback.RemoveIndex( 0 );
				} else if ( pending.Num() > 0 ) {
					job = pending[0];
					pending.RemoveIndex( 0 );
				} else {
					break;
				}
			}
			if ( feedbackJob != NULL ) {
				AnalyzeRuntimeFeedback( *feedbackJob );
				idScopedCriticalSection lock( mutex );
				completedFeedback.Append( feedbackJob );
				continue;
			}
			job->decoded = DecodeRuntimePage( *job );
			{
				idScopedCriticalSection lock( mutex );
				completed.Append( job );
			}
		}
		return 0;
	}
private:
	idSysMutex mutex;
	idList< vtRuntimePageJob_t * > pending;
	idList< vtRuntimePageJob_t * > completed;
	idList< vtFeedbackJob_t * > pendingFeedback;
	idList< vtFeedbackJob_t * > completedFeedback;
};

class idVirtualTextureArchiveWorker : public idSysThread {
public:
	idVirtualTextureArchiveWorker() : success( false ), materialCount( 0 ) {}
	~idVirtualTextureArchiveWorker() { StopThread( true ); }
	bool Start( const char * base, const char * pages, const char * metadata,
			const idList< vtHDPPageSource_t > & sourceList ) {
		cacheBase = base;
		pagesPath = pages;
		metadataPath = metadata;
		sources = sourceList;
		materialCount = sources.Num();
		if ( !StartThread( "VirtualTextureArchive", CORE_ANY,
				THREAD_BELOW_NORMAL, 1024 * 1024 ) ) {
			return false;
		}
		return true;
	}
	virtual int Run() {
		idVirtualTextureEncoder encoder;
		success = encoder.WriteHDPPageArchive( pagesPath.c_str(), metadataPath.c_str(),
			sources.Ptr(), sources.Num(), 30, error );
		// Image pointers are only needed while encoding; release the list before
		// the completed worker waits to be collected by the render thread.
		sources.Clear();
		return 0;
	}

	idStr cacheBase;
	idStr pagesPath;
	idStr metadataPath;
	idStr error;
	bool success;
	int materialCount;

private:
	idList< vtHDPPageSource_t > sources;
};

idVirtualTextureSystem::idVirtualTextureSystem()
	: initialized( false ), runtimeReady( false ), feedbackDestination( NULL ),
	  feedbackWriteIndex( 0 ), feedbackPreviousFBO( 0 ),
	  feedbackPreviousScissorEnabled( false ), runtimeFrame( 0 ),
	  runtimeRequests( 0 ), runtimeUploads( 0 ), runtimeEvictions( 0 ),
	  runtimeFeedbackAnalyses( 0 ), runtimeFeedbackPages( 0 ),
	  runtimeFeedbackPending( false ),
	  runtimePageWorker( NULL ), archiveWorker( NULL ) {
	feedbackPBOIssued[0] = feedbackPBOIssued[1] = false;
	memset( feedbackPreviousViewport, 0, sizeof( feedbackPreviousViewport ) );
	memset( feedbackPreviousScissor, 0, sizeof( feedbackPreviousScissor ) );
	memset( vtPtrs, 0, sizeof( vtPtrs ) );
	for ( int i = 0; i < PAGESOURCE_TOTAL; ++i ) {
		vtPtrs[i] = &vts[i];
		vts[i].pageSource = static_cast< pageSource_t >( i );
	}
	for ( int i = 0; i < PHYSICAL_PAGES_POOL_SHARED_COUNT; ++i ) {
		physicalPagesPool[i].physicalPagesPool = static_cast< physicalPagesPool_t >( i );
	}
}

idVirtualTextureSystem::~idVirtualTextureSystem() {}

void idVirtualTextureSystem::Init() {
	if ( initialized ) {
		return;
	}
	initialized = true;
	cmdSystem->AddCommand( "vt_validateCache", ValidateCache_f, CMD_FL_RENDERER,
		"validate a Tech 5 .pages container: vt_validateCache <OS path> [max pages]" );
	cmdSystem->AddCommand( "vt_testEncoder", TestEncoder_f, CMD_FL_RENDERER,
		"round-trip the Tech 5 DCT encoder and optionally write a one-page cache: vt_testEncoder [OS path] [quality]" );
	cmdSystem->AddCommand( "vt_decodeHDP", DecodeHDP_f, CMD_FL_RENDERER,
		"decode the root HDP page from a Tech 5 cache: vt_decodeHDP <OS path>" );
	cmdSystem->AddCommand( "vt_testHDPWriter", TestHDPWriter_f, CMD_FL_RENDERER,
		"round-trip the bare HDP writer and optionally compare a cache: vt_testHDPWriter [quality] [pages path]" );
	cmdSystem->AddCommand( "vt_setSource", SetSource_f, CMD_FL_RENDERER,
		"set a VT source slot: vt_setSource <1-15> <OS path>" );
	cmdSystem->AddCommand( "vt_emptyCache", EmptyCache_f, CMD_FL_RENDERER,
		"empty the virtual texture physical page caches" );
	cmdSystem->AddCommand( "listVirtualTextures", ListVirtualTextures_f, CMD_FL_RENDERER,
		"list loaded virtual texture page containers" );
	cmdSystem->AddCommand( "vt_buildVmtrCache", BuildVmtrCache_f, CMD_FL_RENDERER,
		"rebuild virtualtextures/<mapname>_vmtr.pages from referenced materials: vt_buildVmtrCache [material ...]" );
	cmdSystem->AddCommand( "vt_runtimeStats", RuntimeStats_f, CMD_FL_RENDERER,
		"print virtual texture feedback and physical-cache statistics" );
	idLib::Printf( "virtual texture container layer initialized\n" );
}

void idVirtualTextureSystem::Shutdown() {
	if ( !initialized ) {
		return;
	}
	InvalidateRuntime();
	delete archiveWorker;
	archiveWorker = NULL;
	for ( int i = 0; i < PAGESOURCE_TOTAL; ++i ) {
		vts[i].ReleaseSource();
	}
	cmdSystem->RemoveCommand( "vt_validateCache" );
	cmdSystem->RemoveCommand( "vt_testEncoder" );
	cmdSystem->RemoveCommand( "vt_decodeHDP" );
	cmdSystem->RemoveCommand( "vt_testHDPWriter" );
	cmdSystem->RemoveCommand( "vt_setSource" );
	cmdSystem->RemoveCommand( "vt_emptyCache" );
	cmdSystem->RemoveCommand( "listVirtualTextures" );
	cmdSystem->RemoveCommand( "vt_buildVmtrCache" );
	cmdSystem->RemoveCommand( "vt_runtimeStats" );
	initialized = false;
}

bool idVirtualTextureSystem::Restart() {
	EmptyCache( false );
	return true;
}

void idVirtualTextureSystem::SetContainerPath( const char * path ) {
	containerPath = path != NULL ? path : "";
}

void idVirtualTextureSystem::SetMapName( const char * name ) {
	currentMapName = name != NULL ? name : "";
	currentMapName.BackSlashesToSlashes();
	currentMapName.StripPath();
	currentMapName.StripFileExtension();
}

void idVirtualTextureSystem::PollArchiveBuild() {
	if ( archiveWorker == NULL || archiveWorker->IsRunning() ) { return; }
	idStr cacheBase = archiveWorker->cacheBase;
	idStr pagesPath = archiveWorker->pagesPath;
	idStr error = archiveWorker->error;
	const bool success = archiveWorker->success;
	const int materialCount = archiveWorker->materialCount;
	delete archiveWorker;
	archiveWorker = NULL;
	if ( !success ) {
		idLib::Warning( "VMTR background builder failed: %s", error.c_str() );
		return;
	}
	idStr expectedBase = currentMapName;
	expectedBase += "_vmtr";
	if ( cacheBase.Icmp( expectedBase ) != 0 ) {
		idLib::Printf( "VMTR background builder: finished '%s' after the map changed; leaving it cached\n",
			pagesPath.c_str() );
		BuildLoadedVmtrCache( false );
		return;
	}
	InvalidateRuntime();
	vts[PAGESOURCE_VMTR].ReleaseSource();
	if ( !vts[PAGESOURCE_VMTR].SetSource( cacheBase.c_str(), pagesPath.c_str(),
			PHYSICAL_PAGES_POOL_VMTR, ID_VTF_VMTR ) ) {
		idLib::Warning( "VMTR background builder wrote the archive but could not reopen it" );
		return;
	}
	vtPageValidation_t validation;
	if ( !vts[PAGESOURCE_VMTR].ValidatePageFile(
			vts[PAGESOURCE_VMTR].header.totalPages, validation ) || validation.badPages != 0 ||
			validation.pagesChecked != vts[PAGESOURCE_VMTR].header.totalPages ) {
		idLib::Warning( "VMTR background builder wrote an invalid page tree (%i checked, %i bad)",
			validation.pagesChecked, validation.badPages );
		vts[PAGESOURCE_VMTR].ReleaseSource();
		return;
	}
	idLib::Printf( "VMTR background builder: attached %i materials, %i HDP pages, %lld bytes\n",
		materialCount, validation.hdpPages, validation.fileBytes );
}

bool idVirtualTextureSystem::BuildLoadedVmtrCache( bool forceRebuild ) {
	if ( !initialized ) {
		return false;
	}
	if ( currentMapName.Length() == 0 ) {
		// EndLevelLoad is also used by front-end/startup media. There is no map
		// archive to build in that case, and encoding it here stalls startup.
		return false;
	}
	PollArchiveBuild();
	if ( archiveWorker != NULL ) {
		idLib::Printf( "VMTR background builder: archive generation is already in progress\n" );
		return false;
	}

	idStr cacheBase = currentMapName;
	cacheBase += "_vmtr";
	idStr outputDirectory = cvarSystem->GetCVarString( "fs_basepath" );
	outputDirectory.AppendPath( "virtualtextures" );
	idStr pagesPath = outputDirectory;
	pagesPath.AppendPath( cacheBase + ".pages" );
	idStr metadataPath = outputDirectory;
	metadataPath.AppendPath( cacheBase + ".vmtr" );

	// Normal map loads reuse the existing per-map archive. The explicit build
	// command is the development path that forces regeneration.
	if ( !forceRebuild ) {
		InvalidateRuntime();
		vts[PAGESOURCE_VMTR].ReleaseSource();
		if ( vts[PAGESOURCE_VMTR].SetSource( cacheBase.c_str(), pagesPath.c_str(),
				PHYSICAL_PAGES_POOL_VMTR, ID_VTF_VMTR ) &&
				vts[PAGESOURCE_VMTR].vmtrData != NULL &&
				vts[PAGESOURCE_VMTR].vmtrData->vmtrs.Num() > 0 ) {
			idLib::Printf( "VMTR cache: using existing per-map archive '%s'\n", pagesPath.c_str() );
			return true;
		}
		vts[PAGESOURCE_VMTR].ReleaseSource();
	}

	idList< vtHDPPageSource_t > sources;
	const int materialCount = declManager->GetNumDecls( DECL_MATERIAL );
	for ( int materialIndex = 0; materialIndex < materialCount; ++materialIndex ) {
		const idMaterial * material = static_cast< const idMaterial * >(
			declManager->DeclByIndex( DECL_MATERIAL, materialIndex, false ) );
		if ( material == NULL || material->GetState() != DS_PARSED ||
				!material->EverReferenced() ) {
			continue;
		}

		const idImage * diffuseImage = NULL;
		const idImage * normalImage = NULL;
		const idImage * specularImage = NULL;
		for ( int stageIndex = 0; stageIndex < material->GetNumStages(); ++stageIndex ) {
			const shaderStage_t * stage = material->GetStage( stageIndex );
			if ( stage->texture.image == NULL ) {
				continue;
			}
			switch ( stage->lighting ) {
				case SL_BUMP:
					if ( normalImage == NULL ) {
						normalImage = stage->texture.image;
					}
					break;
				case SL_DIFFUSE:
					if ( diffuseImage == NULL ) {
						diffuseImage = stage->texture.image;
					}
					break;
				case SL_SPECULAR:
					if ( specularImage == NULL ) {
						specularImage = stage->texture.image;
					}
					break;
				default:
					break;
			}
		}
		if ( diffuseImage == NULL && normalImage == NULL && specularImage == NULL ) {
			continue;
		}

		vtHDPPageSource_t source;
		source.materialName = material->GetName();
		source.diffuseImage = diffuseImage;
		source.normalImage = normalImage;
		source.specularImage = specularImage;
		sources.Append( source );
	}

	if ( sources.Num() == 0 ) {
		idLib::Printf( "VMTR builder: no referenced interaction materials; keeping the existing cache\n" );
		return false;
	}

	// Windows does not permit replacing a pages file while this slot still has
	// it open. Release it before queuing the background encoder. Rendering uses
	// the material's ordinary images until PollArchiveBuild attaches the result.
	InvalidateRuntime();
	vts[PAGESOURCE_VMTR].ReleaseSource();
	idLib::Printf( "VMTR background builder: queued %i materials into '%s'\n",
		sources.Num(), pagesPath.c_str() );
	archiveWorker = new ( TAG_RENDER ) idVirtualTextureArchiveWorker;
	if ( !archiveWorker->Start( cacheBase.c_str(), pagesPath.c_str(), metadataPath.c_str(), sources ) ) {
		delete archiveWorker;
		archiveWorker = NULL;
		idLib::Warning( "VMTR background builder could not start its worker thread" );
		return false;
	}
	return true;
}

void idVirtualTextureSystem::EmptyCache( bool ) {
	InvalidateRuntime();
	for ( int i = 0; i < PHYSICAL_PAGES_POOL_SHARED_COUNT; ++i ) {
		physicalPagesPool[i].EmptyCache();
	}
}

idVirtualMaterial * idVirtualTextureSystem::VirtualMaterialForMaterial( const idMaterial * material ) {
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	return material != NULL && vt.vmtrData != NULL
		? vt.vmtrData->VirtualMaterialForMaterial( material ) : NULL;
}

bool idVirtualTextureSystem::BuildLeafPageIndex() {
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	if ( vt.pageFile == NULL || vt.header.pagesWide <= 0 ) {
		return false;
	}
	const int pageCount = vt.header.pagesWide * vt.header.pagesWide;
	runtimeLeafPages.SetNum( pageCount );
	memset( runtimeLeafPages.Ptr(), 0, pageCount * sizeof( runtimeLeafPages[0] ) );

	idList< runtimePageRef_t > queue;
	runtimePageRef_t root;
	root.offset = static_cast< int64 >( vt.header.diskOffsetScale ) * vt.header.subRoot.finerDiskOffset[0];
	root.length = vt.header.diskOffsetScale * vt.header.subRoot.finerDiskLength[0];
	root.x = root.y = 0;
	root.level = vt.header.numLevels - 1;
	queue.Append( root );
	for ( int cursor = 0; cursor < queue.Num(); ++cursor ) {
		const runtimePageRef_t ref = queue[cursor];
		if ( ref.length < static_cast< int >( sizeof( pageHeader_t ) ) || ref.offset < 0 ||
				ref.offset > vt.pageFile->Length64() - ref.length ) {
			return false;
		}
		pageHeader_t page;
		if ( vt.pageFile->ReadAt( ref.offset, &page, sizeof( page ) ) != sizeof( page ) ) {
			return false;
		}
		page.Swap();
		if ( page.magic != PAGE_SUBROOT_MAGIC || page.x != ref.x || page.y != ref.y ||
				page.level != ref.level ) {
			return false;
		}
		if ( ref.level == 0 ) {
			if ( ref.x >= vt.header.pagesWide || ref.y >= vt.header.pagesWide ) {
				return false;
			}
			vtRuntimeDiskPage_t & leaf = runtimeLeafPages[ref.y * vt.header.pagesWide + ref.x];
			leaf.offset = ref.offset;
			leaf.length = ref.length;
			continue;
		}
		for ( int quadrant = 0; quadrant < 4; ++quadrant ) {
			if ( page.finerDiskLength[quadrant] == 0 ) {
				continue;
			}
			runtimePageRef_t child;
			child.offset = static_cast< int64 >( vt.header.diskOffsetScale ) * page.finerDiskOffset[quadrant];
			child.length = vt.header.diskOffsetScale * page.finerDiskLength[quadrant];
			child.x = ref.x * 2 + ( quadrant & 1 );
			child.y = ref.y * 2 + ( ( quadrant >> 1 ) & 1 );
			child.level = ref.level - 1;
			queue.Append( child );
		}
	}
	for ( int page = 0; page < runtimeLeafPages.Num(); ++page ) {
		if ( runtimeLeafPages[page].length == 0 ) {
			return false;
		}
	}
	return true;
}

bool idVirtualTextureSystem::EnsureRuntimeResources() {
	if ( runtimeReady ) {
		return true;
	}
	if ( !r_virtualTexture.GetBool() || !BuildLeafPageIndex() ) {
		return false;
	}

	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	idImageOpts opts;
	opts.width = VT_CACHE_PAGES_WIDE * VT_PAGE_TEXELS;
	opts.height = VT_CACHE_PAGES_WIDE * VT_PAGE_TEXELS;
	opts.numLevels = 1;
	opts.format = FMT_RGBA8;
	idPhysicalPages & pool = physicalPagesPool[PHYSICAL_PAGES_POOL_VMTR];
	pool.physicalPagesImage0 = globalImages->ScratchImage( "_vtPhysicalDiffuse", &opts,
		TF_LINEAR, TR_CLAMP, TD_LOOKUP_TABLE_RGBA );
	pool.physicalPagesImage1 = globalImages->ScratchImage( "_vtPhysicalNormal", &opts,
		TF_LINEAR, TR_CLAMP, TD_LOOKUP_TABLE_RGBA );
	pool.physicalPagesImage2 = globalImages->ScratchImage( "_vtPhysicalSpecular", &opts,
		TF_LINEAR, TR_CLAMP, TD_LOOKUP_TABLE_RGBA );
	pool.physicalPagesImage0->SetReferencedOutsideLevelLoad();
	pool.physicalPagesImage1->SetReferencedOutsideLevelLoad();
	pool.physicalPagesImage2->SetReferencedOutsideLevelLoad();

	opts.width = vt.header.pagesWide;
	opts.height = vt.header.pagesWide;
	vt.pageTableImage = globalImages->ScratchImage( "_vtVmtrPageTable", &opts,
		TF_NEAREST, TR_CLAMP, TD_LOOKUP_TABLE_RGBA );
	vt.pageTableImage->SetReferencedOutsideLevelLoad();
	runtimePageTableShadow.SetNum( opts.width * opts.height * 4 );
	memset( runtimePageTableShadow.Ptr(), 0, runtimePageTableShadow.Num() );
	vt.pageTableImage->SubImageUpload( 0, 0, 0, 0, opts.width, opts.height,
		runtimePageTableShadow.Ptr() );

	runtimePageToSlot.SetNum( runtimeLeafPages.Num() );
	runtimePagePending.SetNum( runtimeLeafPages.Num() );
	for ( int i = 0; i < runtimePageToSlot.Num(); ++i ) {
		runtimePageToSlot[i] = -1;
		runtimePagePending[i] = 0;
	}
	const int slotCount = VT_CACHE_PAGES_WIDE * VT_CACHE_PAGES_WIDE;
	runtimeSlotToPage.SetNum( slotCount );
	runtimeSlotLastUsed.SetNum( slotCount );
	for ( int i = 0; i < slotCount; ++i ) {
		runtimeSlotToPage[i] = -1;
		runtimeSlotLastUsed[i] = 0;
	}

	feedbackDestination = new ( TAG_RENDER ) idRenderDestination( "vtFeedback",
		VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT, TC_RGBA_DEPTH );
	if ( !feedbackDestination->IsValid() ) {
		InvalidateRuntime();
		return false;
	}
	feedbackDestination->GetColorImage()->SetReferencedOutsideLevelLoad();
	feedbackDestination->GetDepthImage()->SetReferencedOutsideLevelLoad();
	const int feedbackBytes = VT_FEEDBACK_WIDTH * VT_FEEDBACK_HEIGHT * 4;
	if ( !feedbackPBO[0].AllocBufferObject( feedbackBytes ) ||
			!feedbackPBO[1].AllocBufferObject( feedbackBytes ) ) {
		InvalidateRuntime();
		return false;
	}
	runtimePageWorker = new ( TAG_RENDER ) idVirtualTexturePageWorker;
	if ( !runtimePageWorker->StartWorkerThread( "VirtualTexturePages", CORE_ANY,
			THREAD_BELOW_NORMAL, 512 * 1024 ) ) {
		InvalidateRuntime();
		return false;
	}
	feedbackPBOIssued[0] = feedbackPBOIssued[1] = false;
	feedbackWriteIndex = 0;
	runtimeFrame = runtimeRequests = runtimeUploads = runtimeEvictions = 0;
	runtimeFeedbackAnalyses = runtimeFeedbackPages = 0;
	runtimeFeedbackPending = false;
	runtimeReady = true;
	InvalidateTextureBindingCache();
	idLib::Printf( "VT runtime: %ix%i virtual pages, %ix%i physical cache, %ix%i feedback\n",
		vt.header.pagesWide, vt.header.pagesWide, VT_CACHE_PAGES_WIDE,
		VT_CACHE_PAGES_WIDE, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT );
	return true;
}

void idVirtualTextureSystem::UploadPageTable() {
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	if ( vt.pageTableImage == NULL || runtimePageTableShadow.Num() == 0 ) {
		return;
	}
	vt.pageTableImage->SubImageUpload( 0, 0, 0, 0, vt.header.pagesWide,
		vt.header.pagesWide, runtimePageTableShadow.Ptr() );
	InvalidateTextureBindingCache();
}

bool idVirtualTextureSystem::MakePageResident( int pageX, int pageY ) {
	if ( !runtimeReady || runtimePageWorker == NULL ) {
		return false;
	}
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	if ( pageX < 0 || pageY < 0 || pageX >= vt.header.pagesWide || pageY >= vt.header.pagesWide ) {
		return false;
	}
	const int pageIndex = pageY * vt.header.pagesWide + pageX;
	const int residentSlot = runtimePageToSlot[pageIndex];
	if ( residentSlot >= 0 ) {
		runtimeSlotLastUsed[residentSlot] = runtimeFrame;
		return true;
	}
	if ( runtimePagePending[pageIndex] != 0 ) {
		return false;
	}
	const vtRuntimeDiskPage_t & ref = runtimeLeafPages[pageIndex];
	if ( ref.length < static_cast< int >( sizeof( pageHeader_t ) ) || ref.length > 1024 * 1024 ) {
		return false;
	}
	vtRuntimePageJob_t * job = new ( TAG_RENDER ) vtRuntimePageJob_t;
	job->pageIndex = pageIndex;
	job->pageX = pageX;
	job->pageY = pageY;
	job->diskRef = ref;
	job->pageFile = vt.pageFile;
	job->decoded = false;
	runtimePagePending[pageIndex] = 1;
	++runtimeRequests;
	runtimePageWorker->Queue( job );
	return false;
}

void idVirtualTextureSystem::UpdateRuntime() {
	PollArchiveBuild();
	if ( !runtimeReady || runtimePageWorker == NULL ) { return; }
	if ( vtFeedbackJob_t * feedbackJob = runtimePageWorker->PopCompletedFeedback() ) {
		runtimeFeedbackPending = false;
		ProcessFeedbackPages( feedbackJob );
		delete feedbackJob;
	}
	const int uploadBudget = r_virtualTextureUploadBudget.GetInteger();
	bool pageTableChanged = false;
	for ( int uploaded = 0; uploaded < uploadBudget; ) {
		vtRuntimePageJob_t * job = runtimePageWorker->PopCompleted();
		if ( job == NULL ) { break; }
		if ( job->pageIndex >= 0 && job->pageIndex < runtimePagePending.Num() ) {
			runtimePagePending[job->pageIndex] = 0;
		}
		if ( !job->decoded ) {
			idLib::Warning( "VT runtime: worker failed to decode page (%i,%i)", job->pageX, job->pageY );
			delete job;
			continue;
		}
		int slot = -1;
		for ( int i = 0; i < runtimeSlotToPage.Num(); ++i ) {
			if ( runtimeSlotToPage[i] < 0 ) { slot = i; break; }
		}
		if ( slot < 0 ) {
			slot = 0;
			for ( int i = 1; i < runtimeSlotLastUsed.Num(); ++i ) {
				if ( runtimeSlotLastUsed[i] < runtimeSlotLastUsed[slot] ) { slot = i; }
			}
			const int evictedPage = runtimeSlotToPage[slot];
			if ( evictedPage >= 0 ) {
				runtimePageToSlot[evictedPage] = -1;
				memset( runtimePageTableShadow.Ptr() + evictedPage * 4, 0, 4 );
				++runtimeEvictions;
			}
		}
		const int planeBytes = VT_PAGE_TEXELS * VT_PAGE_TEXELS * 4;
		const int physicalX = ( slot % VT_CACHE_PAGES_WIDE ) * VT_PAGE_TEXELS;
		const int physicalY = ( slot / VT_CACHE_PAGES_WIDE ) * VT_PAGE_TEXELS;
		idPhysicalPages & pool = physicalPagesPool[PHYSICAL_PAGES_POOL_VMTR];
		pool.physicalPagesImage0->SubImageUpload( 0, physicalX, physicalY, 0,
			VT_PAGE_TEXELS, VT_PAGE_TEXELS, job->planes.Ptr() );
		pool.physicalPagesImage1->SubImageUpload( 0, physicalX, physicalY, 0,
			VT_PAGE_TEXELS, VT_PAGE_TEXELS, job->planes.Ptr() + planeBytes );
		pool.physicalPagesImage2->SubImageUpload( 0, physicalX, physicalY, 0,
			VT_PAGE_TEXELS, VT_PAGE_TEXELS, job->planes.Ptr() + planeBytes * 2 );
		runtimeSlotToPage[slot] = job->pageIndex;
		runtimeSlotLastUsed[slot] = runtimeFrame;
		runtimePageToSlot[job->pageIndex] = slot;
		byte * mapping = runtimePageTableShadow.Ptr() + job->pageIndex * 4;
		mapping[0] = static_cast< byte >( slot % VT_CACHE_PAGES_WIDE );
		mapping[1] = static_cast< byte >( slot / VT_CACHE_PAGES_WIDE );
		mapping[2] = 255;
		mapping[3] = 255;
		++runtimeUploads;
		++uploaded;
		pageTableChanged = true;
		if ( runtimeUploads == 1 ) {
			idLib::Printf( "VT runtime: worker decoded first page (%i,%i), uploaded to slot %i\n",
				job->pageX, job->pageY, slot );
		}
		delete job;
	}
	if ( pageTableChanged ) { UploadPageTable(); }
}

bool idVirtualTextureSystem::BindVirtualMaterial( const idMaterial * material ) {
	idVirtualMaterial * vmtr = VirtualMaterialForMaterial( material );
	idVec4 disabled( 0.0f );
	if ( vmtr == NULL || !EnsureRuntimeResources() ) {
		renderProgManager.SetUniformValue( static_cast< renderParm_t >( RENDERPARM_USER + 1 ), disabled.ToFloatPtr() );
		return false;
	}
	idVec4 options( static_cast< float >( PAGESOURCE_VMTR ),
		static_cast< float >( vts[PAGESOURCE_VMTR].header.pagesWide ),
		static_cast< float >( VT_CACHE_PAGES_WIDE ), 1.0f );
	renderProgManager.SetUniformValue( static_cast< renderParm_t >( RENDERPARM_USER + 0 ),
		vmtr->scaleAndBias.ToFloatPtr() );
	renderProgManager.SetUniformValue( static_cast< renderParm_t >( RENDERPARM_USER + 1 ),
		options.ToFloatPtr() );
	idPhysicalPages & pool = physicalPagesPool[PHYSICAL_PAGES_POOL_VMTR];
	GL_SelectTexture( 6 ); pool.physicalPagesImage0->Bind();
	GL_SelectTexture( 7 ); pool.physicalPagesImage1->Bind();
	GL_SelectTexture( 8 ); pool.physicalPagesImage2->Bind();
	GL_SelectTexture( 9 ); vts[PAGESOURCE_VMTR].pageTableImage->Bind();
	return true;
}

idVec4 idVirtualTextureSystem::FeedbackParms( const idVirtualMaterial * material ) const {
	if ( material == NULL ) { return idVec4( 0.0f ); }
	return idVec4( static_cast< float >( PAGESOURCE_VMTR ),
		static_cast< float >( vts[PAGESOURCE_VMTR].header.pagesWide ), 1.0f, 0.0f );
}

bool idVirtualTextureSystem::BeginFeedbackPass() {
	if ( !r_virtualTextureFeedback.GetBool() || !EnsureRuntimeResources() ) {
		return false;
	}
	++runtimeFrame;
	qglGetIntegerv( GL_FRAMEBUFFER_BINDING, &feedbackPreviousFBO );
	qglGetIntegerv( GL_VIEWPORT, feedbackPreviousViewport );
	qglGetIntegerv( GL_SCISSOR_BOX, feedbackPreviousScissor );
	feedbackPreviousScissorEnabled = qglIsEnabled( GL_SCISSOR_TEST ) != 0;
	qglBindFramebuffer( GL_FRAMEBUFFER, feedbackDestination->FBOhandle );
	qglDrawBuffer( GL_COLOR_ATTACHMENT0 );
	qglReadBuffer( GL_COLOR_ATTACHMENT0 );
	GL_Viewport( 0, 0, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT );
	GL_Scissor( 0, 0, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT );
	qglEnable( GL_SCISSOR_TEST );
	GL_Clear( true, true, false, 0, 0.0f, 0.0f, 0.0f, 0.0f );
	return true;
}

void idVirtualTextureSystem::QueueFeedbackPixels( const byte * pixels ) {
	if ( pixels == NULL || runtimePageWorker == NULL || runtimeFeedbackPending ) { return; }
	const idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	vtFeedbackJob_t * job = new ( TAG_RENDER ) vtFeedbackJob_t;
	job->frame = runtimeFrame;
	job->pagesWide = vt.header.pagesWide;
	job->numLevels = vt.header.numLevels;
	job->pageCount = 0;
	job->pixels.SetNum( VT_FEEDBACK_WIDTH * VT_FEEDBACK_HEIGHT * 4 );
	memcpy( job->pixels.Ptr(), pixels, job->pixels.Num() );
	runtimeFeedbackPending = true;
	runtimePageWorker->QueueFeedback( job );
}

void idVirtualTextureSystem::ProcessFeedbackPages( vtFeedbackJob_t * job ) {
	if ( job == NULL || job->pageStorage.Num() < static_cast< int >( sizeof( pageIndices_t ) ) ) { return; }
	const idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	pageIndices_t * pageIndices = reinterpret_cast< pageIndices_t * >( job->pageStorage.Ptr() );
	const int count = job->pageCount;
	++runtimeFeedbackAnalyses;
	runtimeFeedbackPages = count;
	if ( runtimeFeedbackAnalyses == 1 ) {
		idLib::Printf( "VT runtime: feedback readback live (%i unique page-tree nodes)\n", count );
	}
	int queuedThisFrame = 0;
	for ( int i = 0; i < count && queuedThisFrame < 8; ++i ) {
		const uint64 key = pageIndices->indices[i] >> 2;
		const int x = static_cast< int >( key & 0x7FF );
		const uint64 yAndTexture = key >> 11;
		const int y = static_cast< int >( yAndTexture & 0x7FF );
		const int textureAndLevel = static_cast< int >( yAndTexture >> 11 );
		const int level = textureAndLevel & 15;
		const int texture = ( textureAndLevel >> 4 ) & 15;
		if ( texture == PAGESOURCE_VMTR && level == 0 && x >= 0 && y >= 0 &&
				x < vt.header.pagesWide && y < vt.header.pagesWide &&
				runtimePageToSlot[y * vt.header.pagesWide + x] < 0 &&
				runtimePagePending[y * vt.header.pagesWide + x] == 0 ) {
			MakePageResident( x, y );
			++queuedThisFrame;
		}
	}
}

void idVirtualTextureSystem::EndFeedbackPass() {
	if ( !runtimeReady || feedbackDestination == NULL ) { return; }
	const int current = feedbackWriteIndex;
	const int previous = current ^ 1;
	qglBindBufferARB( GL_PIXEL_PACK_BUFFER_ARB,
		reinterpret_cast< GLuint >( feedbackPBO[current].GetAPIObject() ) );
	qglReadPixels( 0, 0, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT,
		GL_RGBA, GL_UNSIGNED_BYTE, NULL );
	feedbackPBOIssued[current] = true;
	if ( feedbackPBOIssued[previous] ) {
		const byte * pixels = static_cast< const byte * >( feedbackPBO[previous].TryMapBuffer( BM_READ ) );
		if ( pixels != NULL ) {
			QueueFeedbackPixels( pixels );
			feedbackPBO[previous].UnmapBuffer();
		}
	}
	qglBindBufferARB( GL_PIXEL_PACK_BUFFER_ARB, 0 );
	feedbackWriteIndex ^= 1;
	qglBindFramebuffer( GL_FRAMEBUFFER, feedbackPreviousFBO );
	GL_Viewport( feedbackPreviousViewport[0], feedbackPreviousViewport[1],
		feedbackPreviousViewport[2], feedbackPreviousViewport[3] );
	GL_Scissor( feedbackPreviousScissor[0], feedbackPreviousScissor[1],
		feedbackPreviousScissor[2], feedbackPreviousScissor[3] );
	if ( feedbackPreviousScissorEnabled ) { qglEnable( GL_SCISSOR_TEST ); }
	else { qglDisable( GL_SCISSOR_TEST ); }
	backEnd.currentSpace = NULL;
}

void idVirtualTextureSystem::InvalidateRuntime() {
	runtimeReady = false;
	if ( runtimePageWorker != NULL ) {
		runtimePageWorker->StopThread( true );
		delete runtimePageWorker;
		runtimePageWorker = NULL;
	}
	delete feedbackDestination;
	feedbackDestination = NULL;
	feedbackPBO[0].FreeBufferObject();
	feedbackPBO[1].FreeBufferObject();
	feedbackPBOIssued[0] = feedbackPBOIssued[1] = false;
	runtimeLeafPages.Clear();
	runtimePageToSlot.Clear();
	runtimePagePending.Clear();
	runtimeSlotToPage.Clear();
	runtimeSlotLastUsed.Clear();
	runtimePageTableShadow.Clear();
	runtimeFeedbackPending = false;
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	vt.pageTableImage = NULL;
	idPhysicalPages & pool = physicalPagesPool[PHYSICAL_PAGES_POOL_VMTR];
	pool.physicalPagesImage0 = pool.physicalPagesImage1 = pool.physicalPagesImage2 = NULL;
}

void idVirtualTextureSystem::PrintRuntimeStats() const {
	idLib::Printf( "VT runtime: %s, frame %i, requests %i, uploads %i, evictions %i, feedback %i analyses / %i nodes\n",
		runtimeReady ? "active" : "inactive", runtimeFrame, runtimeRequests,
		runtimeUploads, runtimeEvictions, runtimeFeedbackAnalyses, runtimeFeedbackPages );
}

idFile * idVirtualTextureSystem::OpenCachedPagesFile( const char * path ) {
	if ( path == NULL || path[0] == '\0' ) {
		return NULL;
	}
	idStr resolved = path;
	const bool isAbsolute = ( resolved.Length() > 1 && resolved[1] == ':' ) ||
		resolved[0] == '/' || resolved[0] == '\\';
	if ( !isAbsolute && containerPath.Length() > 0 ) {
		resolved = containerPath;
		resolved.AppendPath( path );
	}
	return fileSystem->OpenExplicitFileRead( resolved.c_str() );
}

void idVirtualTextureSystem::CloseCachedPagesFile( idFile * file ) {
	delete file;
}

void idVirtualTextureSystem::ValidateCache_f( const idCmdArgs & args ) {
	if ( args.Argc() < 2 || args.Argc() > 3 ) {
		idLib::Printf( "usage: vt_validateCache <pageFile.pages> [maxPages]\n" );
		return;
	}
	const int maxPages = args.Argc() == 3 ? idMath::ClampInt( 1, 1000000, atoi( args.Argv( 2 ) ) ) : 1024;
	idVirtualTexture test;
	if ( !test.SetSource( "validation", args.Argv( 1 ), PHYSICAL_PAGES_POOL_UNIQUE, ID_VTF_BAKE ) ) {
		idLib::Printf( "VT VALIDATION FAILED: could not load '%s'\n", args.Argv( 1 ) );
		return;
	}
	vtPageValidation_t validation;
	const bool valid = test.ValidatePageFile( maxPages, validation );
	idLib::Printf( "VT VALIDATION %s: '%s'\n", valid ? "PASSED" : "FAILED", args.Argv( 1 ) );
	test.Print();
	idLib::Printf( "checked %i page headers (%i HDP), discovered %i, bad %i, file %lld bytes\n",
		validation.pagesChecked, validation.hdpPages, validation.queuedPages,
		validation.badPages, validation.fileBytes );
}

void idVirtualTextureSystem::TestEncoder_f( const idCmdArgs & args ) {
	if ( args.Argc() > 3 ) {
		idLib::Printf( "usage: vt_testEncoder [output.pages] [quality]\n" );
		return;
	}
	const int quality = args.Argc() == 3 ? atoi( args.Argv( 2 ) ) : 85;
	idVirtualTextureEncoder encoder;
	idStr error;
	int encodedBytes = 0;
	float meanError = 0.0f;
	int maximumError = 0;
	if ( !encoder.TestDCTRoundTrip( quality, encodedBytes, meanError, maximumError, error ) ) {
		idLib::Printf( "VT ENCODER FAILED: %s\n", error.c_str() );
		return;
	}
	idLib::Printf( "VT DCT ENCODER PASSED: %i bytes, RGB mean error %.2f, max %i\n",
		encodedBytes, meanError, maximumError );
	if ( args.Argc() < 2 ) {
		return;
	}

	idList< byte > diffuse;
	idList< byte > normal;
	idList< byte > specular;
	diffuse.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
	normal.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
	specular.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
	for ( int y = 0; y < idVirtualTextureEncoder::PAGE_TEXELS; ++y ) {
		for ( int x = 0; x < idVirtualTextureEncoder::PAGE_TEXELS; ++x ) {
			const int offset = 4 * ( y * idVirtualTextureEncoder::PAGE_TEXELS + x );
			diffuse[offset + 0] = static_cast< byte >( ( x * 255 ) / 127 );
			diffuse[offset + 1] = static_cast< byte >( ( y * 255 ) / 127 );
			diffuse[offset + 2] = static_cast< byte >( ( ( x ^ y ) * 255 ) / 127 );
			diffuse[offset + 3] = 255;
			normal[offset + 0] = 128;
			normal[offset + 1] = 128;
			normal[offset + 2] = 255;
			normal[offset + 3] = 255;
			specular[offset + 0] = 32;
			specular[offset + 1] = 32;
			specular[offset + 2] = 32;
			specular[offset + 3] = 255;
		}
	}
	if ( !encoder.WriteUncompressedPageFile( args.Argv( 1 ), diffuse.Ptr(),
			normal.Ptr(), specular.Ptr(), error ) ) {
		idLib::Printf( "VT CACHE ENCODER FAILED: %s\n", error.c_str() );
		return;
	}
	idVirtualTexture test;
	vtPageValidation_t validation;
	if ( !test.SetSource( "encoder-test", args.Argv( 1 ),
			PHYSICAL_PAGES_POOL_UNIQUE, ID_VTF_BAKE ) ||
			!test.ValidatePageFile( 1, validation ) ) {
		idLib::Printf( "VT CACHE ENCODER FAILED: output did not validate\n" );
		return;
	}
	idLib::Printf( "VT CACHE ENCODER PASSED: wrote and re-opened %lld bytes at '%s'\n",
		validation.fileBytes, args.Argv( 1 ) );
}

void idVirtualTextureSystem::TestHDPWriter_f( const idCmdArgs & args ) {
	if ( args.Argc() > 3 ) {
		idLib::Printf( "usage: vt_testHDPWriter [quality] [pageFile.pages]\n" );
		return;
	}
	const int quality = args.Argc() == 2 ? atoi( args.Argv( 1 ) ) : 30;
	idVirtualTextureEncoder encoder;
	idStr error;
	int encodedBytes = 0;
	unsigned int checksum = 0;
	if ( !encoder.TestHDPRoundTrip( quality, encodedBytes, checksum, error ) ) {
		idLib::Printf( "VT HDP WRITER FAILED: %s\n", error.c_str() );
		return;
	}
	idLib::Printf( "VT HDP WRITER PASSED: %i-byte patterned YCoCg stream, decoded checksum %08x\n",
		encodedBytes, checksum );
	if ( args.Argc() == 3 ) {
		idList< byte > diffuse;
		idList< byte > normal;
		idList< byte > specular;
		diffuse.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
		normal.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
		specular.SetNum( idVirtualTextureEncoder::PAGE_RGBA_BYTES );
		for ( int y = 0; y < idVirtualTextureEncoder::PAGE_TEXELS; ++y ) {
			for ( int x = 0; x < idVirtualTextureEncoder::PAGE_TEXELS; ++x ) {
				const int offset = ( y * idVirtualTextureEncoder::PAGE_TEXELS + x ) * 4;
				diffuse[offset + 0] = static_cast< byte >( x * 2 );
				diffuse[offset + 1] = static_cast< byte >( y * 2 );
				diffuse[offset + 2] = static_cast< byte >( ( x ^ y ) * 2 );
				diffuse[offset + 3] = 255;
				normal[offset + 0] = static_cast< byte >( 96 + ( x & 63 ) );
				normal[offset + 1] = static_cast< byte >( 96 + ( y & 63 ) );
				normal[offset + 2] = 255;
				normal[offset + 3] = 255;
				specular[offset + 0] = static_cast< byte >( ( x + y ) & 127 );
				specular[offset + 1] = specular[offset + 0];
				specular[offset + 2] = specular[offset + 0];
				specular[offset + 3] = 255;
			}
		}
		if ( !encoder.WriteHDPPageFile( args.Argv( 2 ), diffuse.Ptr(), normal.Ptr(),
				specular.Ptr(), quality, error ) ) {
			idLib::Printf( "VT HDP CONTAINER FAILED: %s\n", error.c_str() );
			return;
		}
		idVirtualTexture test;
		vtPageValidation_t validation;
		if ( !test.SetSource( "hdp-encoder-test", args.Argv( 2 ),
				PHYSICAL_PAGES_POOL_UNIQUE, ID_VTF_BAKE ) ||
				!test.ValidatePageFile( 1, validation ) || validation.hdpPages != 1 ||
				validation.badPages != 0 ) {
			idLib::Printf( "VT HDP CONTAINER FAILED: output did not validate\n" );
			return;
		}
		idLib::Printf( "VT HDP CONTAINER PASSED: wrote and re-opened %lld bytes at '%s'\n",
			validation.fileBytes, args.Argv( 2 ) );
	}
}

void idVirtualTextureSystem::DecodeHDP_f( const idCmdArgs & args ) {
	if ( args.Argc() != 2 ) {
		idLib::Printf( "usage: vt_decodeHDP <pageFile.pages>\n" );
		return;
	}
	idLib::Printf( "VT HDP DECODE: opening '%s'\n", args.Argv( 1 ) );
	idVirtualTexture texture;
	if ( !texture.SetSource( "hdp-decode", args.Argv( 1 ),
			PHYSICAL_PAGES_POOL_UNIQUE, ID_VTF_BAKE ) ) {
		idLib::Printf( "VT HDP DECODE FAILED: could not open '%s'\n", args.Argv( 1 ) );
		return;
	}
	idLib::Printf( "VT HDP DECODE: container opened\n" );
	const int64 rootOffset = static_cast< int64 >( texture.header.diskOffsetScale ) *
		texture.header.subRoot.finerDiskOffset[0];
	const int rootLength = texture.header.diskOffsetScale *
		texture.header.subRoot.finerDiskLength[0];
	if ( rootLength < static_cast< int >( sizeof( pageHeader_t ) + sizeof( HDPHeader_t ) ) ||
			rootLength > 1024 * 1024 ) {
		idLib::Printf( "VT HDP DECODE FAILED: invalid root page length %i\n", rootLength );
		return;
	}
	idList< byte > diskPage;
	diskPage.SetNum( rootLength );
	if ( texture.pageFile->ReadAt( rootOffset, diskPage.Ptr(), rootLength ) != rootLength ) {
		idLib::Printf( "VT HDP DECODE FAILED: short root-page read\n" );
		return;
	}
	idLib::Printf( "VT HDP DECODE: root page read (%i bytes)\n", rootLength );
	pageHeader_t page;
	memcpy( &page, diskPage.Ptr(), sizeof( page ) );
	page.Swap();
	if ( page.magic != PAGE_SUBROOT_MAGIC || page.pageCompression != COMP_HDP ) {
		idLib::Printf( "VT HDP DECODE FAILED: root compression is %i, not HDP\n",
			static_cast< int >( page.pageCompression ) );
		return;
	}
	HDPHeader_t header;
	memcpy( &header, diskPage.Ptr() + sizeof( pageHeader_t ), sizeof( header ) );
	// Tech 5 stores the five codec stream lengths in network byte order even
	// though the surrounding page headers use the platform-swapped disk ABI.
	header.diffuseSize = static_cast< uint16 >( BigShort( static_cast< short >( header.diffuseSize ) ) );
	header.normalSize = static_cast< uint16 >( BigShort( static_cast< short >( header.normalSize ) ) );
	header.specularSize = static_cast< uint16 >( BigShort( static_cast< short >( header.specularSize ) ) );
	header.powerSize = static_cast< uint16 >( BigShort( static_cast< short >( header.powerSize ) ) );
	header.alphaSize = static_cast< uint16 >( BigShort( static_cast< short >( header.alphaSize ) ) );
	const int coverSize = ( header.flags & 8 ) != 0 ? 2048 : 0;
	const int streamBytes = header.diffuseSize + header.normalSize +
		header.specularSize + header.powerSize + coverSize + header.alphaSize;
	if ( header.diffuseSize == 0 || header.normalSize == 0 ||
			header.specularSize == 0 || header.powerSize == 0 ||
			streamBytes > rootLength - sizeof( pageHeader_t ) - sizeof( HDPHeader_t ) ) {
		idLib::Printf( "VT HDP DECODE FAILED: invalid stream lengths (%i bytes)\n", streamBytes );
		return;
	}

	const byte * input = diskPage.Ptr() + sizeof( pageHeader_t ) + sizeof( HDPHeader_t );
	idLib::Printf( "VT HDP DECODE: streams d=%i n=%i s=%i p=%i\n",
		header.diffuseSize, header.normalSize, header.specularSize, header.powerSize );
	idList< byte > output;
	output.SetNum( 0x30000 );
	memset( output.Ptr(), 0, output.Num() );
	idList< byte > scratch;
	scratch.SetNum( 49152 + 127 );
	void * alignedScratch = reinterpret_cast< void * >(
		( reinterpret_cast< uintptr_t >( scratch.Ptr() ) + 127 ) & ~static_cast< uintptr_t >( 127 ) );
	idBareHDP decoder;
	decoder.tempBuffer = alignedScratch;
	decoder.tempBufferSize = 49152;
	decoder.quality = header.qualityDiffuse;
	const bool diffuseOK = decoder.DecompressImageYCoCg( input, output.Ptr(),
		128, 128, header.diffuseSize, 0 );
	const int diffuseStage = hdpDecInitStage;
	const int diffuseX = hdpDecMacroblockX, diffuseY = hdpDecMacroblockY;
	const int diffuseChannel = hdpDecChannel, diffuseBlock = hdpDecBlock;
	decoder.quality = header.qualityNormal;
	const bool normalOK = decoder.DecompressImageNormalMap(
		input + header.diffuseSize, output.Ptr() + 0x10000,
		128, 128, header.normalSize, 0 );
	const int normalStage = hdpDecInitStage;
	const int normalX = hdpDecMacroblockX, normalY = hdpDecMacroblockY;
	const int normalChannel = hdpDecChannel, normalBlock = hdpDecBlock;
	decoder.quality = header.qualitySpecular;
	const byte * specularInput = input + header.diffuseSize + header.normalSize;
	const bool specularOK = ( header.flags & 4 ) != 0
		? decoder.DecompressImageGrayScale( specularInput, output.Ptr() + 0x20000,
			128, 128, header.specularSize, 0 )
		: decoder.DecompressImageRGB( specularInput, output.Ptr() + 0x20000,
			128, 128, header.specularSize, 0 );
	const int specularStage = hdpDecInitStage;
	const int specularX = hdpDecMacroblockX, specularY = hdpDecMacroblockY;
	const int specularChannel = hdpDecChannel, specularBlock = hdpDecBlock;
	decoder.quality = header.qualityPower;
	const bool powerOK = decoder.DecompressImageMono(
		input + header.diffuseSize + header.normalSize + header.specularSize,
		output.Ptr() + 0x2FC00, 32, 32, header.powerSize, 0 );
	const int powerStage = hdpDecInitStage;
	const int powerX = hdpDecMacroblockX, powerY = hdpDecMacroblockY;
	const int powerChannel = hdpDecChannel, powerBlock = hdpDecBlock;
	if ( !diffuseOK || !normalOK || !specularOK || !powerOK ) {
		idLib::Printf( "VT HDP DECODE FAILED:\n"
			"  diffuse=%i@%i mb(%i,%i) c%i b%i\n"
			"  normal=%i@%i mb(%i,%i) c%i b%i\n"
			"  specular=%i@%i mb(%i,%i) c%i b%i\n"
			"  power=%i@%i mb(%i,%i) c%i b%i\n",
			diffuseOK, diffuseStage, diffuseX, diffuseY, diffuseChannel, diffuseBlock,
			normalOK, normalStage, normalX, normalY, normalChannel, normalBlock,
			specularOK, specularStage, specularX, specularY, specularChannel, specularBlock,
			powerOK, powerStage, powerX, powerY, powerChannel, powerBlock );
		return;
	}
	uint64 hash = UINT64_C( 1469598103934665603 );
	int nonZero = 0;
	for ( int index = 0; index < output.Num(); ++index ) {
		hash ^= output[index];
		hash *= UINT64_C( 1099511628211 );
		nonZero += output[index] != 0;
	}
	idLib::Printf( "VT HDP DECODE PASSED: root %i bytes, streams %i, nonzero %i/%i, hash %08x%08x\n",
		rootLength, streamBytes, nonZero, output.Num(),
		static_cast< uint32 >( hash >> 32 ), static_cast< uint32 >( hash ) );
}

void idVirtualTextureSystem::SetSource_f( const idCmdArgs & args ) {
	if ( args.Argc() != 3 ) {
		idLib::Printf( "usage: vt_setSource <pageSource 1-15> <pageFile.pages>\n" );
		return;
	}
	const int source = atoi( args.Argv( 1 ) );
	if ( source <= PAGESOURCE_INVALID || source >= PAGESOURCE_TOTAL ) {
		idLib::Printf( "'%s' is not a valid pageSource\n", args.Argv( 1 ) );
		return;
	}
	idStr sourceName = args.Argv( 2 );
	sourceName.StripPath();
	sourceName.StripFileExtension();
	idVirtualTexture & texture = virtualTextureSystem.vts[source];
	if ( source == PAGESOURCE_VMTR ) {
		virtualTextureSystem.InvalidateRuntime();
	}
	const virtualTextureFlavor_t flavor = texture.flavor == ID_VTF_UNKNOWN ? ID_VTF_BAKE : texture.flavor;
	const physicalPagesPool_t pool = flavor == ID_VTF_VMTR ? PHYSICAL_PAGES_POOL_VMTR : PHYSICAL_PAGES_POOL_UNIQUE;
	if ( texture.SetSource( sourceName.c_str(), args.Argv( 2 ), pool, flavor ) ) {
		idLib::Printf( "VT source %i now uses '%s'\n", source, args.Argv( 2 ) );
		texture.Print();
	} else {
		idLib::Printf( "failed to set VT source %i to '%s'\n", source, args.Argv( 2 ) );
	}
}

void idVirtualTextureSystem::EmptyCache_f( const idCmdArgs & ) {
	virtualTextureSystem.EmptyCache( false );
	idLib::Printf( "virtual texture caches emptied\n" );
}

void idVirtualTextureSystem::ListVirtualTextures_f( const idCmdArgs & ) {
	int count = 0;
	for ( int source = 1; source < PAGESOURCE_TOTAL; ++source ) {
		const idVirtualTexture & texture = virtualTextureSystem.vts[source];
		if ( texture.pageFile == NULL ) {
			continue;
		}
		idLib::Printf( "%2i: %s\n", source, texture.pageFilePath.c_str() );
		++count;
	}
	idLib::Printf( "%i virtual texture source(s) loaded\n", count );
}

void idVirtualTextureSystem::BuildVmtrCache_f( const idCmdArgs & args ) {
	for ( int index = 1; index < args.Argc(); ++index ) {
		const idMaterial * material = declManager->FindMaterial( args.Argv( index ), false );
		if ( material == NULL ) {
			idLib::Warning( "vt_buildVmtrCache: material '%s' was not found", args.Argv( index ) );
			continue;
		}
		material->ReloadImages( false );
	}
	virtualTextureSystem.BuildLoadedVmtrCache( true );
}

void idVirtualTextureSystem::RuntimeStats_f( const idCmdArgs & ) {
	virtualTextureSystem.PrintRuntimeStats();
}
