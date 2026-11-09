#include "engine/framework/streamfilecache_internal.h"

#include "engine/framework/streamfilelog.h"
#include "engine/framework/console.h"
#include "engine/renderer/rendersystem_bink.h"
#include "engine/sound/streamcache.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/color.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <new>
#include <Windows.h>

#pragma warning(disable:4996)

idStreamFileCache StreamFileCacheLocal;
idStreamFileCache* streamFileCache=&StreamFileCacheLocal;

idCVar fc_maxCacheMemoryMB("fc_maxCacheMemoryMB","512",CVAR_INTEGER,
    "Maximum cache size in megabytes",1.0f,1024.0f,nullptr);
idCVar fc_minCacheMemoryMB("fc_minCacheMemoryMB","4",CVAR_INTEGER,
    "Minumum cache size in megabytes, take this much even if over comited",
    0.0f,96.0f,nullptr);
idCVar fc_useHdc("fc_useHdc","0",CVAR_INTEGER,
    "1 = enable hard hard cache, 2 = force hard disk cache",nullptr);
idCVar fc_checkHdcTimestamps("fc_checkHdcTimestamps","0",CVAR_BOOL,
    "If zero, don't clear the hard disk cache when timestamps are dirty",nullptr);
idCVar fc_hardDiskRequestTimeOut("fc_hardDiskRequestTimeOut","24",CVAR_INTEGER,
    "milliseconds after which a request times out",nullptr);
idCVar fc_opticalDiskRequestTimeOut("fc_opticalDiskRequestTimeOut","48",CVAR_INTEGER,
    "milliseconds after which a request times out",nullptr);
idCVar fc_hardDiskNoSeekOffset("fc_hardDiskNoSeekOffset","16",CVAR_INTEGER,
    "number of cache lines that can be skipped without seeking",nullptr);
idCVar fc_opticalDiskNoSeekOffset("fc_opticalDiskNoSeekOffset","16",CVAR_INTEGER,
    "number of cache lines that can be skipped without seeking",nullptr);
idCVar fc_opticalDiskSkipOffset("fc_opticalDiskSkipOffset","5",CVAR_INTEGER,
    "keep on reading if the next cache line is less than this many lines ahead",nullptr);
idCVar fc_verifyCacheReadData("fc_verifyCacheReadData","0",CVAR_BOOL,
    "Compare every cache read with an actual ReadOfs",nullptr);
idCVar fc_printProcessRequestsPerformanceWarning(
    "fc_printProcessRequestsPerformanceWarning","0",CVAR_BOOL,
    "print the \"idStreamControlThread::ProcessRequests() took x msec\" performance warning",
    nullptr);

namespace {
struct uncachedRequest_t {
  fsDevice_t fileDevice;
  std::byte padding0[4];
  idFile* filePtr;
  std::int64_t fileOfs;
  int length;
  std::byte padding1[4];
  void** dest;
  memTag_t tag;
  std::byte padding2[4];
  volatile bool* completionFlag;
};
struct streamRequest_t {
  idFile* filePtr;
  int fileID;
  int fileLine;
  int priority;
  int persistence;
  std::byte remaining[72];
};
struct cachedFile_t { char name[256]; fsDevice_t device; std::uint32_t timestamp; };
struct cachedFileID_t { std::uint32_t uniqID; int id; };
struct hdCache_t {
  int magic1;
  cachedFile_t cachedFiles[128];
  int lineSize;
  std::uint32_t hdLines[16384];
  unsigned char hdMRU[16384];
  int hdLinesWritten;
  int hdLinesRead;
  int magic2;
};
struct fcStats_t {
  std::uint64_t memoryCacheHits,hardDiskCacheHits,hardDiskLinesRead,
      hardDiskCacheLinesWritten,hardDiskSeeks,hardDiskTimeOuts,
      hardDiskReadsFailed,opticalDiskLinesRead,opticalDiskSeeks,
      opticalDiskSkips,opticalDiskTimeOuts,opticalDiskReadsFailed,
      hardDiskLineMinReadTime,hardDiskLineMaxReadTime,
      hardDiskLineTotalReadTime,opticalDiskLineMinReadTime,
      opticalDiskLineMaxReadTime,opticalDiskLineTotalReadTime,
      hardDiskCacheMinWriteTime,hardDiskCacheMaxWriteTime,
      hardDiskCacheTotalWriteTime,hardDiskCacheMinReadTime,
      hardDiskCacheMaxReadTime,hardDiskCacheTotalReadTime,
      uncachedHardDiskReads,uncachedOpticalDiskReads,
      uncachedHardDiskReadSize,uncachedOpticalDiskReadSize;
};
static_assert(sizeof(uncachedRequest_t)==56);
static_assert(offsetof(uncachedRequest_t,fileDevice)==0);
static_assert(offsetof(uncachedRequest_t,filePtr)==8);
static_assert(offsetof(uncachedRequest_t,fileOfs)==16);
static_assert(offsetof(uncachedRequest_t,length)==24);
static_assert(offsetof(uncachedRequest_t,dest)==32);
static_assert(offsetof(uncachedRequest_t,tag)==40);
static_assert(offsetof(uncachedRequest_t,completionFlag)==48);
static_assert(sizeof(streamRequest_t)==96);
static_assert(offsetof(streamRequest_t,filePtr)==0);
static_assert(offsetof(streamRequest_t,fileID)==8);
static_assert(offsetof(streamRequest_t,fileLine)==12);
static_assert(offsetof(streamRequest_t,priority)==16);
static_assert(offsetof(streamRequest_t,persistence)==20);
static_assert(sizeof(cachedFile_t)==264);
static_assert(sizeof(hdCache_t)==115732);
static_assert(sizeof(fcStats_t)==224);
}

class idHardDiskStreamThread final : public idSysThread {
public:
  idHardDiskStreamThread():controlThread(nullptr),hardDiskOffset(0),padding{},
                           tempData(nullptr),tempMemType(MEMORY_TYPE_INVALID){}
  int Run() override;
  void ProcessIO();
  idStreamControlThread* controlThread;
  int hardDiskOffset;
  std::byte padding[4];
  unsigned char* tempData;
  memoryType_t tempMemType;
  std::byte tail[4];
};
class idOpticalDiskStreamThread final : public idSysThread {
public:
  idOpticalDiskStreamThread():controlThread(nullptr),opticalDiskOffset(0),padding{},
      tempData(nullptr),tempMemType(MEMORY_TYPE_INVALID),cancelToTerminate(false),tail{}{}
  int Run() override;
  void ProcessIO();
  idStreamControlThread* controlThread;
  int opticalDiskOffset;
  std::byte padding[4];
  unsigned char* tempData;
  memoryType_t tempMemType;
  bool cancelToTerminate;
  std::byte tail[3];
};
static_assert(sizeof(idHardDiskStreamThread)==120);
static_assert(sizeof(idOpticalDiskStreamThread)==120);

idFile_Permanent* OpenCacheFile(const char* fileName,bool create) {
  idStrStatic<256> path=fileSystem->GetBasePathStr(FSPATH_CACHE);
  path.AppendPath(fileName);
  void* memory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",128,TAG_FILE,
      false,ALIGN_16,HEAP_DEFAULTHEAP);
  auto* file=memory!=nullptr?
      new(memory) idFile_Permanent(fileName,path.data,FS_READ_WRITE,create):nullptr;
  if(file!=nullptr && file->IsOpen()) return file;
  if(file!=nullptr) {
    file->~idFile_Permanent();
    idMem::Singleton()->Free(file,ALIGN_16);
  }
  return nullptr;
}

void RemoveCacheFile(const char* fileName) {
  idStrStatic<256> path=fileSystem->GetBasePathStr(FSPATH_CACHE);
  path.AppendPath(fileName);
  fileSystem->RemoveFile(path.data,FSPATH_BASE);
}

namespace {
void CloseCacheFile(idFile*& file) {
  if(file==nullptr) return;
  static_cast<idFile_Permanent*>(file)->~idFile_Permanent();
  idMem::Singleton()->Free(file,ALIGN_16);
  file=nullptr;
}
}

void CacheLineQuickSort(cacheLineSort_t* values,std::int64_t count) {
  if(count<=0) return;
  std::int64_t loStack[128]{};
  std::int64_t hiStack[128]{};
  std::int64_t low=0;
  std::int64_t stack=0;
  for(std::int64_t high=count-1;;high=hiStack[stack]) {
    while(high-low>=4 && stack<127) {
      std::int64_t left=low;
      std::int64_t right=high;
      const std::int64_t pivot=values[(high+low)/2].sort;
      do {
        while(values[left].sort>pivot) ++left;
        while(values[right].sort<pivot) --right;
        if(left>right) break;
        std::swap(values[left++],values[right--]);
      } while(left<right);
      while(low<right && values[right].sort==pivot) --right;
      while(left<high && values[left].sort==pivot) ++left;
      hiStack[stack]=high;
      loStack[stack++]=left;
      high=right;
    }
    if(low<high) {
      std::int64_t end=high;
      do {
        std::int64_t minimum=low;
        for(std::int64_t scan=low+1;scan<=end;++scan)
          if(values[scan].sort<values[minimum].sort) minimum=scan;
        std::swap(values[minimum],values[end--]);
      } while(end>low);
    }
    if(--stack<0) break;
    low=loStack[stack];
  }
}

void idLocklessCacheLineQueueS::Add(idFileCacheLine* line) {
  queue[queueEnd]=line;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  queueEnd=(static_cast<unsigned short>(queueEnd)+1)&0x7FFF;
  Sys_InterlockedIncrement(const_cast<int*>(&numLines.value));
}

idFileCacheLine* idLocklessCacheLineQueueS::Next() {
  const int start=queueStart;
  if(start==queueEnd) return nullptr;
  idFileCacheLine* line=queue[start];
  queueStart=(static_cast<unsigned short>(start)+1)&0x7FFF;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  Sys_InterlockedDecrement(const_cast<int*>(&numLines.value));
  return line;
}

