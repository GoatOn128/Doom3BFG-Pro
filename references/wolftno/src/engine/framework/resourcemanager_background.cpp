#include "engine/framework/resourcemanager_background.h"

#include "engine/decls/decl.h"
#include "engine/framework/common.h"
#include "engine/framework/resourcemanager.h"
#include "engine/framework/streamfilecache.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstring>
#include <new>

extern void ZlibFree(void* opaque,void* address);

idCVar bgl_debug("bgl_debug","0",CVAR_INTEGER,"Set to 1 for console spew");
idBackgroundLoader bgl;

void* ZlibAlloc(void*,unsigned int items,unsigned int size) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\framework\\ResourceManager_background.cpp(319) : TAG_RESOURCE_BGL",
      static_cast<std::size_t>(items)*size,TAG_RESOURCE_BGL,false,ALIGN_16,
      HEAP_DEFAULTHEAP);
}

namespace {
template <typename T,typename... Args>
T* AllocObject(memTag_t tag,Args&&... args) {
  void* memory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",sizeof(T),tag,
      false,ALIGN_16,HEAP_DEFAULTHEAP);
  return memory!=nullptr?::new(memory) T(static_cast<Args&&>(args)...):nullptr;
}

template <typename T> void FreeObject(T*& object) {
  if (object!=nullptr) {
    object->~T();
    idMem::Singleton()->Free(object,ALIGN_16);
    object=nullptr;
  }
}
}

namespace {
void* AllocBGLObject(std::size_t size,memTag_t tag) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",size,tag,false,
      ALIGN_16,HEAP_DEFAULTHEAP);
}
}

void* idBGLRingBuffer::operator new(std::size_t size) {
  return AllocBGLObject(size,TAG_RESOURCE_BGL_RING);
}
void idBGLRingBuffer::operator delete(void* memory) noexcept {
  idMem::Singleton()->Free(memory,ALIGN_16);
}
void* idFile_RingBuffer::operator new(std::size_t size) {
  return AllocBGLObject(size,TAG_RESOURCE_BGL);
}
void idFile_RingBuffer::operator delete(void* memory) noexcept {
  idMem::Singleton()->Free(memory,ALIGN_16);
}
void* idReadSpawnThread::operator new(std::size_t size) {
  return AllocBGLObject(size,TAG_RESOURCE_BGL);
}
void idReadSpawnThread::operator delete(void* memory) noexcept {
  idMem::Singleton()->Free(memory,ALIGN_16);
}
void* idDecompressSpawnThread::operator new(std::size_t size) {
  return AllocBGLObject(size,TAG_RESOURCE_BGL);
}
void idDecompressSpawnThread::operator delete(void* memory) noexcept {
  idMem::Singleton()->Free(memory,ALIGN_16);
}

idBGLRingBuffer::idBGLRingBuffer(int numBytes)
    : buffer(nullptr),size(numBytes),padding0(0),tail(0),commitHead(0),
      cancelToTerminate(false),padding1{},freeSignal(false),
      lastFreeSignalTail(0),dataSignal(false),lastDataSignalHead(0),
      freeWaituS(0),numFreeWaits(0),dataWaituS(0),numDataWaits(0) {
  buffer=static_cast<unsigned char*>(idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\framework\\ResourceManager_background.cpp(71) : TAG_RESOURCE_BGL_RING",
      static_cast<std::size_t>(numBytes),TAG_RESOURCE_BGL_RING,false,ALIGN_16,
      HEAP_DEFAULTHEAP));
  freeSignal.Clear();
  dataSignal.Clear();
}

idBGLRingBuffer::~idBGLRingBuffer() {
  idMem::Singleton()->Free(buffer,ALIGN_16);
  buffer=nullptr;
}

void idBGLRingBuffer::WriteOfs(std::int64_t dst,const void* source,
                               std::size_t length) {
  const unsigned char* src=static_cast<const unsigned char*>(source);
  while (length!=0) {
    const std::int64_t offset=dst&(size-1);
    const std::size_t count=std::min(length,
        static_cast<std::size_t>(size-offset));
    std::memmove(buffer+offset,src,count);
    dst+=static_cast<std::int64_t>(count);
    src+=count;
    length-=count;
  }
}

