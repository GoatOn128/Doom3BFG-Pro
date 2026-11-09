#include "engine/models/cuttable/rendermodelcuttableresource.h"

#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

idCVar r_cuttablemodel_freecpudata(
    "r_cuttablemodel_freecpudata", "1", CVAR_BOOL,
    "Free cuttable model CPU data");
idCVar g_cuttablemodel_buildepthonly(
    "g_cuttablemodel_buildepthonly", "1", CVAR_BOOL,
    "Build seperate physical texture depth only pass");
idCVar g_cuttablemodel_forcestale(
    "g_cuttablemodel_forcestale", "0", CVAR_BOOL,
    "Force cuttable resource to be stale and therefore reload.");

idTypedResourceList<idRenderModelCuttableResource>
    idRenderModelCuttableResource::resourceList("cuttableResource");

namespace {

constexpr std::uint32_t kCuttableMagic = 0x0A4C4D43u;
constexpr std::uint32_t kOldCuttableMagic = 0x094C4D43u;
constexpr float kFloatEpsilon = 1.192092896e-7f;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) | ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) | ((value & 0xff000000u) >> 24);
}

float ByteSwapFloat(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = ByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void ReadBigInt(idFile* file, int& value) {
  file->Read(&value, sizeof(value));
  value = static_cast<int>(ByteSwap32(static_cast<std::uint32_t>(value)));
}

void ReadBigUInt(idFile* file, std::uint32_t& value) {
  file->Read(&value, sizeof(value));
  value = ByteSwap32(value);
}

void ReadBigFloat(idFile* file, float& value) {
  file->Read(&value, sizeof(value));
  value = ByteSwapFloat(value);
}

void ReadBigVec2Array(idFile* file, idVec2* values, int count) {
  if (count <= 0)
    return;
  file->Read(values, sizeof(idVec2) * static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    values[i].x = ByteSwapFloat(values[i].x);
    values[i].y = ByteSwapFloat(values[i].y);
  }
}

void ReadBigVec3Array(idFile* file, idVec3* values, int count) {
  if (count <= 0)
    return;
  file->Read(values, sizeof(idVec3) * static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    values[i].x = ByteSwapFloat(values[i].x);
    values[i].y = ByteSwapFloat(values[i].y);
    values[i].z = ByteSwapFloat(values[i].z);
  }
}

int VertexSizeFromMask(std::uint32_t mask) {
  return ((mask & 1u) != 0 ? 12 : 0) + 4 * static_cast<int>(mask & 2u) +
         static_cast<int>(mask & 4u) + static_cast<int>((mask >> 1) & 4u) +
         static_cast<int>((mask >> 2) & 4u) -
         ((mask & 0x20u) != 0 ? 4 : 0) -
         ((mask & 0x40u) != 0 ? 4 : 0) +
         ((mask & 0x1000u) != 0 ? 12 : 0) +
         static_cast<int>((mask >> 9) & 4u);
}

void SwapPackedVertexBuffer(std::uint8_t* buffer, int numVerts,
                            std::uint32_t vertexMask) {
  auto swapWord = [](std::uint8_t* bytes) {
    std::swap(bytes[0], bytes[3]);
    std::swap(bytes[1], bytes[2]);
  };
  std::uint8_t* cursor = buffer;
  for (int vertex = 0; vertex < numVerts; ++vertex) {
    if ((vertexMask & 0x20u) != 0) {
      swapWord(cursor);
      swapWord(cursor + 4);
      cursor += 8;
    } else if ((vertexMask & 0x01u) != 0) {
      swapWord(cursor);
      swapWord(cursor + 4);
      swapWord(cursor + 8);
      cursor += 12;
    }
    if ((vertexMask & 0x1000u) != 0) {
      swapWord(cursor);
      swapWord(cursor + 4);
      swapWord(cursor + 8);
      cursor += 12;
    }
    if ((vertexMask & 0x40u) != 0) {
      swapWord(cursor);
      cursor += 4;
    } else if ((vertexMask & 0x02u) != 0) {
      swapWord(cursor);
      swapWord(cursor + 4);
      cursor += 8;
    }
    if ((vertexMask & 0x800u) != 0) {
      swapWord(cursor);
      cursor += 4;
    } else if ((vertexMask & 0x400u) != 0) {
      swapWord(cursor);
      swapWord(cursor + 4);
      cursor += 8;
    }
    if ((vertexMask & 0x04u) != 0)
      cursor += 4;
    if ((vertexMask & 0x10u) != 0)
      cursor += 4;
    if ((vertexMask & 0x08u) != 0)
      cursor += 4;
  }
}

idTriangles* AllocTriangles() {
  auto* triangles = new idTriangles{};
  triangles->allowGpuHosting = true;
  triangles->vsb.xyzScale = {1.0f, 1.0f, 1.0f};
  triangles->vsb.stScale = {1.0f, 1.0f};
  triangles->cpuVertexMask = 31;
  triangles->vertexMask = 31;
  triangles->bounds.Clear();
  return triangles;
}

void FreeTriangles(idTriangles* triangles) {
  if (triangles == nullptr)
    return;
  triangles->FreeCPUData();
  delete triangles;
}

void ReadDrawVerts(idFile* file, idTriangles& triangles) {
  const int count = triangles.numVerts;
  if (count <= 0)
    return;

  if (!triangles.allowGpuHosting || triangles.vertexMask == 31) {
    triangles.verts = new idDrawVert[count]{};
    file->Read(triangles.verts,
               sizeof(idDrawVert) * static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      triangles.verts[i].xyz.x = ByteSwapFloat(triangles.verts[i].xyz.x);
      triangles.verts[i].xyz.y = ByteSwapFloat(triangles.verts[i].xyz.y);
      triangles.verts[i].xyz.z = ByteSwapFloat(triangles.verts[i].xyz.z);
      triangles.verts[i].st.x = ByteSwapFloat(triangles.verts[i].st.x);
      triangles.verts[i].st.y = ByteSwapFloat(triangles.verts[i].st.y);
    }
    return;
  }

  const int stride = VertexSizeFromMask(triangles.vertexMask);
  if (stride <= 0)
    return;
  std::vector<std::uint8_t> packed(static_cast<std::size_t>(stride) * count);
  file->Read(packed.data(), packed.size());
  SwapPackedVertexBuffer(packed.data(), count, triangles.vertexMask);
  triangles.vertexBuffer.AllocBufferObject(
      packed.data(), static_cast<int>(packed.size()), triangles.vertexMask,
      BU_DEFAULT);
}

void ReadTriangles(idFile* file, idTriangles& triangles) {
  ReadBigInt(file, triangles.numVerts);
  ReadBigInt(file, triangles.numIndexes);
  if (triangles.numVerts < 0)
    idLib::FatalError("idTriangles::ReadFromFile: bad numVerts (surf %d)",
                      triangles.numVerts);
  if (triangles.numIndexes < 0)
    idLib::FatalError("idTriangles::ReadFromFile: bad numIndexes (surf %d)",
                      triangles.numIndexes);
  ReadBigUInt(file, triangles.vertexMask);
  triangles.cpuVertexMask = triangles.vertexMask;

  float* scaleAndBias = &triangles.vsb.xyzScale.x;
  for (int i = 0; i < 10; ++i)
    ReadBigFloat(file, scaleAndBias[i]);
  ReadDrawVerts(file, triangles);

  triangles.indexes = triangles.numIndexes > 0
                          ? new std::uint16_t[triangles.numIndexes]
                          : nullptr;
  if (triangles.numIndexes > 0)
    file->Read(triangles.indexes,
               sizeof(std::uint16_t) *
                   static_cast<std::size_t>(triangles.numIndexes));
  for (int i = 0; i < triangles.numIndexes; ++i)
    triangles.indexes[i] = ByteSwap16(triangles.indexes[i]);
  for (int bound = 0; bound < 2; ++bound) {
    ReadBigFloat(file, triangles.bounds[bound].x);
    ReadBigFloat(file, triangles.bounds[bound].y);
    ReadBigFloat(file, triangles.bounds[bound].z);
  }
  ReadBigInt(file, triangles.detailOffset);
}

idTriangles* ReferenceGeometry(const idTriangles* source,
                               std::uint32_t vertexMask) {
  idTriangles* geometry = AllocTriangles();
  geometry->bounds = source->bounds;
  geometry->generateNormals = false;
  geometry->numIndexes = source->numIndexes;
  geometry->numVerts = source->numVerts;
  geometry->detailOffset = source->detailOffset;
  geometry->vertexMask = vertexMask;
  geometry->vsb = source->vsb;
  geometry->verts = nullptr;
  geometry->indexes = nullptr;
  geometry->vertexBuffer.Reference(&source->vertexBuffer);
  geometry->indexBuffer.Reference(&source->indexBuffer);
  geometry->CreateBounds();
  return geometry;
}

const char* SurfaceTypeSuffix(surfTypes_t type) {
  static const char* const suffixes[MAX_NUM_SURFTYPES] = {
      "_none",          "_metal",          "_stone",
      "_flesh",         "_wood",           "_cardboard",
      "_liquid",        "_glass",          "_plastic",
      "_asphalt",       "_dirt",           "_concrete",
      "_foliage",       "_linoleum",       "_fabric",
      "_rubber",        "_rock",           "_steam_pipe",
      "_water_pipe",    "_armor",          "_sludge",
      "_imp_nest",      "_hollow_metal",   "_flesh_player",
      "_flesh_darkangel", "_blood_pool",   "_ash",
      "_flesh_mutant",  "_thick_padding",  "_player_apc_metal",
      "_rickety_metal", "_rickety_wood",   "_sheet_metal",
      "_porcelain",     "_plaster",        "_vent_metal",
      "_electronics",   "_oil_pipe",       "_panzer_metal",
      "_breakable_armor", "_electric_shield"};
  const int index = type == SURFTYPE_INVALID ? 0 : static_cast<int>(type);
  return index >= 0 && index < MAX_NUM_SURFTYPES ? suffixes[index] : "_none";
}

const idMaterial* FindMaterial(const char* name, bool makeDefault) {
  if (name == nullptr || name[0] == '\0')
    return nullptr;
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(name, makeDefault, true));
}

} // namespace