void idLocklessCacheLineQueueM::Add(idFileCacheLine* line) {
  int index=queueEnd;
  while(Sys_InterlockedCompareExchangePointer(
      reinterpret_cast<void**>(&queue[index&0x7FFF]),0,
      reinterpret_cast<std::int64_t>(line))!=0) ++index;
  for(;;) {
    const int end=queueEnd;
    if(queue[end&0x7FFF]==nullptr) break;
    if(Sys_InterlockedCompareExchange(const_cast<int*>(&queueEnd),end,end+1)!=end)
      break;
  }
  Sys_InterlockedIncrement(const_cast<int*>(&numLines.value));
}

idFileCacheLine* idLocklessCacheLineQueueM::Next() {
  for(;;) {
    const int start=queueStart;
    if(start==queueEnd) return nullptr;
    if(Sys_InterlockedCompareExchange(const_cast<int*>(&queueStart),start,
                                      start+1)!=start) continue;
    idFileCacheLine* line=queue[start&0x7FFF];
    queue[start&0x7FFF]=nullptr;
    Sys_InterlockedDecrement(const_cast<int*>(&numLines.value));
    return line;
  }
}

void idLocklessCacheLineMRU::Condense() {
  unsigned end=static_cast<unsigned short>(queueEnd);
  unsigned scan=end;
  if(static_cast<int>(end)!=queueStart) {
    do {
      scan=static_cast<unsigned short>(scan-1);
      idFileCacheLine* line=queue[scan];
      if(line!=nullptr && Sys_InterlockedExchangePointer(
          reinterpret_cast<void**>(&queue[scan]),0)==
          reinterpret_cast<std::int64_t>(line)) {
        end=static_cast<unsigned short>(end-1);
        line->indexMRU=static_cast<int>(end);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        queue[end]=line;
      }
    } while(static_cast<int>(scan)!=queueStart);
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  queueStart=static_cast<int>(end);
}

void idLocklessCacheLineMRU::AddMRU(idFileCacheLine* line) {
  while(queueStart!=queueEnd) {
    if(queue[queueStart]!=nullptr) break;
    queueStart=static_cast<unsigned short>(queueStart+1);
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  int end=queueEnd;
  int adjusted=end;
  if(adjusted<queueStart) adjusted+=0x10000;
  if(end+0x8000==queueStart || adjusted-queueStart>2*numLines.value)
    Condense();
  line->indexMRU=queueEnd;
  queue[queueEnd]=line;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  queueEnd=static_cast<unsigned short>(queueEnd+1);
  Sys_InterlockedIncrement(const_cast<int*>(&numLines.value));
}

void idLocklessCacheLineMRU::Sort() {
  Condense();
  const int replacementSnapshot=replacementCount.value;
  const int now=static_cast<int>(Sys_Milliseconds());
  std::array<cacheLineSort_t,32768> sorted{};
  int count=0;
  int index=queueStart;
  while(index!=queueEnd) {
    idFileCacheLine* line=queue[index];
    if(line!=nullptr) {
      sorted[count].index=index;
      sorted[count].sort=
          (static_cast<std::int64_t>(line->persistence-
               (replacementSnapshot-line->replacements))<<32)-
          static_cast<std::int64_t>(now-line->time);
      ++count;
    }
    index=static_cast<unsigned short>(index+1);
  }
  CacheLineQuickSort(sorted.data(),count);
  queueEnd=static_cast<unsigned short>(queueEnd+count);
  std::atomic_thread_fence(std::memory_order_seq_cst);
  int end=queueEnd;
  for(int sortedIndex=0;sortedIndex<count;++sortedIndex) {
    const int oldIndex=static_cast<int>(sorted[sortedIndex].index);
    idFileCacheLine* line=queue[oldIndex];
    if(line!=nullptr && Sys_InterlockedExchangePointer(
        reinterpret_cast<void**>(&queue[oldIndex]),0)==
        reinterpret_cast<std::int64_t>(line)) {
      end=static_cast<unsigned short>(end-1);
      line->indexMRU=end;
      std::atomic_thread_fence(std::memory_order_seq_cst);
      queue[end]=line;
    }
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  queueStart=end;
}

void idLocklessCacheLineMRU::MoveMRU(idFileCacheLine* line) {
  const int index=line->indexMRU;
  if(index!=-1 && Sys_InterlockedCompareExchangePointer(
      reinterpret_cast<void**>(&queue[index]),
      reinterpret_cast<std::int64_t>(line),0)==
      reinterpret_cast<std::int64_t>(line)) {
    line->indexMRU=-1;
    Sys_InterlockedDecrement(const_cast<int*>(&numLines.value));
  }
  AddMRU(line);
}

idFileCacheLine* idLocklessCacheLineMRU::NextLRU(memoryType_t type) {
  int start=queueStart;
  while(start!=queueEnd) {
    idFileCacheLine* line=queue[start];
    if(line!=nullptr && (type==MEMORY_TYPE_ANY || line->memType==type)) {
      if(Sys_InterlockedIncrement(const_cast<int*>(&line->modifyLock.value))==1) {
        if(Sys_InterlockedDecrement(const_cast<int*>(&line->pinned.value))==-1 &&
           Sys_InterlockedCompareExchangePointer(
               reinterpret_cast<void**>(&queue[start]),
               reinterpret_cast<std::int64_t>(line),0)==
               reinterpret_cast<std::int64_t>(line)) {
          line->indexMRU=-1;
          Sys_InterlockedDecrement(const_cast<int*>(&numLines.value));
          Sys_InterlockedIncrement(const_cast<int*>(&replacementCount.value));
          return line;
        }
        Sys_InterlockedIncrement(const_cast<int*>(&line->pinned.value));
      }
      Sys_InterlockedDecrement(const_cast<int*>(&line->modifyLock.value));
    }
    start=static_cast<unsigned short>(start+1);
  }
  return nullptr;
}

void idLocklessCacheLineHash::Remove(idFileCacheLine* line) {
  const int key=(line->fileLine+32*line->fileID)&0x7FFF;
  const int count=numHashEntries[key];
  for(int index=0;index<count;++index) {
    const int slot=(index+4*key)&0x1FFFF;
    idFileCacheLine* found=hash[slot];
    if(found!=nullptr && found->fileID==line->fileID &&
       found->fileLine==line->fileLine) {
      hash[slot]=nullptr;
      Sys_InterlockedDecrement(const_cast<int*>(&numLines.value));
      return;
    }
  }
}

void idLocklessCacheLineHash::Add(idFileCacheLine* line) {
  const int key=(line->fileLine+32*line->fileID)&0x7FFF;
  int index=0;
  while(index<0x20000 && hash[(4*key+index)&0x1FFFF]!=nullptr) ++index;
  if(index>=0x20000) return;
  hash[(4*key+index)&0x1FFFF]=line;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if(numHashEntries[key]<index+1) numHashEntries[key]=index+1;
  Sys_InterlockedIncrement(const_cast<int*>(&numLines.value));
}

idFileCacheLine* idLocklessCacheLineHash::Find(int fileID,int fileLine) {
  const int key=(fileLine+32*fileID)&0x7FFF;
  const int count=numHashEntries[key];
  for(int index=0;index<count;++index) {
    idFileCacheLine* line=hash[(4*key+index)&0x1FFFF];
    if(line!=nullptr && line->fileID==fileID && line->fileLine==fileLine)
      return line;
  }
  return nullptr;
}

void idLocklessCacheLineHash::Condense() {
  for(int key=0;key<32768;++key) {
    int offset=4*key;
    int index=numHashEntries[key]-1;
    while(index>=0) {
      const int tail=(4*key+index)&0x1FFFF;
      idFileCacheLine* line=hash[tail];
      if(line==nullptr || ((line->fileLine+32*line->fileID)&0x7FFF)!=key) {
        --numHashEntries[key];
        --index;
        continue;
      }
      const int tailOffset=tail;
      while(hash[offset]!=nullptr) {
        offset=(offset+1)&0x1FFFF;
        if(offset==tailOffset) break;
      }
      if(offset==tailOffset) break;
      if(Sys_InterlockedIncrement(
             const_cast<int*>(&line->modifyLock.value))==1) {
          hash[tail]=nullptr;
          hash[offset]=line;
          std::atomic_thread_fence(std::memory_order_seq_cst);
          --numHashEntries[key];
          Sys_InterlockedDecrement(const_cast<int*>(&line->modifyLock.value));
          --index;
      } else {
        Sys_InterlockedDecrement(const_cast<int*>(&line->modifyLock.value));
        break;
      }
    }
  }
}

idStreamFileCache::idStreamFileCache()
    :interfaceMutex(),controlThread(nullptr),cancelToTerminate(false) {}

idStreamFileCache::~idStreamFileCache()=default;

idStreamControlThread::idStreamControlThread():idSysThread(),recoveredStorage{} {
  Field<bool>(88)=false;
  Field<bool>(89)=false;
  Field<int>(92)=0;
  Field<int>(96)=0;
  Field<int>(8296)=Field<int>(8300)=0;
  Field<int>(8304)=0;
  Field<bool>(8308)=Field<bool>(8309)=false;
  Field<int>(925816)=Field<int>(925820)=0;
  Field<int>(1843328)=Field<int>(1843332)=0;
  Field<int>(2760840)=Field<int>(2760844)=0;
  Field<int>(4333712)=Field<int>(4333716)=0;
  Field<int>(7618872)=-1;
}

void idStreamControlThread::UncachedBackgroundRead(
    idFile* file,std::int64_t offset,int length,void** destination,memTag_t tag,
    volatile bool* completion) {
  auto* queue=&Field<uncachedRequest_t>(8312);
  int& start=Field<int>(925816);
  int& end=Field<int>(925820);
  while(((static_cast<unsigned short>(end)+1)&0x3FFF)==start) {
    idLib::Warning("idStreamControlThread::UncachedBackgroundRead: queue is full");
    Sys_Sleep(1);
  }
  uncachedRequest_t& request=queue[end];
  request.filePtr=file;
  request.fileDevice=file->GetDevice();
  request.fileOfs=offset;
  request.length=length;
  request.dest=destination;
  request.tag=tag;
  request.completionFlag=completion;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  end=(static_cast<unsigned short>(end)+1)&0x3FFF;
}

void idStreamControlThread::BatchUncachedBackgroundRead(
    idFile* file,std::int64_t* offsets,int* lengths,void*** destinations,
    memTag_t tag,volatile bool** completions,int count) {
  auto* queue=&Field<uncachedRequest_t>(8312);
  int& start=Field<int>(925816);
  int& end=Field<int>(925820);
  const fsDevice_t device=file->GetDevice();
  for(int index=0;index<count;++index) {
    while(((static_cast<unsigned short>(end)+index+1)&0x3FFF)==start) {
      idLib::Warning("idStreamControlThread::BatchUncachedBackgroundRead: queue is full");
      Sys_Sleep(1);
    }
    uncachedRequest_t& request=queue[(end+index)&0x3FFF];
    request.filePtr=file;
    request.fileDevice=device;
    request.fileOfs=offsets[index];
    request.length=lengths[index];
    request.dest=destinations[index];
    request.tag=tag;
    request.completionFlag=completions[index];
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  end=(static_cast<unsigned short>(end)+count)&0x3FFF;
}

void idStreamControlThread::AddCacheLineRequest(
    idFile* file,int fileID,int fileLine,int priority,int persistence) {
  auto* queue=&Field<streamRequest_t>(2760848);
  int& start=Field<int>(4333712);
  int& end=Field<int>(4333716);
  while(static_cast<unsigned short>(end+1)==start) {
    idLib::Warning("idStreamControlThread::AddCacheLineRequest: queue is full");
    SignalWork();
    WaitForThread();
  }
  streamRequest_t& request=queue[end];
  request.priority=priority;
  request.filePtr=file;
  request.fileID=fileID;
  request.fileLine=fileLine;
  request.persistence=persistence;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  end=static_cast<unsigned short>(end+1);
}

void idStreamControlThread::BatchFreeMemory(void** blocks,int count) {
  auto** queue=&Field<void*>(104);
  int& start=Field<int>(8296);
  int& end=Field<int>(8300);
  for(int index=0;index<count;++index) {
    bool signalled=false;
    while(((static_cast<unsigned short>(end)+index+1)&0x3FF)==start) {
      idLib::Warning("idStreamControlThread::BatchFreeMemory: queue is full");
      if(!signalled){SignalWork();signalled=true;}
      Sys_Sleep(1);
    }
    queue[(end+index)&0x3FF]=blocks[index];
  }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  end=(static_cast<unsigned short>(end)+count)&0x3FF;
  SignalWork();
}

unsigned char* idStreamControlThread::AllocProvidedLineBuffer() {
  auto* total=&Field<idSysInterlockedInteger>(7618864);
  auto* used=&Field<idSysInterlockedInteger>(7618868);
  const int maximum=Field<int>(7618872);
  if(total->value<=0 || (maximum>=0 && used->value>=maximum)) return nullptr;
  auto** buffers=&Field<void*>(7610672);
  for(int index=0;index<1024;++index) {
    void* buffer=buffers[index];
    if(buffer!=nullptr && Sys_InterlockedCompareExchangePointer(
        &buffers[index],reinterpret_cast<std::int64_t>(buffer),0)==
        reinterpret_cast<std::int64_t>(buffer)) {
      Sys_InterlockedIncrement(const_cast<int*>(&used->value));
      return static_cast<unsigned char*>(buffer);
    }
  }
  return nullptr;
}

void idStreamControlThread::FreeLineBuffer(unsigned char* buffer,
                                            memoryType_t type) {
  auto* totalUsed=&Field<idSysInterlockedInteger>(7610664);
  if(type==MEMORY_TYPE_ALLOCATED) {
    VirtualFree(buffer,0,MEM_RELEASE);
    Sys_InterlockedDecrement(const_cast<int*>(&totalUsed->value));
  } else if(type==MEMORY_TYPE_PROVIDED) {
    auto* used=&Field<idSysInterlockedInteger>(7618868);
    auto* total=&Field<idSysInterlockedInteger>(7618864);
    Sys_InterlockedDecrement(const_cast<int*>(&used->value));
    auto** buffers=&Field<void*>(7610672);
    bool returned=false;
    for(int index=0;index<1024;++index) {
      if(buffers[index]==nullptr && Sys_InterlockedCompareExchangePointer(
          &buffers[index],0,reinterpret_cast<std::int64_t>(buffer))==0) {
        returned=true;
        break;
      }
    }
    if(!returned) Sys_InterlockedDecrement(const_cast<int*>(&total->value));
    Sys_InterlockedDecrement(const_cast<int*>(&totalUsed->value));
  }
}

bool idStreamControlThread::FreeLRU() {
  ++Field<int>(92);
  auto& mru=Field<idLocklessCacheLineMRU>(5906688);
  idFileCacheLine* line=mru.NextLRU(MEMORY_TYPE_ALLOCATED);
  if(line==nullptr) return false;
  unsigned char* data=line->data;
  const memoryType_t type=line->memType;
  line->data=nullptr;
  Field<idLocklessCacheLineHash>(6430992).Remove(line);
  line->state=CACHE_LINE_STATE_FREE;
  Field<idLocklessCacheLineQueueM>(5644528).Add(line);
  if(data==nullptr) return false;
  FreeLineBuffer(data,type);
  return true;
}

void idStreamControlThread::Shutdown() {
  auto*& optical=Field<idSysThread*>(7610648);
  if(optical!=nullptr){
    static_cast<idOpticalDiskStreamThread*>(optical)->~idOpticalDiskStreamThread();
    idMem::Singleton()->Free(optical,ALIGN_16);
    optical=nullptr;
  }
  auto*& hard=Field<idSysThread*>(7610656);
  if(hard!=nullptr){
    static_cast<idHardDiskStreamThread*>(hard)->~idHardDiskStreamThread();
    idMem::Singleton()->Free(hard,ALIGN_16);
    hard=nullptr;
  }
  void*& hdcBuffer=Field<void*>(7619120);
  idMem::Singleton()->Free(hdcBuffer,ALIGN_16);
  hdcBuffer=nullptr;
}

void idStreamControlThread::WaitForCompletion() {
  Field<bool>(89)=false;
  std::atomic_thread_fence(std::memory_order_seq_cst);
  for(;;) {
    WaitForThread();
    if(auto* optical=Field<idSysThread*>(7610648)) optical->WaitForThread();
    if(auto* hard=Field<idSysThread*>(7610656)) hard->WaitForThread();
    const bool empty=
        Field<int>(925816)==Field<int>(925820) &&
        Field<int>(1843328)==Field<int>(1843332) &&
        Field<int>(2760840)==Field<int>(2760844) &&
        Field<int>(4333712)==Field<int>(4333716) &&
        Field<idLocklessCacheLineQueueS>(4333720).IsEmpty() &&
        Field<idLocklessCacheLineQueueS>(4595880).IsEmpty() &&
        Field<idLocklessCacheLineQueueS>(4858040).IsEmpty() &&
        Field<idLocklessCacheLineQueueS>(5120200).IsEmpty() &&
        Field<idLocklessCacheLineQueueS>(5382360).IsEmpty() && !Field<bool>(89);
    if(empty) break;
    Field<bool>(89)=false;
    SignalWork();
  }
}

int idStreamControlThread::FindHardDiskCacheLine(int fileID,int fileLine) {
  if(fc_useHdc.GetInteger()==0 || Field<idFile*>(7619104)==nullptr) return -1;
  auto* hdc=Field<hdCache_t*>(7619136);
  const int value=fileLine|(fileID<<24);
  const int base=(32*(static_cast<unsigned short>(fileLine/4)+
                       16*static_cast<unsigned short>(fileID)))&0x3FE0;
  int found=0;
  while(found<32 && hdc->hdLines[base+found]!=static_cast<unsigned>(value)) ++found;
  if(found==32) return -1;
  for(int index=0;index<32;++index)
    if(hdc->hdMRU[base+index]<hdc->hdMRU[base+found]) ++hdc->hdMRU[base+index];
  hdc->hdMRU[base+found]=0;
  return base+found;
}

int idStreamControlThread::AllocHardDiskCacheLine(int fileID,int fileLine) {
  if(fc_useHdc.GetInteger()==0 || Field<idFile*>(7619104)==nullptr) return -1;
  auto* hdc=Field<hdCache_t*>(7619136);
  const int value=fileLine|(fileID<<24);
  const int base=(32*(static_cast<unsigned short>(fileLine/4)+
                       16*static_cast<unsigned short>(fileID)))&0x3FE0;
  int selected=base;
  for(int index=0;index<32;++index) {
    if(hdc->hdMRU[base+index]==31) selected=base+index;
    else ++hdc->hdMRU[base+index];
  }
  hdc->hdLines[selected]=value;
  hdc->hdMRU[selected]=0;
  return selected;
}

void idStreamControlThread::ShutdownHDC() {
  idFile*& header=Field<idFile*>(7619104);
  if(header==nullptr) return;
  WaitForCompletion();
  auto* hdc=Field<hdCache_t*>(7619136);
  hdc->magic1=1346782003;
  hdc->lineSize=0x10000;
  std::memset(hdc->hdLines,0xFF,sizeof(hdc->hdLines));
  for(int block=0;block<512;++block)
    for(int index=0;index<32;++index)
      hdc->hdMRU[block*32+index]=static_cast<unsigned char>(31-index);
  hdc->hdLinesWritten=0;
  hdc->hdLinesRead=0;
  hdc->magic2=1346782003;
  CloseCacheFile(header);
  idFile*& data=Field<idFile*>(7619128);
  CloseCacheFile(data);
}

void idStreamControlThread::ClearMem(bool releaseProvidedMemory) {
  WaitForCompletion();
  Field<int>(8296)=Field<int>(8300);
  Field<int>(925816)=Field<int>(925820);
  Field<int>(1843328)=Field<int>(1843332);
  Field<int>(2760840)=Field<int>(2760844);
  Field<int>(92)=0;
  Field<int>(4333712)=Field<int>(4333716);
  idStreamControlThread* self=this;
  auto clearQueue=[self](std::size_t offset) {
    auto& queue=self->Field<idLocklessCacheLineQueueS>(offset);
    std::memset(queue.queue,0,sizeof(queue.queue));
    queue.Clear();
  };
  clearQueue(4333720);
  clearQueue(4595880);
  clearQueue(4858040);
  clearQueue(5120200);
  clearQueue(5382360);
  {
    auto& queue=Field<idLocklessCacheLineQueueM>(5644528);
    std::memset(queue.queue,0,sizeof(queue.queue));
    queue.Clear();
  }
  {
    auto& mru=Field<idLocklessCacheLineMRU>(5906688);
    std::memset(mru.queue,0,sizeof(mru.queue));
    mru.Clear();
  }
  Field<idLocklessCacheLineHash>(6430992).Clear();
  idFileCacheLine*& lines=Field<idFileCacheLine*>(5644520);
  if(lines!=nullptr) {
    for(int index=0;index<0x4000;++index) {
      if(lines[index].data!=nullptr) {
        FreeLineBuffer(lines[index].data,lines[index].memType);
        lines[index].data=nullptr;
        lines[index].memType=MEMORY_TYPE_INVALID;
      }
    }
    std::memset(lines,0,0x140000);
  } else {
    lines=static_cast<idFileCacheLine*>(idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\engine\\framework\\StreamFileCache.cpp(1903) : TAG_PAGEFILECACHE",
        0x140000,TAG_PAGEFILECACHE,true,ALIGN_16,HEAP_DEFAULTHEAP));
  }
  for(int index=0;index<0x4000;++index) {
    lines[index].state=CACHE_LINE_STATE_FREE;
    lines[index].indexMRU=-1;
    Sys_InterlockedIncrement(const_cast<int*>(&lines[index].modifyLock.value));
    Sys_InterlockedDecrement(const_cast<int*>(&lines[index].pinned.value));
    Field<idLocklessCacheLineQueueM>(5644528).Add(&lines[index]);
  }
  Field<idSysInterlockedInteger>(7610664).value=0;
  if(releaseProvidedMemory) {
    std::memset(&Field<void*>(7610672),0,1024*sizeof(void*));
    Field<idSysInterlockedInteger>(7618864).value=0;
  }
  Field<idSysInterlockedInteger>(7618868).value=0;
  std::memset(&Field<std::byte>(7618880),0,224);
  auto& stats=Field<fcStats_t>(7618880);
  stats.hardDiskLineMinReadTime=1000000;
  stats.opticalDiskLineMinReadTime=1000000;
  stats.hardDiskCacheMinWriteTime=1000000;
  stats.hardDiskCacheMinReadTime=1000000;
}

void idStreamControlThread::ProcessRequests() {
  Field<int>(96)=static_cast<int>(Sys_Milliseconds());
  const std::uint64_t processStart=Sys_Microseconds();
  auto** frees=&Field<void*>(104);
  while(Field<int>(8296)!=Field<int>(8300)) {
    const int index=Field<int>(8296);
    idMem::Singleton()->Free(frees[index],ALIGN_16);
    Field<int>(8296)=(static_cast<unsigned short>(index)+1)&0x3FF;
  }
  if(Field<int>(8304)==1) Field<bool>(8308)=true;
  else if(Field<int>(8304)==2) Field<bool>(8309)=true;
  Field<int>(8304)=0;

  auto* incoming=&Field<uncachedRequest_t>(8312);
  while(Field<int>(925816)!=Field<int>(925820)) {
    const int index=Field<int>(925816);
    const bool optical=incoming[index].fileDevice==FS_DEVICE_OPTICAL_DISK_DRIVE;
    auto* target=optical?&Field<uncachedRequest_t>(925824):
                          &Field<uncachedRequest_t>(1843336);
    int& targetStart=optical?Field<int>(1843328):Field<int>(2760840);
    int& targetEnd=optical?Field<int>(1843332):Field<int>(2760844);
    if(((static_cast<unsigned short>(targetEnd)+1)&0x3FFF)==targetStart) break;
    target[targetEnd]=incoming[index];
    std::atomic_thread_fence(std::memory_order_seq_cst);
    targetEnd=(static_cast<unsigned short>(targetEnd)+1)&0x3FFF;
    Field<int>(925816)=(static_cast<unsigned short>(index)+1)&0x3FFF;
  }
  auto& opticalFinished=Field<idLocklessCacheLineQueueS>(4595880);
  while(idFileCacheLine* line=opticalFinished.Next())
    Field<idLocklessCacheLineMRU>(5906688).AddMRU(line);
  auto& hardFinished=Field<idLocklessCacheLineQueueS>(5382360);
  while(idFileCacheLine* line=hardFinished.Next())
    Field<idLocklessCacheLineMRU>(5906688).AddMRU(line);
  if(Field<bool>(88)) {
    Field<bool>(88)=false;
    Field<idLocklessCacheLineMRU>(5906688).Sort();
    Field<idLocklessCacheLineHash>(6430992).Condense();
  }

  auto* requests=&Field<streamRequest_t>(2760848);
  int& requestStart=Field<int>(4333712);
  const int requestEnd=Field<int>(4333716);
  const int now=static_cast<int>(Sys_Milliseconds());
  while(requestStart!=requestEnd) {
    streamRequest_t& request=requests[requestStart];
    auto& hash=Field<idLocklessCacheLineHash>(6430992);
    if(idFileCacheLine* line=hash.Find(request.fileID,request.fileLine)) {
      if(line->priority<request.priority) line->priority=request.priority;
      if(line->persistence<request.persistence) line->persistence=request.persistence;
      line->time=now;
      line->replacements=Field<idLocklessCacheLineMRU>(5906688).replacementCount.value;
      if(line->indexMRU!=-1) {
        auto& mru=Field<idLocklessCacheLineMRU>(5906688);
        idFileCacheLine* exchanged=reinterpret_cast<idFileCacheLine*>(
            Sys_InterlockedExchangePointer(
                reinterpret_cast<void**>(&mru.queue[line->indexMRU]),0));
        if(exchanged==line) {
          line->indexMRU=-1;
          Sys_InterlockedDecrement(const_cast<int*>(&mru.numLines.value));
          mru.AddMRU(line);
        }
      }
      ++Field<fcStats_t>(7618880).memoryCacheHits;
      requestStart=static_cast<unsigned short>(requestStart+1);
      continue;
    }
    streamFileLog.NoteCacheLineRequest(request.filePtr,request.fileLine,
                                       request.priority);
    idFileCacheLine* line=Field<idLocklessCacheLineQueueM>(5644528).Next();
    if(line==nullptr) {
      line=Field<idLocklessCacheLineMRU>(5906688).NextLRU(MEMORY_TYPE_ANY);
      if(line==nullptr) {requestStart=requestEnd;break;}
      FreeLineBuffer(line->data,line->memType);
      line->data=nullptr;
      line->memType=MEMORY_TYPE_INVALID;
      hash.Remove(line);
      line->state=CACHE_LINE_STATE_FREE;
    }
    line->state=CACHE_LINE_STATE_READ_HARD_DISK;
    line->fileID=request.fileID;
    line->fileLine=request.fileLine;
    line->validLength=0;
    line->filePtr=request.filePtr;
    line->priority=request.priority;
    line->persistence=request.persistence;
    line->time=now;
    line->replacements=Field<idLocklessCacheLineMRU>(5906688).replacementCount.value;
    hash.Add(line);
    Field<idLocklessCacheLineQueueS>(4858040).Add(line);
    requestStart=static_cast<unsigned short>(requestStart+1);
  }
  auto* hard=static_cast<idHardDiskStreamThread*>(Field<idSysThread*>(7610656));
  if(!Field<idLocklessCacheLineQueueS>(4858040).IsEmpty() ||
     Field<int>(2760840)!=Field<int>(2760844) || Field<bool>(8309)) {
    Field<bool>(89)=true;
    hard->SignalWork();
  }
  auto* optical=static_cast<idOpticalDiskStreamThread*>(Field<idSysThread*>(7610648));
  if(Field<int>(1843328)!=Field<int>(1843332) || Field<bool>(8308)) {
    Field<bool>(89)=true;
    optical->SignalWork();
  }
  const std::uint64_t processEnd=Sys_Microseconds();
  const std::uint64_t elapsed=processEnd-processStart;
  if(elapsed>500)
    console->AddTimeRange(RANGE_STREAM_CT,idColor::colorRed,processStart,
                          processEnd);
  if(elapsed>4000 && fc_printProcessRequestsPerformanceWarning.GetInteger()!=0)
    idLib::Printf("idStreamControlThread::ProcessRequests() took %1.1f msec\n",
                  static_cast<float>(elapsed)*0.001f);
}

int idStreamControlThread::Run() {
  while(Field<int>(925816)!=Field<int>(925820) ||
        Field<int>(4333712)!=Field<int>(4333716) ||
        Field<int>(8296)!=Field<int>(8300) ||
        !Field<idLocklessCacheLineQueueS>(4595880).IsEmpty() ||
        !Field<idLocklessCacheLineQueueS>(5382360).IsEmpty() || Field<int>(8304)!=0)
    ProcessRequests();
  return 0;
}

namespace {
unsigned char* AcquireLineBuffer(idStreamControlThread* control,
                                 memoryType_t& type) {
  auto& used=control->Field<idSysInterlockedInteger>(7610664);
  if((Sys_InterlockedIncrement(const_cast<int*>(&used.value))<<16) <=
     (fc_maxCacheMemoryMB.GetInteger()<<20)) {
    if(unsigned char* provided=control->AllocProvidedLineBuffer()) {
      type=MEMORY_TYPE_PROVIDED;
      return provided;
    }
    if(void* allocated=VirtualAlloc(nullptr,0x10000,0x101000,PAGE_READWRITE)) {
      type=MEMORY_TYPE_ALLOCATED;
      return static_cast<unsigned char*>(allocated);
    }
  }
  Sys_InterlockedDecrement(const_cast<int*>(&used.value));
  idFileCacheLine* victim=
      control->Field<idLocklessCacheLineMRU>(5906688).NextLRU(MEMORY_TYPE_ANY);
  if(victim==nullptr){type=MEMORY_TYPE_INVALID;return nullptr;}
  unsigned char* data=victim->data;
  type=victim->memType;
  victim->data=nullptr;
  control->Field<idLocklessCacheLineHash>(6430992).Remove(victim);
  victim->state=CACHE_LINE_STATE_FREE;
  control->Field<idLocklessCacheLineQueueM>(5644528).Add(victim);
  return data;
}

void ProcessUncached(idStreamControlThread* control,std::size_t queueOffset,
                     std::size_t startOffset,std::size_t endOffset,
                     sflDrive_t drive) {
  int& start=control->Field<int>(startOffset);
  if(start==control->Field<int>(endOffset)) return;
  auto* queue=&control->Field<uncachedRequest_t>(queueOffset);
  uncachedRequest_t& request=queue[start];
  const std::uint64_t begin=Sys_Microseconds();
  if(*request.dest==nullptr)
    *request.dest=idMem::Singleton()->AllocWithLocation(
        drive==SFL_HARD_DRIVE?
          "w:\\build\\tech5\\engine\\framework\\StreamFileCache.cpp(2883) : request.tag":
          "w:\\build\\tech5\\engine\\framework\\StreamFileCache.cpp(3256) : request.tag",
        static_cast<unsigned>(request.length),request.tag,false,ALIGN_16,
        HEAP_MAPHEAP);
  streamFileLog.BeginIO(drive,SFL_READ,request.filePtr,request.fileOfs,
                        request.length,99);
  request.filePtr->ReadOfs(request.fileOfs,*request.dest,request.length);
  streamFileLog.EndIO(drive);
  const std::uint64_t end=Sys_Microseconds();
  *request.completionFlag=true;
  auto& stats=control->Field<fcStats_t>(7618880);
  if(drive==SFL_HARD_DRIVE) {
    ++stats.uncachedHardDiskReads;
    stats.uncachedHardDiskReadSize+=request.length;
  } else {
    ++stats.uncachedOpticalDiskReads;
    stats.uncachedOpticalDiskReadSize+=request.length;
  }
  console->AddTimeRange(drive==SFL_HARD_DRIVE?RANGE_STREAM_HD:RANGE_STREAM_OD,
                        idColor::colorBlue,begin,end);
  start=(static_cast<unsigned short>(start)+1)&0x3FFF;
}
}

void idHardDiskStreamThread::ProcessIO() {
  if(controlThread->Field<bool>(8309)) {
    controlThread->Field<bool>(8309)=false;
    volatile bool cancel=false;
    FillBinkBuffer(cancel);
    return;
  }
  if(controlThread->Field<int>(2760840)!=controlThread->Field<int>(2760844)) {
    ProcessUncached(controlThread,1843336,2760840,2760844,SFL_HARD_DRIVE);
    return;
  }
  if(idFileCacheLine* write=controlThread->Field<idLocklessCacheLineQueueS>(5120200).Next()) {
    const int cacheLine=controlThread->AllocHardDiskCacheLine(write->fileID,write->fileLine);
    if(cacheLine!=-1) {
      hardDiskOffset=cacheLine-0xFFFFFF;
      idFile* backing=controlThread->Field<idFile*>(7619128+(cacheLine>>14)*8);
      const std::uint64_t begin=Sys_Microseconds();
      streamFileLog.BeginIO(SFL_HARD_DRIVE,SFL_WRITE,backing,
          static_cast<std::int64_t>(cacheLine&0x3FFF)<<16,0x10000,0);
      backing->WriteOfs(static_cast<std::int64_t>(cacheLine&0x3FFF)<<16,
                        write->data,0x10000);
      streamFileLog.EndIO(SFL_HARD_DRIVE);
      const std::uint64_t elapsed=Sys_Microseconds()-begin;
      const std::uint64_t end=begin+elapsed;
      auto& stats=controlThread->Field<fcStats_t>(7618880);
      ++controlThread->Field<hdCache_t*>(7619136)->hdLinesWritten;
      ++stats.hardDiskCacheLinesWritten;
      stats.hardDiskCacheTotalWriteTime+=elapsed;
      if(stats.hardDiskCacheMinWriteTime>elapsed) stats.hardDiskCacheMinWriteTime=elapsed;
      if(stats.hardDiskCacheMaxWriteTime<elapsed) stats.hardDiskCacheMaxWriteTime=elapsed;
      console->AddTimeRange(RANGE_STREAM_HD,idColor::colorYellow,begin,end);
    }
    controlThread->Field<idLocklessCacheLineQueueS>(5382360).Add(write);
    controlThread->Field<bool>(89)=true;
    controlThread->SignalWork();
    return;
  }
  auto& readQueue=controlThread->Field<idLocklessCacheLineQueueS>(4858040);
  int selected=-1;
  int selectedScore=-1;
  int selectedDelta=0;
  int selectedHdc=-1;
  const int noSeek=fc_hardDiskNoSeekOffset.GetInteger();
  auto* optical=static_cast<idOpticalDiskStreamThread*>(
      controlThread->Field<idSysThread*>(7610648));
  for(int index=readQueue.queueStart;index!=readQueue.queueEnd;
      index=(static_cast<unsigned short>(index)+1)&0x7FFF) {
    idFileCacheLine* candidate=readQueue.queue[index];
    if(candidate==nullptr) continue;
    int hdcLine=-1;
    int diskLine=candidate->fileLine|(candidate->fileID<<24);
    if(candidate->filePtr->GetDevice()==FS_DEVICE_OPTICAL_DISK_DRIVE ||
       fc_useHdc.GetInteger()==2) {
      hdcLine=controlThread->FindHardDiskCacheLine(candidate->fileID,
                                                   candidate->fileLine);
      if(hdcLine==-1) {
        readQueue.queue[index]=nullptr;
        Sys_InterlockedDecrement(const_cast<int*>(&readQueue.numLines.value));
        candidate->state=CACHE_LINE_STATE_READ_OPTICAL_DISK;
        controlThread->Field<idLocklessCacheLineQueueS>(4333720).Add(candidate);
        continue;
      }
      if(diskLine>=optical->opticalDiskOffset &&
         diskLine<optical->opticalDiskOffset+fc_opticalDiskSkipOffset.GetInteger()) {
        readQueue.queue[index]=nullptr;
        Sys_InterlockedDecrement(const_cast<int*>(&readQueue.numLines.value));
        candidate->state=CACHE_LINE_STATE_READ_OPTICAL_DISK_NO_WRITE_BACK;
        controlThread->Field<idLocklessCacheLineQueueS>(4333720).Add(candidate);
        continue;
      }
      diskLine=hdcLine-0x1000000;
    }
    const int delta=diskLine-hardDiskOffset;
    int score=candidate->priority;
    if(std::abs(delta)<=noSeek)
      score=delta<0?delta-noSeek+0x7FFFFFFF:0x7FFFFFFF-delta;
    if(score>selectedScore) {
      selected=index;
      selectedScore=score;
      selectedDelta=delta;
      selectedHdc=hdcLine;
    }
  }
  idFileCacheLine* line=nullptr;
  if(selected!=-1) {
    line=readQueue.queue[selected];
    readQueue.queue[selected]=nullptr;
    Sys_InterlockedDecrement(const_cast<int*>(&readQueue.numLines.value));
  }
  auto& stats=controlThread->Field<fcStats_t>(7618880);
  stats.hardDiskTimeOuts+=controlThread->CondenseCacheLineQueue(
      readQueue,static_cast<int>(Sys_Milliseconds())-
                    fc_hardDiskRequestTimeOut.GetInteger());
  if(!controlThread->Field<idLocklessCacheLineQueueS>(4333720).IsEmpty()) {
    controlThread->Field<bool>(89)=true;
    optical->SignalWork();
  }
  if(line==nullptr) {
    if(!controlThread->Field<idLocklessCacheLineQueueS>(5382360).IsEmpty()) {
      controlThread->Field<bool>(89)=true;
      controlThread->SignalWork();
    }
    return;
  }
  memoryType_t type=tempMemType;
  unsigned char* data=tempData;
  tempData=nullptr;
  tempMemType=MEMORY_TYPE_INVALID;
  if(data==nullptr) data=AcquireLineBuffer(controlThread,type);
  if(data==nullptr) {
    controlThread->Field<idLocklessCacheLineHash>(6430992).Remove(line);
    line->state=CACHE_LINE_STATE_FREE;
    controlThread->Field<idLocklessCacheLineQueueM>(5644528).Add(line);
    return;
  }
  std::int64_t offset=static_cast<std::int64_t>(line->fileLine)<<16;
  idFile* source=line->filePtr;
  if(selectedHdc!=-1) {
    hardDiskOffset=selectedHdc-0xFFFFFF;
    source=controlThread->Field<idFile*>(7619128+(selectedHdc>>14)*8);
    offset=static_cast<std::int64_t>(selectedHdc&0x3FFF)<<16;
  } else {
    hardDiskOffset=(line->fileLine+1)|(line->fileID<<24);
  }
  line->data=data;
  line->memType=type;
  const std::uint64_t begin=Sys_Microseconds();
  streamFileLog.BeginIO(SFL_HARD_DRIVE,SFL_READ,source,offset,0x10000,
                        line->priority);
  line->validLength=static_cast<int>(source->ReadOfs(offset,data,0x10000));
  streamFileLog.EndIO(SFL_HARD_DRIVE);
  const std::uint64_t elapsed=Sys_Microseconds()-begin;
  const std::uint64_t end=begin+elapsed;
  line->filePtr=nullptr;
  if(line->validLength==0) {
    line->data=nullptr;
    line->memType=MEMORY_TYPE_INVALID;
    controlThread->Field<idLocklessCacheLineHash>(6430992).Remove(line);
    line->state=CACHE_LINE_STATE_FREE;
    controlThread->Field<idLocklessCacheLineQueueM>(5644528).Add(line);
    tempData=data;
    tempMemType=type;
    ++stats.hardDiskReadsFailed;
    idLib::Warning(selectedHdc!=-1?
        "failed to read cache line at %lld from the hard disk backing cache":
        "failed to read cache line at %lld from the hard disk",offset);
    return;
  }
  if(selectedHdc!=-1) {
    ++controlThread->Field<hdCache_t*>(7619136)->hdLinesRead;
    ++stats.hardDiskCacheHits;
    stats.hardDiskCacheTotalReadTime+=elapsed;
    if(stats.hardDiskCacheMinReadTime>elapsed) stats.hardDiskCacheMinReadTime=elapsed;
    if(stats.hardDiskCacheMaxReadTime<elapsed) stats.hardDiskCacheMaxReadTime=elapsed;
  } else {
    ++stats.hardDiskLinesRead;
    stats.hardDiskLineTotalReadTime+=elapsed;
    if(stats.hardDiskLineMinReadTime>elapsed) stats.hardDiskLineMinReadTime=elapsed;
    if(stats.hardDiskLineMaxReadTime<elapsed) stats.hardDiskLineMaxReadTime=elapsed;
  }
  stats.hardDiskSeeks+=std::abs(selectedDelta)>noSeek;
  line->state=CACHE_LINE_STATE_VALID;
  Sys_InterlockedIncrement(const_cast<int*>(&line->pinned.value));
  Sys_InterlockedDecrement(const_cast<int*>(&line->modifyLock.value));
  controlThread->Field<idLocklessCacheLineQueueS>(5382360).Add(line);
  console->AddTimeRange(RANGE_STREAM_HD,
      selectedHdc!=-1?idColor::colorRed:idColor::colorPurple,begin,end);
  controlThread->Field<bool>(89)=true;
  controlThread->SignalWork();
}

int idHardDiskStreamThread::Run() {
  while(controlThread->Field<int>(2760840)!=controlThread->Field<int>(2760844) ||
        !controlThread->Field<idLocklessCacheLineQueueS>(4858040).IsEmpty() ||
        !controlThread->Field<idLocklessCacheLineQueueS>(5120200).IsEmpty() ||
        controlThread->Field<bool>(8309)) ProcessIO();
  return 0;
}

void idOpticalDiskStreamThread::ProcessIO() {
  if(controlThread->Field<bool>(8308)) {
    controlThread->Field<bool>(8308)=false;
    FillBinkBuffer(cancelToTerminate);
    return;
  }
  if(controlThread->Field<int>(1843328)!=controlThread->Field<int>(1843332)) {
    ProcessUncached(controlThread,925824,1843328,1843332,SFL_OPTICAL_DRIVE);
    return;
  }
  auto signalResults=[this]() {
    if(!controlThread->Field<idLocklessCacheLineQueueS>(5120200).IsEmpty()) {
      controlThread->Field<bool>(89)=true;
      controlThread->Field<idSysThread*>(7610656)->SignalWork();
    }
    if(!controlThread->Field<idLocklessCacheLineQueueS>(4595880).IsEmpty()) {
      controlThread->Field<bool>(89)=true;
      controlThread->SignalWork();
    }
  };
  auto& readQueue=controlThread->Field<idLocklessCacheLineQueueS>(4333720);
  int selected=-1;
  int selectedDelta=0;
  int selectedScore=-1;
  const int noSeek=fc_opticalDiskNoSeekOffset.GetInteger();
  const int skip=fc_opticalDiskSkipOffset.GetInteger();
  for(int index=readQueue.queueStart;index!=readQueue.queueEnd;
      index=(static_cast<unsigned short>(index)+1)&0x7FFF) {
    idFileCacheLine* candidate=readQueue.queue[index];
    if(candidate==nullptr) continue;
    const int delta=(candidate->fileLine|(candidate->fileID<<24))-
                    opticalDiskOffset;
    int score=candidate->priority;
    if(std::abs(delta)<=noSeek)
      score=delta<0?delta-noSeek+0x7FFFFFFF:0x7FFFFFFF-delta;
    if(score>selectedScore) {
      selected=index;
      selectedScore=score;
      selectedDelta=delta;
    }
  }
  if(selectedDelta<0 || selectedDelta>noSeek) {
    bool changed;
    do {
      changed=false;
      for(int index=readQueue.queueStart;index!=readQueue.queueEnd;
          index=(static_cast<unsigned short>(index)+1)&0x7FFF) {
        idFileCacheLine* candidate=readQueue.queue[index];
        if(candidate==nullptr) continue;
        const int delta=(candidate->fileLine|(candidate->fileID<<24))-
                        opticalDiskOffset;
        if(delta<selectedDelta && delta>selectedDelta-skip &&
           ((delta<0)==(selectedDelta<0))) {
          selected=index;
          selectedDelta=delta;
          changed=true;
          break;
        }
      }
    } while(changed);
  }
  idFileCacheLine* line=nullptr;
  if(selected!=-1) {
    line=readQueue.queue[selected];
    readQueue.queue[selected]=nullptr;
    Sys_InterlockedDecrement(const_cast<int*>(&readQueue.numLines.value));
  }
  auto& stats=controlThread->Field<fcStats_t>(7618880);
  stats.opticalDiskTimeOuts+=controlThread->CondenseCacheLineQueue(
      readQueue,static_cast<int>(Sys_Milliseconds())-
                    fc_opticalDiskRequestTimeOut.GetInteger());
  if(line==nullptr) {signalResults();return;}
  memoryType_t type=tempMemType;
  unsigned char* data=tempData;
  tempData=nullptr;
  tempMemType=MEMORY_TYPE_INVALID;
  if(data==nullptr) data=AcquireLineBuffer(controlThread,type);
  if(data==nullptr) {
    controlThread->Field<idLocklessCacheLineHash>(6430992).Remove(line);
    line->state=CACHE_LINE_STATE_FREE;
    controlThread->Field<idLocklessCacheLineQueueM>(5644528).Add(line);
    signalResults();
    return;
  }
  if(selectedDelta>=1 && selectedDelta<=skip) {
    const std::int64_t skipOffset=
        static_cast<std::int64_t>(opticalDiskOffset++&0xFFFFFF)<<16;
    const std::uint64_t begin=Sys_Microseconds();
    streamFileLog.BeginIO(SFL_OPTICAL_DRIVE,SFL_READ,line->filePtr,
                          skipOffset,0x10000,line->priority);
    line->filePtr->ReadOfs(skipOffset,data,0x10000);
    streamFileLog.EndIO(SFL_OPTICAL_DRIVE);
    const std::uint64_t end=Sys_Microseconds();
    ++stats.opticalDiskSkips;
    tempData=data;
    tempMemType=type;
    readQueue.queueStart=(static_cast<unsigned short>(readQueue.queueStart)-1)&
                         0x7FFF;
    readQueue.queue[readQueue.queueStart]=line;
    Sys_InterlockedIncrement(const_cast<int*>(&readQueue.numLines.value));
    console->AddTimeRange(RANGE_STREAM_OD,idColor::colorYellow,begin,end);
    signalResults();
    return;
  }
  const std::int64_t offset=static_cast<std::int64_t>(line->fileLine)<<16;
  opticalDiskOffset=(line->fileLine+1)|(line->fileID<<24);
  line->data=data;
  line->memType=type;
  const std::uint64_t begin=Sys_Microseconds();
  streamFileLog.BeginIO(SFL_OPTICAL_DRIVE,SFL_READ,line->filePtr,offset,0x10000,
                        line->priority);
  line->validLength=static_cast<int>(line->filePtr->ReadOfs(offset,data,0x10000));
  streamFileLog.EndIO(SFL_OPTICAL_DRIVE);
  const std::uint64_t elapsed=Sys_Microseconds()-begin;
  const std::uint64_t end=begin+elapsed;
  line->filePtr=nullptr;
  if(line->validLength==0) {
    line->data=nullptr;
    line->memType=MEMORY_TYPE_INVALID;
    controlThread->Field<idLocklessCacheLineHash>(6430992).Remove(line);
    line->state=CACHE_LINE_STATE_FREE;
    controlThread->Field<idLocklessCacheLineQueueM>(5644528).Add(line);
    tempData=data;
    tempMemType=type;
    ++stats.opticalDiskReadsFailed;
    return;
  }
  ++stats.opticalDiskLinesRead;
  stats.opticalDiskSeeks+=std::abs(selectedDelta)>noSeek;
  stats.opticalDiskLineTotalReadTime+=elapsed;
  if(stats.opticalDiskLineMinReadTime>elapsed) stats.opticalDiskLineMinReadTime=elapsed;
  if(stats.opticalDiskLineMaxReadTime<elapsed) stats.opticalDiskLineMaxReadTime=elapsed;
  const bool writeBack=line->state==CACHE_LINE_STATE_READ_OPTICAL_DISK;
  line->state=CACHE_LINE_STATE_VALID;
  Sys_InterlockedIncrement(const_cast<int*>(&line->pinned.value));
  Sys_InterlockedDecrement(const_cast<int*>(&line->modifyLock.value));
  if(writeBack) {
    controlThread->Field<idLocklessCacheLineQueueS>(5120200).Add(line);
    controlThread->Field<bool>(89)=true;
    controlThread->Field<idSysThread*>(7610656)->SignalWork();
  } else {
    controlThread->Field<idLocklessCacheLineQueueS>(4595880).Add(line);
    controlThread->Field<bool>(89)=true;
    controlThread->SignalWork();
  }
  console->AddTimeRange(RANGE_STREAM_OD,idColor::colorRed,begin,end);
}

int idOpticalDiskStreamThread::Run() {
  while(controlThread->Field<int>(1843328)!=controlThread->Field<int>(1843332) ||
        !controlThread->Field<idLocklessCacheLineQueueS>(4333720).IsEmpty() ||
        controlThread->Field<bool>(8308)) ProcessIO();
  return 0;
}

void idStreamControlThread::Init() {
  ClearMem(true);
  Field<int>(7619112)=0x20000;
  unsigned char* buffer=static_cast<unsigned char*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\framework\\StreamFileCache.cpp(1822) : TAG_PAGEFILECACHE",
          0x24000,TAG_PAGEFILECACHE,false,ALIGN_16,HEAP_DEFAULTHEAP));
  Field<void*>(7619120)=buffer;
  auto* hdc=reinterpret_cast<hdCache_t*>(
      (reinterpret_cast<std::uintptr_t>(buffer)+0x3FFF)&
      ~static_cast<std::uintptr_t>(0x3FFF));
  Field<hdCache_t*>(7619136)=hdc;
  hdc->magic1=1346782003;
  std::memset(hdc->cachedFiles,0,sizeof(hdc->cachedFiles));
  hdc->lineSize=0x10000;
  std::memset(hdc->hdLines,0xFF,sizeof(hdc->hdLines));
  for(int block=0;block<512;++block)
    for(int index=0;index<32;++index)
      hdc->hdMRU[block*32+index]=static_cast<unsigned char>(31-index);
  hdc->hdLinesWritten=hdc->hdLinesRead=0;
  hdc->magic2=1346782003;
  void* opticalMemory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",120,
      TAG_PAGEFILECACHE,false,ALIGN_16,HEAP_DEFAULTHEAP);
  auto* optical=new(opticalMemory) idOpticalDiskStreamThread;
  optical->controlThread=this;
  Field<idSysThread*>(7610648)=optical;
  optical->StartWorkerThread("OpticalDiskStream",CORE_ANY,THREAD_HIGHEST,0x40000);
  void* hardMemory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",120,
      TAG_PAGEFILECACHE,false,ALIGN_16,HEAP_DEFAULTHEAP);
  auto* hard=new(hardMemory) idHardDiskStreamThread;
  hard->controlThread=this;
  Field<idSysThread*>(7610656)=hard;
  hard->StartWorkerThread("HardDiskStream",CORE_ANY,THREAD_HIGHEST,0x40000);
}

