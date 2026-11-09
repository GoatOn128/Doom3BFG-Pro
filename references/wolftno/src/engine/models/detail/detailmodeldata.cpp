#include "engine/models/detail/detailmodeldata.h"

#include "engine/models/static/staticmodel.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

paletteStats_t paletteStats{};

const idMaterial* CompatibleMaterial(const idMaterial* material) {
  if (material == nullptr)
    return nullptr;
  const idMaterial* compatible = material->CompatibleGenericMaterial();
  return compatible != nullptr ? compatible : material;
}

float WrapTextureCoordinate(float value) {
  if (value <= -0.01f || value >= 1.0f)
    return value - std::floor(value);
  return value < 0.0f ? 0.0f : value;
}

} // namespace


idTypedResourceList<idDetailModelData>
    idDetailModelData::resourceList("detailModelData");
idList<idDetailPaletteData*, 98> idDetailModelData::palette;

void idDetailPaletteData::CreateDefault() {
  material = static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(
          "genericDetailVmtr", true, true));
  material = CompatibleMaterial(material);

  surfaces = new idDetailModelSurface;
  surfaces->numIndices = 36;
  surfaces->numVerts = 8;
  surfaces->indices = new std::uint16_t[36];
  surfaces->verts = new detailPaletteVert_t[8];

  static constexpr std::int16_t cubeVertices[8][3] = {
      {-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1},
      {-1, -1, 1},  {1, -1, 1},  {1, 1, 1},  {-1, 1, 1},
  };
  for (int vertex = 0; vertex < 8; ++vertex) {
    surfaces->verts[vertex].xyz[0] = cubeVertices[vertex][0];
    surfaces->verts[vertex].xyz[1] = cubeVertices[vertex][1];
    surfaces->verts[vertex].xyz[2] = cubeVertices[vertex][2];
    surfaces->verts[vertex].st[0] = 0;
    surfaces->verts[vertex].st[1] = 0;
  }
  static constexpr std::uint16_t cubeIndices[36] = {
      2, 3, 0, 1, 2, 0, 5, 1, 0, 4, 5, 0,
      7, 4, 0, 3, 7, 0, 1, 5, 6, 2, 1, 6,
      3, 2, 6, 7, 3, 6, 4, 7, 6, 5, 4, 6,
  };
  std::memcpy(surfaces->indices, cubeIndices, sizeof(cubeIndices));
}

