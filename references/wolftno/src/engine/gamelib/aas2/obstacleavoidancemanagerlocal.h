#pragma once

#include "engine/gamelib/aas2/obstacleavoidance_abi.h"
#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

class idMD6Anim;

class idObstacleAvoidanceManager {
public:
  virtual ~idObstacleAvoidanceManager();
  static __forceinline void operator delete(void* memory) noexcept {
    idMem::Singleton()->Free(memory, ALIGN_16);
  }
  virtual void Init(idParallelJobList* jobList) = 0;
  virtual void Shutdown() = 0;
  virtual void AllocObstacleBuffers(
      idObstacleAvoidance* obstacleAvoidance) = 0;
  virtual void FreeObstacleBuffers(
      idObstacleAvoidance* obstacleAvoidance) = 0;
  virtual void FrameFreeObstacleBuffers() = 0;
  virtual idParallelJobList* GetJobList() = 0;

protected:
  __forceinline idObstacleAvoidanceManager() noexcept {}
};

class idObstacleAvoidanceManagerLocal : public idObstacleAvoidanceManager {
public:
  idObstacleAvoidanceManagerLocal();
  ~idObstacleAvoidanceManagerLocal() override;

  void Init(idParallelJobList* jobList) override;
  void Shutdown() override;
  void AllocObstacleBuffers(idObstacleAvoidance* obstacleAvoidance) override;
  void FreeObstacleBuffers(idObstacleAvoidance* obstacleAvoidance) override;
  void FrameFreeObstacleBuffers() override;
  idParallelJobList* GetJobList() override { return jobList; }

  idParallelJobList* jobList;
  idList<idObstacleBuffers*, 5> usedBuffers;
  idList<idObstacleBuffers*, 5> freeBuffers;
  idList<idObstacleBuffers*, 5> toBeFreedBuffers;
};

extern idObstacleAvoidanceManagerLocal obstacleAvoidanceManagerLocal;
extern idObstacleAvoidanceManager* obstacleAvoidanceManager;

static_assert(sizeof(idObstacleAvoidanceManager) == 0x08,
              "retail obstacle-manager ABI drift");
static_assert(offsetof(idObstacleAvoidanceManagerLocal, jobList) == 0x08,
              "retail obstacle job-list offset drift");
static_assert(offsetof(idObstacleAvoidanceManagerLocal, usedBuffers) == 0x10,
              "retail used-buffer list offset drift");
static_assert(offsetof(idObstacleAvoidanceManagerLocal, freeBuffers) == 0x28,
              "retail free-buffer list offset drift");
static_assert(offsetof(idObstacleAvoidanceManagerLocal, toBeFreedBuffers) ==
                  0x40,
              "retail pending-buffer list offset drift");
static_assert(sizeof(idObstacleAvoidanceManagerLocal) == 0x58,
              "retail local obstacle-manager ABI drift");