bool idStreamControlThread::CreateNewHDC() {
  if(Field<idFile*>(7619104)!=nullptr) return false;
  const int start=static_cast<int>(Sys_Milliseconds());
  RemoveCacheFile("FileCacheHeader.bin");
  RemoveCacheFile("FileCache000.bin");
  idFile* header=OpenCacheFile("FileCacheHeader.bin",true);
  if(header==nullptr) {
    idLib::Printf("Couldn't create %s\n","FileCacheHeader.bin");
    return false;
  }
  header->SetLength(Field<int>(7619112));
  idFile* backing=OpenCacheFile("FileCache000.bin",true);
  Field<idFile*>(7619128)=backing;
  if(backing==nullptr) {
    CloseCacheFile(header);
    Field<idFile*>(7619128)=nullptr;
    idLib::Printf("Couldn't create %s\n","FileCache000.bin");
    return false;
  }
  backing->SetLength(0x40000000ULL);
  Sys_Sleep(16);
  idLib::Printf("%d seconds to create hard disk backing cache\n",
                (static_cast<int>(Sys_Milliseconds())-start)/1000);
  Field<idFile*>(7619104)=header;
  idLib::Printf("Using hard drive cache: %s\n",header->GetFullPath());
  return true;
}

void idStreamFileCache::Init() {
  idScopedCriticalSection lock(interfaceMutex);
  void* memory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",7619440,
      TAG_PAGEFILECACHE,false,ALIGN_16,HEAP_DEFAULTHEAP);
  controlThread=new(memory) idStreamControlThread;
  controlThread->StartWorkerThread("StreamControl",CORE_0B,THREAD_NORMAL,
                                   0x100000);
  controlThread->Init();
  cancelToTerminate=false;
}

