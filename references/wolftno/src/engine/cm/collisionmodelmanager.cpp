#include "engine/cm/collisionmodelmanager.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <filesystem>

idCollisionModelManager collisionModelManagerLocal;
idCollisionModelManager* collisionModelManager = &collisionModelManagerLocal;

idCollisionModel* LoadCollisionModelByName(const char* path) {
  const cmHandle_t handle = collisionModelManager->LoadModel(path);
  return const_cast<idCollisionModelLocal*>(
      collisionModelManager->GetModel(handle));
}

void idQueryData::AdvanceSubModelQueryFirstSubmittedIndex() {
  firstSubmitted = subModelQueries;
}

void idQueryData::UpdateQueryDataStats() {
  timings.submitted = modelQueries + subModelQueries;
}

void idQueryData::EndFrame() {
  UpdateQueryDataStats();
  firstSubmitted = subModelQueries;
}

void idCollisionModelManager::Init() {
  if (models.empty()) {
    models.push_back(
        builder.BuildForBounds({{{-8.0f, -8.0f, -8.0f}, {8.0f, 8.0f, 8.0f}}}, "default"));
  }
}

void idCollisionModelManager::Shutdown() {
  models.clear();
}

cmHandle_t idCollisionModelManager::LoadModel(const char* path) {
  auto model = std::make_shared<idCollisionModelLocal>();
  if (!model->Load_Binary(path)) {
    return -1;
  }
  models.push_back(std::move(model));
  return static_cast<cmHandle_t>(models.size() - 1);
}

cmHandle_t idCollisionModelManager::ModelFromTrm(const idTraceModel& model, const char* name) {
  models.push_back(builder.BuildForTrm(model, name));
  return static_cast<cmHandle_t>(models.size() - 1);
}

cmHandle_t idCollisionModelManager::ModelFromGrid(const idBounds& bounds, const char* name) {
  models.push_back(builder.BuildForGrid(bounds, name));
  return static_cast<cmHandle_t>(models.size() - 1);
}

bool idCollisionModelManager::IsResident(cmHandle_t handle) const {
  return handle >= 0 && static_cast<std::size_t>(handle) < models.size() && models[handle];
}

const idCollisionModelLocal* idCollisionModelManager::GetModel(cmHandle_t handle) const {
  return IsResident(handle) ? models[handle].get() : nullptr;
}

trace_t idCollisionModelManager::LocalTranslation(const queryParms_t& parms,
                                                  cmHandle_t handle) const {
  const idCollisionModelLocal* model = GetModel(handle);
  if (!model) {
    trace_t result;
    result.endpos = parms.end;
    return result;
  }
  return idCollisionQueryExecute::ExecuteSubModelQuery(*model, parms);
}

trace_t idCollisionModelManager::Translation(const idVec3& start, const idVec3& end,
                                             const idTraceModel* traceModel,
                                             std::uint32_t contentsMask, cmHandle_t model) const {
  queryParms_t parms;
  parms.start = start;
  parms.end = end;
  parms.traceModel = traceModel;
  parms.contentsMask = contentsMask;
  return LocalTranslation(parms, model);
}

trace_t idCollisionModelManager::Rotation(const idVec3& origin, const idRotation& rotation,
                                          const idTraceModel* traceModel,
                                          std::uint32_t contentsMask, cmHandle_t handle) const {
  queryParms_t parms;
  parms.start = origin;
  parms.end = origin;
  parms.traceModel = traceModel;
  parms.contentsMask = contentsMask;
  const idCollisionModelLocal* model = GetModel(handle);
  if (!model) {
    trace_t result;
    result.endpos = origin;
    return result;
  }
  return idPolygonModelCollisionDetection::StartRotation(*model, parms, rotation);
}

trace_t idCollisionModelManager::Motion(const idVec3& start, const idVec3& end,
                                        const idRotation& rotation, const idTraceModel* traceModel,
                                        std::uint32_t contentsMask, cmHandle_t handle) const {
  const trace_t translation = Translation(start, end, traceModel, contentsMask, handle);
  const trace_t rotationTrace =
      Rotation(translation.endpos, rotation, traceModel, contentsMask, handle);
  return translation.fraction <= rotationTrace.fraction ? translation : rotationTrace;
}