std::int64_t idBGLRingBuffer::CheckSpaceAndWait(std::int64_t offset,int minBytes) {
  const int truncatedOffset=static_cast<int>(offset);
  int available=static_cast<int>(tail)+size-truncatedOffset;
  if (available>=minBytes)
    return available;
  for (;;) {
    ++numFreeWaits;
    const int start=static_cast<int>(Sys_Microseconds());
    freeSignal.Wait(0xFFFFFFFFu);
    freeWaituS+=static_cast<int>(Sys_Microseconds())-start;
    if (cancelToTerminate)
      return -1;
    available=static_cast<int>(tail)+size-truncatedOffset;
    if (available>=minBytes)
      return available;
  }
}

void idBGLRingBuffer::Free(std::int64_t base,int numBytes) {
  const std::int64_t end=base+numBytes;
  while (end>commitHead) {
    ++numDataWaits;
    const int start=static_cast<int>(Sys_Microseconds());
    if (cancelToTerminate)
      return;
    dataSignal.Wait(0xFFFFFFFFu);
    dataWaituS+=static_cast<int>(Sys_Microseconds())-start;
    if (cancelToTerminate)
      return;
  }
  if (end>tail)
    tail=end;
  if (tail-lastFreeSignalTail>0x10000) {
    freeSignal.Raise();
    lastFreeSignalTail=tail;
  }
}

std::size_t idBGLRingBuffer::ReadOfs(std::int64_t src,void* destination,
                                    std::size_t length) {
  if (length==0)
    return 0;
  const std::size_t originalLength=length;
  unsigned char* dst=static_cast<unsigned char*>(destination);
  while (length!=0) {
    const std::int64_t offset=src&(size-1);
    int count=static_cast<int>(length);
    if (commitHead-src<static_cast<std::int64_t>(length))
      count=static_cast<int>(commitHead)-static_cast<int>(src);
    count=std::min(count,static_cast<int>(size-offset));
    if (count>0) {
      std::memmove(dst,buffer+offset,static_cast<std::size_t>(count));
      src+=count;
      Free(src,0);
      length-=static_cast<std::size_t>(count);
      dst+=count;
    } else {
      ++numDataWaits;
      const int start=static_cast<int>(Sys_Microseconds());
      dataSignal.Wait(0xFFFFFFFFu);
      dataWaituS+=static_cast<int>(Sys_Microseconds())-start;
      if (cancelToTerminate)
        return 0;
    }
  }
  return originalLength;
}

idFile_RingBuffer::idFile_RingBuffer(const char* fileName,idBGLRingBuffer* rb,
    std::int64_t offset,int length)
    : idFile(),ringBuffer(rb),name(fileName),fileSize(length),baseOffset(offset),
      currOffset(0) {}

idFile_RingBuffer::~idFile_RingBuffer() {
  if (fileSize-currOffset>128)
    idLib::Warning("idFile_RingBuffer file '%s' skipping %d bytes of stream data",
                   name.data,static_cast<int>(fileSize)-static_cast<int>(currOffset));
  ringBuffer->Free(baseOffset,static_cast<int>(fileSize));
}

std::size_t idFile_RingBuffer::Read(void* destination,std::size_t length) {
  std::int64_t count=static_cast<std::int64_t>(length);
  if (currOffset+count>fileSize)
    count=fileSize-currOffset;
  const std::int64_t result=static_cast<std::int64_t>(ringBuffer->ReadOfs(
      baseOffset+currOffset,destination,static_cast<std::size_t>(count)));
  currOffset+=result;
  return static_cast<std::size_t>(result);
}

int idFile_RingBuffer::Seek(std::int64_t offset,fsOrigin_t origin) {
  if (origin==FS_SEEK_SET) {
    if (offset<currOffset)
      idLib::Error(
        "idFile_RingBuffer(%s) does not support seeking backwards due to streaming. old offset: %lli, new: %lli, len: %i",
        GetName(),currOffset,offset,static_cast<int>(fileSize));
    currOffset=offset;
  } else if (origin==FS_SEEK_CUR) {
    currOffset+=offset;
  } else {
    currOffset=fileSize+offset;
  }
  return static_cast<int>(currOffset);
}

