#include "engine/cm/collisionmodelmanager.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <chrono>

void contactsResult_t::CopyFrom(const contactsResult_t& other) {
  contacts = other.contacts;
}

clipResult_t& clipResult_t::operator=(const clipResult_t& other) {
  if (this != &other) {
    vertices = other.vertices;
    indices = other.indices;
  }
  return *this;
}

timings_t::timings_t() = default;

void idQueryData::Clear() {
  parms.clear();
  slideStates.clear();
  intermediateResults.clear();
  finalResults.clear();
  modelQueries = 0;
  subModelQueries = 0;
  firstSubmitted = 0;
  timings = {};
}

std::size_t idQueryData::AllocModelQuery() {
  return modelQueries++;
}
std::size_t idQueryData::AllocSubModelQuery() {
  return subModelQueries++;
}
queryParms_t& idQueryData::AllocQueryParms() {
  return parms.emplace_back();
}
slideMoveState_t& idQueryData::AllocSlideMoveState() {
  return slideStates.emplace_back();
}
cmJobResult_t& idQueryData::AllocIntermediateResults() {
  return intermediateResults.emplace_back();
}
cmJobResult_t& idQueryData::AllocFinalResult() {
  return finalResults.emplace_back();
}
cmJobResult_t CollisionExecuteJob(std::function<cmJobResult_t()> function) {
  return function ? function() : cmJobResult_t{};
}

cmJobResult_t CollisionMergeJob(const std::vector<cmJobResult_t>& inputs) {
  cmJobResult_t result;
  std::vector<trace_t> traces;
  std::vector<int> contents;
  std::vector<contactsResult_t> contacts;
  std::vector<clipResult_t> clips;
  for (const cmJobResult_t& input : inputs) {
    traces.push_back(input.trace);
    contents.push_back(input.contents);
    contacts.push_back(input.contacts);
    clips.push_back(input.clip);
    result.multiTrace.insert(result.multiTrace.end(), input.multiTrace.begin(),
                             input.multiTrace.end());
  }
  result.trace = idCollisionDetectionMerge::MergeQueryResults(traces);
  result.contents = idCollisionDetectionMerge::MergeContentsResults(contents);
  result.contacts = idCollisionDetectionMerge::MergeContactsResults(contacts);
  result.clip = idCollisionDetectionMerge::MergeClipResults(clips);
  return result;
}

idCollisionQueryJobManager::idCollisionQueryJobManager() = default;

void idCollisionQueryJobManager::Init() {
  active = true;
  data.Clear();
  jobs.clear();
  completed.clear();
  finished.clear();
  executeList.clear();
  mergeList.clear();
}

void idCollisionQueryJobManager::Shutdown() {
  WaitForAllQueries();
  std::lock_guard<std::mutex> lock(mutex);
  jobs.clear();
  completed.clear();
  finished.clear();
  executeList.clear();
  mergeList.clear();
  data.Clear();
  active = false;
}

std::size_t idCollisionQueryJobManager::Submit(std::function<cmJobResult_t()> function) {
  std::lock_guard<std::mutex> lock(mutex);
  active = true;
  const std::size_t index = jobs.size();
  jobs.push_back(std::async(std::launch::async, CollisionExecuteJob, std::move(function)));
  completed.emplace_back();
  finished.push_back(false);
  executeList.push_back(index);
  ++data.timings.submitted;
  return index;
}

std::size_t idCollisionQueryJobManager::AllocModelQuery() {
  return data.AllocModelQuery();
}

std::size_t idCollisionQueryJobManager::AllocFinishedFinalResult(const cmJobResult_t& result) {
  std::lock_guard<std::mutex> lock(mutex);
  completed.push_back(result);
  finished.push_back(true);
  jobs.emplace_back();
  return completed.size() - 1;
}

void idCollisionQueryJobManager::AddModelQueryToExecuteList(std::size_t query) {
  if (query < jobs.size() &&
      std::find(executeList.begin(), executeList.end(), query) == executeList.end()) {
    executeList.push_back(query);
  }
}

void idCollisionQueryJobManager::AddModelQueryToMergeList(std::size_t query) {
  if (query < jobs.size() &&
      std::find(mergeList.begin(), mergeList.end(), query) == mergeList.end()) {
    mergeList.push_back(query);
  }
}

