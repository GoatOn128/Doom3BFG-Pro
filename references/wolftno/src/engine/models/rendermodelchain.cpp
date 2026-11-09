#include "engine/models/rendermodelchain.h"

#include "engine/models/static/staticmodel.h"
#include "engine/renderer/declmaterial.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

namespace {

constexpr std::uint8_t kMorphDirty = 1U;
constexpr std::uint8_t kTransformsDirty = 2U;
constexpr float kDegreesToRadians = 0.017453292f;

idRenderModelChainStaticLookup staticModelLookup = nullptr;
idRenderModelChainMaterialLookup materialLookup = nullptr;

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00ff0000U) |
         ((value >> 8) & 0x0000ff00U) | (value >> 24);
}

bool ReadExact(idFile* file, void* data, std::size_t size) {
  return file != nullptr && file->Read(data, size) == size;
}

void WriteBig32(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  file->Write(&value, sizeof(value));
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = ByteSwap32(value);
  return true;
}

void WriteBigFloat(idFile* file, float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteBig32(file, bits);
}

bool ReadBigFloat(idFile* file, float& value) {
  std::uint32_t bits = 0;
  if (!ReadBig32(file, bits))
    return false;
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

void WriteBigVec3(idFile* file, const idVec3& value) {
  WriteBigFloat(file, value.x);
  WriteBigFloat(file, value.y);
  WriteBigFloat(file, value.z);
}

bool ReadBigVec3(idFile* file, idVec3& value) {
  return ReadBigFloat(file, value.x) && ReadBigFloat(file, value.y) &&
         ReadBigFloat(file, value.z);
}

void WriteBigMat3(idFile* file, const idMat3& value) {
  for (const idVec3& row : value.mat)
    WriteBigVec3(file, row);
}

bool ReadBigMat3(idFile* file, idMat3& value) {
  for (idVec3& row : value.mat) {
    if (!ReadBigVec3(file, row))
      return false;
  }
  return true;
}

idVec3 JointOrigin(const idJointMat& joint) {
  return {joint.mat[3], joint.mat[7], joint.mat[11]};
}

idMat3 JointAxis(const idJointMat& joint) {
  idMat3 axis{};
  axis.mat[0] = {joint.mat[0], joint.mat[4], joint.mat[8]};
  axis.mat[1] = {joint.mat[1], joint.mat[5], joint.mat[9]};
  axis.mat[2] = {joint.mat[2], joint.mat[6], joint.mat[10]};
  return axis;
}

void SetJoint(idJointMat& joint, const idVec3& origin, const idMat3& axis) {
  joint.mat[0] = axis.mat[0].x;
  joint.mat[1] = axis.mat[1].x;
  joint.mat[2] = axis.mat[2].x;
  joint.mat[3] = origin.x;
  joint.mat[4] = axis.mat[0].y;
  joint.mat[5] = axis.mat[1].y;
  joint.mat[6] = axis.mat[2].y;
  joint.mat[7] = origin.y;
  joint.mat[8] = axis.mat[0].z;
  joint.mat[9] = axis.mat[1].z;
  joint.mat[10] = axis.mat[2].z;
  joint.mat[11] = origin.z;
}

idVec3 Normalize(const idVec3& value) {
  const float inverse = 1.0f / std::sqrt(std::max(
      value.LengthSqr(), std::numeric_limits<float>::min()));
  return value * inverse;
}

idMat3 LinkAxis(const idVec3& direction, float angleRadians) {
  const idVec3 forward = Normalize(direction);
  const float horizontal = forward.x * forward.x + forward.y * forward.y;
  idVec3 side;
  if (horizontal == 0.0f) {
    side = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverse = 1.0f / std::sqrt(std::max(
        horizontal, std::numeric_limits<float>::min()));
    side = {-forward.y * inverse, forward.x * inverse, 0.0f};
  }
  idMat3 base{};
  base.mat[0] = side.Cross(forward);
  base.mat[1] = side;
  base.mat[2] = forward;
  return base * idMat3(forward, angleRadians);
}

float LinkRotation(std::uint32_t& random, int link, bool rotateRandom,
                   float startRotation) {
  random = 1103515245U * random + 12345U;
  float angle = 0.0f;
  if (rotateRandom) {
    if ((link & 1) == 0)
      angle = ((random >> 16) & 1U) != 0U ? 180.0f : 0.0f;
    else
      angle = ((random >> 16) & 1U) != 0U ? 270.0f : 90.0f;
  }
  return (startRotation + angle) * kDegreesToRadians;
}

void PrepareSkinning(idTriangles& triangles, const idBounds& modelBounds,
                     bool smooth) {
  if (!smooth) {
    for (int index = 0; index < triangles.numVerts; ++index) {
      idDrawVert& vertex = triangles.verts[index];
      std::memset(vertex.color, 0, sizeof(vertex.color));
      vertex.tangent[3] = 0;
      vertex.normal[3] = 0;
    }
    return;
  }

  const float height = modelBounds.b[1].z - modelBounds.b[0].z;
  for (int index = 0; index < triangles.numVerts; ++index) {
    idDrawVert& vertex = triangles.verts[index];
    float fraction = (vertex.xyz.z - modelBounds.b[0].z) / height;
    fraction = std::max(0.0f, std::min(1.0f, fraction));
    const int scaled = static_cast<int>(fraction * 255.10001f);
    const std::uint8_t amount =
        static_cast<std::uint8_t>(std::max(0, std::min(255, scaled)));
    std::uint8_t firstJoint = amount > 127U ? 1U : 0U;
    std::uint8_t secondJoint = amount > 127U ? 0U : 1U;
    std::uint8_t weight = amount > 127U
                              ? static_cast<std::uint8_t>(255U - amount)
                              : amount;
    if (amount == 0U)
      secondJoint = firstJoint;
    else if (amount == 255U)
      secondJoint = firstJoint;
    vertex.color[0] = firstJoint;
    vertex.color[1] = secondJoint;
    vertex.color[2] = firstJoint;
    vertex.color[3] = firstJoint;
    vertex.tangent[3] = weight;
    vertex.normal[3] = 0;
  }
}

} // namespace