void idStreamFileCache::InitHDC() {
  {
    idScopedCriticalSection lock(interfaceMutex);
    controlThread->ShutdownHDC();
  }
  controlThread->CreateNewHDC();
}

int idStreamControlThread::CondenseCacheLineQueue(
    idLocklessCacheLineQueueS& source,int timeout) {
  int end=source.queueEnd;
  int scan=end;
  int removed=0;
  if(end!=source.queueStart) {
    do {
      scan=(static_cast<unsigned short>(scan)-1)&0x7FFF;
      idFileCacheLine* line=source.queue[scan];
      source.queue[scan]=nullptr;
      if(line!=nullptr) {
        if(line->time<=timeout) {
          Sys_InterlockedDecrement(const_cast<int*>(&source.numLines.value));
          Field<idLocklessCacheLineHash>(6430992).Remove(line);
          line->state=CACHE_LINE_STATE_FREE;
          Field<idLocklessCacheLineQueueM>(5644528).Add(line);
          ++removed;
        } else {
          end=(static_cast<unsigned short>(end)-1)&0x7FFF;
          source.queue[end]=line;
        }
      }
    } while(scan!=source.queueStart);
  }
  source.queueStart=end;
  return removed;
}

void idStreamControlThread::ResetFileIDs() {
  ClearMem(false);
  auto* hdc=Field<hdCache_t*>(7619136);
  hdc->magic1=1346782003;
  std::memset(hdc->cachedFiles,0,sizeof(hdc->cachedFiles));
  hdc->lineSize=0x10000;
  std::memset(hdc->hdLines,0xFF,sizeof(hdc->hdLines));
  for(int block=0;block<512;++block)
    for(int index=0;index<32;++index)
      hdc->hdMRU[block*32+index]=static_cast<unsigned char>(31-index);
  hdc->hdLinesWritten=hdc->hdLinesRead=0;
  hdc->magic2=1346782003;
  std::memset(&Field<bool>(7619144),0,128);
  std::memset(&Field<cachedFileID_t>(7619272),0,20*sizeof(cachedFileID_t));
  Field<int>(7619432)=0;
}