void idCollisionQueryJobManager::CreateExecuteJobs() {
  executeList.erase(std::remove_if(executeList.begin(), executeList.end(),
                                   [this](std::size_t index) { return index >= jobs.size(); }),
                    executeList.end());
}

void idCollisionQueryJobManager::CreateMergeJobs() {
  mergeList.erase(std::remove_if(mergeList.begin(), mergeList.end(),
                                 [this](std::size_t index) { return index >= jobs.size(); }),
                  mergeList.end());
}

std::size_t
idCollisionQueryJobManager::SubmitTranslationQuery(const idCollisionModelManager& manager,
                                                   cmHandle_t model, const queryParms_t& parms) {
  return Submit([&manager, model, parms] {
    cmJobResult_t result;
    result.trace = manager.LocalTranslation(parms, model);
    return result;
  });
}

std::size_t idCollisionQueryJobManager::SubmitLocalTranslationQuery(
    const idCollisionModelManager& manager, cmHandle_t model, const queryParms_t& parms) {
  return SubmitTranslationQuery(manager, model, parms);
}

std::size_t idCollisionQueryJobManager::SubmitMultiHitTranslationPointQuery(
    const idCollisionModelManager& manager, cmHandle_t model, const queryParms_t& parms,
    std::size_t maxHits) {
  return Submit([&manager, model, parms, maxHits] {
    cmJobResult_t result;
    result.trace = manager.LocalTranslation(parms, model);
    result.multiTrace = idCollisionDetectionMerge::MergeMultiTraceResults({result.trace}, maxHits);
    return result;
  });
}

std::size_t idCollisionQueryJobManager::SubmitRotationQuery(const idCollisionModelManager& manager,
                                                            cmHandle_t model,
                                                            const queryParms_t& parms,
                                                            const idRotation& rotation) {
  return Submit([&manager, model, parms, rotation] {
    cmJobResult_t result;
    result.trace =
        manager.Rotation(parms.start, rotation, parms.traceModel, parms.contentsMask, model);
    return result;
  });
}

std::size_t idCollisionQueryJobManager::SubmitMotionQuery(const idCollisionModelManager& manager,
                                                          cmHandle_t model,
                                                          const queryParms_t& parms,
                                                          const idRotation& rotation) {
  return Submit([&manager, model, parms, rotation] {
    cmJobResult_t result;
    result.trace = manager.Motion(parms.start, parms.end, rotation, parms.traceModel,
                                  parms.contentsMask, model);
    return result;
  });
}

std::size_t idCollisionQueryJobManager::SubmitContactsQuery(const idCollisionModelManager& manager,
                                                            cmHandle_t model,
                                                            const queryParms_t& parms,
                                                            float distance) {
  return Submit([&manager, model, parms, distance] {
    cmJobResult_t result;
    result.contacts = manager.Contacts(parms, distance, model);
    return result;
  });
}

std::size_t
idCollisionQueryJobManager::SubmitMotionContactsQuery(const idCollisionModelManager& manager,
                                                      cmHandle_t model, const queryParms_t& parms,
                                                      float distance) {
  return SubmitContactsQuery(manager, model, parms, distance);
}

std::size_t idCollisionQueryJobManager::SubmitContentsQuery(const idCollisionModelManager& manager,
                                                            cmHandle_t model,
                                                            const queryParms_t& parms) {
  return Submit([&manager, model, parms] {
    cmJobResult_t result;
    result.contents = manager.Contents(parms.start, parms.traceModel, parms.contentsMask, model);
    return result;
  });
}

std::size_t
idCollisionQueryJobManager::SubmitLocalContentsQuery(const idCollisionModelManager& manager,
                                                     cmHandle_t model, const queryParms_t& parms) {
  return SubmitContentsQuery(manager, model, parms);
}

std::size_t idCollisionQueryJobManager::SubmitClipQuery(const idCollisionModelManager& manager,
                                                        cmHandle_t model,
                                                        const queryParms_t& parms) {
  return Submit([&manager, model, parms] {
    cmJobResult_t result;
    result.clip = manager.Clip(parms, model);
    return result;
  });
}