void idRenderModelCuttableResource::FreeCPUData() {
  if (triangles != nullptr)
    triangles->FreeCPUData();
  delete[] stData;
  stData = nullptr;
}

bool idRenderModelCuttableResource::ReloadIfStale() {
  char binaryFileName[256]{};
  fileSystem->FixLongFilename("", "cmodel", name.str, binaryFileName,
                              sizeof(binaryFileName));
  const bool stale = g_cuttablemodel_forcestale.GetInteger() != 0 ||
                     fileSystem->GetTimestamp(binaryFileName, false) != timestamp;
  if (stale) {
    SetResourceError(nullptr);
    fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
    LoadResource();
    return true;
  }
  return false;
}

void idRenderModelCuttableResource::FreeData() {
  FreeTriangles(triangles);
  triangles = nullptr;
  delete stMap;
  stMap = nullptr;
  delete[] stData;
  stData = nullptr;
  material = nullptr;
  depthOnlyMaterial = nullptr;
  timestamp = ~0u;
  contourBound.ClearFree();
  triangleInterpolator.Clear();
  isOpaque = true;
}

const idMaterial* idRenderModelCuttableResource::BuildDepthOnlyMaterial(
    const char* modelName, const char* coverMapName) {
  if (coverMapName == nullptr || idStr::Length(coverMapName) <= 0)
    return nullptr;

  idStr materialName(modelName);
  materialName.StripFileExtension();
  materialName += "_cover";
  if (idLib::production >= PROD_PRODUCTION) {
    return static_cast<const idMaterial*>(
        idMaterial::resourceList.FindWithInheritance(materialName.data, false,
                                                      true));
  }

  idStr fileName;
  fileName.Format("generated/cuttable/m2/%s.m2", materialName.data);
  idStr definition;
  definition.Format("{\n\tambientProgram cutDepthOnly\n\tdepthFillProgram "
                    "cutDepthOnly\n\ttextureMap \"%s\"\n}\n",
                    coverMapName);

  idMaterial* material = static_cast<idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(materialName.data, false,
                                                    false));
  if (material != nullptr) {
    material->SetText(definition.data);
    material->ReplaceSourceFileText(false);
    material->ReParse(true);
    return material;
  }
  material = static_cast<idMaterial*>(declManager->CreateNewDecl(
      &idMaterial::resourceList, materialName.data, fileName.data,
      definition.data));
  material->ReplaceSourceFileText(false);
  idMaterial::resourceList.Remove(material);
  delete material;
  return static_cast<const idMaterial*>(
      idMaterial::resourceList.Load(materialName.data, true, false));
}

