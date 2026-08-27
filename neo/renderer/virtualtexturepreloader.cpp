#include "../idlib/precompiled.h"
#pragma hdrstop

#include "virtualtexturepreloader.h"

idVirtualTexturePreloader virtualTexturePreloader;

idVirtualTexturePreloadThread::idVirtualTexturePreloadThread()
	: preloadCommandQueueStart( 0 ), preloadCommandQueueEnd( 0 ),
	  currentHandle( 1 ), endCalled( false ), cancelToTerminate( false ) {
	memset( preloadCommands, 0, sizeof( preloadCommands ) );
	memset( preloadCommandQueue, 0, sizeof( preloadCommandQueue ) );
	for ( int i = 0; i < 8; ++i ) {
		preloadCommands[i].type = PRELOAD_INVALID;
		preloadCommands[i].done = true;
	}
}

preloadCommand_t * idVirtualTexturePreloadThread::AllocCommand(
		preloadType_t type, int minLod, idVirtualTexture * texture ) {
	for ( int i = 0; i < 8; ++i ) {
		preloadCommand_t & command = preloadCommands[i];
		if ( !command.done ) {
			continue;
		}
		memset( &command, 0, sizeof( command ) );
		command.handle = currentHandle++;
		command.type = type;
		command.texture = texture;
		command.minLod = minLod;
		command.done = false;
		return &command;
	}
	return NULL;
}

int idVirtualTexturePreloadThread::AddCommand( preloadType_t type,
		const preloadMaterial_t * const, int, int minLod ) {
	preloadCommand_t * command = AllocCommand( type, minLod, NULL );
	if ( command == NULL ) {
		return -1;
	}
	const int handle = command->handle;
	preloadCommandQueue[preloadCommandQueueEnd] = command;
	preloadCommandQueueEnd = ( preloadCommandQueueEnd + 1 ) & 15;
	SignalWork();
	return handle;
}

int idVirtualTexturePreloadThread::Run() {
	while ( preloadCommandQueueStart != preloadCommandQueueEnd ) {
		preloadCommand_t * command = preloadCommandQueue[preloadCommandQueueStart];
		preloadCommandQueueStart = ( preloadCommandQueueStart + 1 ) & 15;
		// The scheduling boundary is live.  Page discovery will populate the
		// command when VMTR atlas metadata is available.
		command->done = true;
	}
	return 0;
}

void idVirtualTexturePreloadThread::StopPreloading() {
	endCalled = true;
	WaitForThread();
	endCalled = false;
}

idVirtualTexturePreloader::idVirtualTexturePreloader() : thread( NULL ), running( false ) {}
idVirtualTexturePreloader::~idVirtualTexturePreloader() { DestroyThread(); }

void idVirtualTexturePreloader::SpawnThread() {
	if ( thread != NULL ) {
		return;
	}
	thread = new ( TAG_RENDER ) idVirtualTexturePreloadThread;
	thread->StartWorkerThread( "VirtualTexturePreloader", CORE_ANY, THREAD_BELOW_NORMAL );
}

void idVirtualTexturePreloader::DestroyThread() {
	if ( thread != NULL ) {
		thread->StopThread( true );
		delete thread;
		thread = NULL;
	}
	running = false;
}

void idVirtualTexturePreloader::BeginPreloading() { SpawnThread(); running = true; }
void idVirtualTexturePreloader::EndPreloading() {
	if ( thread != NULL ) {
		thread->StopPreloading();
	}
	running = false;
}

bool idVirtualTexturePreloader::MaterialPreloadIsDone( int handle ) {
	if ( thread == NULL || handle < 0 ) {
		return true;
	}
	for ( int i = 0; i < 8; ++i ) {
		if ( thread->preloadCommands[i].handle == handle ) {
			return thread->preloadCommands[i].done;
		}
	}
	return true;
}

int idVirtualTexturePreloader::PreloadVMTRsForMap( const char *, const char * ) {
	SpawnThread();
	return thread->AddCommand( PRELOAD_ALL, NULL, 0, 0 );
}