idDecompressSpawnThread::idDecompressSpawnThread()
    : idSysThread(),compressedOffsetInCurrentEntry(0),
      uncompressedOffsetInCurrentEntry(0),z{} {}

void idDecompressSpawnThread::BeginBackgroundLoads() {
  std::memset(&z,0,sizeof(z));
  z.zalloc=ZlibAlloc;
  z.zfree=ZlibFree;
  if (inflateInit2_(&z,-15,"1.2.3",sizeof(z_stream))!=Z_OK)
    idLib::FatalError("zlib inflateInit2 error");
  unsigned char compressedZero[7]={98,0,0,0,0,0xFF,0xFF};
  unsigned char uncompressedZero[1]={0};
  z.next_in=compressedZero;
  z.next_out=uncompressedZero;
  z.avail_in=7;
  z.avail_out=1;
  inflate(&z,Z_FINISH);
  z.total_out=0;
  z.total_in=0;
  z.next_in=nullptr;
  z.next_out=nullptr;
}

int idReadSpawnThread::Run() {
  if (!bgl.cancelToTerminate) {
    const int start=static_cast<int>(Sys_Milliseconds());
    streamFileCache->UncachedScheduledRead(
        bgl.resourceFile,bgl.readBufferOfs[bgl.currentReadBuffer&1],
        bgl.readBuffers[bgl.currentReadBuffer&1],0x10000);
    bgl.backgroundReadMsec+=static_cast<int>(Sys_Milliseconds())-start;
    if (decompressThread!=nullptr)
      decompressThread->SignalWork();
  }
  return 0;
}

idBackgroundLoader::idBackgroundLoader()
    : resourceFile(nullptr),fileTable(nullptr),bgrEntries(),readIndex(0),
      decompressIndex(0),returnedIndex(0),cancelToTerminate(false),padding0{},
      entryStartedSignal(false),decompressBuffer(nullptr),readBuffers{},
      readBufferOfs{},currentReadBuffer(0),padding1(0),readSpawnThread(nullptr),
      decompressSpawnThread(nullptr),foregroundSleepMsec(0),backgroundSeeks(0),
      backgroundReadMsec(0),backgroundSleepMsec(0),decompressSleepMsec(0),
      decompressWorkMsec(0),startForegroundTime(0),padding2(0) {}

void idBackgroundLoader::SpawnThreads() {
  readSpawnThread=AllocObject<idReadSpawnThread>(TAG_RESOURCE_BGL);
  decompressSpawnThread=AllocObject<idDecompressSpawnThread>(TAG_RESOURCE_BGL);
  readSpawnThread->decompressThread=decompressSpawnThread;
  readSpawnThread->StartWorkerThread("BGL_ReadThread",CORE_1A,THREAD_HIGHEST,
                                     0x40000);
  decompressSpawnThread->StartWorkerThread("BGL_DecompressThread",CORE_1B,
                                           THREAD_HIGHEST,0x40000);
}

void idBackgroundLoader::DestroyThreads() {
  FreeObject(readSpawnThread);
  FreeObject(decompressSpawnThread);
}

idFile* idBackgroundLoader::GetFile(const char* filename) {
  if (resourceFile==nullptr)
    idLib::FatalError("idBackgroundLoader::GetFile() called ouside a background load");
  if (cancelToTerminate)
    return nullptr;
  idStr canonical(filename);
  canonical.MakeNameCanonical();
  if (idStr::Length(filename)>5 &&
      (filename[0]=='/' || filename[0]=='\\' || filename[1]==':')) {
    const int base=canonical.Find("/base/",true,0,-1);
    if (base>=0)
      canonical=canonical.data+base+6;
  }
  canonical.StripLeading('/');
  int index=returnedIndex;
  while (index<fileTable->num && fileTable->list[index].filename!=canonical)
    ++index;
  if (index>=fileTable->num)
    return resourceManager->LoadCacheFile(filename);
  const idResourceFileEntry& entry=fileTable->list[index];
  while (decompressIndex<index || bgrEntries.list[index].ringBufferOffset==-1 ||
         entry.uncompressedLength==0) {
    if (common->IsInitialized())
      common->UpdateLevelLoadPacifier(0.0f,PACIFIER_STATE_1,false);
    if (cancelToTerminate)
      return nullptr;
    const int start=static_cast<int>(Sys_Milliseconds());
    entryStartedSignal.Wait(10);
    foregroundSleepMsec+=static_cast<int>(Sys_Milliseconds())-start;
  }
  returnedIndex=index+1;
  if (entry.uncompressedLength==0)
    return nullptr;
  idFile_RingBuffer* file=AllocObject<idFile_RingBuffer>(
      TAG_RESOURCE_BGL,filename,decompressBuffer,bgrEntries.list[index].ringBufferOffset,
      entry.uncompressedLength);
  return file;
}