void SetRenderModelChainStaticLookup(idRenderModelChainStaticLookup lookup) {
  staticModelLookup = lookup;
}

void SetRenderModelChainMaterialLookup(idRenderModelChainMaterialLookup lookup) {
  materialLookup = lookup;
}

idRenderModelChain::idRenderModelChain()
    : currentJointBuffer(0), currentMorphBuffer(0), numMorphVertices(0),
      numMorphLinkVertices(0), numLinks(0), linkSize(0.0f),
      chainLinkModel(""), morphMaterial(nullptr), dirtyFlags(0),
      genInfo{true, false, {}, 0.0f} {
  renderModelType = RENDERMODEL_CHAIN;
  constexpr int modelDepthSortAfterWorld = 1;
  std::memcpy(recoveredParms + 172, &modelDepthSortAfterWorld,
              sizeof(modelDepthSortAfterWorld));
}

idRenderModelChain::~idRenderModelChain() = default;

int idRenderModelChain::GetNumLinks() const { return numLinks; }

const idJointMat* idRenderModelChain::GetLinkPosition(int id) const {
  return &transforms.list[id];
}

void idRenderModelChain::SetLinkPosition(int id, const idVec3& origin,
                                         const idMat3& axis) {
  SetJoint(transforms.list[id], origin, axis);
  dirtyFlags |= kTransformsDirty;
}

void idRenderModelChain::SetLinkMorphAmount(int id, float amount) {
  if (numMorphVertices <= 0)
    return;
  int value = static_cast<int>(amount * 255.89999f);
  value = std::max(0, std::min(255, value));
  const int first = 4 * id * numMorphLinkVertices;
  for (int vertex = 0; vertex < numMorphLinkVertices; ++vertex)
    morphMap.list[first + 4 * vertex + 3] = static_cast<std::uint8_t>(value);
  dirtyFlags |= kMorphDirty;
}

