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
idCVar vt_showFeedback( "vt_showFeedback", "0",
	CVAR_RENDERER | CVAR_INTEGER,
	"visualize streamed VMTR pages: 1 = borders/numbers, 2 = borders/numbers/page colors", 0, 2 );

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

// The interaction VT contains opaque surface response only. Translucent and
// post-process materials (notably glass using _currentRender) do not
// participate in feedback and must never switch their stage images according
// to incidental page residency.
bool MaterialIsEligibleForVT( const idMaterial * material ) {
	return material != NULL && material->ReceivesLighting() &&
		material->Coverage() != MC_TRANSLUCENT &&
		material->GetSort() < SS_POST_PROCESS;
}

void AppendUniqueImage( idList< idImage * > & images, const idImage * image ) {
	if ( image == NULL ) {
		return;
	}
	idImage * mutableImage = const_cast< idImage * >( image );
	if ( images.FindIndex( mutableImage ) < 0 ) {
		images.Append( mutableImage );
	}
}

}

struct vtRuntimePageJob_t {
	int pageIndex;
	int pageX;
	int pageY;
	int pageLevel;
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

static const char * vtDebugDigits[10] = {
	"111101101101111", "010110010010111", "111001111100111", "111001111001111",
	"101101111001001", "111100111001111", "111100111101111", "111001001001001",
	"111101111101111", "111101111001111"
};

static void SetVTDebugPixel( byte * rgba, int x, int y, const byte color[3] ) {
	if ( x < 0 || y < 0 || x >= idVirtualTextureSystem::VT_PAGE_TEXELS ||
			y >= idVirtualTextureSystem::VT_PAGE_TEXELS ) { return; }
	byte * pixel = rgba + ( y * idVirtualTextureSystem::VT_PAGE_TEXELS + x ) * 4;
	pixel[0] = color[0]; pixel[1] = color[1]; pixel[2] = color[2]; pixel[3] = 255;
}

static void DrawVTDebugDigit( byte * rgba, int originX, int originY, int scale,
		int digit, const byte color[3] ) {
	digit = idMath::ClampInt( 0, 9, digit );
	const char * glyph = vtDebugDigits[digit];
	for ( int gy = 0; gy < 5; ++gy ) {
		for ( int gx = 0; gx < 3; ++gx ) {
			if ( glyph[gy * 3 + gx] != '1' ) { continue; }
			for ( int py = 0; py < scale; ++py ) {
				for ( int px = 0; px < scale; ++px ) {
					SetVTDebugPixel( rgba, originX + gx * scale + px,
						originY + gy * scale + py, color );
				}
			}
		}
	}
}

static void OverlayVTPageDebug( vtRuntimePageJob_t & job, int mode ) {
	if ( mode <= 0 || job.planes.Num() < idVirtualTextureSystem::VT_PAGE_TEXELS *
			idVirtualTextureSystem::VT_PAGE_TEXELS * 4 ) { return; }
	byte * diffuse = job.planes.Ptr();
	if ( mode >= 2 ) {
		const byte tint[3] = {
			static_cast< byte >( 48 + ( job.pageX * 67 + job.pageY * 17 ) % 160 ),
			static_cast< byte >( 48 + ( job.pageX * 23 + job.pageY * 83 ) % 160 ),
			static_cast< byte >( 48 + ( job.pageX * 97 + job.pageY * 31 ) % 160 ) };
		for ( int y = 4; y < 124; ++y ) {
			for ( int x = 4; x < 124; ++x ) {
				byte * pixel = diffuse + ( y * idVirtualTextureSystem::VT_PAGE_TEXELS + x ) * 4;
				for ( int channel = 0; channel < 3; ++channel ) {
					pixel[channel] = static_cast< byte >( ( pixel[channel] * 2 + tint[channel] ) / 3 );
				}
			}
		}
	}
	const byte borderColor[3] = { 190, 255, 24 };
	for ( int i = 4; i < 124; ++i ) {
		for ( int thickness = 0; thickness < 2; ++thickness ) {
			SetVTDebugPixel( diffuse, i, 4 + thickness, borderColor );
			SetVTDebugPixel( diffuse, i, 123 - thickness, borderColor );
			SetVTDebugPixel( diffuse, 4 + thickness, i, borderColor );
			SetVTDebugPixel( diffuse, 123 - thickness, i, borderColor );
		}
	}
	const byte shadow[3] = { 8, 8, 8 };
	const byte xColor[3] = { 24, 255, 150 };
	const byte yColor[3] = { 255, 40, 110 };
	const int scale = 4;
	const int startX = 28;
	const int startY = 54;
	const int positions[4] = { startX, startX + 16, startX + 44, startX + 60 };
	const int digits[4] = { ( job.pageX / 10 ) % 10, job.pageX % 10,
		( job.pageY / 10 ) % 10, job.pageY % 10 };
	for ( int digit = 0; digit < 4; ++digit ) {
		DrawVTDebugDigit( diffuse, positions[digit] + 2, startY + 2, scale,
			digits[digit], shadow );
		DrawVTDebugDigit( diffuse, positions[digit], startY, scale,
			digits[digit], digit < 2 ? xColor : yColor );
	}
}

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
			page.x != job.pageX || page.y != job.pageY || page.level != job.pageLevel ||
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

idVirtualTextureSystem::idVirtualTextureSystem()
	: initialized( false ), runtimeReady( false ), feedbackDestination( NULL ),
	  feedbackWriteIndex( 0 ), feedbackPreviousFBO( 0 ),
	  feedbackPreviousScissorEnabled( false ), feedbackUsesSceneDepth( false ), runtimeFrame( 0 ),
	  runtimeLatestFeedbackFrame( 0 ),
	  runtimeRequests( 0 ), runtimeUploads( 0 ), runtimeEvictions( 0 ),
	  runtimeFeedbackAnalyses( 0 ), runtimeFeedbackPages( 0 ),
	  runtimeFeedbackPending( false ), runtimeDebugMode( 0 ),
	  runtimePageWorker( NULL ), runtimeSourceBytesReleased( 0 ) {
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
		"rebuild <modfolder>/generated/virtualtextures/<mapname>_vmtr.pages from referenced materials: vt_buildVmtrCache [material ...]" );
	cmdSystem->AddCommand( "vt_runtimeStats", RuntimeStats_f, CMD_FL_RENDERER,
		"print virtual texture feedback and physical-cache statistics" );
	idLib::Printf( "virtual texture container layer initialized\n" );
}