void idRenderModelCuttableResource::BuildSurfaces(idRenderModel* model) {
  if (triangles == nullptr)
    return;

  idRenderModelSurface renderSurface{};
  renderSurface.material = material;
  if (renderSurface.material == nullptr)
    return;

  idRenderModelSurface depthSurface{};
  const idMaterial* selectedDepthMaterial = nullptr;
  if (g_cuttablemodel_buildepthonly.GetInteger() != 0) {
    selectedDepthMaterial = depthOnlyMaterial;
    depthSurface.material = selectedDepthMaterial;
  }

  renderSurface.geometry = ReferenceGeometry(triangles, triangles->vertexMask);
  if (selectedDepthMaterial != nullptr) {
    depthSurface.geometry =
        ReferenceGeometry(triangles, triangles->vertexMask | 0x2400u);
    depthSurface.stMap = new idVertexBuffer{};
    depthSurface.stMap->Reference(stMap);
    model->AddSurface(depthSurface);
  }
  model->AddSurface(renderSurface);
}

idRenderModelCuttableResource::~idRenderModelCuttableResource() {
  FreeData();
}

idResourceList* idRenderModelCuttableResource::GetResourceList() const {
  return &resourceList;
}

idRenderModelCuttableResource::idRenderModelCuttableResource()
    : material(nullptr), depthOnlyMaterial(nullptr), surfaceTypeMaterial(nullptr),
      coverMap(), triangles(nullptr), stData(nullptr), stMap(nullptr),
      contourBound(), triangleInterpolator(), numInterpolatorTriangles(0),
      timestamp(~0u), thickness(0.0f), surfaceType(SURFTYPE_NONE),
      isOpaque(true) {}