void idBackgroundLoader::SkipPastResource(const idResource* resource) {
  if (resourceFile==nullptr)
    idLib::FatalError(
      "idBackgroundLoader::SkipPastResource() called ouside a background load");
  while (returnedIndex!=fileTable->num) {
    const idResourceFileEntry& entry=fileTable->list[returnedIndex];
    if (entry.resourceName!=resource->GetAtomicName().str ||
        entry.typeName!=resource->GetResourceTypeName())
      break;
    if (decompressIndex<=returnedIndex) {
      while (!cancelToTerminate) {
        Sys_Sleep(1);
        if (decompressIndex>returnedIndex)
          break;
      }
      if (cancelToTerminate)
        return;
    }
    const int index=returnedIndex++;
    if (entry.uncompressedLength!=0)
      decompressBuffer->Free(bgrEntries.list[index].ringBufferOffset,
                             entry.uncompressedLength);
  }
}

void idBackgroundLoader::DetermineNextReadBlock() {
  std::int64_t offset=readBufferOfs[currentReadBuffer&1]+0x10000;
  while (readIndex<fileTable->num) {
    const idResourceFileEntry& entry=fileTable->list[readIndex];
    if (offset<static_cast<std::int64_t>(entry.offset)+entry.compressedLength) {
      if (offset+0x10000<entry.offset) {
        offset=static_cast<unsigned int>(entry.offset)&0xFFFF0000u;
        ++backgroundSeeks;
      }
      readBufferOfs[++currentReadBuffer&1]=offset;
      if (readSpawnThread!=nullptr)
        readSpawnThread->SignalWork();
      return;
    }
    ++readIndex;
  }
}

void idBackgroundLoader::BeginBackgroundLoads(
    idFile& resourceFile_,const idList<idResourceFileEntry,101>& fileTable_) {
  startForegroundTime=static_cast<int>(Sys_Milliseconds());
  resourceFile=&resourceFile_;
  fileTable=&fileTable_;
  idMem::Singleton()->PushMapHeap();
  for (int index=0;index<2;++index) {
    readBuffers[index]=static_cast<unsigned char*>(
      idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\engine\\framework\\ResourceManager_background.cpp(418) : TAG_RESOURCE_BGL",
        0x10000,TAG_RESOURCE_BGL,false,ALIGN_16,HEAP_DEFAULTHEAP));
    readBufferOfs[index]=-0x10000;
  }
  idMem::Singleton()->PopMapHeap();
  currentReadBuffer=0;
  idMem::Singleton()->PushMapHeap();
  decompressBuffer=AllocObject<idBGLRingBuffer>(TAG_RESOURCE_BGL_RING,0x1000000);
  idMem::Singleton()->PopMapHeap();
  if (decompressSpawnThread!=nullptr)
    decompressSpawnThread->BeginBackgroundLoads();
  bgrEntries.SetNum(fileTable->num);
  for (int index=0;index<fileTable->num;++index) {
    if (index>0 && fileTable->list[index].offset<fileTable->list[index-1].offset)
      idLib::FatalError(
        "idBackgroundLoader::BeginBackgroundLoads: FileTable is out of order",
        index,index*static_cast<int>(sizeof(idResourceFileEntry)),index);
    bgrEntries.list[index].ringBufferOffset=-1;
  }
  readIndex=0;
  decompressIndex=0;
  returnedIndex=0;
  foregroundSleepMsec=0;
  backgroundSeeks=0;
  backgroundReadMsec=0;
  backgroundSleepMsec=0;
  decompressSleepMsec=0;
  decompressWorkMsec=0;
  cancelToTerminate=false;
  DetermineNextReadBlock();
}