bool idDetailPaletteData::Generate(const char* name) {
  const idStaticModel* model =
      staticModelManager != nullptr
          ? staticModelManager->FindStaticModel(name, false)
          : nullptr;
  if (model == nullptr || model->surfaces.Num() == 0)
    return false;

  material = model->surfaces[0].material;
  idVec4 firstMapping{};
  if (material == nullptr || !material->GetVirtualMapping(firstMapping)) {
    idLib::Warning("Detail model %s has invalid material %s", name,
                   material != nullptr ? material->name.str : "<null>");
    return false;
  }
  material = CompatibleMaterial(material);

  int totalIndices = 0;
  int totalVerts = 0;
  for (int surfaceIndex = 0; surfaceIndex < model->surfaces.Num();
       ++surfaceIndex) {
    const idStaticModelSurface& source = model->surfaces[surfaceIndex];
    if (source.geometry == nullptr)
      continue;
    if (source.material != material &&
        CompatibleMaterial(source.material) != material) {
      idLib::Error(
          "idDetailModel: material '%s' and '%s' are not compatible generic materials on '%s'",
          material->name.str,
          source.material != nullptr ? source.material->name.str : "<null>",
          model->name.str);
    }
    totalIndices += source.geometry->numIndexes;
    totalVerts += source.geometry->numVerts;
  }

  auto* mergedIndices = new std::uint16_t[totalIndices];
  auto* mergedVerts = new detailPaletteVert_t[totalVerts];
  int indexOffset = 0;
  int vertexOffset = 0;
  for (int surfaceIndex = 0; surfaceIndex < model->surfaces.Num();
       ++surfaceIndex) {
    const idStaticModelSurface& source = model->surfaces[surfaceIndex];
    const idTriangles* geometry = source.geometry;
    if (geometry == nullptr)
      continue;

    const auto* sourceIndices = geometry->indexes;
    const bool mappedIndices = sourceIndices == nullptr;
    if (mappedIndices) {
      sourceIndices = static_cast<const std::uint16_t*>(
          geometry->indexBuffer.MapBuffer(BM_READ));
    }
    const auto* sourceVerts = geometry->verts;
    const bool mappedVerts = sourceVerts == nullptr;
    if (mappedVerts) {
      sourceVerts = static_cast<const idDrawVert*>(
          geometry->vertexBuffer.MapBuffer(BM_READ));
    }

    for (int index = 0; index < geometry->numIndexes; ++index)
      mergedIndices[indexOffset + index] = static_cast<std::uint16_t>(
          vertexOffset + sourceIndices[index]);

    const idVec4 mapping = source.material->virtualMapping;
    for (int vertex = 0; vertex < geometry->numVerts; ++vertex) {
      detailPaletteVert_t& output = mergedVerts[vertexOffset + vertex];
      output.xyz[0] = static_cast<std::int16_t>(sourceVerts[vertex].xyz.x);
      output.xyz[1] = static_cast<std::int16_t>(sourceVerts[vertex].xyz.y);
      output.xyz[2] = static_cast<std::int16_t>(sourceVerts[vertex].xyz.z);
      const float u = WrapTextureCoordinate(sourceVerts[vertex].st.x);
      const float v = WrapTextureCoordinate(sourceVerts[vertex].st.y);
      output.st[0] = static_cast<std::uint16_t>(
          (u * mapping.x + mapping.z) * 65535.0f);
      output.st[1] = static_cast<std::uint16_t>(
          (v * mapping.y + mapping.w) * 65535.0f);
    }
    vertexOffset += geometry->numVerts;
    indexOffset += geometry->numIndexes;

    if (mappedIndices)
      geometry->indexBuffer.UnmapBuffer();
    if (mappedVerts)
      geometry->vertexBuffer.UnmapBuffer();

    bounds.b[0].x = std::min(bounds.b[0].x, geometry->bounds.b[0].x);
    bounds.b[0].y = std::min(bounds.b[0].y, geometry->bounds.b[0].y);
    bounds.b[0].z = std::min(bounds.b[0].z, geometry->bounds.b[0].z);
    bounds.b[1].x = std::max(bounds.b[1].x, geometry->bounds.b[1].x);
    bounds.b[1].y = std::max(bounds.b[1].y, geometry->bounds.b[1].y);
    bounds.b[1].z = std::max(bounds.b[1].z, geometry->bounds.b[1].z);
  }

  surfaces = new idDetailModelSurface;
  surfaces->numIndices = indexOffset;
  surfaces->numVerts = vertexOffset;
  surfaces->indices = new std::uint16_t[(indexOffset + 1) & ~1];
  surfaces->verts = new detailPaletteVert_t[vertexOffset];
  std::memmove(surfaces->indices, mergedIndices,
               sizeof(std::uint16_t) * indexOffset);
  std::memmove(surfaces->verts, mergedVerts,
               sizeof(detailPaletteVert_t) * vertexOffset);
  ++numSurfaces;
  delete[] mergedIndices;
  delete[] mergedVerts;
  return true;
}

void idDetailModelData::FreeData() {
  if (paletteData != nullptr) {
    paletteData->material = nullptr;
    paletteData->numSurfaces = 0;
    delete paletteData->surfaces;
    paletteData->surfaces = nullptr;
    paletteData->bounds = {};
    delete paletteData;
    paletteData = nullptr;
  }
  paletteIdx = -1;
}

idResourceList* idDetailModelData::GetResourceList() const {
  return &resourceList;
}

template <>
int idTypedResourceList<idDetailModelData>::AllocSize() const {
  return 64;
}

template <>
idDetailModelData*
idTypedResourceList<idDetailModelData>::Alloc(const char* name) const {
  idDetailModelData* resource = new idDetailModelData;
  resource->SetName(name);
  return resource;
}

bool idDetailModelData::Generate() {
  paletteData = new idDetailPaletteData{};
  if (!paletteData->Generate(name.str)) {
    SetResourceError("defaulted '%s'", name.str);
    paletteData->CreateDefault();
  }

  int paletteIndex = -1;
  for (int index = 0; index < palette.Num(); ++index) {
    if (palette[index] == paletteData) {
      paletteIndex = index;
      break;
    }
  }
  if (paletteIndex < 0) {
    const idDetailModelSurface& surface = paletteData->surfaces[0];
    const int combinedVerts = paletteStats.totalNumVerts + surface.numVerts;
    const int combinedIndices =
        paletteStats.totalNumIndices + surface.numIndices;
    if (surface.numVerts > 50 || surface.numIndices > 300 ||
        2 * (combinedIndices + 5 * combinedVerts) > 0x8000) {
      idLib::Warning(
          "Can't add model %s - exceeds limitations: numverts %d numindices %d palette full",
          name.str, surface.numVerts, surface.numIndices);
      FreeData();
      return false;
    }
    ++paletteStats.totalNumModels;
    paletteStats.totalNumVerts = combinedVerts;
    paletteStats.totalNumIndices = combinedIndices;
    paletteIndex = palette.Append(paletteData);
  }
  paletteIdx = paletteIndex;
  return true;
}

void idDetailModelData::LoadResource() {
  FreeData();
  Generate();
}