int idStreamControlThread::GetHardDiskFileID(idFile* file) {
  auto* recent=&Field<cachedFileID_t>(7619272);
  int& recentIndex=Field<int>(7619432);
  for(int count=0;count<20;++count) {
    const int index=recentIndex&0x13;
    if(recent[index].uniqID==file->uniqID) return recent[index].id;
    ++recentIndex;
  }
  auto* hdc=Field<hdCache_t*>(7619136);
  const char* path=file->GetFullPath();
  int probe=0;
  const unsigned char hash=static_cast<unsigned char>(idStr::Hash(path));
  bool reset=false;
  int id=0;
  for(;;) {
    if(reset) {idLib::Warning("cleaning hard disk cache");ResetFileIDs();}
    id=(static_cast<unsigned char>(probe)+hash)&0x7F;
    cachedFile_t& cached=hdc->cachedFiles[id];
    if(idStr::Icmp(cached.name,path)==0) break;
    if(cached.name[0]=='\0') {
      std::strncpy(cached.name,path,255);
      cached.device=file->GetDevice();
      cached.timestamp=fc_checkHdcTimestamps.GetInteger()!=0?file->Timestamp():0;
      Field<bool>(7619144+id)=true;
      const int slot=(++recentIndex)&0x13;
      recent[slot].id=id;
      recent[slot].uniqID=file->uniqID;
      return id;
    }
    reset=(++probe==128);
    if(probe>128) idLib::FatalError(
        "idStreamControlThread::GetHardDiskFileID: failed");
  }
  if(!Field<bool>(7619144+id) && fc_checkHdcTimestamps.GetInteger()!=0) {
    const std::uint32_t timestamp=file->Timestamp();
    if(hdc->cachedFiles[id].timestamp!=timestamp) {
      ResetFileIDs();
      id=hash&0x7F;
      std::strncpy(hdc->cachedFiles[id].name,path,255);
      hdc->cachedFiles[id].timestamp=timestamp;
    }
  }
  Field<bool>(7619144+id)=true;
  const int slot=(++recentIndex)&0x13;
  recent[slot].id=id;
  recent[slot].uniqID=file->uniqID;
  return id;
}

