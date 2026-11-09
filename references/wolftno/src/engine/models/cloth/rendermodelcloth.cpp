#include "engine/models/cloth/rendermodelcloth.h"

#include "engine/models/serializer.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

idCVar clothsim_useJobs(
    "clothsim_useJobs", "1", CVAR_INTEGER,
    "0 = off, 1 = threaded, 2 = SPURS, 3 = SPURS emulation", 0.0f, 3.0f);
idCVar r_skipCloth("r_skipCloth", "0", CVAR_INTEGER,
                   "1 = skip cloth rendering", 0.0f, 1.0f,
                   idCmdSystem::ArgCompletion_Integer<0, 1>);
idCVar g_clothTestNormalWind("g_clothTestNormalWind", "1", CVAR_BOOL, "");
idCVar g_clothTestOverideIterations(
    "g_clothTestOverideIterations", "0", CVAR_INTEGER, "");

namespace {

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

std::uint32_t FloatBits(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float BitsFloat(std::uint32_t bits) {
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

std::size_t WriteBig32(idFile* file, std::uint32_t value) {
  const std::uint32_t swapped = ByteSwap32(value);
  return file->Write(&swapped, sizeof(swapped));
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  std::uint32_t swapped = 0;
  if (file->Read(&swapped, sizeof(swapped)) == 0)
    return false;
  value = ByteSwap32(swapped);
  return true;
}

void ClearBounds(idBounds& bounds) {
  bounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
}

void AddPoint(idBounds& bounds, const idVec3& point) {
  bounds.b[0].x = std::fmin(bounds.b[0].x, point.x);
  bounds.b[0].y = std::fmin(bounds.b[0].y, point.y);
  bounds.b[0].z = std::fmin(bounds.b[0].z, point.z);
  bounds.b[1].x = std::fmax(bounds.b[1].x, point.x);
  bounds.b[1].y = std::fmax(bounds.b[1].y, point.y);
  bounds.b[1].z = std::fmax(bounds.b[1].z, point.z);
}

int RenderTimeFromView(const idRenderView* view) {
  int renderTime = 0;
  std::memcpy(&renderTime,
              reinterpret_cast<const std::uint8_t*>(view) + 0x1a40,
              sizeof(renderTime));
  return renderTime;
}

idParallelJobList* JobListFromUpdateTools(
    const idRenderModelUpdateTools* updateTools) {
  idParallelJobList* jobList = nullptr;
  std::memcpy(&jobList, updateTools, sizeof(jobList));
  return jobList;
}

}  // namespace

template void idCmdSystem::ArgCompletion_Integer<0, 1>(idAutoComplete*);
template idList<idRenderModelCloth::clothAttachInfo_t, 5>&
idList<idRenderModelCloth::clothAttachInfo_t, 5>::operator=(
    const idList<idRenderModelCloth::clothAttachInfo_t, 5>&);
template bool idList<idRenderModelCloth::clothAttachInfo_t, 5>::Resize(int);
template int idList<idRenderModelCloth::clothAttachInfo_t, 5>::Append(
    const idRenderModelCloth::clothAttachInfo_t&);

template <>
std::size_t idFile::WriteBigArray<idVec4>(const idVec4* values, int count) {
  std::size_t bytes = 0;
  for (int index = 0; index < count; ++index) {
    bytes += WriteBig32(this, FloatBits(values[index].x));
    bytes += WriteBig32(this, FloatBits(values[index].y));
    bytes += WriteBig32(this, FloatBits(values[index].z));
    bytes += WriteBig32(this, FloatBits(values[index].w));
  }
  return bytes;
}

template <>
std::size_t idFile::ReadBigArray<idVec4>(idVec4* values, int count) {
  std::size_t bytes = 0;
  for (int index = 0; index < count; ++index) {
    std::uint32_t components[4]{};
    for (std::uint32_t& component : components) {
      std::uint32_t swapped = 0;
      const std::size_t read = Read(&swapped, sizeof(swapped));
      if (read == 0)
        return bytes;
      bytes += read;
      component = ByteSwap32(swapped);
    }
    values[index] = {BitsFloat(components[0]), BitsFloat(components[1]),
                     BitsFloat(components[2]), BitsFloat(components[3])};
  }
  return bytes;
}

idRenderModelCloth::idRenderModelCloth(const idDeclCloth* clothDecl)
    : clothSystem(clothDecl) {
  renderModelType = RENDERMODEL_CLOTH;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x01u;
}

idRenderModelCloth::~idRenderModelCloth() {
  delete clothSimulation;
  clothSimulation = nullptr;
  for (idVertexBuffer& buffer : vertexBuffer) {
    if (buffer.IsMapped())
      buffer.UnmapBuffer();
    buffer.FreeBufferObject();
  }
  indexBuffer.FreeBufferObject();
  delete clothBounds;
  clothBounds = nullptr;
  delete clothParms;
  clothParms = nullptr;
}

void idRenderModelCloth::BuildClothModel() {
  if (clothSystem == nullptr) {
    idLib::Warning(
        "idRenderModelCloth::BuildClothModel() - clothSystem is NULL");
    return;
  }
  if (clothSimulation == nullptr) {
    idLib::Warning(
        "idRenderModelCloth::BuildClothModel() - clothSimulation is NULL");
    return;
  }

  if (clothParms == nullptr)
    clothParms = new clothParms_t{};
  if (clothBounds == nullptr)
    clothBounds = new idBounds{};

  const int modelHeight = clothSimulation->height;
  const int modelWidth = clothSimulation->width;
  const float horizontalSpacing = clothSimulation->hSpacing;
  const float verticalSpacing = clothSimulation->vSpacing;
  if (modelWidth <= 1 || modelHeight <= 1 || horizontalSpacing < 0.001f ||
      verticalSpacing < 0.001f) {
    idLib::Warning(
        "idRenderModelCloth::BuildClothModel() - bad width, height, hSpacing, or vSpacing");
    return;
  }

  const int numIndexes =
      rect ? 6 * (modelHeight - 1) * (modelWidth - 1)
           : (modelHeight - 1) * (6 * modelHeight - 6);
  numVerts = rect ? modelHeight * modelWidth : modelHeight * modelHeight;

  FreeSurfaces();
  deferredVerts = nullptr;
  for (idVertexBuffer& buffer : vertexBuffer) {
    if (buffer.IsMapped())
      buffer.UnmapBuffer();
    buffer.FreeBufferObject();
  }
  indexBuffer.FreeBufferObject();

  std::vector<std::uint16_t> indices(static_cast<std::size_t>(numIndexes));
  int outputIndex = 0;
  if (rect) {
    for (int row = 0; row < modelHeight - 1; ++row) {
      for (int column = 0; column < modelWidth - 1; ++column) {
        const int first = row * modelWidth + column;
        indices[outputIndex++] = static_cast<std::uint16_t>(first);
        indices[outputIndex++] = static_cast<std::uint16_t>(first + 1);
        indices[outputIndex++] =
            static_cast<std::uint16_t>(first + modelWidth);
        indices[outputIndex++] = static_cast<std::uint16_t>(first + 1);
        indices[outputIndex++] =
            static_cast<std::uint16_t>(first + modelWidth + 1);
        indices[outputIndex++] =
            static_cast<std::uint16_t>(first + modelWidth);
      }
    }
  } else {
    for (int row = 0; row < modelHeight - 1; ++row) {
      const int rowWidth = 2 * row + 1;
      int first = row * row;
      indices[outputIndex++] = static_cast<std::uint16_t>(first);
      indices[outputIndex++] =
          static_cast<std::uint16_t>(first + rowWidth);
      indices[outputIndex++] =
          static_cast<std::uint16_t>(first + rowWidth + 1);
      for (int column = 0; column < rowWidth - 1; ++column) {
        indices[outputIndex++] = static_cast<std::uint16_t>(first);
        indices[outputIndex++] =
            static_cast<std::uint16_t>(first + rowWidth + 1);
        indices[outputIndex++] =
            static_cast<std::uint16_t>(first + rowWidth + 2);
        indices[outputIndex++] = static_cast<std::uint16_t>(first + 1);
        indices[outputIndex++] = static_cast<std::uint16_t>(first);
        indices[outputIndex++] =
            static_cast<std::uint16_t>(first + rowWidth + 2);
        ++first;
      }
      indices[outputIndex++] = static_cast<std::uint16_t>(first);
      indices[outputIndex++] =
          static_cast<std::uint16_t>(first + rowWidth + 1);
      indices[outputIndex++] =
          static_cast<std::uint16_t>(first + rowWidth + 2);
    }
  }
  indexBuffer.AllocBufferObject(indices.data(), numIndexes * 2, BU_DEFAULT);

  std::vector<idDrawVert> vertices(static_cast<std::size_t>(numVerts));
  if (rect) {
    int vertexIndex = 0;
    for (int row = 0; row < modelHeight; ++row) {
      for (int column = 0; column < modelWidth; ++column, ++vertexIndex) {
        idDrawVert& vertex = vertices[vertexIndex];
        vertex.xyz = {column * horizontalSpacing, row * verticalSpacing, 0.0f};
        vertex.st = {
            vertex.xyz.y / ((modelHeight - 1) * verticalSpacing),
            vertex.xyz.x / ((modelWidth - 1) * horizontalSpacing)};
        std::memset(vertex.color, 0xff, sizeof(vertex.color));
      }
    }
  } else {
    int vertexIndex = 0;
    int rowWidth = 1;
    const float fullWidth = (modelWidth - 1) * horizontalSpacing;
    float firstX = fullWidth * 0.5f;
    for (int row = 0; row < modelHeight; ++row, rowWidth += 2) {
      float x = firstX;
      const float y = row * verticalSpacing;
      for (int column = 0; column < rowWidth;
           ++column, ++vertexIndex, x += horizontalSpacing) {
        idDrawVert& vertex = vertices[vertexIndex];
        vertex.xyz = {x, y, 128.0f};
        vertex.st = {y / ((modelHeight - 1) * verticalSpacing),
                     x / fullWidth};
        std::memset(vertex.color, 0xff, sizeof(vertex.color));
      }
      firstX -= horizontalSpacing;
    }
  }

  idBounds initialBounds;
  ClearBounds(initialBounds);
  for (const idDrawVert& vertex : vertices)
    AddPoint(initialBounds, vertex.xyz);

  for (idVertexBuffer& buffer : vertexBuffer) {
    buffer.AllocBufferObject(vertices.data(), numVerts * sizeof(idDrawVert),
                             31, BU_DYNAMIC);
  }

  auto* triangles = new idTriangles{};
  triangles->numVerts = numVerts;
  triangles->numIndexes = numIndexes;
  triangles->vertexBuffer.Reference(vertexBuffer[0]);
  triangles->indexBuffer.Reference(indexBuffer);
  triangles->bounds = initialBounds;

  idRenderModelSurface surface{};
  surface.material = clothSystem->material;
  surface.geometry = triangles;
  AddSurface(surface);

  const float boundsExpansion =
      std::fmax(modelWidth * horizontalSpacing,
                modelHeight * verticalSpacing);
  triangles->bounds.b[0].x -= boundsExpansion;
  triangles->bounds.b[0].y -= boundsExpansion;
  triangles->bounds.b[0].z -= boundsExpansion;
  triangles->bounds.b[1].x += boundsExpansion;
  triangles->bounds.b[1].y += boundsExpansion;
  triangles->bounds.b[1].z += boundsExpansion;
  FinishSurfaces();

  *clothSimulation->clothBoundsFromJob = triangles->bounds;
  clothSimulation->clothBounds = triangles->bounds;
  *clothBounds = clothSimulation->clothBounds;
}

void idRenderModelCloth::CreateClothSim(
    const idDeclCloth* clothDecl, idClothType_t newClothType,
    idList<clothAttachInfo_t, 5>* newAttachPoints,
    idList<idVec4, 5>* newCollisionSpheres, int widthOverride,
    int heightOverride, float hSpacingOverride, float vSpacingOverride) {
  if (newClothType == CLOTH_MODEL_INVALID)
    idLib::Error("Invalid cloth type");

  clothSystem = clothDecl;
  clothType = newClothType;
  attachPoints = *newAttachPoints;
  collisionSpheres = *newCollisionSpheres;
  width = clothDecl->width;
  height = clothDecl->height;
  hSpacing = clothDecl->hSpacing;
  vSpacing = clothDecl->vSpacing;
  if (widthOverride > 0)
    width = widthOverride;
  if (heightOverride > 0)
    height = heightOverride;
  if (hSpacingOverride > 0.0f)
    hSpacing = hSpacingOverride;
  if (vSpacingOverride > 0.0f)
    vSpacing = vSpacingOverride;
  rect = newClothType != CLOTH_FLAG_TRIANGLE &&
         newClothType != CLOTH_TARP_TRIANGLE;

  clothSimulation =
      new idClothSim(width, height, hSpacing, vSpacing, clothDecl, rect);
  for (int index = 0; index < newAttachPoints->num; ++index) {
    clothAttachInfo_t& attachment = newAttachPoints->list[index];
    int particleIndex = 0;
    switch (attachment.clothAnchorType) {
      case clothAttachInfo_t::ANCHOR_CORNER1:
        attachment.col = 0;
        attachment.row = 0;
        break;
      case clothAttachInfo_t::ANCHOR_CORNER2:
        attachment.col = 0;
        attachment.row = height - 1;
        particleIndex = clothSimulation->height * clothSimulation->height - 1;
        break;
      case clothAttachInfo_t::ANCHOR_CORNER3:
        attachment.col = width - 1;
        attachment.row = height - 1;
        particleIndex = clothSimulation->height * clothSimulation->height -
                        clothSimulation->width;
        break;
      case clothAttachInfo_t::ANCHOR_CORNER4:
        attachment.col = width - 1;
        attachment.row = 0;
        break;
      case clothAttachInfo_t::ANCHOR_MID1:
        attachment.col = 0;
        attachment.row = height / 2;
        break;
      case clothAttachInfo_t::ANCHOR_MID2:
        attachment.col = width / 2;
        attachment.row = height - 1;
        break;
      case clothAttachInfo_t::ANCHOR_MID3:
        attachment.col = width - 1;
        attachment.row = height / 2;
        break;
      case clothAttachInfo_t::ANCHOR_MID4:
        attachment.col = width / 2;
        attachment.row = 0;
        break;
      case clothAttachInfo_t::ANCHOR_CENTER:
        attachment.col = width / 2;
        attachment.row = height / 2;
        break;
      case clothAttachInfo_t::ANCHOR_DEFINED:
        if (attachment.row > height || attachment.col > width)
          continue;
        break;
      default:
        break;
    }
    if (rect)
      particleIndex = attachment.col + clothSimulation->width * attachment.row;
    clothSimulation->SetAnchor(particleIndex, attachment.tempAnchor);
    clothSimulation->SetAnchorPosition(particleIndex, attachment.point);
  }

  for (int index = 0; index < newCollisionSpheres->num; ++index) {
    const idVec4& value = newCollisionSpheres->list[index];
    clothSimulation->AddCollisionSphere({{value.x, value.y, value.z}, value.w},
                                        true);
  }
  BuildClothModel();
  CommitThisFrame();
}

void idRenderModelCloth::SerializeSnapshot(idSerializer* serializer,
                                           bool serializeParmBlock) {
  idRenderModel::SerializeSnapshot(serializer, serializeParmBlock);
  const bool hadNoClothSystem = clothSystem == nullptr;
  if (serializer->writing) {
    idDeclCloth::resourceList.WriteResourceToMsg(clothSystem, serializer->msg);
  } else {
    clothSystem = static_cast<const idDeclCloth*>(
        idDeclCloth::resourceList.ReadResourceFromMsg(serializer->msg));
  }

  int serializedClothType = clothType;
  if (serializer->writing)
    serializer->msg->WriteBits(serializedClothType, 3);
  else
    serializedClothType = serializer->msg->ReadBits(3);
  clothType = static_cast<idClothType_t>(serializedClothType);

  int numAttachments = attachPoints.num;
  if (serializer->writing)
    serializer->msg->WriteBits(numAttachments, 6);
  else
    numAttachments = serializer->msg->ReadBits(6);
  attachPoints.SetNum(numAttachments);
  for (int index = 0; index < numAttachments; ++index) {
    clothAttachInfo_t& attachment = attachPoints[index];
    for (int component = 0; component < 3; ++component) {
      if (serializer->writing) {
        serializer->msg->WriteBits(
            static_cast<int>(FloatBits(attachment.point[component])), 32);
      } else {
        attachment.point[component] = BitsFloat(
            static_cast<std::uint32_t>(serializer->msg->ReadBits(32)));
      }
    }
    if (serializer->writing) {
      serializer->msg->WriteBits(attachment.row, 5);
      serializer->msg->WriteBits(attachment.col, 5);
      serializer->msg->WriteBits(attachment.clothAnchorType, 4);
    } else {
      attachment.row = serializer->msg->ReadBits(5);
      attachment.col = serializer->msg->ReadBits(5);
      attachment.clothAnchorType =
          static_cast<clothAttachInfo_t::idClothAnchorType_t>(
              serializer->msg->ReadBits(4));
    }
  }

  int numCollisions = collisionSpheres.num;
  if (serializer->writing)
    serializer->msg->WriteBits(numCollisions, 5);
  else
    numCollisions = serializer->msg->ReadBits(5);
  collisionSpheres.SetNum(numCollisions);
  for (int index = 0; index < collisionSpheres.num; ++index) {
    for (int component = 0; component < 4; ++component) {
      if (serializer->writing) {
        serializer->msg->WriteBits(
            static_cast<int>(FloatBits(collisionSpheres[index][component])), 32);
      } else {
        collisionSpheres[index][component] = BitsFloat(
            static_cast<std::uint32_t>(serializer->msg->ReadBits(32)));
      }
    }
  }

  if (serializer->writing) {
    serializer->msg->WriteBits(width, 6);
    serializer->msg->WriteBits(height, 6);
    serializer->msg->WriteBits(static_cast<int>(FloatBits(hSpacing)), 32);
    serializer->msg->WriteBits(static_cast<int>(FloatBits(vSpacing)), 32);
  } else {
    width = serializer->msg->ReadBits(6);
    height = serializer->msg->ReadBits(6);
    hSpacing = BitsFloat(
        static_cast<std::uint32_t>(serializer->msg->ReadBits(32)));
    vSpacing = BitsFloat(
        static_cast<std::uint32_t>(serializer->msg->ReadBits(32)));
  }

  if (hadNoClothSystem) {
    const bool deferred = deferredPositionInitialized != 0;
    clothSimulation->SetOriginAxis(
        deferred ? deferredOrigin : g.origin,
        deferred ? deferredAxis : g.axis, false);
    clothSimulation->windDirection = {};
    clothSimulation->timeDelta = 0.016000001f;
  }

  idVec3 wind = clothParms->windDirection;
  for (int component = 0; component < 3; ++component) {
    if (serializer->writing) {
      serializer->msg->WriteBits(
          static_cast<int>(FloatBits(wind[component])), 32);
    } else {
      wind[component] = BitsFloat(
          static_cast<std::uint32_t>(serializer->msg->ReadBits(32)));
    }
  }
  if (!serializer->writing)
    clothSimulation->windDirection = wind;
}

void idRenderModelCloth::Save(idFile* file) const {
  idRenderModel::Save(file);
  const std::uint8_t hasClothSystem = clothSystem != nullptr;
  file->Write(&hasClothSystem, 1);
  if (clothSystem != nullptr)
    file->WriteString(clothSystem->GetName());
  WriteBig32(file, static_cast<std::uint32_t>(clothType));
  WriteBig32(file, static_cast<std::uint32_t>(attachPoints.num));
  for (int index = 0; index < attachPoints.num; ++index) {
    const clothAttachInfo_t& attachment = attachPoints[index];
    WriteBig32(file, FloatBits(attachment.point.x));
    WriteBig32(file, FloatBits(attachment.point.y));
    WriteBig32(file, FloatBits(attachment.point.z));
    WriteBig32(file, static_cast<std::uint32_t>(attachment.clothAnchorType));
    WriteBig32(file, static_cast<std::uint32_t>(attachment.row));
    WriteBig32(file, static_cast<std::uint32_t>(attachment.col));
  }
  WriteBig32(file, static_cast<std::uint32_t>(width));
  WriteBig32(file, static_cast<std::uint32_t>(height));
  WriteBig32(file, FloatBits(vSpacing));
  WriteBig32(file, FloatBits(hSpacing));
  WriteBig32(file, static_cast<std::uint32_t>(collisionSpheres.num));
  if (collisionSpheres.num > 0)
    file->WriteBigArray(collisionSpheres.list, collisionSpheres.num);
}

bool idRenderModelCloth::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  std::uint8_t hasClothSystem = 0;
  if (file->Read(&hasClothSystem, 1) == 0)
    return false;
  if (hasClothSystem != 0) {
    idStr clothName;
    if (file->ReadString(clothName) == 0)
      return false;
    clothSystem = static_cast<const idDeclCloth*>(
        idDeclCloth::resourceList.FindWithInheritance(clothName.data, true, true));
  }

  std::uint32_t value = 0;
  if (!ReadBig32(file, value))
    return false;
  clothType = static_cast<idClothType_t>(value);
  if (!ReadBig32(file, value))
    return false;
  const int numAttachments = static_cast<int>(value);
  attachPoints.ClearFree();
  for (int index = 0; index < numAttachments; ++index) {
    clothAttachInfo_t attachment{};
    std::uint32_t components[3]{};
    if (!ReadBig32(file, components[0]) ||
        !ReadBig32(file, components[1]) ||
        !ReadBig32(file, components[2]))
      return false;
    attachment.point = {BitsFloat(components[0]), BitsFloat(components[1]),
                        BitsFloat(components[2])};
    if (!ReadBig32(file, value))
      return false;
    attachment.clothAnchorType =
        static_cast<clothAttachInfo_t::idClothAnchorType_t>(value);
    if (!ReadBig32(file, value))
      return false;
    attachment.row = static_cast<int>(value);
    if (!ReadBig32(file, value))
      return false;
    attachment.col = static_cast<int>(value);
    attachment.tempAnchor = false;
    attachPoints.Append(attachment);
  }

  if (!ReadBig32(file, value))
    return false;
  width = static_cast<int>(value);
  if (!ReadBig32(file, value))
    return false;
  height = static_cast<int>(value);
  if (!ReadBig32(file, value))
    return false;
  vSpacing = BitsFloat(value);
  if (!ReadBig32(file, value))
    return false;
  hSpacing = BitsFloat(value);
  if (!ReadBig32(file, value))
    return false;
  const int numCollisions = static_cast<int>(value);
  collisionSpheres.SetNum(numCollisions);
  if (numCollisions > 0 &&
      file->ReadBigArray(collisionSpheres.list, numCollisions) == 0)
    return false;

  CreateClothSim(clothSystem, clothType, &attachPoints, &collisionSpheres,
                 0, 0, 0.0f, 0.0f);
  return true;
}

void idRenderModelCloth::SetupClothJob(idParallelJobList* parallelJobList,
                                       idDrawVert* vertices,
                                       int renderTime) {
  if (clothSimulation->numClothParticles > 100)
    return;

  const int deferred = clothSimulation->currentDeferred;
  clothParms->type = CLOTH_FLAG;
  clothParms->numIterations = clothSimulation->numIterations;
  if (g_clothTestOverideIterations.valueInteger != 0)
    clothParms->numIterations = g_clothTestOverideIterations.valueInteger;
  clothSimulation->numIterations = 1;
  clothSimulation->temporaryCollisionSpheres[deferred].Append(
      clothSimulation->permanentCollisionSpheres);
  clothParms->cloth = clothSimulation->cloth;
  clothParms->numClothParticles = clothSimulation->numClothParticles;
  clothParms->springs = clothSimulation->springs.list;
  clothParms->numSprings = clothSimulation->springs.num;
  clothParms->collisions =
      clothSimulation->temporaryCollisionSpheres[deferred].list;
  clothParms->numCollisions =
      clothSimulation->temporaryCollisionSpheres[deferred].num;
  clothParms->collisionPlane = clothSimulation->collisionPlane[deferred];
  clothParms->hasCollisionPlane =
      clothParms->collisionPlane.a != 0.0f ||
      clothParms->collisionPlane.b != 0.0f ||
      clothParms->collisionPlane.c != 0.0f;
  clothParms->collisionFriction = clothSimulation->clothDecl->collisionFriction;
  clothParms->weaponTraces = clothSimulation->weaponTraces[deferred].list;
  clothParms->numWeaponTraces = clothSimulation->weaponTraces[deferred].num;
  clothParms->vertices = vertices;
  clothParms->bounds = clothBounds;
  clothParms->org = clothSimulation->origin;
  clothParms->axis = clothSimulation->axis;
  clothParms->friction = clothSystem->friction;
  clothParms->gravity = clothSimulation->gravity;
  clothParms->windDirection = clothSimulation->windDirection;
  clothParms->width = clothSimulation->width;
  clothParms->height = clothSimulation->height;
  clothParms->vSpacing = clothSimulation->vSpacing;
  clothParms->hSpacing = clothSimulation->hSpacing;
  clothParms->rect = clothSimulation->isRect;
  clothParms->timeDelta =
      lastRenderTime == -1
          ? clothSimulation->timeDelta
          : std::fmin(clothSimulation->timeDelta,
                      (renderTime - lastRenderTime) * 0.001f);
  clothParms->normalWind = g_clothTestNormalWind.valueInteger != 0;
  lastRenderTime = renderTime;

  if (parallelJobList != nullptr &&
      (clothsim_useJobs.valueInteger == 1 || vertices == nullptr)) {
    parallelJobList->AddJob(reinterpret_cast<jobRun_t>(ClothGenJob),
                            clothParms);
  } else {
    ClothGenJob(clothParms);
  }
  *clothSimulation->clothBoundsFromJob = *clothBounds;
  clothSimulation->Swap();
}

void idRenderModelCloth::RunSimulation(idParallelJobList* parallelJobList,
                                       bool writeVerts) {
  std::unique_ptr<idDrawVert[]> vertices;
  if (writeVerts)
    vertices = std::make_unique<idDrawVert[]>(numVerts);
  SetupClothJob(parallelJobList, vertices.get(), -1);
  if (vertices != nullptr) {
    for (idVertexBuffer& buffer : vertexBuffer) {
      void* destination = buffer.MapBuffer(BM_WRITE_NOSYNC);
      std::memcpy(destination, vertices.get(),
                  static_cast<std::size_t>(numVerts) * sizeof(idDrawVert));
      buffer.UnmapBuffer();
    }
  }
}

bool idRenderModelCloth::UpdateInView(
    const idRenderView* currentView, const idRenderView*,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipCloth.valueInteger != 0 || clothSimulation == nullptr ||
      surfaces.num == 0) {
    return false;
  }
  if (deferredVerts != nullptr && vertexBuffer[currentIndex].IsMapped())
    vertexBuffer[currentIndex].UnmapBuffer();

  idTriangles* geometry = surfaces[0].geometry;
  geometry->vertexBuffer.Reference(vertexBuffer[currentIndex]);
  geometry->bounds = *clothBounds;
  referenceBounds = *clothBounds;
  currentIndex = (currentIndex + 1) % NUM_VERTEX_BUFFERS;
  deferredVerts = static_cast<idDrawVert*>(
      vertexBuffer[currentIndex].MapBuffer(BM_WRITE_NOSYNC));
  SetupClothJob(JobListFromUpdateTools(updateTools), deferredVerts,
                RenderTimeFromView(currentView));
  return true;
}
