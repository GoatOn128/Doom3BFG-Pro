#include "engine/cm/collisionquery.h"

#include <atomic>

namespace {
std::atomic<std::size_t> traceWorkAllocations{};
}

void idTraceWork::Init(const queryParms_t& source) {
  parms = source;
  contacts.clear();
  sweptBounds.Clear();
  sweptBounds.AddPoint(source.start);
  sweptBounds.AddPoint(source.end);
  if (source.traceModel) {
    sweptBounds[0] += source.traceModel->bounds[0];
    sweptBounds[1] += source.traceModel->bounds[1];
  }
}

void idModelCheckCounts::SetupForSubModel(const idCollisionModelLocal& model) {
  const cmModelStats_t stats = model.GetPolytopeStats();
  vertices = stats.vertices;
  edges = stats.edges;
  polygons = stats.polygons;
}

std::unique_ptr<idTraceWork>
idPolygonModelCollisionDetection::AllocTraceWork(const queryParms_t& parms) {
  ++traceWorkAllocations;
  auto work = std::make_unique<idTraceWork>();
  work->Init(parms);
  return work;
}

std::size_t idPolygonModelCollisionDetection::GetTraceWorkSPUSize() {
  return sizeof(idTraceWork);
}

std::size_t CM_GetTraceWorkAllocationCount() {
  return traceWorkAllocations.load();
}

std::unique_ptr<queryParms_t> CM_AllocTraceWork(const queryParms_t& parms) {
  ++traceWorkAllocations;
  return std::make_unique<queryParms_t>(parms);
}