int idStreamControlThread::FetchCacheLines(
    idFile* file,std::int64_t offset,int length,bool,int priority,
    int persistence,idFileCacheLine** readLines) {
  const int fileID=GetHardDiskFileID(file);
  const int first=static_cast<int>(offset>>16);
  int last=static_cast<int>((offset+length-1)>>16);
  if(last-first>=24) last=first+23;
  if(priority!=-1 && persistence!=-1)
    for(int line=first;line<=last;++line)
      AddCacheLineRequest(file,fileID,line,priority,persistence);
  int count=0;
  for(int lineNumber=first;lineNumber<=last;++lineNumber) {
    idFileCacheLine* line=Field<idLocklessCacheLineHash>(6430992).Find(fileID,lineNumber);
    if(line==nullptr) break;
    if(Sys_InterlockedIncrement(const_cast<int*>(&line->fetchLock.value))!=1) {
      Sys_InterlockedDecrement(const_cast<int*>(&line->fetchLock.value));
      break;
    }
    if(Sys_InterlockedIncrement(const_cast<int*>(&line->pinned.value))<=0 ||
       line->fileID!=fileID || line->fileLine!=lineNumber) {
      Sys_InterlockedDecrement(const_cast<int*>(&line->pinned.value));
      Sys_InterlockedDecrement(const_cast<int*>(&line->fetchLock.value));
      break;
    }
    Sys_InterlockedDecrement(const_cast<int*>(&line->fetchLock.value));
    readLines[count++]=line;
  }
  if(count==last-first+1) return count;
  for(int index=0;index<count;++index)
    Sys_InterlockedDecrement(const_cast<int*>(&readLines[index]->pinned.value));
  return 0;
}

