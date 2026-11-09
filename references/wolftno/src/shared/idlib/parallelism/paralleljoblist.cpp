#include "shared/idlib/parallelism/paralleljoblist.h"

#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace {
struct registeredJob {
  jobRun_t function;
  const char* name;
};
std::mutex registrationMutex;
std::vector<registeredJob> registeredJobs;
std::uint64_t Now() {
  return static_cast<std::uint64_t>(Sys_Microseconds());
}
} // namespace
void RegisterJob(jobRun_t function, const char* name) {
  std::lock_guard<std::mutex> lock(registrationMutex);
  registeredJobs.push_back({function, name});
}

idParallelJobList_Threads::idParallelJobList_Threads(jobListId_t id, jobListPriority_t priority,
                                                     unsigned jobsMaximum, unsigned syncMaximum)
    : listId(id), listPriority(priority), maxJobs(jobsMaximum), maxSyncs(syncMaximum) {
  jobs.reserve(maxJobs);
}
idParallelJobList_Threads::~idParallelJobList_Threads() {
  Wait();
}
void idParallelJobList_Threads::AddJob(jobRun_t function, std::uint64_t data) {
  if (submitted || !function || jobs.size() >= maxJobs)
    return;
  jobs.push_back({function, reinterpret_cast<void*>(data), 0, SYNC_NONE});
}
void idParallelJobList_Threads::InsertSyncPoint(jobSyncType_t type) {
  if (submitted || type == SYNC_NONE || numSyncs >= maxSyncs)
    return;
  jobs.push_back({nullptr, nullptr, 0, type});
  ++numSyncs;
}
int idParallelJobList_Threads::RunJobsInternal(unsigned threadNum, threadJobListState_t* state,
                                               bool singleJob) {
  int executed = 0;
  while (true) {
    const unsigned index = nextJob.fetch_add(1);
    if (index >= jobs.size())
      break;
    job_t& job = jobs[index];
    if (job.sync != SYNC_NONE) {
      ++stats.numExecutedSyncs;
      continue;
    }
    const auto start = Now();
    job.function(job.data);
    job.executed = 1;
    ++stats.numExecutedJobs;
    ++executed;
    stats.threadExecTime[threadNum % 32] += Now() - start;
    if (state)
      state->nextJobIndex = static_cast<int>(index + 1);
    if (singleJob)
      break;
  }
  return executed;
}
void idParallelJobList_Threads::Submit(idParallelJobList_Threads* waitFor,
                                       int requestedParallelism) {
  if (submitted.exchange(true))
    return;
  if (waitFor)
    waitFor->Wait();
  done = false;
  nextJob = 0;
  stats = {};
  stats.submitTime = Now();
  parallelism =
      std::clamp(requestedParallelism <= 0 ? static_cast<int>(std::thread::hardware_concurrency())
                                           : requestedParallelism,
                 1, 32);
  work = std::async(std::launch::async, [this] {
    stats.startTime = Now();
    std::vector<std::thread> workers;
    for (int i = 1; i < parallelism; ++i)
      workers.emplace_back([this, i] {
        const auto start = Now();
        RunJobsInternal(i, nullptr, false);
        stats.threadTotalTime[i] = Now() - start;
      });
    const auto start = Now();
    RunJobsInternal(0, nullptr, false);
    stats.threadTotalTime[0] = Now() - start;
    for (auto& worker : workers)
      worker.join();
    stats.endTime = Now();
    done = true;
  });
}
void idParallelJobList_Threads::Wait() {
  if (!submitted)
    return;
  const auto start = Now();
  if (work.valid())
    work.wait();
  stats.waitTime += Now() - start;
  done = true;
}
bool idParallelJobList_Threads::TryWait() {
  if (!submitted)
    return true;
  if (done)
    return true;
  if (work.valid() && work.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    done = true;
    return true;
  }
  return false;
}
std::uint64_t idParallelJobList_Threads::GetTotalWastedTimeMicroSec() const {
  std::uint64_t total = 0;
  for (int i = 0; i < 32; ++i)
    if (stats.threadTotalTime[i] > stats.threadExecTime[i])
      total += stats.threadTotalTime[i] - stats.threadExecTime[i];
  return total;
}

idParallelJobList::idParallelJobList(jobListId_t id, jobListPriority_t priority, unsigned maxJobs,
                                     unsigned maxSyncs, const idColor* listColor)
    : jobListThreads(new idParallelJobList_Threads(id, priority, maxJobs, maxSyncs)),
      color(listColor) {}
idParallelJobList::~idParallelJobList() {
  delete jobListThreads;
}
void idParallelJobList::AddJob(jobRun_t function, std::uint64_t data) {
  jobListThreads->AddJob(function, data);
}