void idVirtualTextureSystem::Shutdown() {
	if ( !initialized ) {
		return;
	}
	InvalidateRuntime();
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
	idStrStatic< 128 > newMapName = name != NULL ? name : "";
	newMapName.BackSlashesToSlashes();
	newMapName.StripPath();
	newMapName.StripFileExtension();
	if ( currentMapName.Icmp( newMapName ) != 0 ) {
		pendingFallbackImages.Clear();
		demotedFallbackImages.Clear();
		runtimeSourceBytesReleased = 0;
	}
	currentMapName = newMapName;
}

void idVirtualTextureSystem::QueueVirtualTextureFallbackImages() {
	pendingFallbackImages.Clear();
	idList< idImage * > candidates;
	idList< idImage * > protectedImages;
	const int materialCount = declManager->GetNumDecls( DECL_MATERIAL );
	for ( int materialIndex = 0; materialIndex < materialCount; ++materialIndex ) {
		const idMaterial * material = static_cast< const idMaterial * >(
			declManager->DeclByIndex( DECL_MATERIAL, materialIndex, false ) );
		if ( material == NULL || material->GetState() != DS_PARSED || !material->EverReferenced() ) {
			continue;
		}
		const bool mapped = MaterialIsEligibleForVT( material ) &&
			VirtualMaterialForMaterial( material ) != NULL;
		for ( int stageIndex = 0; stageIndex < material->GetNumStages(); ++stageIndex ) {
			const shaderStage_t * stage = material->GetStage( stageIndex );
			if ( stage == NULL || stage->texture.image == NULL ) {
				continue;
			}
			const bool interactionStage = stage->lighting == SL_BUMP ||
				stage->lighting == SL_DIFFUSE || stage->lighting == SL_SPECULAR;
			AppendUniqueImage( mapped && interactionStage ? candidates : protectedImages,
				stage->texture.image );
		}
	}

	for ( int i = 0; i < candidates.Num(); ++i ) {
		idImage * image = candidates[i];
		if ( protectedImages.FindIndex( image ) < 0 && image->CanUseVirtualTextureFallback() ) {
			pendingFallbackImages.Append( image );
		}
	}
	if ( pendingFallbackImages.Num() > 0 ) {
		idLib::Printf( "VT runtime: queued %i source images for lowest-mip fallback\n",
			pendingFallbackImages.Num() );
	}
}

