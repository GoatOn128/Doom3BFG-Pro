#pragma once

#include "engine/framework/streamfilecache.h"

#include <cstdint>

enum cacheLineState_t : int {
  CACHE_LINE_FREE=0,
  CACHE_LINE_STATE_FREE=CACHE_LINE_FREE,
  CACHE_LINE_STATE_READ_HARD_DISK=1,
  CACHE_LINE_STATE_READ_OPTICAL_DISK=2,
  CACHE_LINE_STATE_READ_OPTICAL_DISK_NO_WRITE_BACK=3,
  CACHE_LINE_STATE_VALID=4,
};

enum memoryType_t : int {
  MEMORY_TYPE_INVALID=0,
  MEMORY_TYPE_ANY=1,
  MEMORY_TYPE_ALLOCATED=2,
  MEMORY_TYPE_PROVIDED=3,
};

struct idSysInterlockedInteger { volatile int value; };

struct idFileCacheLine {
  idFileCacheLine()
      :state(CACHE_LINE_FREE),filePtr(nullptr),fileID(0),fileLine(0),
       validLength(0),data(nullptr),memType(MEMORY_TYPE_INVALID),priority(0),
       persistence(0),time(0),replacements(0),indexMRU(-1),modifyLock{},
       fetchLock{},pinned{} {}
  cacheLineState_t state;
  std::byte alignmentBeforeFile[4];
  idFile* filePtr;
  int fileID;
  int fileLine;
  int validLength;
  std::byte alignmentBeforeData[4];
  unsigned char* data;
  memoryType_t memType;
  int priority;
  int persistence;
  int time;
  int replacements;
  int indexMRU;
  idSysInterlockedInteger modifyLock;
  idSysInterlockedInteger fetchLock;
  idSysInterlockedInteger pinned;
  std::byte alignmentTail[4];
};

class idLocklessCacheLineQueueS {
public:
  idLocklessCacheLineQueueS():queue{},queueStart(0),queueEnd(0),numLines{} {}
  void Clear(){queueStart=queueEnd=0;numLines.value=0;}
  bool IsEmpty() const{return queueStart==queueEnd;}
  int GetNumLines() const{return numLines.value;}
  void Add(idFileCacheLine*);
  idFileCacheLine* Next();
  idFileCacheLine* queue[32768];
  volatile int queueStart;
  volatile int queueEnd;
  idSysInterlockedInteger numLines;
  std::byte alignmentTail[4];
};

class idLocklessCacheLineQueueM : public idLocklessCacheLineQueueS {
public:
  void Add(idFileCacheLine*);
  idFileCacheLine* Next();
};

class idLocklessCacheLineMRU {
public:
  idLocklessCacheLineMRU()
      :queue{},queueStart(0),queueEnd(0),numLines{},replacementCount{} {}
  void Clear(){queueStart=queueEnd=0;numLines.value=0;replacementCount.value=0;}
  void Condense();
  void Sort();
  void AddMRU(idFileCacheLine*);
  void MoveMRU(idFileCacheLine*);
  idFileCacheLine* NextLRU(memoryType_t);
  idFileCacheLine* queue[65536];
  volatile int queueStart;
  volatile int queueEnd;
  idSysInterlockedInteger numLines;
  idSysInterlockedInteger replacementCount;
};

class idLocklessCacheLineHash {
public:
  idLocklessCacheLineHash():hash{},numHashEntries{},numLines{} {}
  void Clear(){for(auto& entry:hash)entry=nullptr;for(auto& count:numHashEntries)count=0;numLines.value=0;}
  void Add(idFileCacheLine*);
  idFileCacheLine* Find(int,int);
  void Condense();
  void Remove(idFileCacheLine*);
  idFileCacheLine* hash[131072];
  int numHashEntries[32768];
  idSysInterlockedInteger numLines;
  std::byte alignmentTail[4];
};

struct cacheLineSort_t { std::int64_t index; std::int64_t sort; };
void CacheLineQuickSort(cacheLineSort_t*,std::int64_t);

class idStreamControlThread : public idSysThread {
public:
  idStreamControlThread();
  ~idStreamControlThread() override=default;
  int Run() override;
  void Init();
  void Shutdown();
  void WaitForCompletion();
  void UncachedBackgroundRead(idFile*,std::int64_t,int,void**,memTag_t,
                              volatile bool*);
  void BatchUncachedBackgroundRead(idFile*,std::int64_t*,int*,void***,
                                   memTag_t,volatile bool**,int);
  void AddCacheLineRequest(idFile*,int,int,int,int);
  void BatchFreeMemory(void**,int);
  void ClearMem(bool);
  void ResetFileIDs();
  int GetHardDiskFileID(idFile*);
  void ShutdownHDC();
  int FindHardDiskCacheLine(int,int);
  int AllocHardDiskCacheLine(int,int);
  bool CreateNewHDC();
  void ProcessRequests();
  bool FreeLRU();
  unsigned char* AllocProvidedLineBuffer();
  void FreeLineBuffer(unsigned char*,memoryType_t);
  int CondenseCacheLineQueue(idLocklessCacheLineQueueS&,int);
  int FetchCacheLines(idFile*,std::int64_t,int,bool,int,int,idFileCacheLine**);
  void BatchRequestCacheLines(const cacheLineRequest_t*,int);

  template<typename T>T& Field(std::size_t offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<std::byte*>(this)+offset);
  }
  template<typename T>const T& Field(std::size_t offset) const {
    return *reinterpret_cast<const T*>(reinterpret_cast<const std::byte*>(this)+offset);
  }
  std::byte recoveredStorage[7619440-88];
};

static_assert(sizeof(idFileCacheLine)==80,"cache line ABI drift");
static_assert(sizeof(idLocklessCacheLineQueueS)==262160,"single queue ABI drift");
static_assert(sizeof(idLocklessCacheLineQueueM)==262160,"multi queue ABI drift");
static_assert(sizeof(idLocklessCacheLineMRU)==524304,"MRU ABI drift");
static_assert(sizeof(idLocklessCacheLineHash)==1179656,"hash ABI drift");
static_assert(sizeof(cacheLineSort_t)==16,"cache sort ABI drift");
static_assert(sizeof(idStreamControlThread)==7619440,
              "stream control thread ABI drift");