void idParallelJobList::AddJob(jobRun_t function, void* data) {
  jobListThreads->AddJob(function, reinterpret_cast<std::uint64_t>(data));
}
void idParallelJobList::InsertSyncPoint(jobSyncType_t type) {
  jobListThreads->InsertSyncPoint(type);
}
void idParallelJobList::Submit(idParallelJobList* waitFor, int parallelism) {
  jobListThreads->Submit(waitFor ? waitFor->jobListThreads : nullptr, parallelism);
}
void idParallelJobList::Wait() {
  jobListThreads->Wait();
}
bool idParallelJobList::TryWait() {
  return jobListThreads->TryWait();
}
bool idParallelJobList::IsSubmitted() const {
  return jobListThreads->IsSubmitted();
}
int idParallelJobList::GetNumExecutedJobs() const {
  return static_cast<int>(jobListThreads->stats.numExecutedJobs);
}
int idParallelJobList::GetNumSyncs() const {
  return static_cast<int>(jobListThreads->numSyncs);
}
std::uint64_t idParallelJobList::GetSubmitTimeMicroSec() const {
  return jobListThreads->stats.submitTime;
}
std::uint64_t idParallelJobList::GetStartTimeMicroSec() const {
  return jobListThreads->stats.startTime;
}
std::uint64_t idParallelJobList::GetFinishTimeMicroSec() const {
  return jobListThreads->stats.endTime;
}
std::uint64_t idParallelJobList::GetWaitTimeMicroSec() const {
  return jobListThreads->stats.waitTime;
}
std::uint64_t idParallelJobList::GetTotalProcessingTimeMicroSec() const {
  return jobListThreads->stats.endTime > jobListThreads->stats.startTime
             ? jobListThreads->stats.endTime - jobListThreads->stats.startTime
             : 0;
}
std::uint64_t idParallelJobList::GetAvgTotalProcessingTimeMicroSec() const {
  const auto count = GetNumExecutedJobs();
  return count ? GetTotalProcessingTimeMicroSec() / count : 0;
}
std::uint64_t idParallelJobList::GetTotalWastedTimeMicroSec() const {
  return jobListThreads->GetTotalWastedTimeMicroSec();
}
std::uint64_t idParallelJobList::GetUnitProcessingTimeMicroSec(unsigned unit) const {
  return unit < 32 ? jobListThreads->stats.threadExecTime[unit] : 0;
}
std::uint64_t idParallelJobList::GetUnitWastedTimeMicroSec(unsigned unit) const {
  return unit < 32 && jobListThreads->stats.threadTotalTime[unit] >
                          jobListThreads->stats.threadExecTime[unit]
             ? jobListThreads->stats.threadTotalTime[unit] -
                   jobListThreads->stats.threadExecTime[unit]
             : 0;
}

idJobThread::~idJobThread() {
  StopThread(true);
}
void idJobThread::Start(core_t core, unsigned number) {
  threadNum = number;
  StartWorkerThread("idJobThread", core, THREAD_NORMAL);
}
void idJobThread::AddJobList(idParallelJobList_Threads* list) {
  std::lock_guard<std::mutex> lock(queueMutex);
  queue.push_back(list);
  SignalWork();
}
int idJobThread::Run() {
  std::vector<idParallelJobList_Threads*> local;
  {
    std::lock_guard<std::mutex> lock(queueMutex);
    local.swap(queue);
  }
  for (auto* list : local)
    list->RunJobsInternal(threadNum, nullptr, false);
  return static_cast<int>(local.size());
}

idParallelJobManagerLocal parallelJobManagerLocal;
idParallelJobManager* parallelJobManager = &parallelJobManagerLocal;
idParallelJobManagerLocal::idParallelJobManagerLocal() {
  Init();
}
idParallelJobManagerLocal::~idParallelJobManagerLocal() {
  Shutdown();
}
void idParallelJobManagerLocal::Init() {
  maxThreads = std::clamp(std::thread::hardware_concurrency(), 1u, 32u);
}
void idParallelJobManagerLocal::Shutdown() {
  WaitForAllJobLists();
  for (auto* list : jobLists)
    delete list;
  jobLists.clear();
}
idParallelJobList* idParallelJobManagerLocal::AllocJobList(jobListId_t id,
                                                           jobListPriority_t priority,
                                                           unsigned maxJobs, unsigned maxSyncs,
                                                           const idColor* color) {
  if (jobLists.size() >= 64)
    return nullptr;
  auto* list = new idParallelJobList(id, priority, maxJobs, maxSyncs, color);
  jobLists.push_back(list);
  return list;
}
void idParallelJobManagerLocal::FreeJobList(idParallelJobList* list) {
  if (!list)
    return;
  list->Wait();
  const auto it = std::find(jobLists.begin(), jobLists.end(), list);
  if (it != jobLists.end())
    jobLists.erase(it);
  delete list;
}
int idParallelJobManagerLocal::GetNumJobLists() const {
  return static_cast<int>(jobLists.size());
}
int idParallelJobManagerLocal::GetNumFreeJobLists() const {
  return 64 - GetNumJobLists();
}
idParallelJobList* idParallelJobManagerLocal::GetJobList(int index) {
  return index >= 0 && index < GetNumJobLists() ? jobLists[index] : nullptr;
}
int idParallelJobManagerLocal::GetNumProcessingUnits() const {
  return static_cast<int>(maxThreads);
}
void idParallelJobManagerLocal::WaitForAllJobLists() {
  for (auto* list : jobLists)
    list->Wait();
}
void idParallelJobManagerLocal::Submit(idParallelJobList_Threads* list, int count) {
  list->Submit(nullptr, count);
}
void SubmitJobList(idParallelJobList_Threads* list, int parallelism) {
  parallelJobManagerLocal.Submit(list, parallelism);
}
