#include "engine/models/cuttable/rendermodelcuttablesurface.h"

#include "shared/idlib/lib.h"

#include <cstdint>

namespace {

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24u) | ((value & 0x0000ff00u) << 8u) |
         ((value & 0x00ff0000u) >> 8u) | ((value & 0xff000000u) >> 24u);
}

} // namespace

idRenderModelCuttable::~idRenderModelCuttable() {
  modelResource = nullptr;
}

const idPlane& idRenderModelCuttable::GetReferencePlane() const {
  return modelResource->triangleInterpolator.refPlane;
}

const idList<idVec3>& idRenderModelCuttable::GetContourBounds() const {
  return modelResource->contourBound;
}

bool idRenderModelCuttable::LoadModel(const char* modelName) {
  FreeSurfaces();
  modelResource = static_cast<idRenderModelCuttableResource*>(
      idRenderModelCuttableResource::resourceList.Load(modelName, false, false));
  if (modelResource == nullptr) {
    idLib::Warning("idRenderModelCuttableResource - failed to load model '%s'",
                   modelName);
    return false;
  }
  modelResource->BuildSurfaces(this);
  FinishSurfaces();
  return true;
}

idRenderModelCuttable::idRenderModelCuttable(const char* modelName) {
  modelResource = nullptr;
  renderModelType = RENDERMODEL_CUTTABLESURFACE;
  recoveredParms[116] |= 0x80u;
  recoveredParms[117] |= 0x01u;
  SetName(modelName);
  LoadModel(modelName);
}

void idRenderModelCuttable::Save(idFile* file) const {
  idRenderModel::Save(file);
  const std::uint32_t stored = ByteSwap32(static_cast<std::uint32_t>(bufferIndex));
  file->Write(&stored, sizeof(stored));
}

bool idRenderModelCuttable::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  const std::size_t bytes = file->Read(&bufferIndex, sizeof(bufferIndex));
  bufferIndex = static_cast<int>(
      ByteSwap32(static_cast<std::uint32_t>(bufferIndex)));
  return bytes != 0;
}

int idRenderModelCuttable::UpdateSurfaces(int bufferIndex_,
                                          const idVertexBuffer& vertexBuffer,
                                          const idVertexBuffer& stBuffer,
                                          const idIndexBuffer& indexBuffer,
                                          const idCuttableGenGeometry& genGeometry) {
  const int previousBufferIndex = bufferIndex;
  bufferIndex = bufferIndex_;
  for (int surfaceIndex = 0; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
    idRenderModelSurface& surface = surfaces[surfaceIndex];
    if (surface.geometry == nullptr)
      continue;
    const cuttableGenSurface_t& generated = genGeometry.surfaces[surfaceIndex];
    surface.material = generated.material;
    surface.geometry->FreeCPUData();
    surface.geometry->vertexBuffer.Reference(&vertexBuffer);
    surface.geometry->indexBuffer.Reference(&indexBuffer, 2 * generated.indexOffset,
                                            2 * generated.numIndices);
    if (surface.stMap != nullptr)
      surface.stMap->Reference(&stBuffer);
    surface.geometry->numVerts = generated.numVertices;
    surface.geometry->numIndexes = generated.numIndices;
  }
  return previousBufferIndex;
}