void idStreamFileCache::NewFrame() {
  controlThread->Field<bool>(88)=true;
  streamFileLog.NoteFrame();
}

void idStreamFileCache::ServiceBink(fsDevice_t source) {
  controlThread->Field<int>(8304)=(source!=FS_DEVICE_OPTICAL_DISK_DRIVE)+1;
  controlThread->SignalWork();
}

void idStreamFileCache::SignalWork() {
  idScopedCriticalSection lock(interfaceMutex);
  controlThread->SignalWork();
}

void idStreamFileCache::UncachedBackgroundRead(
    idFile* file,std::int64_t offset,int length,void** destination,memTag_t tag,
    volatile bool* completion) {
  idScopedCriticalSection lock(interfaceMutex);
  controlThread->UncachedBackgroundRead(file,offset,length,destination,tag,
                                        completion);
}

void idStreamFileCache::BatchUncachedBackgroundRead(
    idFile* file,std::int64_t* offsets,int* lengths,void*** destinations,
    memTag_t tag,volatile bool** completions,int count) {
  idScopedCriticalSection lock(interfaceMutex);
  controlThread->BatchUncachedBackgroundRead(file,offsets,lengths,destinations,
                                             tag,completions,count);
}

void idStreamFileCache::BatchFreeMemory(void** blocks,int count) {
  idScopedCriticalSection lock(interfaceMutex);
  controlThread->BatchFreeMemory(blocks,count);
}