void idRenderModelChain::RemoveSmoothSkinning(int id) {
  if (surfaces.Num() <= 0 || id == numLinks - 1)
    return;
  idTriangles* geometry = surfaces[0].geometry;
  if (geometry == nullptr)
    return;
  auto* vertices = static_cast<idDrawVert*>(geometry->vertexBuffer.MapBuffer(2));
  if (vertices == nullptr)
    return;

  const float distance =
      (JointOrigin(transforms[id + 1]) - JointOrigin(transforms[id])).Length();
  const int verticesPerLink = geometry->numVerts / numLinks;
  const int first = id * verticesPerLink;
  const int end = first + verticesPerLink;
  for (int index = first; index < end; ++index) {
    idDrawVert& vertex = vertices[index];
    float extra = 0.0f;
    if (vertex.color[0] != id) {
      extra += static_cast<float>(255U - vertex.tangent[3]) * distance *
               (1.0f / 255.0f);
    }
    if (vertex.color[1] != id) {
      extra += static_cast<float>(vertex.tangent[3]) * distance *
               (1.0f / 255.0f);
    }
    vertex.xyz.z += extra;
    std::memset(vertex.color, static_cast<unsigned char>(id),
                sizeof(vertex.color));
    vertex.tangent[3] = 0;
    vertex.normal[3] = 0;
  }
  geometry->vertexBuffer.UnmapBuffer();
}

bool idRenderModelChain::CommitSubclass() {
  if ((dirtyFlags & kTransformsDirty) != 0U) {
    currentJointBuffer = 0;
    jointBuffers[0].Update(
        transforms.list != nullptr ? transforms.list->mat : nullptr,
        transforms.Num());
    for (int surfaceIndex = 0; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
      surfaces[surfaceIndex].joints->Reference(
          &jointBuffers[currentJointBuffer], 0, transforms.Num());
    }

    idVec3 minimum{1.0e30f, 1.0e30f, 1.0e30f};
    idVec3 maximum{-1.0e30f, -1.0e30f, -1.0e30f};
    for (int index = 0; index < transforms.Num(); ++index) {
      const idVec3 origin = JointOrigin(transforms[index]);
      minimum.x = std::fmin(minimum.x, origin.x);
      minimum.y = std::fmin(minimum.y, origin.y);
      minimum.z = std::fmin(minimum.z, origin.z);
      maximum.x = std::fmax(maximum.x, origin.x);
      maximum.y = std::fmax(maximum.y, origin.y);
      maximum.z = std::fmax(maximum.z, origin.z);
    }
    referenceBounds.b[0] = {minimum.x - 8.0f, minimum.y - 8.0f,
                            minimum.z - 8.0f};
    referenceBounds.b[1] = {maximum.x + 8.0f, maximum.y + 8.0f,
                            maximum.z + 8.0f};
    dirtyFlags &= static_cast<std::uint8_t>(~kTransformsDirty);
  }

  if ((dirtyFlags & kMorphDirty) != 0U) {
    currentMorphBuffer = (currentMorphBuffer + 1) % 2;
    morphBuffers[currentMorphBuffer].Update(morphMap.list, morphMap.Num(), 4);
    for (int surfaceIndex = 0; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
      idRenderModelSurface& surface = surfaces[surfaceIndex];
      const int vertexCount = surface.geometry != nullptr
                                  ? surface.geometry->numVerts
                                  : 0;
      if (surface.morphMap != nullptr) {
        surface.morphMultiplier = 1.0f;
        surface.morphMap->Reference(&morphBuffers[currentMorphBuffer], 0,
                                    4 * vertexCount);
      }
    }
    dirtyFlags &= static_cast<std::uint8_t>(~kMorphDirty);
  }
  return true;
}