void idBackgroundLoader::EndBackgroundLoads() {
  if (decompressSpawnThread!=nullptr)
    inflateEnd(&decompressSpawnThread->z);
  const int now=static_cast<int>(Sys_Milliseconds());
  const int bytes=currentReadBuffer<<16;
  idLib::Printf("--------- EndBackgroundLoads() ----------\n");
  idLib::Printf("IO: %i seeks, %i reads, %3.0fMB, %5.1fMB/s\n",
    backgroundSeeks,currentReadBuffer,static_cast<float>(bytes)*0.000001f,
    static_cast<float>(bytes)*0.001f/static_cast<float>(backgroundReadMsec));
  idLib::Printf("Read       sleep msec: %5i\n",backgroundSleepMsec);
  idLib::Printf("Decompress sleep msec: %5i\n",decompressSleepMsec);
  idLib::Printf("Foreground sleep msec: %5i\n",foregroundSleepMsec);
  idLib::Printf("Decompress work msec: %5i\n",decompressWorkMsec);
  idLib::Printf("Ringbuf: freeWait %i/%i, dataWait %i/%i\n",
    decompressBuffer->freeWaituS/1000,decompressBuffer->numFreeWaits,
    decompressBuffer->dataWaituS/1000,decompressBuffer->numDataWaits);
  idLib::Printf("Total            msec: %5i\n",now-startForegroundTime);
  idMem::Singleton()->PushMapHeap();
  for (unsigned char*& readBuffer:readBuffers) {
    idMem::Singleton()->Free(readBuffer,ALIGN_16);
    readBuffer=nullptr;
  }
  FreeObject(decompressBuffer);
  idMem::Singleton()->PopMapHeap();
  bgrEntries.ClearFree();
  fileTable=nullptr;
  resourceFile=nullptr;
}