void idVirtualTextureSystem::ProcessVirtualTextureFallbackImages() {
	if ( !runtimeReady || !r_virtualTexture.GetBool() ) {
		return;
	}
	static const int DEMOTIONS_PER_FRAME = 16;
	int demotedThisFrame = 0;
	while ( pendingFallbackImages.Num() > 0 && demotedThisFrame < DEMOTIONS_PER_FRAME ) {
		const int index = pendingFallbackImages.Num() - 1;
		idImage * image = pendingFallbackImages[index];
		pendingFallbackImages.RemoveIndexFast( index );
		if ( image == NULL || !image->CanUseVirtualTextureFallback() ) {
			continue;
		}
		const int originalWidth = image->GetUploadWidth();
		const int originalHeight = image->GetUploadHeight();
		const int originalBytes = image->IsLoaded() ? image->StorageSize() : 0;
		image->DemoteToVirtualTextureFallback();
		if ( image->IsVirtualTextureFallback() ) {
			const int fallbackBytes = image->IsLoaded() ? image->StorageSize() : 0;
			runtimeSourceBytesReleased += Max( 0, originalBytes - fallbackBytes );
			demotedFallbackImages.Append( image );
			++demotedThisFrame;
			if ( demotedFallbackImages.Num() == 1 ) {
				idLib::Printf( "VT runtime: source-image eviction active; '%s' reduced %ix%i -> %ix%i\n",
					image->GetName(), originalWidth, originalHeight,
					image->GetUploadWidth(), image->GetUploadHeight() );
			}
		}
	}
	if ( pendingFallbackImages.Num() == 0 && demotedThisFrame > 0 ) {
		idLib::Printf( "VT runtime: %i source images lowest-mip-only, %.1f MiB released\n",
			demotedFallbackImages.Num(), runtimeSourceBytesReleased / ( 1024.0f * 1024.0f ) );
	}
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

	idStr cacheBase = currentMapName;
	cacheBase += "_vmtr";
	// generated VT archives belong inside the active mod folder, not beside
	// the executable; fs_game wins when a mod is running
	idStr outputDirectory = cvarSystem->GetCVarString( "fs_basepath" );
	const char * modFolder = cvarSystem->GetCVarString( "fs_game" );
	if ( modFolder == nullptr || modFolder[0] == '\0' ) {
		modFolder = BASE_GAMEDIR;
	}
	outputDirectory.AppendPath( modFolder );
	outputDirectory.AppendPath( "generated" );
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
			QueueVirtualTextureFallbackImages();
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
				!material->EverReferenced() || !MaterialIsEligibleForVT( material ) ) {
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
		source.diffuseGeneratedName = NULL;
		source.normalGeneratedName = NULL;
		source.specularGeneratedName = NULL;
		source.width = 1;
		source.height = 1;
		const idImage * sourceImages[3] = { diffuseImage, normalImage, specularImage };
		for ( int imageIndex = 0; imageIndex < 3; ++imageIndex ) {
			int width = 0;
			int height = 0;
			if ( sourceImages[imageIndex] != NULL &&
					sourceImages[imageIndex]->GetVirtualTextureSourceDimensions( width, height ) ) {
				source.width = Max( source.width, width );
				source.height = Max( source.height, height );
			}
		}
		sources.Append( source );
	}

	if ( sources.Num() == 0 ) {
		idLib::Printf( "VMTR builder: no referenced interaction materials; keeping the existing cache\n" );
		return false;
	}

	// Windows does not permit replacing a pages file while this slot still has
	// it open.  A missing per-map archive is a blocking level-load operation:
	// build, publish, validate, and attach it before returning to the game.
	InvalidateRuntime();
	vts[PAGESOURCE_VMTR].ReleaseSource();
	idLib::Printf( "Building VMTR page archive for '%s' (%i materials). "
		"Map loading will resume after it is ready.\n", currentMapName.c_str(), sources.Num() );
	idList< idStr > generatedImageNames;
	generatedImageNames.SetNum( sources.Num() * 3 );
	for ( int sourceIndex = 0; sourceIndex < sources.Num(); ++sourceIndex ) {
		const idImage * images[3] = { sources[sourceIndex].diffuseImage,
			sources[sourceIndex].normalImage, sources[sourceIndex].specularImage };
		for ( int plane = 0; plane < 3; ++plane ) {
			if ( images[plane] != NULL ) {
				images[plane]->GetVirtualTextureGeneratedName(
					generatedImageNames[sourceIndex * 3 + plane] );
			}
		}
		sources[sourceIndex].diffuseGeneratedName =
			generatedImageNames[sourceIndex * 3 + 0].c_str();
		sources[sourceIndex].normalGeneratedName =
			generatedImageNames[sourceIndex * 3 + 1].c_str();
		sources[sourceIndex].specularGeneratedName =
			generatedImageNames[sourceIndex * 3 + 2].c_str();
	}
	const int startTime = Sys_Milliseconds();
	idVirtualTextureEncoder encoder;
	idStr error;
	if ( !encoder.WriteHDPPageArchive( pagesPath.c_str(), metadataPath.c_str(),
			sources.Ptr(), sources.Num(), 30, error ) ) {
		idLib::Warning( "VMTR page build failed: %s", error.c_str() );
		return false;
	}

	if ( !vts[PAGESOURCE_VMTR].SetSource( cacheBase.c_str(), pagesPath.c_str(),
			PHYSICAL_PAGES_POOL_VMTR, ID_VTF_VMTR ) ) {
		idLib::Warning( "VMTR page build completed but the archive could not be reopened" );
		return false;
	}
	vtPageValidation_t validation;
	if ( !vts[PAGESOURCE_VMTR].ValidatePageFile(
			vts[PAGESOURCE_VMTR].header.totalPages, validation ) || validation.badPages != 0 ||
			validation.pagesChecked != vts[PAGESOURCE_VMTR].header.totalPages ) {
		idLib::Warning( "VMTR page build produced an invalid page tree (%i checked, %i bad)",
			validation.pagesChecked, validation.badPages );
		vts[PAGESOURCE_VMTR].ReleaseSource();
		return false;
	}
	idLib::Printf( "VMTR page archive ready: %i materials, %i HDP pages, "
		"%lld bytes in %.1f seconds\n", sources.Num(), validation.hdpPages,
		validation.fileBytes, ( Sys_Milliseconds() - startTime ) * 0.001f );
	QueueVirtualTextureFallbackImages();
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
	return MaterialIsEligibleForVT( material ) && vt.vmtrData != NULL
		? vt.vmtrData->VirtualMaterialForMaterial( material ) : NULL;
}