int idCollisionModelManager::Contents(const idVec3& point, const idTraceModel* traceModel,
                                      std::uint32_t contentsMask, cmHandle_t handle) const {
  const idCollisionModelLocal* model = GetModel(handle);
  if (!model) {
    return CONTENTS_NONE;
  }
  queryParms_t parms;
  parms.start = point;
  parms.end = point;
  parms.traceModel = traceModel;
  parms.contentsMask = contentsMask;
  return idPolygonModelCollisionDetection::StartContents(*model, parms);
}

contactsResult_t idCollisionModelManager::Contacts(const queryParms_t& parms, float distance,
                                                   cmHandle_t handle) const {
  const idCollisionModelLocal* model = GetModel(handle);
  return model ? idPolygonModelCollisionDetection::StartContacts(*model, parms, distance)
               : contactsResult_t{};
}

clipResult_t idCollisionModelManager::Clip(const queryParms_t& parms, cmHandle_t handle) const {
  const idCollisionModelLocal* model = GetModel(handle);
  return model ? idPolygonModelCollisionDetection::StartClip(*model, parms) : clipResult_t{};
}

idCollisionGridState::idCollisionGridState() = default;
void idCollisionGridState::Create(std::size_t count) {
  active.assign(count, false);
}
void idCollisionGridState::Activate(std::size_t index) {
  if (index >= active.size()) {
    active.resize(index + 1, false);
  }
  active[index] = true;
}
void idCollisionGridState::Inactivate(std::size_t index) {
  if (index < active.size()) {
    active[index] = false;
  }
}
int idCollisionGridState::FirstActive() const {
  return NextActive(static_cast<std::size_t>(-1));
}
int idCollisionGridState::NextActive(std::size_t index) const {
  const std::size_t start = index == static_cast<std::size_t>(-1) ? 0 : index + 1;
  for (std::size_t current = start; current < active.size(); ++current) {
    if (active[current]) {
      return static_cast<int>(current);
    }
  }
  return -1;
}

bool TrmFromModel(const idCollisionModelLocal& model, idTraceModel& output) {
  if (model.GetType() == CM_TYPE_SPHERE) {
    output.SetupCylinder(model.GetBounds(), 16);
  } else {
    output.SetupBox(model.GetBounds());
  }
  return output.type != TRM_INVALID;
}

bool idTrmFromSubModel::TrmFromSubModel(const idCollisionModelLocal& model) {
  valid = ::TrmFromModel(model, traceModel);
  return valid;
}
bool idTrmFromSubModel::IsTrmFromSubModelValid() const {
  return valid;
}

cmHandle_t idCollisionModelManager::ModelFromSurface(const std::vector<idVec3>& surfaceVertices,
                                                     const std::vector<std::uint32_t>& indices,
                                                     const char* modelName) {
  if (surfaceVertices.empty()) {
    return -1;
  }
  idBounds surfaceBounds;
  surfaceBounds.Clear();
  for (const idVec3& vertex : surfaceVertices) {
    surfaceBounds.AddPoint(vertex);
  }
  auto model = builder.BuildForBounds(surfaceBounds, modelName);
  model->vertices = surfaceVertices;
  model->polygons.clear();
  for (std::size_t index = 0; index + 2 < indices.size(); index += 3) {
    if (indices[index] >= surfaceVertices.size() || indices[index + 1] >= surfaceVertices.size() ||
        indices[index + 2] >= surfaceVertices.size()) {
      continue;
    }
    cmPolygon_t polygon;
    polygon.vertices = {indices[index], indices[index + 1], indices[index + 2]};
    const idVec3& first = surfaceVertices[indices[index]];
    const idVec3& second = surfaceVertices[indices[index + 1]];
    const idVec3& third = surfaceVertices[indices[index + 2]];
    idVec3 normal = (second - first).Cross(third - first);
    normal.Normalize();
    polygon.plane = {normal.x, normal.y, normal.z, -(normal * first)};
    polygon.bounds.Clear();
    polygon.bounds.AddPoint(first);
    polygon.bounds.AddPoint(second);
    polygon.bounds.AddPoint(third);
    model->polygons.push_back(std::move(polygon));
  }
  models.push_back(std::move(model));
  return static_cast<cmHandle_t>(models.size() - 1);
}

cmHandle_t idCollisionModelManager::LoadGrid(const char* path) {
  const cmHandle_t handle = LoadModel(path);
  if (IsResident(handle)) {
    models[handle]->type = CM_TYPE_GRID;
  }
  return handle;
}