bool idRenderModelCuttableResource::LoadBinary(const char* fileName) {
  idFileLocal file(fileSystem->OpenFileRead(fileName, true, false));
  if (file.file == nullptr) {
    idLib::Warning(
        "idRenderModelCuttableResource::LoadBinary - Couldn't load cuttable "
        "'%s', unable to open file for reading!",
        fileName);
    return false;
  }

  std::uint32_t magic = 0;
  ReadBigUInt(file.file, magic);
  if (magic != kCuttableMagic) {
    idLib::Warning(
        "idRenderModelCuttableResource::LoadBinary - Old version, re-compile "
        "cuttables.");
    if (magic != kOldCuttableMagic)
      return false;
  }

  file.file->Read(&isOpaque, sizeof(isOpaque));
  ReadBigFloat(file.file, thickness);
  ReadBigInt(file.file, numInterpolatorTriangles);

  int numContourVertices = 0;
  ReadBigInt(file.file, numContourVertices);
  contourBound.SetNum(numContourVertices);
  ReadBigVec3Array(file.file, contourBound.list, numContourVertices);

  idStr materialName;
  idStr coverMapName;
  file.file->ReadString(materialName);
  file.file->ReadString(coverMapName);

  triangles = AllocTriangles();
  ReadTriangles(file.file, *triangles);
  if (triangles->numVerts > 0) {
    stData = new idVec2[triangles->numVerts];
    ReadBigVec2Array(file.file, stData, triangles->numVerts);
  }

  if (magic == kOldCuttableMagic) {
    surfaceType = SURFTYPE_METAL;
  } else {
    int storedSurfaceType = 0;
    ReadBigInt(file.file, storedSurfaceType);
    surfaceType = static_cast<surfTypes_t>(storedSurfaceType);
  }

  if (!materialName.IsEmpty())
    material = FindMaterial(materialName.data, true);
  if (!coverMapName.IsEmpty())
    depthOnlyMaterial = BuildDepthOnlyMaterial(name.str, coverMapName.data);

  idStr surfaceTypeMaterialName;
  surfaceTypeMaterialName.Format("textures/common/collision%s",
                                 SurfaceTypeSuffix(surfaceType));
  surfaceTypeMaterialName.ToLower();
  surfaceTypeMaterial = FindMaterial(surfaceTypeMaterialName.data, false);
  if (surfaceTypeMaterial == nullptr)
    surfaceTypeMaterial = material;
  timestamp = file.file->Timestamp();
  return true;
}

