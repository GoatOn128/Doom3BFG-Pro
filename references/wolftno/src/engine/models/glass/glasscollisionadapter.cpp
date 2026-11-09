#include "engine/cm/collisionmodelmanager.h"

#include "engine/renderer/triangles.h"

idCollisionModel* idCollisionModelManager::ModelFromSurface(
    const char* modelName, const idRenderModelSurface** surfaces,
    int numSurfaces, bool) {
  std::vector<idVec3> vertices;
  std::vector<std::uint32_t> indices;
  for (int surfaceIndex = 0; surfaceIndex < numSurfaces; ++surfaceIndex) {
    const idRenderModelSurface* surface =
        surfaces != nullptr ? surfaces[surfaceIndex] : nullptr;
    const idTriangles* geometry =
        surface != nullptr
            ? *reinterpret_cast<idTriangles* const*>(
                  reinterpret_cast<const unsigned char*>(surface) + 24)
            : nullptr;
    if (geometry == nullptr)
      continue;
    const std::uint32_t vertexBase =
        static_cast<std::uint32_t>(vertices.size());
    vertices.reserve(vertices.size() + geometry->numVerts);
    for (int vertexIndex = 0; vertexIndex < geometry->numVerts; ++vertexIndex)
      vertices.push_back(geometry->verts[vertexIndex].xyz);
    indices.reserve(indices.size() + geometry->numIndexes);
    for (int index = 0; index < geometry->numIndexes; ++index)
      indices.push_back(vertexBase + geometry->indexes[index]);
  }
  const cmHandle_t handle = ModelFromSurface(vertices, indices, modelName);
  return const_cast<idCollisionModelLocal*>(GetModel(handle));
}

idCollisionModel* BuildGlassCollisionModel(
    const char* name, const idRenderModelSurface** surfaces, int numSurfaces,
    bool writeBinary) {
  return collisionModelManager != nullptr
             ? collisionModelManager->ModelFromSurface(
                   name, surfaces, numSurfaces, writeBinary)
             : nullptr;
}
