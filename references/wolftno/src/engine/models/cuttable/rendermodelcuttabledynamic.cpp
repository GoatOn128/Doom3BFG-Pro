#include "engine/models/cuttable/rendermodelcuttabledynamic.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr unsigned kRenderVertexMask = 31U;
constexpr unsigned kDepthSTMask = 0x2400U;
constexpr int kMapWrite = 3;

} // namespace

idRenderModelCuttableDynamic::idRenderModelCuttableDynamic() {
  renderModelType = RENDERMODEL_CUTTABLEDYNAMIC;
  FreeSurfaces();

  idRenderModelSurface renderSurface{};
  renderSurface.geometry = new idTriangles;
  renderSurface.geometry->vertexMask = kRenderVertexMask;
  renderSurface.geometry->vertexBuffer.AllocBufferObject(
      nullptr, MAX_VERTICES * static_cast<int>(sizeof(idDrawVert)),
      kRenderVertexMask, BU_DYNAMIC);
  renderSurface.geometry->indexBuffer.AllocBufferObject(
      nullptr, MAX_INDICES * static_cast<int>(sizeof(std::uint16_t)),
      BU_DYNAMIC);

  idRenderModelSurface depthSurface{};
  depthSurface.geometry = new idTriangles;
  depthSurface.geometry->vertexBuffer.Reference(
      &renderSurface.geometry->vertexBuffer);
  depthSurface.geometry->indexBuffer.Reference(
      &renderSurface.geometry->indexBuffer);
  depthSurface.geometry->vertexMask =
      renderSurface.geometry->vertexMask | kDepthSTMask;
  depthSurface.stMap = new idVertexBuffer;
  depthSurface.stMap->AllocBufferObject(
      nullptr, MAX_VERTICES * static_cast<int>(sizeof(idVec2)), kDepthSTMask,
      BU_DYNAMIC);

  AddSurface(depthSurface);
  AddSurface(renderSurface);
  FinishSurfaces();
}

idRenderModelCuttableDynamic::~idRenderModelCuttableDynamic() {
  FreeSurfaces();
}

void idRenderModelCuttableDynamic::UploadRenderGeometry(
    const idMaterial* material, const idList<idDrawVert>& vertices,
    const idList<std::uint16_t>& indices) {
  idRenderModelSurface& renderSurface = surfaces[1];
  idTriangles* geometry = renderSurface.geometry;
  geometry->FreeCPUData();
  renderSurface.material = material;
  if (material == nullptr) {
    geometry->numVerts = 0;
    geometry->numIndexes = 0;
    return;
  }

  const int numVerts = std::min(static_cast<int>(MAX_VERTICES), vertices.Num());
  const int numIndexes =
      std::min(static_cast<int>(MAX_INDICES), indices.Num());
  auto* mappedVertices = static_cast<idDrawVert*>(
      geometry->vertexBuffer.MapBuffer(kMapWrite));
  auto* mappedIndices = static_cast<std::uint16_t*>(
      geometry->indexBuffer.MapBuffer(kMapWrite));
  std::memmove(mappedVertices, vertices.list,
               static_cast<std::size_t>(numVerts) * sizeof(idDrawVert));
  std::memmove(mappedIndices, indices.list,
               static_cast<std::size_t>(numIndexes) * sizeof(std::uint16_t));

  idBounds bounds;
  bounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < numVerts; ++index)
    bounds.AddPoint(mappedVertices[index].xyz);

  geometry->vertexBuffer.UnmapBuffer();
  geometry->indexBuffer.UnmapBuffer();
  geometry->numVerts = numVerts;
  geometry->numIndexes = numIndexes;
  geometry->bounds = bounds;
  referenceBounds = bounds;
}

void idRenderModelCuttableDynamic::UploadDepthGeometry(
    const idMaterial* material, const idList<idVec2>& st) {
  idRenderModelSurface& depthSurface = surfaces[0];
  const idRenderModelSurface& renderSurface = surfaces[1];
  if (renderSurface.material == nullptr)
    material = nullptr;
  depthSurface.material = material;
  if (material == nullptr) {
    depthSurface.geometry->numVerts = 0;
    depthSurface.geometry->numIndexes = 0;
    return;
  }

  const int numST = std::min(static_cast<int>(MAX_VERTICES), st.Num());
  void* mappedST = depthSurface.stMap->MapBuffer(kMapWrite);
  std::memmove(mappedST, st.list,
               static_cast<std::size_t>(numST) * sizeof(idVec2));
  depthSurface.stMap->UnmapBuffer();
  depthSurface.geometry->numVerts = renderSurface.geometry->numVerts;
  depthSurface.geometry->numIndexes = renderSurface.geometry->numIndexes;
  depthSurface.geometry->bounds = renderSurface.geometry->bounds;
}

bool idRenderModelCuttableDynamic::IsValid() const {
  const int numSurfaces = surfaces.Num();
  if (numSurfaces <= 0)
    return false;
  for (int index = 0; index < numSurfaces; ++index) {
    if (surfaces.list != nullptr && surfaces[index].geometry != nullptr &&
        surfaces[index].material != nullptr)
      return true;
  }
  return false;
}

void idRenderModelCuttableDynamic::Upload(
    const idMaterial* renderMaterial, const idMaterial* depthMaterial,
    const idList<idDrawVert>& vertices, const idList<idVec2>& st,
    const idList<std::uint16_t>& indices) {
  UploadRenderGeometry(renderMaterial, vertices, indices);
  UploadDepthGeometry(depthMaterial, st);
}