int idVirtualTextureSystem::RuntimePageIndex( int level, int pageX, int pageY ) const {
	const idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	if ( level < 0 || level >= vt.header.numLevels ) {
		return -1;
	}
	int offset = 0;
	for ( int mip = 0; mip < level; ++mip ) {
		const int mipWidth = Max( 1, vt.header.pagesWide >> mip );
		offset += mipWidth * mipWidth;
	}
	const int mipWidth = Max( 1, vt.header.pagesWide >> level );
	if ( pageX < 0 || pageY < 0 || pageX >= mipWidth || pageY >= mipWidth ) {
		return -1;
	}
	const int index = offset + pageY * mipWidth + pageX;
	return index >= 0 && index < runtimePages.Num() ? index : -1;
}

bool idVirtualTextureSystem::BuildRuntimePageIndex() {
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	if ( vt.pageFile == NULL || vt.header.pagesWide <= 0 || vt.header.numLevels <= 0 ||
			vt.header.numLevels > idVirtualTexture::PAGEFILE_MAX_LOD_CLAMP ) {
		return false;
	}
	int pageCount = 0;
	for ( int level = 0; level < vt.header.numLevels; ++level ) {
		const int mipWidth = Max( 1, vt.header.pagesWide >> level );
		pageCount += mipWidth * mipWidth;
	}
	runtimePages.SetNum( pageCount );
	memset( runtimePages.Ptr(), 0, pageCount * sizeof( runtimePages[0] ) );

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
		const int pageIndex = RuntimePageIndex( ref.level, ref.x, ref.y );
		if ( pageIndex < 0 ) {
			return false;
		}
		vtRuntimeDiskPage_t & runtimePage = runtimePages[pageIndex];
		runtimePage.offset = ref.offset;
		runtimePage.length = ref.length;
		runtimePage.x = ref.x;
		runtimePage.y = ref.y;
		runtimePage.level = ref.level;
		if ( ref.level == 0 ) {
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
	for ( int page = 0; page < runtimePages.Num(); ++page ) {
		if ( runtimePages[page].length == 0 ) {
			return false;
		}
	}
	return true;
}

bool idVirtualTextureSystem::EnsureRuntimeResources() {
	if ( runtimeReady ) {
		return true;
	}
	if ( !r_virtualTexture.GetBool() || !BuildRuntimePageIndex() ) {
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

	runtimePageToSlot.SetNum( runtimePages.Num() );
	runtimePagePending.SetNum( runtimePages.Num() );
	runtimePageLastVisible.SetNum( runtimePages.Num() );
	for ( int i = 0; i < runtimePageToSlot.Num(); ++i ) {
		runtimePageToSlot[i] = -1;
		runtimePagePending[i] = 0;
		runtimePageLastVisible[i] = 0;
	}
	const int slotCount = VT_CACHE_PAGES_WIDE * VT_CACHE_PAGES_WIDE;
	runtimeSlotToPage.SetNum( slotCount );
	runtimeSlotLastUsed.SetNum( slotCount );
	runtimeSlotPinned.SetNum( slotCount );
	for ( int i = 0; i < slotCount; ++i ) {
		runtimeSlotToPage[i] = -1;
		runtimeSlotLastUsed[i] = 0;
		runtimeSlotPinned[i] = 0;
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
	runtimeFrame = runtimeLatestFeedbackFrame = 0;
	runtimeRequests = runtimeUploads = runtimeEvictions = 0;
	runtimeFeedbackAnalyses = runtimeFeedbackPages = 0;
	runtimeFeedbackPending = false;
	runtimeDebugMode = vt_showFeedback.GetInteger();
	runtimeReady = true;
	// Keep the root and the next two quadtree levels resident. These 21 pages
	// provide a stable fallback while finer feedback-driven pages stream.
	const int firstPinnedLevel = Max( 0, vt.header.numLevels - 3 );
	for ( int level = vt.header.numLevels - 1; level >= firstPinnedLevel; --level ) {
		const int mipWidth = Max( 1, vt.header.pagesWide >> level );
		for ( int y = 0; y < mipWidth; ++y ) {
			for ( int x = 0; x < mipWidth; ++x ) {
				MakePageResident( level, x, y );
			}
		}
	}
	InvalidateTextureBindingCache();
	idLib::Printf( "VT runtime: %ix%i virtual pages, %ix%i physical cache (%ix%i texels), %ix%i feedback\n",
		vt.header.pagesWide, vt.header.pagesWide, VT_CACHE_PAGES_WIDE,
		VT_CACHE_PAGES_WIDE, VT_CACHE_PAGES_WIDE * VT_PAGE_TEXELS,
		VT_CACHE_PAGES_WIDE * VT_PAGE_TEXELS, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT );
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

void idVirtualTextureSystem::RefreshPageTableRegion( int pageIndex ) {
	if ( pageIndex < 0 || pageIndex >= runtimePages.Num() || runtimePageTableShadow.Num() == 0 ) {
		return;
	}
	const idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	const vtRuntimeDiskPage_t & changedPage = runtimePages[pageIndex];
	const int regionSize = 1 << changedPage.level;
	const int firstX = changedPage.x * regionSize;
	const int firstY = changedPage.y * regionSize;
	const int lastX = Min( vt.header.pagesWide, firstX + regionSize );
	const int lastY = Min( vt.header.pagesWide, firstY + regionSize );
	for ( int leafY = firstY; leafY < lastY; ++leafY ) {
		for ( int leafX = firstX; leafX < lastX; ++leafX ) {
			byte * mapping = runtimePageTableShadow.Ptr() +
				( leafY * vt.header.pagesWide + leafX ) * 4;
			memset( mapping, 0, 4 );
			for ( int level = 0; level < vt.header.numLevels; ++level ) {
				const int candidate = RuntimePageIndex( level, leafX >> level, leafY >> level );
				if ( candidate < 0 ) {
					continue;
				}
				const int slot = runtimePageToSlot[candidate];
				if ( slot < 0 ) {
					continue;
				}
				mapping[0] = static_cast< byte >( slot % VT_CACHE_PAGES_WIDE );
				mapping[1] = static_cast< byte >( slot / VT_CACHE_PAGES_WIDE );
				mapping[2] = static_cast< byte >( level );
				mapping[3] = 255;
				break;
			}
		}
	}
}

bool idVirtualTextureSystem::MakePageResident( int level, int pageX, int pageY ) {
	if ( !runtimeReady || runtimePageWorker == NULL ) {
		return false;
	}
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	const int pageIndex = RuntimePageIndex( level, pageX, pageY );
	if ( pageIndex < 0 ) {
		return false;
	}
	const int residentSlot = runtimePageToSlot[pageIndex];
	if ( residentSlot >= 0 ) {
		runtimeSlotLastUsed[residentSlot] = runtimeFrame;
		return true;
	}
	if ( runtimePagePending[pageIndex] != 0 ) {
		return false;
	}
	const vtRuntimeDiskPage_t & ref = runtimePages[pageIndex];
	if ( ref.length < static_cast< int >( sizeof( pageHeader_t ) ) || ref.length > 1024 * 1024 ) {
		return false;
	}
	vtRuntimePageJob_t * job = new ( TAG_RENDER ) vtRuntimePageJob_t;
	job->pageIndex = pageIndex;
	job->pageX = pageX;
	job->pageY = pageY;
	job->pageLevel = level;
	job->diskRef = ref;
	job->pageFile = vt.pageFile;
	job->decoded = false;
	runtimePagePending[pageIndex] = 1;
	++runtimeRequests;
	runtimePageWorker->Queue( job );
	return false;
}

void idVirtualTextureSystem::UpdateRuntime() {
	if ( !runtimeReady || runtimePageWorker == NULL ) { return; }
	ProcessVirtualTextureFallbackImages();
	if ( runtimeDebugMode != vt_showFeedback.GetInteger() ) {
		// Debug markings are applied during page transcode/upload, matching the
		// Tech 5 page visualization. Recreate residency when the mode changes so
		// already-loaded pages cannot retain stale markings.
		InvalidateRuntime();
		return;
	}
	if ( vtFeedbackJob_t * feedbackJob = runtimePageWorker->PopCompletedFeedback() ) {
		runtimeFeedbackPending = false;
		ProcessFeedbackPages( feedbackJob );
		delete feedbackJob;
	}

	// SubImageUpload is a legacy bind-to-edit path.  Pin those binds to the VT
	// page-table unit instead of allowing them to hit whichever hardware unit a
	// previous DSA/fallback bind happened to leave active (often a glass stage).
	// The renderer's logical currenttmu is deliberately not changed here.
	GLint previousActiveTexture = GL_TEXTURE0_ARB;
	GL_GetIntegerv( GET_ACTIVE_TEXTURE, &previousActiveTexture );
	GL_ActiveTexture( 9 );

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
		if ( job->pageIndex < 0 || job->pageIndex >= runtimePageToSlot.Num() ) {
			delete job;
			continue;
		}
		const int existingSlot = runtimePageToSlot[job->pageIndex];
		if ( existingSlot >= 0 ) {
			runtimeSlotLastUsed[existingSlot] = Max( runtimeSlotLastUsed[existingSlot],
				runtimePageLastVisible[job->pageIndex] );
			delete job;
			continue;
		}
		int slot = -1;
		for ( int i = 0; i < runtimeSlotToPage.Num(); ++i ) {
			if ( runtimeSlotToPage[i] < 0 ) { slot = i; break; }
		}
		if ( slot < 0 ) {
			// Do not let an old asynchronous request displace pages reported by
			// either of the two most recent feedback analyses. Tech 5 makes the
			// same decision from pageFrameNum before allocating a physical page.
			const int protectedFrame = runtimeLatestFeedbackFrame - 1;
			if ( protectedFrame > 0 &&
					runtimePageLastVisible[job->pageIndex] < protectedFrame ) {
				delete job;
				continue;
			}
			slot = -1;
			for ( int i = 0; i < runtimeSlotLastUsed.Num(); ++i ) {
				if ( runtimeSlotPinned[i] != 0 ) { continue; }
				if ( slot < 0 || runtimeSlotLastUsed[i] < runtimeSlotLastUsed[slot] ) { slot = i; }
			}
			if ( slot < 0 ) {
				delete job;
				continue;
			}
			if ( protectedFrame > 0 && runtimeSlotLastUsed[slot] >= protectedFrame ) {
				// The working set is larger than the physical cache. Keep the
				// current set stable and allow feedback to request this page again.
				delete job;
				continue;
			}
			const int evictedPage = runtimeSlotToPage[slot];
			if ( evictedPage >= 0 ) {
				runtimePageToSlot[evictedPage] = -1;
				RefreshPageTableRegion( evictedPage );
				++runtimeEvictions;
			}
		}
		const int planeBytes = VT_PAGE_TEXELS * VT_PAGE_TEXELS * 4;
		OverlayVTPageDebug( *job, runtimeDebugMode );
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
		runtimeSlotLastUsed[slot] = runtimePageLastVisible[job->pageIndex] > 0
			? runtimePageLastVisible[job->pageIndex] : runtimeFrame;
		runtimeSlotPinned[slot] = job->pageLevel >=
			Max( 0, vts[PAGESOURCE_VMTR].header.numLevels - 3 ) ? 1 : 0;
		runtimePageToSlot[job->pageIndex] = slot;
		RefreshPageTableRegion( job->pageIndex );
		++runtimeUploads;
		++uploaded;
		pageTableChanged = true;
		if ( runtimeUploads == 1 ) {
			idLib::Printf( "VT runtime: worker decoded first page mip %i (%i,%i), uploaded to slot %i\n",
				job->pageLevel, job->pageX, job->pageY, slot );
		}
		delete job;
	}
	if ( pageTableChanged ) { UploadPageTable(); }
	GL_ActiveTexture( previousActiveTexture );
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
	const float feedbackToViewX = static_cast< float >( VT_FEEDBACK_WIDTH ) /
		Max( 1, feedbackPreviousViewport[2] );
	const float feedbackToViewY = static_cast< float >( VT_FEEDBACK_HEIGHT ) /
		Max( 1, feedbackPreviousViewport[3] );
	return idVec4( static_cast< float >( PAGESOURCE_VMTR ),
		static_cast< float >( vts[PAGESOURCE_VMTR].header.pagesWide ),
		feedbackToViewX, feedbackToViewY );
}

bool idVirtualTextureSystem::BeginFeedbackPass() {
	if ( !r_virtualTextureFeedback.GetBool() || !EnsureRuntimeResources() ) {
		return false;
	}
	++runtimeFrame;
	GL_GetIntegerv( GET_FRAMEBUFFER_BINDING, &feedbackPreviousFBO );
	GL_GetIntegerv( GET_VIEWPORT, feedbackPreviousViewport );
	GL_GetIntegerv( GET_SCISSOR_BOX, feedbackPreviousScissor );
	feedbackPreviousScissorEnabled = GL_IsEnabled( CAP_SCISSOR_TEST ) != 0;
	feedbackUsesSceneDepth = false;
	GL_BindFramebuffer( feedbackDestination->FBOhandle );
	GL_DrawBuffer( BUFFER_SELECT_COLOR_ATTACHMENT0 );
	GL_ReadBuffer( BUFFER_SELECT_COLOR_ATTACHMENT0 );
	GL_Viewport( 0, 0, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT );
	GL_Scissor( 0, 0, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT );
	GL_Enable( CAP_SCISSOR_TEST );
	// Rasterize visibility at feedback resolution.  Blitting the full-resolution
	// scene depth and then testing newly rasterized low-resolution fragments
	// against those nearest-sampled values rejects the entire pass on common
	// view sizes.  A local depth prepass gives stable closest-surface IDs and
	// keeps the feedback target completely independent of the scene FBO.
	// glClear obeys both write masks; post-process passes commonly leave depth
	// writes disabled, so make the auxiliary target writable before clearing.
	// RB_DrawVirtualTextureFeedback force-restores the cached scene state.
	GL_ColorMask( true, true, true, true );
	GL_DepthMask( true );
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
	runtimeLatestFeedbackFrame = Max( runtimeLatestFeedbackFrame, job->frame );
	++runtimeFeedbackAnalyses;
	runtimeFeedbackPages = count;
	if ( runtimeFeedbackAnalyses == 1 ) {
		idLib::Printf( "VT runtime: feedback readback live (%i unique page-tree nodes)\n", count );
	}
	// Refresh every resident feedback hit before considering allocations. The
	// feedback list is priority sorted, but the request budget must not prevent
	// later resident pages from being marked as used.
	for ( int i = 0; i < count; ++i ) {
		const uint64 key = pageIndices->indices[i] >> 2;
		const int x = static_cast< int >( key & 0x7FF );
		const uint64 yAndTexture = key >> 11;
		const int y = static_cast< int >( yAndTexture & 0x7FF );
		const int textureAndLevel = static_cast< int >( yAndTexture >> 11 );
		const int level = textureAndLevel & 15;
		const int texture = ( textureAndLevel >> 4 ) & 15;
		const int pageIndex = texture == PAGESOURCE_VMTR ? RuntimePageIndex( level, x, y ) : -1;
		if ( pageIndex >= 0 ) {
			runtimePageLastVisible[pageIndex] = Max( runtimePageLastVisible[pageIndex], job->frame );
			const int slot = runtimePageToSlot[pageIndex];
			if ( slot >= 0 ) {
				runtimeSlotLastUsed[slot] = Max( runtimeSlotLastUsed[slot], job->frame );
			}
		}
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
		const int pageIndex = texture == PAGESOURCE_VMTR ? RuntimePageIndex( level, x, y ) : -1;
		if ( pageIndex >= 0 ) {
			if ( runtimePageToSlot[pageIndex] < 0 && runtimePagePending[pageIndex] == 0 ) {
				MakePageResident( level, x, y );
				++queuedThisFrame;
			}
		}
	}
}

void idVirtualTextureSystem::EndFeedbackPass() {
	if ( !runtimeReady || feedbackDestination == NULL ) { return; }
	const int current = feedbackWriteIndex;
	const int previous = current ^ 1;
	GL_BindBuffer( BUFFER_TARGET_PIXEL_PACK,
		reinterpret_cast< GLuint >( feedbackPBO[current].GetAPIObject() ) );
	GL_ReadPixels( 0, 0, VT_FEEDBACK_WIDTH, VT_FEEDBACK_HEIGHT, PIXELFORMAT_RGBA, NULL );
	feedbackPBOIssued[current] = true;
	if ( feedbackPBOIssued[previous] ) {
		const byte * pixels = static_cast< const byte * >( feedbackPBO[previous].TryMapBuffer( BM_READ ) );
		if ( pixels != NULL ) {
			QueueFeedbackPixels( pixels );
			feedbackPBO[previous].UnmapBuffer();
		}
	}
	GL_BindBuffer( BUFFER_TARGET_PIXEL_PACK, 0 );
	feedbackWriteIndex ^= 1;
	GL_BindFramebuffer( feedbackPreviousFBO );
	GL_Viewport( feedbackPreviousViewport[0], feedbackPreviousViewport[1],
		feedbackPreviousViewport[2], feedbackPreviousViewport[3] );
	GL_Scissor( feedbackPreviousScissor[0], feedbackPreviousScissor[1],
		feedbackPreviousScissor[2], feedbackPreviousScissor[3] );
	if ( feedbackPreviousScissorEnabled ) { GL_Enable( CAP_SCISSOR_TEST ); }
	else { GL_Disable( CAP_SCISSOR_TEST ); }
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
	runtimePages.Clear();
	runtimePageToSlot.Clear();
	runtimePagePending.Clear();
	runtimePageLastVisible.Clear();
	runtimeSlotToPage.Clear();
	runtimeSlotLastUsed.Clear();
	runtimeSlotPinned.Clear();
	runtimePageTableShadow.Clear();
	runtimeFeedbackPending = false;
	idVirtualTexture & vt = vts[PAGESOURCE_VMTR];
	vt.pageTableImage = NULL;
	idPhysicalPages & pool = physicalPagesPool[PHYSICAL_PAGES_POOL_VMTR];
	pool.physicalPagesImage0 = pool.physicalPagesImage1 = pool.physicalPagesImage2 = NULL;
}

void idVirtualTextureSystem::PrintRuntimeStats() const {
	int residentPages = 0;
	int pinnedPages = 0;
	for ( int slot = 0; slot < runtimeSlotToPage.Num(); ++slot ) {
		if ( runtimeSlotToPage[slot] >= 0 ) {
			++residentPages;
			pinnedPages += runtimeSlotPinned[slot] != 0 ? 1 : 0;
		}
	}
	idLib::Printf( "VT runtime: %s, frame %i, requests %i, uploads %i, evictions %i, feedback %i analyses / %i nodes\n",
		runtimeReady ? "active" : "inactive", runtimeFrame, runtimeRequests,
		runtimeUploads, runtimeEvictions, runtimeFeedbackAnalyses, runtimeFeedbackPages );
	idLib::Printf( "VT physical cache: %ix%i texels, %i/%i pages resident (%i pinned fallbacks)\n",
		VT_CACHE_PAGES_WIDE * VT_PAGE_TEXELS, VT_CACHE_PAGES_WIDE * VT_PAGE_TEXELS,
		residentPages, VT_CACHE_PAGES_WIDE * VT_CACHE_PAGES_WIDE, pinnedPages );
	idLib::Printf( "VT source images: %i lowest-mip-only, %i pending, %.1f MiB released\n",
		demotedFallbackImages.Num(), pendingFallbackImages.Num(),
		runtimeSourceBytesReleased / ( 1024.0f * 1024.0f ) );
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