cmHandle_t idCollisionModelManager::StreamModel(const char* path) {
  return LoadModel(path);
}

int idCollisionModelManager::FindStreamArea(cmHandle_t handle, const idVec3& point) const {
  const idCollisionModelLocal* model = GetModel(handle);
  return model && CM_PointInsideBounds(point, model->GetBounds()) ? 0 : -1;
}

int idCollisionModelManager::LocalContents(const queryParms_t& parms, cmHandle_t model) const {
  return Contents(parms.start, parms.traceModel, parms.contentsMask, model);
}

contactsResult_t idCollisionModelManager::MotionContacts(const queryParms_t& parms, float distance,
                                                         cmHandle_t model) const {
  contactsResult_t start = Contacts(parms, distance, model);
  queryParms_t endParms = parms;
  endParms.start = parms.end;
  contactsResult_t end = Contacts(endParms, distance, model);
  return idCollisionDetectionMerge::MergeContactsResults({start, end});
}

trace_t StepMoveInternal(const idCollisionModelManager& manager, const queryParms_t& parms,
                         float stepHeight, cmHandle_t model) {
  const trace_t direct = manager.LocalTranslation(parms, model);
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
  const trace_t stepped =
      idCollisionDetectionMerge::MergeStepMoveResults(forwardTrace, upTrace, downTrace);
  return stepped.fraction > direct.fraction ? stepped : direct;
}

trace_t idCollisionModelManager::StepMove(const queryParms_t& parms, float stepHeight,
                                          cmHandle_t model) const {
  return StepMoveInternal(*this, parms, stepHeight, model);
}

contactsResult_t idCollisionModelManager::StepMoveContacts(const queryParms_t& parms,
                                                           float stepHeight,
                                                           cmHandle_t model) const {
  const trace_t trace = StepMove(parms, stepHeight, model);
  queryParms_t contactParms = parms;
  contactParms.start = trace.endpos;
  return Contacts(contactParms, 0.01f, model);
}

trace_t idCollisionModelManager::SlideMove(const queryParms_t& parms, int maxBumps,
                                           cmHandle_t model) const {
  slideMoveState_t state;
  idCollisionDetectionMerge::InitSlideMoveState(state, parms.start, parms.end - parms.start);
  trace_t last;
  queryParms_t current = parms;
  for (int bump = 0; bump < std::max(maxBumps, 1); ++bump) {
    current.start = state.position;
    current.end = state.position + state.velocity;
    last = LocalTranslation(current, model);
    if (!idCollisionDetectionMerge::UpdateSlideMoveState(state, last)) {
      break;
    }
  }
  return idCollisionDetectionMerge::FinishSlideMoveState(state, last);
}

contactsResult_t idCollisionModelManager::SlideMoveContacts(const queryParms_t& parms,
                                                            float distance,
                                                            cmHandle_t model) const {
  const trace_t slide = SlideMove(parms, 4, model);
  queryParms_t contactParms = parms;
  contactParms.start = slide.endpos;
  return Contacts(contactParms, distance, model);
}

bool idCollisionModelManager::TrmFromModel(cmHandle_t handle, idTraceModel& output) const {
  const idCollisionModelLocal* model = GetModel(handle);
  return model && ::TrmFromModel(*model, output);
}
bool idCollisionModelManager::IsTrmFromModelValid(cmHandle_t handle) const {
  idTraceModel output;
  return TrmFromModel(handle, output);
}
std::vector<idTraceModel> idCollisionModelManager::CompoundTrmFromModel(cmHandle_t handle) const {
  const idCollisionModelLocal* model = GetModel(handle);
  if (!model) {
    return {};
  }
  std::vector<idTraceModel> result;
  if (model->polytopes.empty()) {
    result.emplace_back(model->GetBounds());
  } else {
    for (const cmPolytope_t& polytope : model->polytopes) {
      result.emplace_back(polytope.bounds);
    }
  }
  return result;
}
bool idCollisionModelManager::IsCompundTrmFromModelValid(cmHandle_t handle) const {
  return !CompoundTrmFromModel(handle).empty();
}

