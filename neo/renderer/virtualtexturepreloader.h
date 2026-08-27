#pragma once

class idMaterial;
class idVirtualTexture;
struct preloadMaterial_t;

enum preloadType_t {
	PRELOAD_INVALID = 0,
	PRELOAD_ALL = 1,
	PRELOAD_MATERIAL_LIST = 2
};

struct preloadTextureRect_t {
	uint16 x1;
	uint16 y1;
	uint16 x2;
	uint16 y2;
};

#pragma pack( push, 4 )
struct preloadCommand_t {
	int			handle;
	preloadType_t type;
	idVirtualTexture * texture;
	preloadTextureRect_t textureRects[1024];
	uint8		textureMinLod[1024];
	int			numTextureRects;
	int			minLod;
	int			totalTimeMSec;
	int			pagesLoaded[16];
	volatile bool done;
};
#pragma pack( pop )

struct idTmpPreloadMaterial_t {
	const idMaterial * material;
	int minLod;
};

class idVirtualTexturePreloadThread : public idSysThread {
public:
					idVirtualTexturePreloadThread();
	virtual int		Run();
	int				AddCommand( preloadType_t type, const preloadMaterial_t * const materials,
						int numMaterials, int minLod );
	void			StopPreloading();

	preloadCommand_t preloadCommands[8];
	preloadCommand_t * preloadCommandQueue[16];
	volatile int	preloadCommandQueueStart;
	volatile int	preloadCommandQueueEnd;
	int				currentHandle;
	volatile bool	endCalled;
	volatile bool	cancelToTerminate;

private:
	preloadCommand_t * AllocCommand( preloadType_t type, int minLod,
		idVirtualTexture * virtualTexture );
};

class idVirtualTexturePreloader {
public:
					idVirtualTexturePreloader();
					~idVirtualTexturePreloader();
	void			SpawnThread();
	void			DestroyThread();
	void			BeginPreloading();
	void			EndPreloading();
	bool			MaterialPreloadIsDone( int handle );
	int				PreloadVMTRsForMap( const char * mapName, const char * spawnSpot );

	idVirtualTexturePreloadThread * thread;
	bool			running;
};

extern idVirtualTexturePreloader virtualTexturePreloader;

static_assert( sizeof( preloadTextureRect_t ) == 8, "preload rect ABI drift" );