int idDecompressSpawnThread::Run() {
  if (bgl.cancelToTerminate || bgl.decompressIndex==bgl.fileTable->num)
    return 0;
  if (bgl.decompressIndex>bgl.fileTable->num)
    idLib::FatalError(
      "idBackgroundLoader::DecompressThread: decompressIndex >= fileTable->Num()");
  const int sleepStart=static_cast<int>(Sys_Milliseconds());
  const int sleepEnd=static_cast<int>(Sys_Milliseconds());
  if (bgl.decompressIndex!=0)
    bgl.decompressSleepMsec+=sleepEnd-sleepStart;
  const int bufferIndex=bgl.currentReadBuffer&1;
  const std::int64_t readOffset=bgl.readBufferOfs[bufferIndex];
  unsigned char* readBuffer=bgl.readBuffers[bufferIndex];
  bgl.DetermineNextReadBlock();
  if (bgl.decompressIndex>=bgl.bgrEntries.num)
    return 0;
  while (bgl.decompressIndex<bgl.bgrEntries.num) {
    const idResourceFileEntry& entry=bgl.fileTable->list[bgl.decompressIndex];
    if (entry.uncompressedLength==0) {
      ++bgl.decompressIndex;
      continue;
    }
    const int entryIndex=bgl.decompressIndex;
    idBackgroundLoader::bgrEntry_t& bgrEntry=bgl.bgrEntries.list[entryIndex];
    const std::int64_t inputOffset=compressedOffsetInCurrentEntry+entry.offset-
                                   readOffset;
    if (inputOffset>=0x10000)
      return 0;
    if (compressedOffsetInCurrentEntry==0) {
      bgrEntry.ringBufferOffset=bgl.decompressBuffer->commitHead;
      bgl.entryStartedSignal.Raise();
    }
    const std::int64_t remaining=entry.uncompressedLength-
                                 uncompressedOffsetInCurrentEntry;
    const int minimum=static_cast<int>(std::min<std::int64_t>(remaining,0x40000));
    int writable=static_cast<int>(bgl.decompressBuffer->CheckSpaceAndWait(
        uncompressedOffsetInCurrentEntry+bgrEntry.ringBufferOffset,minimum));
    writable=static_cast<int>(std::min<std::int64_t>(remaining,writable));
    if (bgl.cancelToTerminate)
      return 0;
    const int workStart=static_cast<int>(Sys_Milliseconds());
    int inputBytes=entry.compressedLength-
                   static_cast<int>(compressedOffsetInCurrentEntry);
    if (inputOffset+inputBytes>0x10000)
      inputBytes=0x10000-static_cast<int>(inputOffset);
    if (entry.compressedLength==entry.uncompressedLength) {
      writable=std::min(writable,inputBytes);
      bgl.decompressBuffer->WriteOfs(
        uncompressedOffsetInCurrentEntry+bgrEntry.ringBufferOffset,
        readBuffer+inputOffset,static_cast<std::size_t>(writable));
      uncompressedOffsetInCurrentEntry+=writable;
      compressedOffsetInCurrentEntry+=writable;
    } else {
      z.avail_in=inputBytes;
      z.next_in=readBuffer+inputOffset;
      for (;;) {
        if (z.avail_in==0 || (writable==0 && remaining!=0))
          break;
        if (bgl.cancelToTerminate)
          return 0;
        const int outputOffset=(static_cast<int>(uncompressedOffsetInCurrentEntry)+
            static_cast<int>(bgrEntry.ringBufferOffset))&(bgl.decompressBuffer->size-1);
        z.avail_out=writable;
        z.next_out=bgl.decompressBuffer->buffer+outputOffset;
        const int contiguous=bgl.decompressBuffer->size-outputOffset;
        if (contiguous>=writable) {
          const int flush=(compressedOffsetInCurrentEntry+inputBytes==entry.compressedLength &&
                           uncompressedOffsetInCurrentEntry+writable==entry.uncompressedLength)
                            ?Z_FINISH:Z_NO_FLUSH;
          const int result=inflate(&z,flush);
          if (result!=Z_OK && z.total_out!=
              static_cast<unsigned long>(entry.uncompressedLength)) {
            idLib::Warning("zlib inflate error");
            goto inflate_error;
          }
          uncompressedOffsetInCurrentEntry=z.total_out;
          break;
        }
        z.avail_out=contiguous;
        if (inflate(&z,Z_NO_FLUSH)!=Z_OK) {
          idLib::Warning("zlib inflate error");
          goto inflate_error;
        }
        const int produced=static_cast<int>(z.total_out)-
                           static_cast<int>(uncompressedOffsetInCurrentEntry);
        uncompressedOffsetInCurrentEntry=z.total_out;
        writable-=produced;
      }
      compressedOffsetInCurrentEntry=z.total_in;
    }
    {
      const std::int64_t committed=bgrEntry.ringBufferOffset+
                                   uncompressedOffsetInCurrentEntry;
      bgl.decompressBuffer->commitHead=committed;
      if (uncompressedOffsetInCurrentEntry==entry.uncompressedLength ||
          committed-bgl.decompressBuffer->lastDataSignalHead>0x10000) {
        bgl.decompressBuffer->dataSignal.Raise();
        bgl.decompressBuffer->lastDataSignalHead=committed;
      }
    }
    if (compressedOffsetInCurrentEntry==entry.compressedLength) {
      compressedOffsetInCurrentEntry=0;
      uncompressedOffsetInCurrentEntry=0;
      z.total_out=0;
      z.total_in=0;
      ++bgl.decompressIndex;
    }
    bgl.decompressWorkMsec+=static_cast<int>(Sys_Milliseconds())-workStart;
    continue;

inflate_error:
    inflateEnd(&z);
    if (!bgl.cancelToTerminate) {
      bgl.cancelToTerminate=true;
      if (bgl.decompressBuffer!=nullptr) {
        bgl.decompressBuffer->cancelToTerminate=true;
        bgl.decompressBuffer->freeSignal.Raise();
        bgl.decompressBuffer->dataSignal.Raise();
      }
    }
    return 0;
  }
  return 0;
}