std::size_t idCollisionQueryJobManager::SubmitStepMoveQuery(const idCollisionModelManager& manager,
                                                            cmHandle_t model,
                                                            const queryParms_t& parms,
                                                            float stepHeight) {
  return Submit([&manager, model, parms, stepHeight] {
    cmJobResult_t result;
    queryParms_t up = parms;
    up.end = parms.start + idVec3{0, 0, stepHeight};
    const trace_t upTrace = manager.LocalTranslation(up, model);
    queryParms_t forward = parms;
    forward.start = upTrace.endpos;
    forward.end = parms.end + idVec3{0, 0, stepHeight};
    const trace_t forwardTrace = manager.LocalTranslation(forward, model);
    queryParms_t down = parms;
    down.start = forwardTrace.endpos;
    down.end = forwardTrace.endpos - idVec3{0, 0, stepHeight};
    const trace_t downTrace = manager.LocalTranslation(down, model);
    result.trace =
        idCollisionDetectionMerge::MergeStepMoveResults(forwardTrace, upTrace, downTrace);
    return result;
  });
}

std::size_t
idCollisionQueryJobManager::SubmitStepMoveContactsQuery(const idCollisionModelManager& manager,
                                                        cmHandle_t model, const queryParms_t& parms,
                                                        float stepHeight) {
  return Submit([&manager, model, parms, stepHeight] {
    cmJobResult_t result;
    result.contacts = manager.Contacts(parms, stepHeight, model);
    return result;
  });
}

std::size_t idCollisionQueryJobManager::SubmitSlideMoveQuery(const idCollisionModelManager& manager,
                                                             cmHandle_t model,
                                                             const queryParms_t& parms) {
  return SubmitTranslationQuery(manager, model, parms);
}

std::size_t idCollisionQueryJobManager::SubmitSlideMoveContactsQuery(
    const idCollisionModelManager& manager, cmHandle_t model, const queryParms_t& parms,
    float distance) {
  return SubmitContactsQuery(manager, model, parms, distance);
}

cmJobResult_t idCollisionQueryJobManager::Get(std::size_t query) {
  std::lock_guard<std::mutex> lock(mutex);
  if (query >= jobs.size()) {
    return {};
  }
  if (!finished[query]) {
    if (!jobs[query].valid()) {
      return {};
    }
    const auto start = std::chrono::steady_clock::now();
    completed[query] = jobs[query].get();
    data.timings.executeMilliseconds +=
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    finished[query] = true;
    ++data.timings.completed;
  }
  return completed[query];
}

trace_t idCollisionQueryJobManager::GetTranslationResult(std::size_t query) {
  return Get(query).trace;
}
std::vector<trace_t>
idCollisionQueryJobManager::GetMultiHitTranslationPointResult(std::size_t query) {
  return Get(query).multiTrace;
}
contactsResult_t idCollisionQueryJobManager::GetMotionContactsResult(std::size_t query) {
  return Get(query).contacts;
}
contactsResult_t idCollisionQueryJobManager::GetContactsResult(std::size_t query) {
  return Get(query).contacts;
}
clipResult_t idCollisionQueryJobManager::GetClipResult(std::size_t query) {
  return Get(query).clip;
}
contactsResult_t idCollisionQueryJobManager::GetStepMoveContactsResult(std::size_t query) {
  return Get(query).contacts;
}

void idCollisionQueryJobManager::SubmitQueries() {
  CreateExecuteJobs();
  CreateMergeJobs();
  data.UpdateQueryDataStats();
}

void idCollisionQueryJobManager::WaitForQueryResults(std::size_t query) {
  if (query < jobs.size() && jobs[query].valid()) {
    jobs[query].wait();
  }
}

void idCollisionQueryJobManager::WaitForAllQueries() {
  for (std::size_t query = 0; query < jobs.size(); ++query) {
    WaitForQueryResults(query);
  }
}

void idCollisionQueryJobManager::ShowDebugInfo() const {
  idLib::Printf("cm jobs: %zu submitted, %llu completed, %.3f ms\n", jobs.size(),
                static_cast<unsigned long long>(data.timings.completed),
                data.timings.executeMilliseconds);
}
