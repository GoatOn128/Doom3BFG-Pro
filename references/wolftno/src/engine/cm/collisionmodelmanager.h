#pragma once

#include "engine/cm/collisiongrid.h"
#include "engine/cm/collisionmodelbuilder.h"
#include "engine/cm/collisionquery.h"

#include <future>
#include <map>

using cmHandle_t = int;

class idCollisionQueryJobManager;
class idCollisionQuery;
class idPositionedCollisionModel;
class idRenderModelSurface;

class idCollisionGridState {
public:
  idCollisionGridState();
  void Create(std::size_t count);
  void Activate(std::size_t index);
  void Inactivate(std::size_t index);
  int FirstActive() const;
  int NextActive(std::size_t index) const;

private:
  std::vector<bool> active;
};

class idTrmFromSubModel {
public:
  bool TrmFromSubModel(const idCollisionModelLocal& model);
  bool IsTrmFromSubModelValid() const;
  const idTraceModel& Get() const { return traceModel; }

private:
  idTraceModel traceModel;
  bool valid{};
};

struct timings_t {
  std::uint64_t submitted{};
  std::uint64_t completed{};
  double executeMilliseconds{};
  timings_t();
};

struct cmJobResult_t {
  trace_t trace;
  contactsResult_t contacts;
  clipResult_t clip;
  std::vector<trace_t> multiTrace;
  int contents{};
};

class idQueryData {
public:
  void Clear();
  std::size_t AllocModelQuery();
  std::size_t AllocSubModelQuery();
  queryParms_t& AllocQueryParms();
  slideMoveState_t& AllocSlideMoveState();
  cmJobResult_t& AllocIntermediateResults();
  cmJobResult_t& AllocFinalResult();
  void AdvanceSubModelQueryFirstSubmittedIndex();
  void UpdateQueryDataStats();
  void EndFrame();

  std::vector<queryParms_t> parms;
  std::vector<slideMoveState_t> slideStates;
  std::vector<cmJobResult_t> intermediateResults;
  std::vector<cmJobResult_t> finalResults;
  std::size_t modelQueries{};
  std::size_t subModelQueries{};
  std::size_t firstSubmitted{};
  timings_t timings;
};

class idCollisionModelManager {
public:
  void Init();
  void Shutdown();
  cmHandle_t LoadModel(const char* path);
  cmHandle_t ModelFromTrm(const idTraceModel& model, const char* name = "traceModel");
  cmHandle_t ModelFromGrid(const idBounds& bounds, const char* name = "grid");
  cmHandle_t ModelFromSurface(const std::vector<idVec3>& vertices,
                              const std::vector<std::uint32_t>& indices,
                              const char* name = "surface");
  idCollisionModel* ModelFromSurface(
      const char* name, const idRenderModelSurface** surfaces,
      int numSurfaces, bool writeBinary);
  cmHandle_t LoadGrid(const char* path);
  cmHandle_t StreamModel(const char* path);
  int FindStreamArea(cmHandle_t model, const idVec3& point) const;
  bool IsResident(cmHandle_t handle) const;
  const idCollisionModelLocal* GetModel(cmHandle_t handle) const;

  trace_t Translation(const idVec3& start, const idVec3& end, const idTraceModel* traceModel,
                      std::uint32_t contentsMask, cmHandle_t model) const;
  trace_t LocalTranslation(const queryParms_t& parms, cmHandle_t model) const;
  trace_t Rotation(const idVec3& origin, const idRotation& rotation, const idTraceModel* traceModel,
                   std::uint32_t contentsMask, cmHandle_t model) const;
  trace_t Motion(const idVec3& start, const idVec3& end, const idRotation& rotation,
                 const idTraceModel* traceModel, std::uint32_t contentsMask,
                 cmHandle_t model) const;
  idCollisionQuery Motion(
      trace_t* result, const idVec3& start, const idVec3& end,
      const idRotation& rotation, const idBounds& globalBounds,
      const idTraceModel** const traceModels, int numTraceModels,
      const idMat3& traceModelAxis, int contentsMask,
      const idPositionedCollisionModel* models, int numModels,
      const char* userName);
  int Contents(const idVec3& point, const idTraceModel* traceModel, std::uint32_t contentsMask,
               cmHandle_t model) const;
  contactsResult_t Contacts(const queryParms_t& parms, float distance, cmHandle_t model) const;
  clipResult_t Clip(const queryParms_t& parms, cmHandle_t model) const;
  int LocalContents(const queryParms_t& parms, cmHandle_t model) const;
  contactsResult_t MotionContacts(const queryParms_t& parms, float distance,
                                  cmHandle_t model) const;
  trace_t StepMove(const queryParms_t& parms, float stepHeight, cmHandle_t model) const;
  contactsResult_t StepMoveContacts(const queryParms_t& parms, float stepHeight,
                                    cmHandle_t model) const;
  trace_t SlideMove(const queryParms_t& parms, int maxBumps, cmHandle_t model) const;
  contactsResult_t SlideMoveContacts(const queryParms_t& parms, float distance,
                                     cmHandle_t model) const;
  bool TrmFromModel(cmHandle_t model, idTraceModel& output) const;
  bool IsTrmFromModelValid(cmHandle_t model) const;
  std::vector<idTraceModel> CompoundTrmFromModel(cmHandle_t model) const;
  bool IsCompundTrmFromModelValid(cmHandle_t model) const;

