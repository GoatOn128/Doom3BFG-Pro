#include "../idlib/precompiled.h"
#pragma hdrstop

#include "File.h"
#include "streamfilecache.h"

idStreamFileCache StreamFileCacheLocal;
idStreamFileCache * streamFileCache = &StreamFileCacheLocal;

idStreamFileCache::idStreamFileCache() : cancelToTerminate( false ) {}
idStreamFileCache::~idStreamFileCache() { Shutdown(); }

void idStreamFileCache::Init() { cancelToTerminate = false; }
void idStreamFileCache::InitHDC() {}
void idStreamFileCache::Shutdown() { cancelToTerminate = true; }
void idStreamFileCache::ClearMem( bool ) {}
void idStreamFileCache::NewFrame() {}
void idStreamFileCache::Flush() {}
void idStreamFileCache::SignalWork() {}
void idStreamFileCache::BatchRequestCacheLines( const cacheLineRequest_t *, int ) {}
bool idStreamFileCache::OutOfMemoryCallback() { return false; }
int64 idStreamFileCache::GetCurrentUsageLimit() const { return 0; }

int idStreamFileCache::ReadCachedData( idFile * file, int64 offset, int length,
		void * destination, bool, int, int, pfcRequest_t ) {
	if ( file == NULL || destination == NULL || length <= 0 ) {
		return 0;
	}
	return file->ReadAt( offset, destination, length );
}

uint64 idStreamFileCache::UncachedScheduledRead( idFile * file, int64 offset,
		void * destination, int length ) {
	return static_cast< uint64 >( ReadCachedData( file, offset, length,
		destination, true, 0, 0, PFC_DEFAULT ) );
}

void idStreamFileCache::UncachedBackgroundRead( idFile * file, int64 offset,
		int length, void ** destination, memTag_t tag,
		volatile bool * completionFlag ) {
	if ( completionFlag != NULL ) {
		*completionFlag = false;
	}
	if ( destination != NULL && *destination == NULL && length > 0 ) {
		*destination = Mem_Alloc( length, tag );
	}
	if ( destination != NULL && *destination != NULL ) {
		ReadCachedData( file, offset, length, *destination, true, 0, 0, PFC_DEFAULT );
	}
	if ( completionFlag != NULL ) {
		*completionFlag = true;
	}
}

void idStreamFileCache::BatchUncachedBackgroundRead( idFile * file,
		int64 * offsets, int * lengths, void *** destinations, memTag_t tag,
		volatile bool ** completionFlags, int count ) {
	for ( int i = 0; i < count; ++i ) {
		UncachedBackgroundRead( file, offsets[i], lengths[i], destinations[i],
			tag, completionFlags != NULL ? completionFlags[i] : NULL );
	}
}

void idStreamFileCache::BatchFreeMemory( void ** allocations, int count ) {
	for ( int i = 0; i < count; ++i ) {
		if ( allocations[i] != NULL ) {
			Mem_Free( allocations[i] );
			allocations[i] = NULL;
		}
	}
}