void idRenderModelChain::Save(idFile* file) const {
  idRenderModel::Save(file);
  const bool hasMorphMaterial = morphMaterial != nullptr;
  file->Write(&hasMorphMaterial, 1);
  if (hasMorphMaterial)
    file->WriteString(morphMaterial->GetName());
  file->WriteString(chainLinkModel.data);
  WriteBig32(file, static_cast<std::uint32_t>(splineSettings.Num()));
  for (int index = 0; index < splineSettings.Num(); ++index) {
    WriteBigVec3(file, splineSettings[index].value);
    WriteBigFloat(file, splineSettings[index].time);
  }
  file->Write(&genInfo.rotateRandom, 1);
  file->Write(&genInfo.smoothSkinning, 1);
  WriteBigFloat(file, genInfo.startRotation);
  WriteBig32(file, static_cast<std::uint32_t>(numLinks));
  for (int index = 0; index < numLinks; ++index) {
    WriteBigMat3(file, JointAxis(transforms[index]));
    WriteBigVec3(file, JointOrigin(transforms[index]));
  }
  const bool hasMorph = numMorphVertices != 0;
  file->Write(&hasMorph, 1);
  if (hasMorph) {
    for (int index = 0; index < numLinks; ++index) {
      const std::uint8_t amount =
          morphMap[4 * index * numMorphLinkVertices + 3];
      file->Write(&amount, 1);
    }
  }
}

bool idRenderModelChain::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;

  bool hasMorphMaterial = false;
  if (!ReadExact(file, &hasMorphMaterial, 1))
    return false;
  if (hasMorphMaterial) {
    idStr materialName;
    if (file->ReadString(materialName) == 0)
      return false;
    morphMaterial = materialLookup != nullptr ? materialLookup(materialName.data)
                                              : nullptr;
  }
  if (file->ReadString(chainLinkModel) == 0)
    return false;

  std::uint32_t valueCountBits = 0;
  if (!ReadBig32(file, valueCountBits))
    return false;
  const int valueCount = static_cast<int>(valueCountBits);
  if (valueCount > splineSettings.size)
    splineSettings.ResizeForPort(valueCount);
  for (int index = 0; index < valueCount; ++index) {
    splinePoint_t* point = &splineSettings.Alloc();
    if (!ReadBigVec3(file, point->value) || !ReadBigFloat(file, point->time))
      return false;
  }
  if (!ReadExact(file, &genInfo.rotateRandom, 1) ||
      !ReadExact(file, &genInfo.smoothSkinning, 1) ||
      !ReadBigFloat(file, genInfo.startRotation)) {
    return false;
  }

  idCurve_NURBS<idVec3> spline;
  for (int index = 0; index < splineSettings.Num(); ++index) {
    spline.AddValue(splineSettings[index].time, splineSettings[index].value,
                    1.0f);
  }
  BuildModel(spline);

  std::uint32_t savedTransformBits = 0;
  if (!ReadBig32(file, savedTransformBits))
    return false;
  const int savedTransforms = static_cast<int>(savedTransformBits);
  for (int index = 0; index < savedTransforms; ++index) {
    idMat3 axis{};
    idVec3 origin{};
    if (!ReadBigMat3(file, axis) || !ReadBigVec3(file, origin))
      return false;
    if (index < numLinks)
      SetJoint(transforms[index], origin, axis);
  }
  dirtyFlags |= kTransformsDirty;

  bool savedMorph = false;
  if (!ReadExact(file, &savedMorph, 1))
    return false;
  if (savedMorph) {
    for (int index = 0; index < savedTransforms; ++index) {
      std::uint8_t amount = 0;
      if (!ReadExact(file, &amount, 1))
        return false;
      if (numMorphVertices != 0)
        morphMap[4 * index * numMorphLinkVertices + 3] = amount;
    }
    dirtyFlags |= kMorphDirty;
  }
  return true;
}

void idRenderModelChain::Build(const chainGenerateInfo_t& generateInfo,
                               const idStr& modelName,
                               const idMaterial* morphMaterialValue,
                               const idCurve_NURBS<idVec3>& spline) {
  splineSettings.ClearFree();
  splineSettings.ResizeForPort(spline.GetNumValues());
  for (int index = 0; index < spline.GetNumValues(); ++index) {
    splinePoint_t* point = &splineSettings.Alloc();
    point->value = spline.GetValue(index);
    point->time = spline.GetTime(index);
  }
  chainLinkModel = modelName;
  morphMaterial = morphMaterialValue;
  genInfo = generateInfo;
  BuildModel(spline);
}

