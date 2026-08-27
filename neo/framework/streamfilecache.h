#pragma once

class idFile;

enum pfcRequest_t {
	PFC_TEXTURE = 0,
	PFC_DEFAULT = PFC_TEXTURE,
	PFC_SOUND = 1
};

struct cacheLineRequest_t {
	idFile *	file;
	int			fileLine;
	int			priority;
	int			persistence;
};

struct pinnedCacheLines_t;

// Tech 5's public stream-cache boundary.  The first implementation is a
// synchronous positional-reader; callers can be ported unchanged while the
// cache-line scheduler is introduced behind this interface later.
class idStreamFileCache {
public:
					idStreamFileCache();
					~idStreamFileCache();

	void			Init();
	void			InitHDC();
	void			Shutdown();
	void			ClearMem( bool forceClear );
	void			NewFrame();
	int				ReadCachedData( idFile * file, int64 offset, int length,
						void * destination, bool blockingRead, int priority,
						int persistence, pfcRequest_t request );
	void			Flush();
	uint64			UncachedScheduledRead( idFile * file, int64 offset,
						void * destination, int length );
	void			UncachedBackgroundRead( idFile * file, int64 offset, int length,
						void ** destination, memTag_t tag, volatile bool * completionFlag );
	void			BatchUncachedBackgroundRead( idFile * file, int64 * offsets,
						int * lengths, void *** destinations, memTag_t tag,
						volatile bool ** completionFlags, int count );
	void			BatchFreeMemory( void ** allocations, int count );
	void			SignalWork();
	void			BatchRequestCacheLines( const cacheLineRequest_t * requests, int numRequests );
	bool			OutOfMemoryCallback();
	int64			GetCurrentUsageLimit() const;

	idSysMutex	interfaceMutex;
	bool			cancelToTerminate;
};

extern idStreamFileCache * streamFileCache;
extern idStreamFileCache StreamFileCacheLocal;