std::int64_t idStreamFileCache::GetCurrentUsageLimit() const {
  return static_cast<std::int64_t>(controlThread->Field<idSysInterlockedInteger>(
      7610664).value)<<16;
}

bool idStreamFileCache::OutOfMemoryCallback() {
  return controlThread!=nullptr && controlThread->FreeLRU();
}

bool StaticOutOfMemoryCallback() {
  return streamFileCache->controlThread!=nullptr &&
         streamFileCache->controlThread->FreeLRU();
}

void idStreamFileCache::Flush() {
  idScopedCriticalSection lock(interfaceMutex);
  controlThread->WaitForCompletion();
  std::memset(&controlThread->Field<cachedFileID_t>(7619272),0,
              20*sizeof(cachedFileID_t));
  controlThread->Field<int>(7619432)=0;
}

void idStreamFileCache::Shutdown() {
  idScopedCriticalSection lock(interfaceMutex);
  if(controlThread!=nullptr) {
    controlThread->Shutdown();
    controlThread->~idStreamControlThread();
    idMem::Singleton()->Free(controlThread,ALIGN_16);
    controlThread=nullptr;
  }
}

std::uint64_t idStreamFileCache::UncachedScheduledRead(
    idFile* file,std::int64_t offset,void* destination,int length) {
  void* output=destination;
  volatile bool completed=false;
  {
    idScopedCriticalSection lock(interfaceMutex);
    controlThread->UncachedBackgroundRead(file,offset,length,&output,
                                           TAG_PAGEFILECACHE,&completed);
  }
  SignalWork();
  while(!completed && !cancelToTerminate) Sys_Sleep(1);
  return static_cast<std::uint64_t>(length);
}

void idStreamFileCache::ClearMem(bool releaseProvidedMemory) {
  idScopedCriticalSection lock(interfaceMutex);
  controlThread->ClearMem(releaseProvidedMemory);
  streamFileLog.Clear();
}

void idStreamFileCache::BatchRequestCacheLines(
    const cacheLineRequest_t* requests,int count) {
  idScopedCriticalSection lock(interfaceMutex);
  for(int index=0;index<count;++index) {
    const int fileID=controlThread->GetHardDiskFileID(requests[index].file);
    controlThread->AddCacheLineRequest(requests[index].file,fileID,
        requests[index].fileLine,requests[index].priority,
        requests[index].persistence);
  }
}

bool idStreamFileCache::PinCachedData(
    idFile* file,std::int64_t offset,int length,pinnedCacheLines_t& lines,
    bool blocking,int priority,int persistence,pfcRequest_t) {
  lines.numLines=0;
  if(offset<0) {
    idLib::Warning("idStreamFileCache::PinCachedData: ofs = %lld for '%s'",
                   offset,file->GetFullPath());
    return false;
  }
  if(length<=0) {
    idLib::Warning("idStreamFileCache::PinCachedData: length = %d for '%s'",
                   length,file->GetFullPath());
    return false;
  }
  idScopedCriticalSection lock(interfaceMutex);
  idFileCacheLine* readLines[24]{};
  int count=controlThread->FetchCacheLines(file,offset,length,blocking,
                                            priority,persistence,readLines);
  if(blocking && count==0) {
    do {
      controlThread->WaitForCompletion();
      count=controlThread->FetchCacheLines(file,offset,length,blocking,
                                            priority,persistence,readLines);
    } while(count==0);
  }
  if(count>24) idLib::FatalError(
      "idStreamFileCache::PinCachedData: data crosses more than %d cache lines",24);
  lines.cacheFile=file;
  lines.ofs=offset;
  lines.length=length;
  lines.numLines=count;
  for(int index=0;index<count;++index) {
    idFileCacheLine* line=readLines[index];
    const std::int64_t relative=offset-(static_cast<std::int64_t>(line->fileLine)<<16);
    const int lineOffset=static_cast<int>(relative>0?relative:0);
    lines.lineData[index]=line->data+lineOffset;
    const std::int64_t end=offset+length-
        (static_cast<std::int64_t>(line->fileLine)<<16);
    lines.lineLength[index]=static_cast<int>(
        (end<line->validLength?end:line->validLength)-lineOffset);
  }
  return count!=0;
}

void idStreamFileCache::UnpinData(pinnedCacheLines_t& lines) {
  if(lines.numLines==0) return;
  if(lines.ofs<0) {
    idLib::Warning("idStreamFileCache::UnpinData: ofs = %lld for '%s'",
                   lines.ofs,lines.cacheFile->GetFullPath());
    return;
  }
  if(lines.length<=0) {
    idLib::Warning("idStreamFileCache::UnpinData: length = %d for '%s'",
                   lines.length,lines.cacheFile->GetFullPath());
    return;
  }
  idScopedCriticalSection lock(interfaceMutex);
  idFileCacheLine* readLines[24]{};
  const int count=controlThread->FetchCacheLines(lines.cacheFile,lines.ofs,
      lines.length,false,-1,-1,readLines);
  if(count>24) idLib::FatalError(
      "idStreamFileCache::UnpinData: data crosses more than %d cache lines",24);
  for(int index=0;index<count;++index)
    Sys_InterlockedDecrement(const_cast<int*>(&readLines[index]->pinned.value));
  for(int index=0;index<count;++index)
    Sys_InterlockedDecrement(const_cast<int*>(&readLines[index]->pinned.value));
  lines.numLines=0;
}

int idStreamFileCache::ReadCachedData(
    idFile* file,std::int64_t offset,int length,void* destination,
    bool blocking,int priority,int persistence,pfcRequest_t) {
  if(offset<0) {
    idLib::Warning("idStreamFileCache::ReadCachedData: ofs = %lld for %s",
                   offset,file->GetFullPath());
    return 0;
  }
  if(length<=0) {
    idLib::Warning("idStreamFileCache::ReadCachedData: length = %d for %s",
                   length,file->GetFullPath());
    return 0;
  }
  idScopedCriticalSection lock(interfaceMutex);
  idFileCacheLine* readLines[24]{};
  int count=controlThread->FetchCacheLines(file,offset,length,blocking,
                                            priority,persistence,readLines);
  if(blocking && count==0) {
    do {
      controlThread->WaitForCompletion();
      count=controlThread->FetchCacheLines(file,offset,length,blocking,
                                            priority,persistence,readLines);
    } while(count==0);
  }
  if(destination!=nullptr && count>0) {
    auto* bytes=static_cast<unsigned char*>(destination);
    for(int index=0;index<count;++index) {
      idFileCacheLine* line=readLines[index];
      const std::int64_t lineBase=static_cast<std::int64_t>(line->fileLine)<<16;
      const std::int64_t sourceOffset=offset-lineBase>0?offset-lineBase:0;
      const std::int64_t available=line->validLength-sourceOffset>0?
          line->validLength-sourceOffset:0;
      const std::int64_t destinationOffset=lineBase-offset>0?lineBase-offset:0;
      const std::int64_t remaining=length-destinationOffset>0?
          length-destinationOffset:0;
      const std::int64_t copyLength=available<remaining?available:remaining;
      std::memmove(bytes+destinationOffset,line->data+sourceOffset,
                   static_cast<std::size_t>(copyLength));
    }
    if(fc_verifyCacheReadData.GetInteger()!=0) {
      unsigned char* realData=static_cast<unsigned char*>(
          idMem::Singleton()->AllocWithLocation(
              "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
              static_cast<unsigned>(length),TAG_TEMP,false,ALIGN_16,
              HEAP_DEFAULTHEAP));
      file->ReadOfs(offset,realData,static_cast<unsigned>(length));
      if(std::memcmp(destination,realData,static_cast<unsigned>(length))!=0)
        idLib::Error("idStreamFileCache::ReadCachedData: data compare failed");
      idMem::Singleton()->Free(realData,ALIGN_16);
    }
  }
  for(int index=0;index<count;++index)
    Sys_InterlockedDecrement(const_cast<int*>(&readLines[index]->pinned.value));
  return count!=0?length:0;
}