  trace_t GetTranslationResult(idCollisionQueryJobManager& jobs, std::size_t query) const;
  std::vector<trace_t> GetMultiHitTranslationResult(idCollisionQueryJobManager& jobs,
                                                    std::size_t query) const;
  contactsResult_t GetMotionContactsResult(idCollisionQueryJobManager& jobs,
                                           std::size_t query) const;
  contactsResult_t GetContactsResult(idCollisionQueryJobManager& jobs, std::size_t query) const;
  clipResult_t GetClipResult(idCollisionQueryJobManager& jobs, std::size_t query) const;
  contactsResult_t GetStepMoveContactsResult(idCollisionQueryJobManager& jobs,
                                             std::size_t query) const;
  bool GetStepMoveResult(trace_t* result, idCollisionQuery& query, bool peek);

  void StartQueryFrame();
  void EndQueryFrame();
  void SubmitQueries(idCollisionQueryJobManager& jobs) const;
  void WaitForAllQueries(idCollisionQueryJobManager& jobs) const;
  void WaitForAllQueries();
  std::size_t NumModels() const { return models.size(); }
  void DrawCollisionModel(cmHandle_t handle) const;
  void DebugOutput() const;

private:
  idCollisionModelBuilder builder;
  std::vector<std::shared_ptr<idCollisionModelLocal>> models;
  bool queryFrameActive{};
};

class idCollisionQueryJobManager {
public:
  idCollisionQueryJobManager();
  void Init();
  void Shutdown();
  std::size_t AllocModelQuery();
  std::size_t AllocFinishedFinalResult(const cmJobResult_t& result);
  void AddModelQueryToExecuteList(std::size_t query);
  void AddModelQueryToMergeList(std::size_t query);
  void CreateExecuteJobs();
  void CreateMergeJobs();
  std::size_t SubmitTranslationQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                     const queryParms_t& parms);
  std::size_t SubmitLocalTranslationQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                          const queryParms_t& parms);
  std::size_t SubmitMultiHitTranslationPointQuery(const idCollisionModelManager& manager,
                                                  cmHandle_t model, const queryParms_t& parms,
                                                  std::size_t maxHits);
  std::size_t SubmitRotationQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                  const queryParms_t& parms, const idRotation& rotation);
  std::size_t SubmitMotionQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                const queryParms_t& parms, const idRotation& rotation);
  std::size_t SubmitMotionContactsQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                        const queryParms_t& parms, float distance);
  std::size_t SubmitContentsQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                  const queryParms_t& parms);
  std::size_t SubmitLocalContentsQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                       const queryParms_t& parms);
  std::size_t SubmitContactsQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                  const queryParms_t& parms, float distance);
  std::size_t SubmitClipQuery(const idCollisionModelManager& manager, cmHandle_t model,
                              const queryParms_t& parms);
  std::size_t SubmitStepMoveQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                  const queryParms_t& parms, float stepHeight);
  std::size_t SubmitStepMoveContactsQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                          const queryParms_t& parms, float stepHeight);
  std::size_t SubmitSlideMoveQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                   const queryParms_t& parms);
  std::size_t SubmitSlideMoveContactsQuery(const idCollisionModelManager& manager, cmHandle_t model,
                                           const queryParms_t& parms, float distance);
  trace_t GetTranslationResult(std::size_t query);
  std::vector<trace_t> GetMultiHitTranslationPointResult(std::size_t query);
  contactsResult_t GetMotionContactsResult(std::size_t query);
  contactsResult_t GetContactsResult(std::size_t query);
  clipResult_t GetClipResult(std::size_t query);
  contactsResult_t GetStepMoveContactsResult(std::size_t query);
  void SubmitQueries();
  void WaitForQueryResults(std::size_t query);
  void WaitForAllQueries();
  void ShowDebugInfo() const;

private:
  std::size_t Submit(std::function<cmJobResult_t()> function);
  cmJobResult_t Get(std::size_t query);
  bool active{};
  std::mutex mutex;
  idQueryData data;
  std::vector<std::future<cmJobResult_t>> jobs;
  std::vector<cmJobResult_t> completed;
  std::vector<bool> finished;
  std::vector<std::size_t> executeList;
  std::vector<std::size_t> mergeList;
};

cmJobResult_t CollisionExecuteJob(std::function<cmJobResult_t()> function);
cmJobResult_t CollisionMergeJob(const std::vector<cmJobResult_t>& inputs);

extern idCollisionModelManager collisionModelManagerLocal;
extern idCollisionModelManager* collisionModelManager;

trace_t StepMoveInternal(const idCollisionModelManager& manager, const queryParms_t& parms,
                         float stepHeight, cmHandle_t model);
bool TrmFromModel(const idCollisionModelLocal& model, idTraceModel& output);
std::size_t MultiTraceFindSlotSorted(std::vector<trace_t>& traces, const trace_t& trace,
                                     std::size_t maximum);
void PrintSubModelInfo(const idCollisionModelLocal& model);
void PrintCollisionModelInfo(const idCollisionModelLocal& model);
idStr CollisionModelInfo(const idCollisionModelLocal& model);
void ListCollisionModels(const idCollisionModelManager& manager);
void ListCollisionModels_f();
void CollisionModelInfo_f(cmHandle_t model);
bool ExportCollisionModel_f(cmHandle_t model, const char* path);
bool ExportDuplicateCollisionGeometryModel_f(cmHandle_t model, const char* path);
bool StripBCM_f(const char* source, const char* destination);
cmHandle_t BuildCollisionModelsForMap_f(const idBounds& mapBounds, const char* name);
clipResult_t CreateSurfaceOnModel(const idCollisionModelLocal& model);
