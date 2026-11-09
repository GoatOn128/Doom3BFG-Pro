#include "engine/cm/collisionmodelmanager.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <chrono>

idCVar cm_drawMask("cm_drawMask", "-1", CVAR_INTEGER | CVAR_SYSTEM,
                   "Collision contents mask used by debug drawing");
idCVar cm_drawFilled("cm_drawFilled", "0", CVAR_BOOL | CVAR_SYSTEM,
                     "Draw filled collision polygons");
idCVar cm_drawNormals("cm_drawNormals", "0", CVAR_BOOL | CVAR_SYSTEM,
                      "Draw collision polygon normals");
idCVar cm_testCollision("cm_testCollision", "0", CVAR_BOOL | CVAR_SYSTEM,
                        "Enable collision query diagnostics");

void CM_DebugOutput(const idCollisionModelManager& manager) {
  idLib::Printf("collision models: %zu\n", manager.NumModels());
  for (std::size_t i = 0; i < manager.NumModels(); ++i) {
    const idCollisionModelLocal* model = manager.GetModel(static_cast<cmHandle_t>(i));
    if (!model) {
      continue;
    }
    const cmModelStats_t stats = model->GetPolytopeStats();
    idLib::Printf("  %zu: %s, %zu verts, %zu polygons, %zu bytes\n", i, model->GetName(),
                  stats.vertices, stats.polygons, stats.bytes);
  }
}

void DrawEdge(const idVec3& start, const idVec3& end, int index) {
  idLib::Printf("cm edge %d: %s -> %s\n", index, start.ToString(), end.ToString());
}

void DrawPolygon(const idCollisionModelLocal& model, const cmPolygon_t& polygon, int index) {
  idLib::Printf("cm polygon %d: %zu vertices, contents 0x%x\n", index, polygon.vertices.size(),
                polygon.contents);
  for (std::size_t edge = 0; edge < polygon.vertices.size(); ++edge) {
    const idVec3* start = model.GetVertex(polygon.vertices[edge]);
    const idVec3* end = model.GetVertex(polygon.vertices[(edge + 1) % polygon.vertices.size()]);
    if (start && end) {
      DrawEdge(*start, *end, static_cast<int>(edge));
    }
  }
}

void DrawNodePolygons(const idCollisionModelLocal& model) {
  for (std::size_t index = 0; index < model.polygons.size(); ++index) {
    DrawPolygon(model, model.polygons[index], static_cast<int>(index));
  }
}

void idCollisionModelManager::DrawCollisionModel(cmHandle_t handle) const {
  const idCollisionModelLocal* model = GetModel(handle);
  if (model) {
    DrawNodePolygons(*model);
  }
}

void idCollisionModelManager::DebugOutput() const {
  CM_DebugOutput(*this);
}

double SpeedTest(const idCollisionModelManager& manager, cmHandle_t handle, int iterations) {
  const auto startTime = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    const float offset = static_cast<float>(i & 7);
    (void)manager.Translation({-64.0f, offset, 0.0f}, {64.0f, offset, 0.0f}, nullptr, CONTENTS_ALL,
                              handle);
  }
  const auto elapsed =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime);
  return elapsed.count();
}

void DebugTranslationFailure(const queryParms_t& parms, const trace_t& trace) {
  idLib::Warning("translation failed: %s -> %s, fraction %.6f", parms.start.ToString(),
                 parms.end.ToString(), trace.fraction);
}

void DebugRotationFailure(const idRotation& rotation, const trace_t& trace) {
  idLib::Warning("rotation failed: angle %.3f, fraction %.6f", rotation.angle, trace.fraction);
}

void DebugFailedQuery(const queryParms_t& parms) {
  idLib::Warning("collision query failed: %s -> %s", parms.start.ToString(), parms.end.ToString());
}

void DebugTooBigQuery(const queryParms_t& parms, float maximumLength) {
  idLib::Warning("collision query length %.3f exceeds %.3f", (parms.end - parms.start).Length(),
                 maximumLength);
}