void idRenderModelChain::BuildModel(const idCurve_NURBS<idVec3>& spline) {
  FreeSurfaces();

  idStaticModel* staticModel =
      staticModelLookup != nullptr
          ? staticModelLookup(chainLinkModel.data != nullptr
                                  ? chainLinkModel.data
                                  : "",
                              true)
          : nullptr;
  if (staticModel == nullptr)
    return;

  const idBounds& modelBounds = staticModel->referenceBounds;
  const float width = modelBounds.b[1].x - modelBounds.b[0].x;
  const float depth = modelBounds.b[1].y - modelBounds.b[0].y;
  const float height = modelBounds.b[1].z - modelBounds.b[0].z;
  const float linkAdvance = height - depth * 2.0f;
  const float curveLength = spline.GetNumValues() > 0
                                ? spline.GetLengthForTime(
                                      spline.GetTime(spline.GetNumValues() - 1))
                                : 0.0f;
  linkSize = std::max(std::max(width, linkAdvance), std::max(depth, height));
  numLinks = 1;
  if (curveLength > height) {
    numLinks =
        std::abs(static_cast<int>((curveLength - height) / linkAdvance +
                                  0.94999999f)) +
        1;
  }

  std::vector<std::unique_ptr<idTriangles>> sourceTriangles;
  sourceTriangles.reserve(static_cast<std::size_t>(staticModel->surfaces.Num()));
  int maximumIndexes = 0;
  for (int surfaceIndex = 0; surfaceIndex < staticModel->surfaces.Num();
       ++surfaceIndex) {
    idTriangles* copy = idTriangles::CreateStandardTriangles(
        staticModel->surfaces[surfaceIndex].geometry);
    PrepareSkinning(*copy, modelBounds, genInfo.smoothSkinning);
    maximumIndexes = std::max(maximumIndexes, copy->numIndexes);
    sourceTriangles.emplace_back(copy);
  }

  int maximumLinks = maximumIndexes != 0
                         ? std::min(256, 65536 / maximumIndexes)
                         : 0;
  numLinks = std::min(numLinks, maximumLinks);
  if (!sourceTriangles.empty() && morphMaterial != nullptr) {
    numMorphLinkVertices = sourceTriangles[0]->numVerts;
    numMorphVertices = numLinks * numMorphLinkVertices;
  }

  if (numLinks > 0)
    jointBuffers[0].AllocBufferObject(nullptr, numLinks);
  if (numMorphVertices > 0) {
    for (idVertexBuffer& buffer : morphBuffers) {
      buffer.AllocBufferObject(nullptr, 4 * numMorphVertices, 4, BU_DEFAULT);
    }
    morphMap.SetNum(4 * numMorphVertices);
    for (int index = 0; index < morphMap.Num(); index += 4) {
      morphMap[index] = 127;
      morphMap[index + 1] = 127;
      morphMap[index + 2] = 127;
      morphMap[index + 3] = 0;
    }
    dirtyFlags |= kMorphDirty;
  }
  dirtyFlags |= kTransformsDirty;

  transforms.ResizeForPort(numLinks);
  std::uint32_t random = static_cast<std::uint32_t>(idStr::Hash(
      chainLinkModel.data != nullptr ? chainLinkModel.data : ""));
  if (numLinks > 0 && spline.GetNumValues() > 0) {
    if (spline.GetNumValues() <= 2) {
      const idVec3 start = spline.GetValue(0);
      const idVec3 end = spline.GetValue(spline.GetNumValues() - 1);
      const idVec3 direction = Normalize(end - start);
      const float spacing = (end - start).Length() / static_cast<float>(numLinks);
      for (int link = 0; link < numLinks; ++link) {
        const idVec3 origin = start + direction * (spacing * link);
        const idMat3 axis = LinkAxis(
            direction, LinkRotation(random, link, genInfo.rotateRandom,
                                    genInfo.startRotation));
        SetJoint(transforms.Alloc(), origin, axis);
      }
    } else {
      const float spacing = curveLength / static_cast<float>(numLinks);
      for (int link = 0; link < numLinks; ++link) {
        const float time = spline.GetTimeForLength(spacing * link, 0.1f);
        const float nextTime =
            spline.GetTimeForLength(spacing * (link + 1), 0.1f);
        const idVec3 origin = spline.GetCurrentValue(time);
        const idVec3 next = spline.GetCurrentValue(nextTime);
        const idVec3 direction = Normalize(next - origin);
        const idMat3 axis = LinkAxis(
            direction, LinkRotation(random, link, genInfo.rotateRandom,
                                    genInfo.startRotation));
        SetJoint(transforms.Alloc(), origin, axis);
      }
    }
  }

  for (int surfaceIndex = 0; surfaceIndex < staticModel->surfaces.Num();
       ++surfaceIndex) {
    idTriangles* source = sourceTriangles[surfaceIndex].get();
    if (source == nullptr)
      continue;
    auto* geometry = new idTriangles;
    const int totalVertices = source->numVerts * numLinks;
    const int totalIndexes = source->numIndexes * numLinks;
    idTriangles::AllocStaticTriSurfVerts(geometry, totalVertices);
    idTriangles::AllocStaticTriSurfIndexes(geometry, totalIndexes);

    for (int link = 0; link < numLinks; ++link) {
      idDrawVert* destination = geometry->verts + source->numVerts * link;
      for (int vertexIndex = 0; vertexIndex < source->numVerts; ++vertexIndex) {
        destination[vertexIndex] = source->verts[vertexIndex];
        idDrawVert& vertex = destination[vertexIndex];
        if (link == numLinks - 1) {
          std::memset(vertex.color, static_cast<unsigned char>(link),
                      sizeof(vertex.color));
          vertex.tangent[3] = 0;
          vertex.normal[3] = 0;
        } else {
          for (std::uint8_t& joint : vertex.color)
            joint = static_cast<std::uint8_t>(joint + link);
          if (genInfo.smoothSkinning)
            vertex.xyz.z = 0.0f;
        }
      }
      for (int index = 0; index < source->numIndexes; ++index) {
        geometry->indexes[source->numIndexes * link + index] =
            static_cast<std::uint16_t>(source->indexes[index] +
                                       source->numVerts * link);
      }
    }
    geometry->numVerts = totalVertices;
    geometry->numIndexes = totalIndexes;
    geometry->bounds = source->bounds;
    idTriangles::UpdateVertexBuffer(geometry);
    idTriangles::UpdateIndexBuffer(geometry);

    idRenderModelSurface surface{};
    const idStaticModelSurface& staticSurface =
        staticModel->surfaces[surfaceIndex];
    surface.material = staticSurface.material;
    surface.materialNum = staticSurface.materialNum;
    surface.extraGLState = staticSurface.extraGLState;
    surface.geometry = geometry;
    surface.joints = new idJointBuffer;
    surface.joints->Reference(&jointBuffers[currentJointBuffer], 0, numLinks);
    if (surfaceIndex == 0 && numMorphVertices > 0) {
      surface.morphMap = new idVertexBuffer;
      surface.morphMap->Reference(&morphBuffers[currentMorphBuffer], 0,
                                  4 * numMorphVertices);
    }
    AddSurface(surface);

    if (surfaceIndex == 0 && numMorphVertices > 0) {
      idRenderModelSurface morphSurface{};
      morphSurface.material = morphMaterial;
      morphSurface.geometry = geometry;
      morphSurface.geometryIsReference = true;
      morphSurface.joints = new idJointBuffer;
      morphSurface.joints->Reference(&jointBuffers[currentJointBuffer], 0,
                                     numLinks);
      morphSurface.morphMap = new idVertexBuffer;
      morphSurface.morphMap->Reference(&morphBuffers[currentMorphBuffer], 0,
                                       4 * numMorphVertices);
      AddSurface(morphSurface);
    }

    geometry->FreeCPUData();
    source->FreeCPUData();
  }
  FinishSurfaces();
}