trace_t idCollisionModelManager::GetTranslationResult(idCollisionQueryJobManager& jobs,
                                                      std::size_t query) const {
  return jobs.GetTranslationResult(query);
}
std::vector<trace_t>
idCollisionModelManager::GetMultiHitTranslationResult(idCollisionQueryJobManager& jobs,
                                                      std::size_t query) const {
  return jobs.GetMultiHitTranslationPointResult(query);
}
contactsResult_t idCollisionModelManager::GetMotionContactsResult(idCollisionQueryJobManager& jobs,
                                                                  std::size_t query) const {
  return jobs.GetMotionContactsResult(query);
}
contactsResult_t idCollisionModelManager::GetContactsResult(idCollisionQueryJobManager& jobs,
                                                            std::size_t query) const {
  return jobs.GetContactsResult(query);
}
clipResult_t idCollisionModelManager::GetClipResult(idCollisionQueryJobManager& jobs,
                                                    std::size_t query) const {
  return jobs.GetClipResult(query);
}
contactsResult_t
idCollisionModelManager::GetStepMoveContactsResult(idCollisionQueryJobManager& jobs,
                                                   std::size_t query) const {
  return jobs.GetStepMoveContactsResult(query);
}

void idCollisionModelManager::StartQueryFrame() {
  queryFrameActive = true;
}
void idCollisionModelManager::EndQueryFrame() {
  queryFrameActive = false;
}
void idCollisionModelManager::SubmitQueries(idCollisionQueryJobManager& jobs) const {
  jobs.SubmitQueries();
}
void idCollisionModelManager::WaitForAllQueries(idCollisionQueryJobManager& jobs) const {
  jobs.WaitForAllQueries();
}

std::size_t MultiTraceFindSlotSorted(std::vector<trace_t>& traces, const trace_t& trace,
                                     std::size_t maximum) {
  const auto position = std::lower_bound(
      traces.begin(), traces.end(), trace,
      [](const trace_t& first, const trace_t& second) { return first.fraction < second.fraction; });
  const std::size_t index = static_cast<std::size_t>(position - traces.begin());
  traces.insert(position, trace);
  if (traces.size() > maximum) {
    traces.resize(maximum);
  }
  return index < maximum ? index : maximum;
}

idStr CollisionModelInfo(const idCollisionModelLocal& model) {
  const cmModelStats_t stats = model.GetPolytopeStats();
  idStr result;
  result.Format("%s: %zu vertices, %zu edges, %zu polygons, %zu bytes", model.GetName(),
                stats.vertices, stats.edges, stats.polygons, stats.bytes);
  return result;
}
void PrintSubModelInfo(const idCollisionModelLocal& model) {
  idLib::Printf("submodel %s\n", CollisionModelInfo(model).data);
}
void PrintCollisionModelInfo(const idCollisionModelLocal& model) {
  idLib::Printf("collision model %s\n", CollisionModelInfo(model).data);
}
void ListCollisionModels(const idCollisionModelManager& manager) {
  for (std::size_t index = 0; index < manager.NumModels(); ++index) {
    const idCollisionModelLocal* model = manager.GetModel(static_cast<cmHandle_t>(index));
    if (model) {
      PrintCollisionModelInfo(*model);
    }
  }
}
void ListCollisionModels_f() {
  ListCollisionModels(collisionModelManagerLocal);
}
void CollisionModelInfo_f(cmHandle_t model) {
  const idCollisionModelLocal* local = collisionModelManagerLocal.GetModel(model);
  if (local) {
    PrintCollisionModelInfo(*local);
  }
}
bool ExportCollisionModel_f(cmHandle_t model, const char* path) {
  const idCollisionModelLocal* local = collisionModelManagerLocal.GetModel(model);
  return local && local->Write_Binary(path);
}
bool ExportDuplicateCollisionGeometryModel_f(cmHandle_t model, const char* path) {
  return ExportCollisionModel_f(model, path);
}
bool StripBCM_f(const char* source, const char* destination) {
  idCollisionModelLocal model;
  return model.Load_Binary(source) && model.Write_Binary(destination);
}
cmHandle_t BuildCollisionModelsForMap_f(const idBounds& mapBounds, const char* name) {
  return collisionModelManagerLocal.ModelFromGrid(mapBounds, name);
}
clipResult_t CreateSurfaceOnModel(const idCollisionModelLocal& model) {
  clipResult_t result;
  result.vertices = model.vertices;
  for (const cmPolygon_t& polygon : model.polygons) {
    for (std::size_t index = 2; index < polygon.vertices.size(); ++index) {
      result.indices.insert(result.indices.end(), {polygon.vertices[0], polygon.vertices[index - 1],
                                                   polygon.vertices[index]});
    }
  }
  return result;
}
