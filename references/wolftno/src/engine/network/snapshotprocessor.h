#pragma once

#include "engine/network/lightweightcompression.h"

#include <cstddef>
#include <cstdint>

class idParallelJobList;
class idSnapShot;

class idSnapshotProcessor {
public:
  idSnapshotProcessor();
  ~idSnapshotProcessor();
  bool TrySetPendingSnapshot(idSnapShot& snapshot);
  void SubmitPendingSnap(int visIndex,std::uint8_t* objectMemory,int memorySize,
                         lzwCompressionData_t* compression,
                         idParallelJobList* jobList);
  int GetPendingSnapDelta(std::uint8_t* buffer,int bufferSize);
  bool ApplySnapshotDelta(int visIndex,int sequence);

  int BaseSequence() const {
    return *reinterpret_cast<const int*>(storage+4);
  }
  bool HasPendingSnap() const {
    return *reinterpret_cast<const bool*>(storage+67024);
  }
  int LastDeltaSequence() const {
    const std::byte* queue=storage+96;
    const auto* list=*reinterpret_cast<const std::byte* const*>(queue);
    const int count=*reinterpret_cast<const int*>(queue+8);
    return count!=0 ? *reinterpret_cast<const int*>(list+(count-1)*12) : -1;
  }

private:
  std::byte storage[67288];
};

class idSnapshotMetrics {
public:
  void ProcessEvent(const struct sysEvent_t* event);
  void LogSnapshotDelta(const idSnapShot& snapshot,const idSnapShot* base,
                        int sequence,int baseSequence,int visIndex,int size,
                        bool fullSnapshot);
  void GenerateReport(idSnapShot* snapshot,idSnapShot* oldSnapshot,
                      int sequence,int baseSequence,int peer,
                      class idRenderModelGui* gui,bool reportMetrics,
                      bool printConsole);
private:
  std::byte storage[1440];
};

extern idSnapshotMetrics snapshotMetrics;

static_assert(sizeof(lzwCompressionData_t)==12320,"LZW compression ABI drift");
static_assert(sizeof(idSnapshotProcessor)==67288,"snapshot processor ABI drift");
static_assert(sizeof(idSnapshotMetrics)==1440,"snapshot metrics ABI drift");
