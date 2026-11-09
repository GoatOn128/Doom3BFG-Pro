#pragma once

#include "shared/idlib/color.h"
#include "shared/idlib/parallelism/thread.h"

#include <atomic>
#include <cstdint>
#include <future>
#include <mutex>
#include <vector>

using jobRun_t = void (*)(void*);
enum jobListId_t {
  JOBLIST_COLLISION = 0,
  JOBLIST_GAME,
  JOBLIST_RENDERMODEL,
  JOBLIST_RENDERER,
  JOBLIST_VIDEO,
  JOBLIST_ANALYZE,
  JOBLIST_VIRTUAL_TEXTURE1,
  JOBLIST_VIRTUAL_TEXTURE2,
  JOBLIST_VIRTUAL_TEXTURE3,
  JOBLIST_SOUND,
  JOBLIST_UTILITY,
  JOBLIST_SNAPSHOT,
  MAX_JOBLISTS = 64
};
enum jobListPriority_t {
  JOBLIST_PRIORITY_NONE = 0,
  JOBLIST_PRIORITY_LOW,
  JOBLIST_PRIORITY_MEDIUM,
  JOBLIST_PRIORITY_HIGH
};
enum jobSyncType_t { SYNC_NONE = 0, SYNC_SIGNAL, SYNC_SYNCHRONIZE };

void RegisterJob(jobRun_t function, const char* name);
class idParallelJobRegistration {
public:
  idParallelJobRegistration(jobRun_t function, const char* name) { RegisterJob(function, name); }
};

struct threadStats_t {
  std::uint32_t numExecutedJobs{}, numExecutedSyncs{};
  std::uint64_t submitTime{}, startTime{}, endTime{}, waitTime{}, trailingExecTime{},
      threadExecTime[32]{}, threadTotalTime[32]{};
};
struct threadJobListState_t {
  class idParallelJobList_Threads* jobList{};
  int version{}, signalIndex{}, lastJobIndex{}, nextJobIndex{};
};

class idParallelJobList_Threads {
public:
  struct job_t {
    jobRun_t function{};
    void* data{};
    int executed{};
    jobSyncType_t sync{SYNC_NONE};
  };
  idParallelJobList_Threads(jobListId_t id, jobListPriority_t priority, unsigned maxJobs,
                            unsigned maxSyncs);
  ~idParallelJobList_Threads();
  void AddJob(jobRun_t function, std::uint64_t data);
  void InsertSyncPoint(jobSyncType_t type);
  void Submit(idParallelJobList_Threads* waitForJobList, int parallelism);
  void Wait();
  bool TryWait();
  int RunJobsInternal(unsigned threadNum, threadJobListState_t* state, bool singleJob);
  std::uint64_t GetTotalWastedTimeMicroSec() const;
  bool IsSubmitted() const { return submitted; }
  jobListId_t listId;
  jobListPriority_t listPriority;
  unsigned maxJobs, maxSyncs, numSyncs{};
  std::vector<job_t> jobs;
  threadStats_t stats;

private:
  mutable std::mutex mutex;
  std::future<void> work;
  std::atomic<unsigned> nextJob{0};
  std::atomic<bool> submitted{false}, done{false};
  int parallelism{1};
};

class idParallelJobList {
public:
  idParallelJobList(jobListId_t id, jobListPriority_t priority, unsigned maxJobs, unsigned maxSyncs,
                    const idColor* color);
  ~idParallelJobList();
  void AddJob(jobRun_t function, std::uint64_t data);
  void AddJob(jobRun_t function, void* data);
  void InsertSyncPoint(jobSyncType_t type);
  void Submit(idParallelJobList* waitForJobList = nullptr, int parallelism = 0);
  void Wait();
  bool TryWait();
  bool IsSubmitted() const;
  int GetNumExecutedJobs() const;
  int GetNumSyncs() const;
  std::uint64_t GetSubmitTimeMicroSec() const;
  std::uint64_t GetStartTimeMicroSec() const;
  std::uint64_t GetFinishTimeMicroSec() const;
  std::uint64_t GetWaitTimeMicroSec() const;
  std::uint64_t GetTotalProcessingTimeMicroSec() const;
  std::uint64_t GetAvgTotalProcessingTimeMicroSec() const;
  std::uint64_t GetTotalWastedTimeMicroSec() const;
  std::uint64_t GetUnitProcessingTimeMicroSec(unsigned unit) const;
  std::uint64_t GetUnitWastedTimeMicroSec(unsigned unit) const;
  void* jobListSPURS{};
  idParallelJobList_Threads* jobListThreads{};
  const idColor* color{};
};

class idJobThread final : public idSysThread {
public:
  idJobThread() = default;
  ~idJobThread() override;
  void Start(core_t core, unsigned threadNumber);
  void AddJobList(idParallelJobList_Threads* list);
  int Run() override;

private:
  std::mutex queueMutex;
  std::vector<idParallelJobList_Threads*> queue;
  unsigned threadNum{};
};

class idParallelJobManager {
public:
  virtual ~idParallelJobManager() = default;
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual idParallelJobList* AllocJobList(jobListId_t, jobListPriority_t, unsigned, unsigned,
                                          const idColor*) = 0;
  virtual void FreeJobList(idParallelJobList*) = 0;
  virtual int GetNumJobLists() const = 0;
  virtual int GetNumFreeJobLists() const = 0;
  virtual idParallelJobList* GetJobList(int) = 0;
  virtual int GetNumProcessingUnits() const = 0;
  virtual void WaitForAllJobLists() = 0;
};

class idParallelJobManagerLocal final : public idParallelJobManager {
public:
  idParallelJobManagerLocal();
  ~idParallelJobManagerLocal() override;
  void Init() override;
  void Shutdown() override;
  idParallelJobList* AllocJobList(jobListId_t, jobListPriority_t, unsigned, unsigned,
                                  const idColor*) override;
  void FreeJobList(idParallelJobList*) override;
  int GetNumJobLists() const override;
  int GetNumFreeJobLists() const override;
  idParallelJobList* GetJobList(int) override;
  int GetNumProcessingUnits() const override;
  void WaitForAllJobLists() override;
  void Submit(idParallelJobList_Threads*, int);

private:
  std::vector<idParallelJobList*> jobLists;
  unsigned maxThreads{};
};

void SubmitJobList(idParallelJobList_Threads* jobList, int parallelism);
extern idParallelJobManagerLocal parallelJobManagerLocal;
extern idParallelJobManager* parallelJobManager;
static_assert(sizeof(idParallelJobList) == 24, "parallel job list ABI drift");
