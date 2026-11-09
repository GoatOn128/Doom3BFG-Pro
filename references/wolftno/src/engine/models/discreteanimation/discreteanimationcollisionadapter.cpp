#include "engine/cm/collisionmodelmanager.h"

#include <cstdint>
#include <vector>

idCollisionModel* BuildDiscreteAnimationCollisionModel(
    const char* name, const idVec3* vertices, int numVertices,
    const std::uint32_t* indexes, int numIndexes, idTraceModel* traceModel) {
  if (collisionModelManager == nullptr || vertices == nullptr ||
      numVertices <= 0 || indexes == nullptr || numIndexes <= 0) {
    return nullptr;
  }

  std::vector<idVec3> vertexList(vertices, vertices + numVertices);
  std::vector<std::uint32_t> indexList(indexes, indexes + numIndexes);
  const cmHandle_t handle = collisionModelManager->ModelFromSurface(
      vertexList, indexList, name != nullptr ? name : "discreteanimation");
  if (handle < 0)
    return nullptr;
  if (traceModel != nullptr &&
      !collisionModelManager->TrmFromModel(handle, *traceModel)) {
    return nullptr;
  }
  return const_cast<idCollisionModelLocal*>(
      collisionModelManager->GetModel(handle));
}

bool WriteDiscreteAnimationCollisionModel(
    const char* name, const idVec3* vertices, int numVertices,
    const std::uint32_t* indexes, int numIndexes) {
  idTraceModel unusedTrace;
  auto* model = static_cast<idCollisionModelLocal*>(
      BuildDiscreteAnimationCollisionModel(name, vertices, numVertices,
                                           indexes, numIndexes, &unusedTrace));
  return model != nullptr && name != nullptr && model->Write_Binary(name);
}
