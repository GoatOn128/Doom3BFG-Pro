#include "engine/models/discreteanimation/rendermodeldiscreteanimation.h"

#include "engine/decls/declbreakable.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

idCVar r_noBreakableShadows(
    "r_noBreakableShadows", "0", CVAR_BOOL,
    "if 1, don't allow any dimshadows on breakables");

namespace {

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffu) << 24) |
         ((value & 0x0000ff00u) << 8) |
         ((value & 0x00ff0000u) >> 8) |
         ((value & 0xff000000u) >> 24);
}

bool WriteBig32(idFile* file, std::uint32_t value) {
  value = ByteSwap32(value);
  return file != nullptr && file->Write(&value, sizeof(value)) == sizeof(value);
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  if (file == nullptr || file->Read(&value, sizeof(value)) != sizeof(value))
    return false;
  value = ByteSwap32(value);
  return true;
}

std::uint32_t FloatBits(float value) {
  std::uint32_t bits{};
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float BitsFloat(std::uint32_t bits) {
  float value{};
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

void JointToTransform(const idJointMat& joint, idVec3& position,
                      idMat3& orientation) {
  position = {joint.mat[3], joint.mat[7], joint.mat[11]};
  orientation[0] = {joint.mat[0], joint.mat[4], joint.mat[8]};
  orientation[1] = {joint.mat[1], joint.mat[5], joint.mat[9]};
  orientation[2] = {joint.mat[2], joint.mat[6], joint.mat[10]};
}

void TransformToJoint(idJointMat& joint, const idVec3& position,
                      const idMat3& orientation) {
  joint.mat[3] = position.x;
  joint.mat[7] = position.y;
  joint.mat[11] = position.z;
  joint.mat[0] = orientation[0].x;
  joint.mat[1] = orientation[1].x;
  joint.mat[2] = orientation[2].x;
  joint.mat[4] = orientation[0].y;
  joint.mat[5] = orientation[1].y;
  joint.mat[6] = orientation[2].y;
  joint.mat[8] = orientation[0].z;
  joint.mat[9] = orientation[1].z;
  joint.mat[10] = orientation[2].z;
}

void AddBounds(idBounds& destination, const idBounds& source) {
  destination.AddPoint(source.b[0]);
  destination.AddPoint(source.b[1]);
}

} // namespace

idRenderModelDiscreteAnimation::idRenderModelDiscreteAnimation(
    const char* modelName, const idDeclBreakable* decl)
    : breakableDecl(decl) {
  renderModelType = RENDERMODEL_DISCRETEANIMATION;
  modelTimestamp = -1;
  changed = false;
  currentJointBuffer = 0;
  LoadModel(modelName);
  if ((useDeferredPosition & (deferredPositionInitialized >> 1U) & 1U) == 0U)
    g.origin = vec3_origin;
  deferredOrigin = vec3_origin;
  if ((useDeferredPosition & (deferredPositionInitialized >> 1U) & 1U) == 0U)
    g.axis = mat3_identity;
  deferredAxis = mat3_identity;
  g.flags3 |= 8U;
}

idRenderModelDiscreteAnimation::~idRenderModelDiscreteAnimation() {
  FreeData();
}

void idRenderModelDiscreteAnimation::Update() {
  CommitThisFrame();
}

void idRenderModelDiscreteAnimation::SerializeSnapshot(
    idSerializer* serializer, bool serializeParmBlock) {
  idRenderModel::SerializeSnapshot(serializer, serializeParmBlock);
}

void idRenderModelDiscreteAnimation::GetPieceTransform(
    int index, idVec3& position, idMat3& orientation) const {
  JointToTransform(transforms[index], position, orientation);
}

void idRenderModelDiscreteAnimation::SetPieceTransform(
    int index, const idVec3& position, const idMat3& orientation) {
  TransformToJoint(transforms[index], position, orientation);
  collapsed[index] = false;
  changed = true;
}

void idRenderModelDiscreteAnimation::CollapsePiece(int index) {
  const idMat3 zeroOrientation{};
  TransformToJoint(transforms[index], vec3_origin, zeroOrientation);
  collapsed[index] = true;
  changed = true;
}

void idRenderModelDiscreteAnimation::FreeData() {
  FreeSurfaces();
  for (idJointBuffer& jointBuffer : jointBuffers)
    jointBuffer.FreeBufferObject();
  transforms.ClearFree();
  collapsed.ClearFree();
  modelTimestamp = -1;
  changed = true;
}

bool idRenderModelDiscreteAnimation::CommitSubclass() {
  if (!changed)
    return true;

  idBounds jointBounds;
  jointBounds.Clear();
  for (int index = 0; index < transforms.Num(); ++index) {
    if (collapsed[index])
      continue;
    jointBounds.AddPoint({transforms[index].mat[3], transforms[index].mat[7],
                          transforms[index].mat[11]});
  }

  currentJointBuffer = (currentJointBuffer + 1) & 1;
  jointBuffers[currentJointBuffer].Update(
      transforms.Num() > 0 ? transforms[0].mat : nullptr, transforms.Num());
  if (modelData != nullptr) {
    for (int index = 0; index < modelData->surfaces.Num(); ++index) {
      const idDiscreteAnimationModelSurface* dataSurface =
          modelData->surfaces[index];
      surfaces[index].joints->Reference(jointBuffers[currentJointBuffer],
                                        dataSurface->jointOffset,
                                        dataSurface->numJoints);
    }

    const float radius = modelData->maxRadius;
    jointBounds.b[0].x -= radius;
    jointBounds.b[0].y -= radius;
    jointBounds.b[0].z -= radius;
    jointBounds.b[1].x += radius;
    jointBounds.b[1].y += radius;
    jointBounds.b[1].z += radius;

    const int baseIndex = modelData->traceModels.Num();
    if (transforms.Num() > baseIndex) {
      idVec3 basePosition;
      idMat3 baseOrientation;
      JointToTransform(transforms[baseIndex], basePosition, baseOrientation);
      idBounds baseBounds;
      baseBounds.FromTransformedBounds(modelData->bounds, basePosition,
                                       baseOrientation, g.scale);
      if (jointBounds.b[0].x <= jointBounds.b[1].x)
        AddBounds(baseBounds, jointBounds);
      jointBounds = baseBounds;
    }
  }
  referenceBounds = jointBounds;
  changed = false;
  return true;
}

void idRenderModelDiscreteAnimation::SetBasePiecePosition(
    const idVec3& position, const idMat3& orientation) {
  const int baseIndex = modelData != nullptr ? modelData->traceModels.Num() : 0;
  if (transforms.Num() <= baseIndex)
    return;
  TransformToJoint(transforms[baseIndex], position, orientation);
  collapsed[baseIndex] = false;
  changed = true;
}

void idRenderModelDiscreteAnimation::GetPieceTraceModel(
    int index, idTraceModel& traceModel) {
  traceModel = modelData->traceModels[index];
}

const idMaterial* idRenderModelDiscreteAnimation::GetPieceTraceMaterial(
    int index) const {
  return modelData->traceMaterials[index];
}

void idRenderModelDiscreteAnimation::Save(idFile* file) const {
  idRenderModel::Save(file);
  WriteBig32(file, static_cast<std::uint32_t>(transforms.Num()));
  for (int index = 0; index < transforms.Num(); ++index) {
    for (float value : transforms[index].mat)
      WriteBig32(file, FloatBits(value));
  }
  for (int index = 0; index < collapsed.Num(); ++index) {
    const std::uint8_t value = collapsed[index] ? 1U : 0U;
    file->Write(&value, sizeof(value));
  }
}

bool idRenderModelDiscreteAnimation::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  std::uint32_t countBits{};
  if (!ReadBig32(file, countBits) ||
      countBits > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  const int count = static_cast<int>(countBits);
  if (!transforms.SetNum(count))
    return false;
  for (int index = 0; index < count; ++index) {
    for (float& value : transforms[index].mat) {
      std::uint32_t bits{};
      if (!ReadBig32(file, bits))
        return false;
      value = BitsFloat(bits);
    }
  }
  if (!collapsed.SetNum(count))
    return false;
  for (int index = 0; index < count; ++index) {
    std::uint8_t value{};
    if (file->Read(&value, sizeof(value)) != sizeof(value))
      return false;
    collapsed[index] = value != 0;
  }
  changed = true;
  return true;
}

void idRenderModelDiscreteAnimation::Reset() {
  const int count = modelData->transforms.Num();
  transforms.SetNum(count);
  for (int index = 0; index < transforms.Num(); ++index)
    transforms[index] = modelData->transforms[index];
  changed = true;
  CommitThisFrame();
}

bool idRenderModelDiscreteAnimation::LoadModel(const char* modelName) {
  if (modelData != nullptr)
    return false;
  SetName(modelName);
  modelData = static_cast<const idDiscreteAnimationModelData*>(
      idDiscreteAnimationModelData::resourceList.Load(modelName, false, false));
  if (modelData == nullptr) {
    idLib::Error(
        "idRenderModelDiscreteAnimation - failed to load model '%s'. A map rebuild may be required.",
        modelName != nullptr ? modelName : "");
  }
  FreeData();
  LoadModelData();
  return true;
}

void idRenderModelDiscreteAnimation::LoadModelData() {
  if (modelData == nullptr)
    return;

  modelTimestamp = static_cast<int>(modelData->binaryTimestamp);
  transforms.SetNum(modelData->transforms.Num());
  collapsed.SetNum(modelData->transforms.Num());
  for (int index = 0; index < transforms.Num(); ++index) {
    transforms[index] = modelData->transforms[index];
    collapsed[index] = false;
  }

  currentJointBuffer = 0;
  for (idJointBuffer& jointBuffer : jointBuffers) {
    jointBuffer.AllocBufferObject(
        transforms.Num() > 0 ? transforms[0].mat : nullptr, transforms.Num());
  }

  for (int index = 0; index < modelData->surfaces.Num(); ++index) {
    const idDiscreteAnimationModelSurface* dataSurface =
        modelData->surfaces[index];
    idRenderModelSurface surface{};
    surface.material = dataSurface->material;
    surface.geometry = dataSurface->triangles;
    surface.geometryIsReference = true;
    surface.referenceMask = 1U;
    surface.joints = new idJointBuffer;
    surface.joints->Reference(jointBuffers[currentJointBuffer],
                              dataSurface->jointOffset,
                              dataSurface->numJoints);
    AddSurface(surface);
  }

  dimShadow_t shadows = DIMSHADOW_OFF;
  if (breakableDecl != nullptr && !breakableDecl->settings.noShadows &&
      r_noBreakableShadows.valueInteger == 0) {
    shadows = DIMSHADOW_ON;
  }
  g.castDimShadows = shadows;
  FinishSurfaces();
  changed = true;
}