bool idRenderModelCuttableResource::GenerateSurfaces(
    idEarClipTriangulate& triangulator, idCuttableGenGeometry& genGeometry,
    const idVec3& centerOfMass) {
  if (!triangulator.Triangulate())
    return false;

  const bool buildSides = isOpaque && thickness > kFloatEpsilon;
  const int numPositions = triangulator.maxIndex + 1;
  const int numVertices = numPositions * (buildSides ? 3 : 2);
  const int numFrontIndices = triangulator.triangles.Num();
  const int numIndices =
      2 * numFrontIndices + (buildSides ? 6 * numPositions : 0);

  genGeometry.surfaces.EnsureCapacity(
      depthOnlyMaterial != nullptr ? 2 : 1);
  genGeometry.indices.EnsureCapacity(numIndices);
  genGeometry.vertices.SetNum(numVertices);
  genGeometry.st.SetNum(numVertices);

  idList<idVec2> positions;
  triangulator.BuildGeometry(triangleInterpolator.refPlane, positions,
                             genGeometry.indices);
  genGeometry.indices.SetNum(numFrontIndices);

  const idVec3 thicknessOffset{
      thickness * triangleInterpolator.refPlane.a * 2.0f,
      thickness * triangleInterpolator.refPlane.b * 2.0f,
      thickness * triangleInterpolator.refPlane.c * 2.0f};
  for (int i = 0; i < positions.Num(); ++i) {
    idDrawVert interpolated[2]{};
    idVec2 interpolatedST[2]{};
    triangleInterpolator.Interpolate(positions[i], interpolated,
                                     interpolatedST);
    interpolated[0].xyz -= centerOfMass;
    interpolated[1].xyz -= thicknessOffset;
    interpolated[1].xyz -= centerOfMass;
    genGeometry.vertices[i] = interpolated[0];
    genGeometry.vertices[i + positions.Num()] = interpolated[1];
    genGeometry.st[i] = interpolatedST[0];
    genGeometry.st[i + positions.Num()] = interpolatedST[1];
  }

  for (int i = 0; i < numFrontIndices; i += 3) {
    genGeometry.indices.Append(
        static_cast<std::uint16_t>(positions.Num() + genGeometry.indices[i]));
    genGeometry.indices.Append(static_cast<std::uint16_t>(
        positions.Num() + genGeometry.indices[i + 2]));
    genGeometry.indices.Append(static_cast<std::uint16_t>(
        positions.Num() + genGeometry.indices[i + 1]));
  }

  if (buildSides) {
    const int sideOffset = 2 * numPositions;
    for (int i = 0; i < positions.Num(); ++i) {
      genGeometry.vertices[sideOffset + i] = genGeometry.vertices[i];
      genGeometry.vertices[sideOffset + i].xyz =
          genGeometry.vertices[numPositions + i].xyz;
      genGeometry.st[sideOffset + i] = genGeometry.st[i];
    }

    for (int contourIndex = 0; contourIndex < triangulator.contours.Num();
         ++contourIndex) {
      const idEarClipTriangulate::contour_t* contour =
          triangulator.contours[contourIndex];
      for (int i = 0; i < contour->indices.Num(); ++i) {
        const std::uint16_t current =
            static_cast<std::uint16_t>(contour->indices[i]);
        const std::uint16_t next = static_cast<std::uint16_t>(
            contour->indices[(i + 1) % contour->indices.Num()]);
        genGeometry.indices.Append(current);
        genGeometry.indices.Append(
            static_cast<std::uint16_t>(current + sideOffset));
        genGeometry.indices.Append(
            static_cast<std::uint16_t>(next + sideOffset));
        genGeometry.indices.Append(current);
        genGeometry.indices.Append(
            static_cast<std::uint16_t>(next + sideOffset));
        genGeometry.indices.Append(next);
      }
    }
  }

  cuttableGenSurface_t surface{};
  surface.indexOffset = 0;
  surface.numIndices = numIndices;
  surface.numVertices = numVertices;
  if (depthOnlyMaterial != nullptr) {
    surface.material = depthOnlyMaterial;
    genGeometry.surfaces.Append(surface);
  }
  if (material != nullptr) {
    surface.material = material;
    genGeometry.surfaces.Append(surface);
  }
  return true;
}

void idRenderModelCuttableResource::LoadResource() {
  FreeData();
  char binaryFileName[256]{};
  fileSystem->FixLongFilename("", "cmodel", name.str, binaryFileName,
                              sizeof(binaryFileName));
  if (!fileSystem->FileExists(binaryFileName, false)) {
    SetResourceError("missing file '%s'", binaryFileName);
    return;
  }
  if (!LoadBinary(binaryFileName)) {
    SetResourceError("Binary file corrupt or inaccessible '%s'",
                     binaryFileName);
    return;
  }

  if (triangles != nullptr) {
    triangleInterpolator.AddTriangles(
        triangles->verts, stData, triangles->indexes,
        numInterpolatorTriangles);
    idTriangles::UpdateVertexBuffer(triangles);
    idTriangles::UpdateIndexBuffer(triangles);
  }
  if (stData != nullptr) {
    stMap = new idVertexBuffer{};
    stMap->AllocBufferObject(stData, 8 * triangles->numVerts, 0x2400,
                             BU_DEFAULT);
  }
  if (r_cuttablemodel_freecpudata.GetInteger() != 0)
    FreeCPUData();
}

bool idRenderModelCuttableResource::GenerateSurfaces(
    const Contour* outer, const Contour* inner,
    idCuttableGenGeometry& genGeometry) {
  idEarClipTriangulate triangulator;
  triangulator.SetOuterFromContour(outer);
  triangulator.AddInnerFromContour(inner);
  const idVec3 centerOfMass{0.0f, 0.0f, 0.0f};
  return GenerateSurfaces(triangulator, genGeometry, centerOfMass);
}
