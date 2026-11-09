#include "engine/models/skeletalanimation/treeanimator.h"

#include "engine/decls/declskins.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/animation.h"
#include "engine/models/skeletalanimation/declmd6.h"
#include "engine/models/skeletalanimation/jointgroup.h"
#include "engine/models/skeletalanimation/md6anim.h"
#include "engine/models/skeletalanimation/md6mesh.h"
#include "engine/models/skeletalanimation/md6morph.h"
#include "engine/models/skeletalanimation/md6skel.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "engine/models/sourcesurface.h"
#include "engine/models/renderworld.h"
#include "engine/renderer/autorenderparms.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/color.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <vector>

extern int g_md6UtilGameTimePerSecond;

idCVar r_showSkel(
    "r_showSkel", "0", CVAR_INTEGER,
    "draw the skeleton when model animates, 1 = draw model with skeleton, 2 = draw skeleton only",
    0.0f, 3.0f, idCmdSystem::ArgCompletion_Integer<0, 3>);
idCVar r_showRefPose(
    "r_showRefPose", "0", CVAR_BOOL,
    "selects the pose to draw with r_showskel, 0 = draw the final pose, 1 = draw the reference pose");
idCVar r_showAnimatedBounds(
    "r_showAnimatedBounds", "0", CVAR_INTEGER,
    "displays the bounding boxes of all animated models (1 = frame, 2 = normalized, 4 = translated)");
idCVar r_disableBlendShapes("r_disableBlendShapes", "0", CVAR_BOOL,
                            "disable blendshapes");
idCodeResource<idDeclRenderParm> rpSkinRoughnessMap("skinRoughnessMap");
idCodeResource<idDeclRenderParm> rpSkinRoughnessMap2("skinRoughnessMap2");
idCodeResource<idDeclRenderParm> rpSkinSSSPostSpecular(
    "skinSSSPostSpecular");

namespace {

idMat3 Transpose(const idMat3& matrix) {
  idMat3 result;
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column)
      result[row][column] = matrix[column][row];
  }
  return result;
}

const md6BlendOutputs_t* LastBlendOutput(const idTreeAnimator& animator) {
  return animator.jointAnimator != nullptr
             ? animator.jointAnimator->blendOutputs[1]
             : nullptr;
}

idQuat OutputQuat(const float (&values)[4]) {
  return {values[0], values[1], values[2], values[3]};
}

void SetOutputQuat(float (&values)[4], const idQuat& quaternion) {
  values[0] = quaternion.x;
  values[1] = quaternion.y;
  values[2] = quaternion.z;
  values[3] = quaternion.w;
}

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
         ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}

std::uint64_t ByteSwap64(std::uint64_t value) {
  return (static_cast<std::uint64_t>(ByteSwap32(
              static_cast<std::uint32_t>(value)))
          << 32u) |
         ByteSwap32(static_cast<std::uint32_t>(value >> 32u));
}

bool ReadExact(idFile* file, void* destination, std::size_t size) {
  return file != nullptr && file->Read(destination, size) == size;
}

bool WriteExact(idFile* file, const void* source, std::size_t size) {
  return file != nullptr && file->Write(source, size) == size;
}

std::uint8_t* AllocateMorphMap(std::size_t size, bool zero) {
  if (size == 0)
    return nullptr;
  return static_cast<std::uint8_t*>(idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\skeletalanimation\\TreeAnimator.cpp : TAG_MORPH",
      size, static_cast<memTag_t>(5), zero, ALIGN_16, HEAP_DEFAULTHEAP));
}

void SetSkinBlendingParm(idParmBlock& parms, float value) {
  if (rp_useSkinBlending.ConstPtr() != nullptr)
    parms.SetFloat(rp_useSkinBlending.ConstPtr(), value);
}

} // namespace

idTreeAnimator::idTreeAnimator(const idDeclMD6* declMD6)
    : decl(declMD6), jointAnimator(nullptr), meshVisibilityMask(0),
      meshUsabilityMask(0), morphSkin(-1), initialMorphValue(0.0f),
      treeFlags(0), currentBlendShapeBuffer(0) {
  renderModelType = RENDERMODEL_TREEANIMATOR;
  g.castDimShadows = DIMSHADOW_ON;
  useDeferredPosition = 1;

  if (decl == nullptr || decl->model == nullptr) {
    idLib::Error("md6 model has been purged");
    return;
  }

  idStr modelName(decl->name.str);
  modelName.SetFileExtension(".md6");
  SetName(modelName.data);
  jointAnimator = new idJointAnimator(decl);

  const int numMeshes = decl->model->meshes.Num();
  meshVisibilityMask =
      numMeshes <= MAX_SUB_MESHES
          ? ((std::uint64_t{1} << static_cast<unsigned>(numMeshes)) - 1u)
          : std::numeric_limits<std::uint64_t>::max();
  if (numMeshes > MAX_SUB_MESHES) {
    idLib::Warning(
        "The md6Def '%s' has %d submeshes which is more than the limit of %d, visibility masking disabled",
        decl->name.str, numMeshes, MAX_SUB_MESHES);
  }

  for (int component = 0; component < MESH_KIT_NUM; ++component) {
    const char* selected = decl->GetMeshKitDefaultOverride(component);
    if (selected == nullptr || *selected == '\0')
      selected = component > 0
                     ? decl->GetMeshKitDefaultOverride(component - 1)
                     : "";
    HideKitsExcept(static_cast<meshKitComponents_t>(component), selected);
  }
  SetKitSkin(decl->GetDefaultSkinOverride());

  InitializeBlendShapes();
  morphSkin = static_cast<std::int16_t>(decl->model->morphSkin);
  if (decl->HasMorphSets())
    SetMorphSkin(nullptr, -1, 0.0f, false);
  HideAllKits(MESH_KIT_GORE);

  bool usesWrinkleMaps = false;
  for (int meshIndex = 0; meshIndex < decl->model->meshes.Num(); ++meshIndex) {
    const idMD6Mesh* mesh = decl->model->meshes[meshIndex];
    if (mesh != nullptr && mesh->material != nullptr &&
        mesh->material->MaterialUsesWrinkleMaps()) {
      usesWrinkleMaps = true;
      break;
    }
  }
  if (usesWrinkleMaps && decl->NumWrinkleMapRegionExpressions() > 0) {
    wrinkleMapExpressions.SetNum(decl->NumWrinkleMapRegionExpressions());
    for (int index = 0; index < wrinkleMapExpressions.Num(); ++index) {
      const char* expression = decl->GetWrinkleMapRegionExpression(index);
      if (expression != nullptr && idStr::Length(expression) > 0)
        wrinkleMapExpressions[index].Parse(expression, decl);
    }
  }

  idPropsCollection* props = decl->GetPropsCollection();
  if (props != nullptr) {
    const tagData_t& localLightTag = props->GetTag(
        idPropInfo::INFO_PROP_NAME,
        idPropInfo::LOCALLIGHTPOSITION_TAG_NAME);
    idVec3 localLightPosition;
    if (localLightTag.IsValid() &&
        jointAnimator->GetModelSpaceTagPosition(localLightTag,
                                                localLightPosition)) {
      g.flags4 |= 0x20u;
      g.localLightPositionOverride = ScaleTranslation(localLightPosition);
    }
  }
  referenceBounds = jointAnimator->frameBounds;
}

idTreeAnimator::~idTreeAnimator() {
  if ((treeFlags & TREE_JOINT_ANIM_REF) == 0)
    delete jointAnimator;
  jointAnimator = nullptr;
  treeFlags &= static_cast<std::uint8_t>(~TREE_JOINT_ANIM_REF);
  FreeSurfaces();
  FreeSurfaceParms();
  FreeBlendShapes();
  FreeMorphBuffers();
}

idVec3 idTreeAnimator::ScaleTranslation(const idVec3& translation) const {
  return {translation.x * g.scale.x, translation.y * g.scale.y,
          translation.z * g.scale.z};
}

idVec3 idTreeAnimator::ModelToWorld(const idVec3& modelPosition) const {
  return g.axis * modelPosition + g.origin;
}

idMat3 idTreeAnimator::FrameDeltaAxis() const {
  const idMat3& targetAxis = useDeferredPosition != 0 ? deferredAxis : g.axis;
  return targetAxis * Transpose(g.axis);
}

idVec3 idTreeAnimator::FrameDeltaOrigin() const {
  const idVec3& targetOrigin =
      useDeferredPosition != 0 ? deferredOrigin : g.origin;
  return targetOrigin - g.origin;
}

void idTreeAnimator::SyncJoints(int time) {
  (void)time;
  if (jointAnimator != nullptr)
    jointAnimator->SyncJoints();
}

void idTreeAnimator::UpdateTime(int time) {
  (void)time;
  ShowSkel();
}

void idTreeAnimator::ShowSkel() {
  if (r_showSkel.valueInteger == 0 || jointAnimator == nullptr)
    return;
  jointAnimator->DrawJoints(GetRenderDebug(),
                            r_showRefPose.valueInteger == 0
                                ? ANIMATION_POSE_CURRENT
                                : DRIVER_MODEL,
                            g.origin, g.axis, g.scale, nullptr, true, true,
                            true, true, &idColor::colorLtGrey);
}

void idTreeAnimator::DrawBounds(const idBounds& modelSpaceBounds,
                                const idVec4& color, bool depthTest) {
  idRenderDebug* debug = GetRenderDebug();
  if (debug == nullptr)
    return;
  idBounds scaled;
  scaled.b[0] = ScaleTranslation(modelSpaceBounds.b[0]);
  scaled.b[1] = ScaleTranslation(modelSpaceBounds.b[1]);
  idBounds worldBounds;
  worldBounds.FromTransformedBounds(scaled, vec3_origin, g.axis);
  worldBounds.b[0] += g.origin;
  worldBounds.b[1] += g.origin;
  debug->DebugBounds(color, worldBounds, vec3_origin, 0, depthTest);
}

void idTreeAnimator::ReferenceJointAnimator(idJointAnimator* animator) {
  if (jointAnimator != nullptr && (treeFlags & TREE_JOINT_ANIM_REF) == 0)
    delete jointAnimator;
  treeFlags |= TREE_JOINT_ANIM_REF;
  jointAnimator = animator;
}

int idTreeAnimator::GetMeshIndex(const char* meshName) const {
  if (decl == nullptr || decl->model == nullptr)
    return -1;
  for (int index = 0; index < decl->model->meshes.Num(); ++index) {
    const idMD6Mesh* mesh = decl->model->meshes[index];
    if (mesh != nullptr && idStr::Icmp(mesh->name.data, meshName) == 0)
      return index;
  }
  return -1;
}

void idTreeAnimator::ShowMesh(int meshIndex) {
  if (meshVisibilityMask != std::numeric_limits<std::uint64_t>::max() &&
      meshIndex >= 0 && meshIndex < 64)
    meshVisibilityMask |= std::uint64_t{1} << meshIndex;
}

void idTreeAnimator::HideMesh(int meshIndex) {
  if (meshVisibilityMask != std::numeric_limits<std::uint64_t>::max() &&
      meshIndex >= 0 && meshIndex < 64)
    meshVisibilityMask &= ~(std::uint64_t{1} << meshIndex);
}

void idTreeAnimator::HideAllMeshes() {
  if (meshVisibilityMask != std::numeric_limits<std::uint64_t>::max())
    meshVisibilityMask = 0;
}

idStr idTreeAnimator::ShowKit(meshKitComponents_t kitType,
                              const char* name) {
  if (decl == nullptr || name == nullptr || *name == '\0')
    return idStr("");
  for (int kitIndex = 0; kitIndex < decl->NumMeshKits(kitType); ++kitIndex) {
    if (idStr::Icmp(decl->GetMeshKitName(kitType, kitIndex), name) != 0)
      continue;
    const idList<int, 19>* meshes =
        decl->GetMeshKitMeshes(kitType, kitIndex);
    if (meshes != nullptr) {
      for (int index = 0; index < meshes->Num(); ++index)
        ShowMesh((*meshes)[index]);
    }
    return idStr(name);
  }
  return idStr("");
}

void idTreeAnimator::HideKit(meshKitComponents_t kitType,
                             const char* name) {
  if (decl == nullptr)
    return;
  for (int kitIndex = 0; kitIndex < decl->NumMeshKits(kitType); ++kitIndex) {
    if (idStr::Icmp(decl->GetMeshKitName(kitType, kitIndex), name) != 0)
      continue;
    const idList<int, 19>* meshes =
        decl->GetMeshKitMeshes(kitType, kitIndex);
    if (meshes != nullptr) {
      for (int index = 0; index < meshes->Num(); ++index)
        HideMesh((*meshes)[index]);
    }
  }
}

void idTreeAnimator::HideKitsExcept(meshKitComponents_t kitType,
                                    const char* name) {
  if (decl == nullptr)
    return;
  int selected = -1;
  if (name != nullptr && *name != '\0') {
    for (int kitIndex = 0; kitIndex < decl->NumMeshKits(kitType);
         ++kitIndex) {
      if (idStr::Icmp(decl->GetMeshKitName(kitType, kitIndex), name) == 0) {
        selected = kitIndex;
        break;
      }
    }
  }
  for (int kitIndex = 0; kitIndex < decl->NumMeshKits(kitType); ++kitIndex) {
    const idList<int, 19>* meshes =
        decl->GetMeshKitMeshes(kitType, kitIndex);
    if (meshes == nullptr)
      continue;
    for (int index = 0; index < meshes->Num(); ++index) {
      if (kitIndex == selected)
        ShowMesh((*meshes)[index]);
      else
        HideMesh((*meshes)[index]);
    }
  }
}

void idTreeAnimator::HideAllKits(meshKitComponents_t kitType) {
  if (decl == nullptr)
    return;
  for (int kitIndex = 0; kitIndex < decl->NumMeshKits(kitType); ++kitIndex) {
    const idList<int, 19>* meshes =
        decl->GetMeshKitMeshes(kitType, kitIndex);
    if (meshes != nullptr) {
      for (int index = 0; index < meshes->Num(); ++index)
        HideMesh((*meshes)[index]);
    }
  }
}

int idTreeAnimator::NumJoints() const {
  return decl != nullptr ? decl->NumJoints() : 0;
}

const char* idTreeAnimator::GetJointName(idJointIndex_t jointIndex) const {
  return decl != nullptr ? decl->GetJointName(jointIndex) : nullptr;
}

idJointHandle_t
idTreeAnimator::GetJointHandle(idJointIndex_t jointIndex) const {
  return decl != nullptr && decl->model != nullptr &&
                 decl->model->skeleton != nullptr
             ? decl->model->skeleton->GetJointHandle(jointIndex.value)
             : NULL_JOINT;
}

idJointIndex_t idTreeAnimator::GetJointIndex(const char* jointName) const {
  return decl != nullptr && decl->model != nullptr &&
                 decl->model->skeleton != nullptr
             ? decl->model->skeleton->GetJointIndex(jointName)
             : NULL_JOINT_INDEX;
}

idJointIndex_t
idTreeAnimator::GetJointIndex(const idJointHandle_t& jointHandle) const {
  return decl != nullptr && decl->model != nullptr &&
                 decl->model->skeleton != nullptr
             ? decl->model->skeleton->GetJointIndex(jointHandle)
             : NULL_JOINT_INDEX;
}

bool idTreeAnimator::GetModelSpaceJointBindTransform(
    idJointIndex_t jointIndex, idVec3& origin, idMat3& axis) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceJointBindTransform(
                          jointIndex, origin, axis);
  if (result)
    origin = ScaleTranslation(origin);
  return result;
}

bool idTreeAnimator::GetModelSpaceJointPosition(
    animationPose_t pose, idJointIndex_t jointIndex, idVec3& origin) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceJointPosition(
                          pose, jointIndex, origin);
  if (result)
    origin = ScaleTranslation(origin);
  return result;
}

bool idTreeAnimator::GetModelSpaceJointAxis(animationPose_t pose,
                                            idJointIndex_t jointIndex,
                                            idMat3& axis) const {
  return jointAnimator != nullptr &&
         jointAnimator->GetModelSpaceJointAxis(pose, jointIndex, axis);
}

bool idTreeAnimator::GetModelSpaceJointTransform(
    animationPose_t pose, idJointIndex_t jointIndex, idVec3& origin,
    idMat3& axis) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceJointTransform(
                          pose, jointIndex, origin, axis);
  if (result)
    origin = ScaleTranslation(origin);
  return result;
}

bool idTreeAnimator::GetWorldSpaceJointPosition(animationPose_t pose,
                                                idJointIndex_t jointIndex,
                                                idVec3& origin) const {
  if (!GetModelSpaceJointPosition(pose, jointIndex, origin)) {
    origin = g.origin;
    return false;
  }
  origin = ModelToWorld(origin);
  return true;
}

bool idTreeAnimator::GetWorldSpaceJointAxis(animationPose_t pose,
                                            idJointIndex_t jointIndex,
                                            idMat3& axis) const {
  if (!GetModelSpaceJointAxis(pose, jointIndex, axis)) {
    axis = g.axis;
    return false;
  }
  axis *= g.axis;
  return true;
}

bool idTreeAnimator::GetWorldSpaceJointTransform(
    animationPose_t pose, idJointIndex_t jointIndex, idVec3& origin,
    idMat3& axis) const {
  if (!GetModelSpaceJointTransform(pose, jointIndex, origin, axis)) {
    origin = g.origin;
    axis = g.axis;
    return false;
  }
  origin = ModelToWorld(origin);
  axis *= g.axis;
  return true;
}

bool idTreeAnimator::GetModelSpaceAttachmentTransform(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idVec3& origin, idMat3& axis) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceAttachmentTransform(
                          jointIndex, translationOffset, rotationOffset,
                          origin, axis);
  if (result) {
    origin = ScaleTranslation(origin);
  } else {
    origin = vec3_origin;
    axis = mat3_identity;
  }
  return result;
}

bool idTreeAnimator::GetModelSpaceAttachmentPosition(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idVec3& origin) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceAttachmentPosition(
                          jointIndex, translationOffset, rotationOffset,
                          origin);
  origin = result ? ScaleTranslation(origin) : vec3_origin;
  return result;
}

bool idTreeAnimator::GetModelSpaceAttachmentAxis(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idMat3& axis) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceAttachmentAxis(
                          jointIndex, translationOffset, rotationOffset,
                          axis);
  if (!result)
    axis = mat3_identity;
  return result;
}

bool idTreeAnimator::GetWorldSpaceAttachmentTransform(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idVec3& origin, idMat3& axis) const {
  if (!GetModelSpaceAttachmentTransform(jointIndex, translationOffset,
                                        rotationOffset, origin, axis)) {
    origin = g.origin;
    axis = g.axis;
    return false;
  }
  origin = ModelToWorld(origin);
  axis *= g.axis;
  return true;
}

bool idTreeAnimator::GetWorldSpaceAttachmentPosition(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idVec3& origin) const {
  if (!GetModelSpaceAttachmentPosition(jointIndex, translationOffset,
                                       rotationOffset, origin)) {
    origin = g.origin;
    return false;
  }
  origin = ModelToWorld(origin);
  return true;
}

bool idTreeAnimator::GetWorldSpaceAttachmentAxis(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idMat3& axis) const {
  if (!GetModelSpaceAttachmentAxis(jointIndex, translationOffset,
                                   rotationOffset, axis)) {
    axis = g.axis;
    return false;
  }
  axis *= g.axis;
  return true;
}

bool idTreeAnimator::GetModelSpaceTagTransform(const tagData_t& tagData,
                                               idVec3& origin,
                                               idMat3& axis) const {
  const bool result = jointAnimator != nullptr &&
                      jointAnimator->GetModelSpaceTagTransform(tagData,
                                                               origin, axis);
  if (result)
    origin = ScaleTranslation(origin);
  return result;
}

bool idTreeAnimator::GetWorldSpaceTagTransform(const tagData_t& tagData,
                                               idVec3& origin,
                                               idMat3& axis) const {
  return GetWorldSpaceAttachmentTransform(GetJointIndex(idJointHandle_t{
                                              tagData.parentJoint.value}),
                                          tagData.trans, tagData.rot, origin,
                                          axis);
}

bool idTreeAnimator::GetWorldSpaceTagPosition(const tagData_t& tagData,
                                              idVec3& origin) const {
  return GetWorldSpaceAttachmentPosition(GetJointIndex(idJointHandle_t{
                                             tagData.parentJoint.value}),
                                         tagData.trans, tagData.rot, origin);
}

bool idTreeAnimator::GetWorldSpaceTagAxis(const tagData_t& tagData,
                                          idMat3& axis) const {
  return GetWorldSpaceAttachmentAxis(GetJointIndex(idJointHandle_t{
                                         tagData.parentJoint.value}),
                                     tagData.trans, tagData.rot, axis);
}

bool idTreeAnimator::GetModelSpaceJointFrameDeltas(
    const idJointIndex_t* indices, int numIndices, idVec3* translations,
    idMat3* rotations) const {
  if (jointAnimator == nullptr ||
      !jointAnimator->GetModelSpaceJointFrameDeltas(
          indices, numIndices, translations, rotations))
    return false;
  for (int index = 0; index < numIndices; ++index)
    translations[index] = ScaleTranslation(translations[index]);
  return true;
}

bool idTreeAnimator::GetModelSpaceReferenceJointFrameDeltas(
    const idJointIndex_t* indices, int numIndices, idVec3* translations,
    idMat3* rotations) const {
  if (jointAnimator == nullptr ||
      !jointAnimator->GetModelSpaceReferenceJointFrameDeltas(
          indices, numIndices, translations, rotations))
    return false;
  for (int index = 0; index < numIndices; ++index)
    translations[index] = ScaleTranslation(translations[index]);
  return true;
}

bool idTreeAnimator::GetWorldSpaceJointFrameDeltas(
    const idJointIndex_t* indices, int numIndices, idVec3* translations,
    idMat3* rotations) const {
  if (jointAnimator == nullptr ||
      !jointAnimator->GetModelSpaceJointFrameDeltas(
          indices, numIndices, translations, rotations))
    return false;
  const idMat3 deltaAxis = FrameDeltaAxis();
  const idMat3 inverseModelAxis = Transpose(g.axis);
  const idVec3 deltaOrigin = FrameDeltaOrigin();
  for (int index = 0; index < numIndices; ++index) {
    translations[index] =
        deltaAxis * ScaleTranslation(translations[index]) + deltaOrigin;
    rotations[index] = rotations[index] * inverseModelAxis * deltaAxis;
  }
  return true;
}

bool idTreeAnimator::GetModelSpaceAttachmentFrameDeltas(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idVec3& translationDelta,
    idMat3& rotationDelta) const {
  const bool result =
      jointAnimator != nullptr &&
      jointAnimator->GetModelSpaceAttachmentFrameDeltas(
          jointIndex, translationOffset, rotationOffset, translationDelta,
          rotationDelta);
  if (result)
    translationDelta = ScaleTranslation(translationDelta);
  return result;
}

bool idTreeAnimator::GetWorldSpaceAttachmentFrameDeltas(
    idJointIndex_t jointIndex, const idVec3& translationOffset,
    const idQuat& rotationOffset, idVec3& translationDelta,
    idMat3& rotationDelta) const {
  if (!GetModelSpaceAttachmentFrameDeltas(
          jointIndex, translationOffset, rotationOffset, translationDelta,
          rotationDelta))
    return false;
  const idMat3 deltaAxis = FrameDeltaAxis();
  const idMat3 inverseModelAxis = Transpose(g.axis);
  translationDelta = deltaAxis * translationDelta + FrameDeltaOrigin();
  rotationDelta = rotationDelta * inverseModelAxis * deltaAxis;
  return true;
}

idJointGroupCollection* idTreeAnimator::GetJointGroupCollection() const {
  return decl != nullptr ? decl->GetJointGroupCollection() : nullptr;
}

idPropsCollection* idTreeAnimator::GetPropsCollection() const {
  return decl != nullptr ? decl->GetPropsCollection() : nullptr;
}

void idTreeAnimator::ApplyOriginDeltas(const idMat3& axis,
                                       const idVec3& origin,
                                       idMat3& outAxis, idVec3& outOrigin,
                                       const idVec3& extraScale) const {
  if (jointAnimator == nullptr) {
    outAxis = axis;
    outOrigin = origin;
    return;
  }
  jointAnimator->ApplyOriginDeltas(
      outOrigin, outAxis, origin, axis,
      {g.scale.x * extraScale.x, g.scale.y * extraScale.y,
       g.scale.z * extraScale.z});
}

void idTreeAnimator::ApplyOriginDeltas(const idQuat& axis,
                                       const idVec3& origin,
                                       idQuat& outAxis, idVec3& outOrigin,
                                       const idVec3& extraScale) const {
  if (jointAnimator == nullptr) {
    outAxis = axis;
    outOrigin = origin;
    return;
  }
  jointAnimator->ApplyOriginDeltas(
      outOrigin, outAxis, origin, axis,
      {g.scale.x * extraScale.x, g.scale.y * extraScale.y,
       g.scale.z * extraScale.z});
}

const idQuat& idTreeAnimator::GetLastOriginRotation() const {
  static const idQuat identity{0.0f, 0.0f, 0.0f, 1.0f};
  const md6BlendOutputs_t* output = LastBlendOutput(*this);
  return output != nullptr
             ? *reinterpret_cast<const idQuat*>(output->animRotation)
             : identity;
}

void idTreeAnimator::GetLastOriginDeltas(idVec3& translation) const {
  const md6BlendOutputs_t* output = LastBlendOutput(*this);
  translation = output != nullptr
                    ? ScaleTranslation({output->deltaTranslation[0],
                                        output->deltaTranslation[1],
                                        output->deltaTranslation[2]})
                    : vec3_origin;
}

void idTreeAnimator::GetLastOriginDeltas(idQuat& rotation,
                                         idVec3& translation) const {
  const md6BlendOutputs_t* output = LastBlendOutput(*this);
  if (output == nullptr) {
    rotation = quat_identity;
    translation = vec3_origin;
    return;
  }
  rotation = OutputQuat(output->deltaRotation);
  translation = ScaleTranslation({output->deltaTranslation[0],
                                  output->deltaTranslation[1],
                                  output->deltaTranslation[2]});
}

void idTreeAnimator::GetLastOriginDeltas(idMat3& rotation,
                                         idVec3& translation) const {
  idQuat quaternion;
  GetLastOriginDeltas(quaternion, translation);
  rotation = quaternion.ToMat3();
}

void idTreeAnimator::GetTotalOriginTransform(idQuat& rotation,
                                             idVec3& translation) const {
  const md6BlendOutputs_t* output = LastBlendOutput(*this);
  if (output == nullptr) {
    rotation = quat_identity;
    translation = vec3_origin;
    return;
  }
  rotation = OutputQuat(output->animRotation);
  translation = ScaleTranslation({output->animTranslation[0],
                                  output->animTranslation[1],
                                  output->animTranslation[2]});
}

int idTreeAnimator::GetLastOriginDeltaTime() const {
  const md6BlendOutputs_t* output = LastBlendOutput(*this);
  return output != nullptr ? output->time : -1;
}

bool idTreeAnimator::IsDeltaFudged() const {
  const md6BlendOutputs_t* output = LastBlendOutput(*this);
  return output != nullptr && output->fudged;
}

void idTreeAnimator::SetRemoveOriginTranslation(bool removeTranslation) {
  if (jointAnimator != nullptr) {
    jointAnimator->animatorFlags = static_cast<std::uint8_t>(
        (jointAnimator->animatorFlags & ~0x08u) |
        (removeTranslation ? 0x08u : 0u));
  }
}

bool idTreeAnimator::GetRemoveOriginTranslation() const {
  return jointAnimator != nullptr &&
         (jointAnimator->animatorFlags & 0x08u) != 0;
}

void idTreeAnimator::SetRemoveOriginRotation(bool removeRotation) {
  SetRemoveOriginTranslation(removeRotation);
}

bool idTreeAnimator::GetRemoveOriginRotation() const {
  return GetRemoveOriginTranslation();
}

idVec3 idTreeAnimator::GetVisualOffset() const {
  return decl != nullptr && decl->config != nullptr
             ? ScaleTranslation({decl->config->visualOffset[0],
                                 decl->config->visualOffset[1],
                                 decl->config->visualOffset[2]})
             : vec3_origin;
}

void idTreeAnimator::SetOriginDeltaLookAhead(bool lookAhead) {
  if (jointAnimator != nullptr) {
    jointAnimator->animatorFlags = static_cast<std::uint8_t>(
        (jointAnimator->animatorFlags & ~0x10u) |
        (lookAhead ? 0x10u : 0u));
  }
}

const idMD6Anim*
idTreeAnimator::LookupAnimByAlias(const idDeclMD6* declaration,
                                  const char* animationName) {
  if (declaration == nullptr)
    return nullptr;
  const idMD6Alias* alias = declaration->FindAlias(animationName, true, true);
  const idMD6Anim* result = nullptr;
  if (alias != nullptr) {
    if (animation != nullptr)
      static_cast<idAnimationLocal*>(animation)->StrongLoadAlias(
          declaration, idAliasHandle_t(alias->aliasHandle));
    if (alias->mappings.Num() > 0)
      result = alias->mappings[0].anim;
  } else {
    result = static_cast<const idMD6Anim*>(
        idMD6Anim::resourceList.Load(animationName, true, false));
  }
  if (result == nullptr) {
    idLib::Warning(
        "Invalid animation name '%s' for LookupAnimByAlias in md6Def '%s'",
        animationName != nullptr ? animationName : "n/a",
        declaration->name.str);
  }
  return result;
}

void idTreeAnimator::SetFrameFromAnim(const idMD6Anim* animation, int frame,
                                      bool updateRenderModel,
                                      bool checkForOriginTranslation,
                                      const idVec3* translationScale) {
  if (jointAnimator == nullptr)
    return;
  jointAnimator->SetFromAnim(
      animation, frame * g_md6UtilGameTimePerSecond / 30, updateRenderModel,
      checkForOriginTranslation, translationScale, nullptr, 0);
  if (updateRenderModel) {
    CommitSubclass();
    CommitThisFrame();
  }
}

void idTreeAnimator::SetFrameFromAnimWithJointMods(
    const idMD6Anim* animation, int frame, bool updateRenderModel,
    bool checkForOriginTranslation, const idVec3* translationScale,
    const idMD6Blend::jointMod_t* jointMods, int numJointMods) {
  if (jointAnimator == nullptr)
    return;
  jointAnimator->SetFromAnim(
      animation, frame * g_md6UtilGameTimePerSecond / 30, updateRenderModel,
      checkForOriginTranslation, translationScale, jointMods, numJointMods);
  if (updateRenderModel) {
    CommitSubclass();
    CommitThisFrame();
  }
}

const idMD6Anim* idTreeAnimator::GetSingleAnimFrame(
    idJointMat* joints, const idMD6Anim* animation, int time,
    const idVec3& offset, bool removeOriginOffset) const {
  if (animation == nullptr || decl == nullptr ||
      !decl->GetJointsForAnimTime(joints, animation, time, offset,
                                  removeOriginOffset))
    return nullptr;
  return animation;
}

const idMD6Anim* idTreeAnimator::GetSingleAnimFrame(
    idJointMat* joints, const char* animationName, int time,
    const idVec3& offset, bool removeOriginOffset) const {
  return GetSingleAnimFrame(joints, LookupAnimByAlias(decl, animationName),
                            time, offset, removeOriginOffset);
}

const idMD6Anim* idTreeAnimator::GetSingleAnimFrame(
    idJointMat* joints, const idAliasHandle_t& animationAlias, int time,
    const idVec3& offset, bool removeOriginOffset) const {
  if (decl == nullptr || !animationAlias.IsValid())
    return nullptr;
  return GetSingleAnimFrame(joints,
                            decl->AnimForAlias(animationAlias, false), time,
                            offset, removeOriginOffset);
}

void idTreeAnimator::GetJointOffsetAndRotation(
    const idMD6Anim* animation, int time, const idVec3& offset,
    bool removeOriginOffset, int jointIndex, idVec3& jointOffset,
    idMat3& jointAxis) const {
  (void)time;
  (void)offset;
  (void)removeOriginOffset;
  jointOffset = vec3_origin;
  jointAxis = mat3_identity;
  if (decl == nullptr || jointIndex < 0 || jointIndex >= NumJoints())
    return;
  std::vector<idJointMat> joints(static_cast<std::size_t>(NumJoints()));
  if (animation != nullptr &&
      !decl->GetJointsForAnimTime(joints.data(), animation, 0, vec3_origin,
                                  false))
    return;
  const idJointMat& joint = joints[static_cast<std::size_t>(jointIndex)];
  jointOffset = {joint.mat[3], joint.mat[7], joint.mat[11]};
  jointAxis[0] = {joint.mat[0], joint.mat[4], joint.mat[8]};
  jointAxis[1] = {joint.mat[1], joint.mat[5], joint.mat[9]};
  jointAxis[2] = {joint.mat[2], joint.mat[6], joint.mat[10]};
}

idStrStatic<256> idTreeAnimator::GetSkinName() const {
  idStrStatic<256> result;
  if (decl != nullptr && decl->model != nullptr) {
    result = decl->model->name.str;
    result.StripFileExtension();
  }
  return result;
}

const idDeclSkins* idTreeAnimator::GetSkins() const {
  const idStrStatic<256> skinName = GetSkinName();
  return skinName.IsEmpty()
             ? nullptr
             : static_cast<const idDeclSkins*>(
                   idDeclSkins::resourceList.FindWithInheritance(
                       skinName.data, false, true));
}

void idTreeAnimator::SetKitSkin(const char* skin) {
  const idDeclSkins* skins = GetSkins();
  if (skins == nullptr || skin == nullptr)
    return;
  const int index = skins->IndexForSkin(skin);
  if (index >= 0)
    g.customSkin = index;
}

void idTreeAnimator::FreeSurfaceParms() {
  surfaceParms.DeleteContents();
}

void idTreeAnimator::FreeMorphBuffers() {
  for (int index = 0; index < blendShapeBuffers.Num(); ++index) {
    delete blendShapeBuffers[index].object;
    blendShapeBuffers[index].object = nullptr;
  }
  for (idVertexBuffer& buffer : morphMapBuffers)
    buffer.FreeBufferObject();
  blendShapeBuffers.ClearFree();
}

void idTreeAnimator::FreeBlendShapes() {
  FreeMorphBuffers();
  for (int index = 0; index < blendShapes.Num(); ++index) {
    if (blendShapes[index].map != nullptr)
      idMem::Singleton()->Free(blendShapes[index].map, ALIGN_16);
    blendShapes[index].map = nullptr;
  }
  blendShapes.ClearFree();
}

void idTreeAnimator::ClearMorphPoints() {
  if (decl == nullptr || decl->model == nullptr || blendShapes.Num() == 0)
    return;
  const int meshCount =
      std::min(blendShapes.Num(), decl->model->meshes.Num());
  for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
    blendShape_t& shape = blendShapes[meshIndex];
    const idMD6Mesh* mesh = decl->model->meshes[meshIndex];
    if (shape.map == nullptr || mesh == nullptr)
      continue;
    const int numVertices = mesh->tris != nullptr ? mesh->tris->numVerts : 0;
    for (int vertex = 0; vertex < numVertices; ++vertex)
      shape.map[4 * vertex + 3] = 0;
  }
  treeFlags |= TREE_UPDATE_BLEND_SHAPES;
}

void idTreeAnimator::InitializeBlendShapes() {
  FreeBlendShapes();
  if (feature_hq.valueInteger == 0 || decl == nullptr ||
      decl->model == nullptr)
    return;

  const idMD6Model& model = *decl->model;
  bool hasMaps = decl->HasMorphSets();
  bool hasExpressions = false;
  for (int meshIndex = 0; meshIndex < model.meshes.Num(); ++meshIndex) {
    const idMD6Mesh* mesh = model.meshes[meshIndex];
    if (mesh == nullptr)
      continue;
    hasMaps = hasMaps || mesh->morphMap.list != nullptr;
    hasExpressions = hasExpressions || mesh->blendShapes.Num() > 0;
  }
  if (!hasMaps && !hasExpressions)
    return;

  blendShapes.SetNum(model.meshes.Num());
  int evaluationCount = 0;
  for (int meshIndex = 0; meshIndex < model.meshes.Num(); ++meshIndex) {
    const idMD6Mesh* mesh = model.meshes[meshIndex];
    if (mesh == nullptr)
      continue;
    blendShape_t& destination = blendShapes[meshIndex];
    const std::size_t mapSize =
        static_cast<std::size_t>(std::max(mesh->numVerts, 0)) * 4u;
    if (mesh->blendShapes.Num() > 0) {
      destination.map = AllocateMorphMap(mapSize, false);
      if (destination.map != nullptr)
        std::memset(destination.map, 128, mapSize);
      destination.blendShapeExpressions.SetNum(mesh->blendShapes.Num());
      for (int expressionIndex = 0;
           expressionIndex < mesh->blendShapes.Num(); ++expressionIndex) {
        const idMD6Mesh::blendShape_t& source =
            mesh->blendShapes[expressionIndex];
        const char* expression = source.blendShapeExpression.data;
        bool parsedTargetChannel = false;
        idParser parser;
        if (parser.LoadMemory(expression, idStr::Length(expression), nullptr)) {
          idToken punctuation;
          if (parser.CheckTokenType(TT_PUNCTUATION, 44, punctuation)) {
            idToken targetUserChannel;
            if (parser.ReadToken(targetUserChannel) &&
                parser.CheckTokenType(TT_PUNCTUATION, 40, punctuation)) {
              destination.blendShapeExpressions[expressionIndex].Parse(
                  targetUserChannel.data, decl);
              parsedTargetChannel = true;
            }
          }
        }
        if (!parsedTargetChannel) {
          destination.blendShapeExpressions[expressionIndex].Parse(
              expression, decl);
        }
        destination.multiplier = source.multiplier;
      }
      ++evaluationCount;
    }
    if (destination.map == nullptr && mesh->morphMap.list != nullptr) {
      destination.map = AllocateMorphMap(mapSize, false);
      if (destination.map != nullptr)
        std::memcpy(destination.map, mesh->morphMap.list, mapSize);
    } else if (destination.map == nullptr && decl->HasMorphSets()) {
      destination.map = AllocateMorphMap(mapSize, true);
    }
  }

  if (jointAnimator == nullptr)
    return;
  jointAnimator->blendShapesEval.SetNum(evaluationCount);
  int evaluationIndex = 0;
  for (int meshIndex = 0; meshIndex < model.meshes.Num(); ++meshIndex) {
    const idMD6Mesh* mesh = model.meshes[meshIndex];
    if (mesh == nullptr || mesh->blendShapes.Num() == 0)
      continue;
    idMD6Blend::blendShapeEval_t& evaluation =
        jointAnimator->blendShapesEval[evaluationIndex++];
    std::memset(&evaluation, 0, sizeof(evaluation));
    evaluation.numBlendShapes = static_cast<std::uint8_t>(
        std::min(mesh->blendShapes.Num(), idMD6Blend::MAX_BLENDSHAPES));
    evaluation.destData = blendShapes[meshIndex].map;
    evaluation.numVerts = mesh->numVerts;
    for (int expressionIndex = 0;
         expressionIndex < evaluation.numBlendShapes; ++expressionIndex) {
      evaluation.expressions[expressionIndex] =
          &blendShapes[meshIndex]
               .blendShapeExpressions[expressionIndex];
      evaluation.sourceDatas[expressionIndex] =
          mesh->blendShapes[expressionIndex].blendShapeData.list;
    }
  }
}

void idTreeAnimator::InitializeMorphMaps() {
  FreeBlendShapes();
  if (feature_hq.valueInteger == 0 || decl == nullptr ||
      decl->model == nullptr)
    return;
  blendShapes.SetNum(decl->model->meshes.Num());
  for (int meshIndex = 0; meshIndex < blendShapes.Num(); ++meshIndex) {
    const idMD6Mesh* mesh = decl->model->meshes[meshIndex];
    if (mesh == nullptr)
      continue;
    const std::size_t size =
        static_cast<std::size_t>(std::max(mesh->numVerts, 0)) * 4u;
    blendShapes[meshIndex].map = AllocateMorphMap(size, false);
    if (blendShapes[meshIndex].map != nullptr)
      std::memset(blendShapes[meshIndex].map, 128, size);
  }
}

void idTreeAnimator::SetMorphSkin(const char* skinName, int skinIndex,
                                  float morphAmount, bool silentError) {
  if (feature_hq.valueInteger == 0 || decl == nullptr ||
      decl->model == nullptr)
    return;

  if (skinName != nullptr) {
    const idDeclSkins* skins = GetSkins();
    if (skins == nullptr) {
      if (rp_useSkinBlending.ConstPtr() != nullptr)
        gameParmBlock.ClearParm(rp_useSkinBlending.ConstPtr());
      if (!silentError) {
        const idStrStatic<256> skinDeclName = GetSkinName();
        idLib::Warning(
            "no skins defined for md6Def '%s' using model '%s', skin decl '%s'\n",
            name.str, decl->model->name.str, skinDeclName.data);
      }
      return;
    }
    if (*skinName != '\0')
      skinIndex = skins->IndexForSkin(skinName);
    else if (skinIndex < 0 || skinIndex >= skins->skins.Num())
      skinIndex = -1;
    if (skinIndex == -1) {
      morphSkin = -1;
      if (rp_useSkinBlending.ConstPtr() != nullptr)
        gameParmBlock.ClearParm(rp_useSkinBlending.ConstPtr());
      if (!silentError) {
        const idStrStatic<256> skinDeclName = GetSkinName();
        idLib::Error(
            "no skin found with name '%s' for md6Def '%s' using model '%s', skin decl '%s'\n",
            skinName, name.str, decl->model->name.str, skinDeclName.data);
      }
      return;
    }
    morphSkin = static_cast<std::int16_t>(skinIndex);
  }

  initialMorphValue = morphAmount;
  SetSkinBlendingParm(gameParmBlock, morphAmount > 0.0f ? 1.0f : 0.0f);
  const std::uint8_t quantized = static_cast<std::uint8_t>(
      std::clamp(morphAmount, 0.0f, 1.0f) * 255.0f);
  const std::uint32_t packed =
      (static_cast<std::uint32_t>(quantized) << 24u) | 0x00808080u;
  if (blendShapes.Num() != decl->model->meshes.Num())
    InitializeMorphMaps();
  for (int meshIndex = 0; meshIndex < blendShapes.Num(); ++meshIndex) {
    const idMD6Mesh* mesh = decl->model->meshes[meshIndex];
    if (mesh == nullptr || mesh->morphMap.list != nullptr ||
        blendShapes[meshIndex].map == nullptr)
      continue;
    auto* words = reinterpret_cast<std::uint32_t*>(
        blendShapes[meshIndex].map);
    for (int vertex = 0; vertex < mesh->numVerts; ++vertex)
      words[vertex] = packed;
  }
  treeFlags |= TREE_UPDATE_BLEND_SHAPES;
}

void idTreeAnimator::MorphLocation(float morphAmount, int morphIndex) {
  if (morphAmount <= 0.0f || feature_hq.valueInteger == 0 ||
      decl == nullptr || !decl->HasMorphSets() || decl->model == nullptr)
    return;
  if (blendShapes.Num() != decl->model->meshes.Num())
    InitializeBlendShapes();
  SetSkinBlendingParm(gameParmBlock, 1.0f);
  const int amount = static_cast<int>(morphAmount * 255.0f);
  bool badData = false;
  for (int setIndex = 0; setIndex < decl->NumMorphSets(); ++setIndex) {
    const idMD6MorphSet* set = decl->GetMorphSet(setIndex);
    if (set == nullptr || set->groupIndex != morphIndex || set->meshIndex < 0 ||
        set->meshIndex >= decl->model->sourceSurfaces.Num())
      continue;
    const sourceSurface_t& source =
        decl->model->sourceSurfaces[set->meshIndex];
    if (source.renderSurface < 0 || source.renderSurface >= blendShapes.Num())
      continue;
    std::uint8_t* map = blendShapes[source.renderSurface].map;
    if (map == nullptr)
      continue;
    const int vertexCount = source.lastVertex - source.firstVertex + 1;
    for (int index = 0; index < set->indices.Num(); ++index) {
      const int vertex = set->indices[index];
      if (vertex < 0 || vertex >= vertexCount) {
        badData = true;
        continue;
      }
      std::uint8_t* value =
          map + 4 * (source.firstVertex + vertex);
      if (set->direction.Num() > 3 * index + 2) {
        value[0] = set->direction[3 * index];
        value[1] = set->direction[3 * index + 1];
        value[2] = set->direction[3 * index + 2];
      }
      value[3] = static_cast<std::uint8_t>(
          std::min(255, amount + static_cast<int>(value[3])));
    }
  }
  if (badData)
    idLib::Warning("%s has bad morphing data", name.str);
  treeFlags |= TREE_UPDATE_BLEND_SHAPES;
}

void idTreeAnimator::InitializeSurfaces() {
  FreeSurfaces();
  if (decl == nullptr || decl->model == nullptr)
    return;
  surfaces.SetNum(decl->model->meshes.Num());
  surfaceParms.SetNum(decl->model->meshes.Num());
  for (int index = 0; index < surfaces.Num(); ++index) {
    surfaceParms[index] = nullptr;
    surfaces[index].joints = new idJointBuffer;
  }
}

void idTreeAnimator::InitializeSurfaceParms() {
  FreeSurfaceParms();
  if (decl == nullptr || decl->model == nullptr)
    return;
  surfaceParms.SetNum(decl->model->meshes.Num());
  for (int index = 0; index < surfaceParms.Num(); ++index)
    surfaceParms[index] = nullptr;
  for (int index = 0; index < decl->NumSkipUseHintMeshes(); ++index) {
    const int meshIndex = decl->GetSkipUseHintMesh(index);
    if (meshIndex < 0 || meshIndex >= surfaceParms.Num())
      continue;
    surfaceParms[meshIndex] = new idParmBlock;
    if (rp_highlightColor.ConstPtr() != nullptr)
      surfaceParms[meshIndex]->SetParm(rp_highlightColor.ConstPtr(),
                                       idVec4{});
  }
}

void idTreeAnimator::InitializeMorphBuffers() {
  FreeMorphBuffers();
  for (int index = 0; index < surfaces.Num(); ++index) {
    surfaces[index].morphMap = nullptr;
    surfaces[index].morphMultiplier = 0.0f;
  }
  if (blendShapes.Num() == 0 || decl == nullptr || decl->model == nullptr)
    return;
  blendShapeBuffers.SetNum(decl->model->meshes.Num());
  int totalSize = 0;
  for (int meshIndex = 0; meshIndex < blendShapeBuffers.Num(); ++meshIndex) {
    blendShapeBuffer_t& buffer = blendShapeBuffers[meshIndex];
    buffer = {};
    const idMD6Mesh* mesh = decl->model->meshes[meshIndex];
    if (mesh == nullptr || blendShapes[meshIndex].map == nullptr)
      continue;
    buffer.size = 4 * mesh->numVerts;
    if (buffer.size == 0)
      continue;
    buffer.object = new idVertexBuffer;
    buffer.offset = totalSize;
    totalSize += (buffer.size + 15) & ~15;
  }
  if (totalSize != 0) {
    for (idVertexBuffer& buffer : morphMapBuffers)
      buffer.AllocBufferObject(nullptr, totalSize, 4, BU_DEFAULT);
  }
  treeFlags |= TREE_UPDATE_BLEND_SHAPES;
}

void idTreeAnimator::Save(idFile* file) const {
  idRenderModel::Save(file);
  std::uint64_t visibility = ByteSwap64(meshVisibilityMask);
  std::uint16_t skin = ByteSwap16(static_cast<std::uint16_t>(morphSkin));
  std::uint32_t morphBits = 0;
  std::memcpy(&morphBits, &initialMorphValue, sizeof(morphBits));
  morphBits = ByteSwap32(morphBits);
  WriteExact(file, &visibility, sizeof(visibility));
  WriteExact(file, &skin, sizeof(skin));
  WriteExact(file, &morphBits, sizeof(morphBits));
  if (jointAnimator != nullptr)
    jointAnimator->Save(*file);
}

bool idTreeAnimator::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  std::uint64_t visibility = 0;
  std::uint16_t skin = 0;
  std::uint32_t morphBits = 0;
  if (!ReadExact(file, &visibility, sizeof(visibility)) ||
      !ReadExact(file, &skin, sizeof(skin)) ||
      !ReadExact(file, &morphBits, sizeof(morphBits)))
    return false;
  meshVisibilityMask = ByteSwap64(visibility);
  morphSkin = static_cast<std::int16_t>(ByteSwap16(skin));
  morphBits = ByteSwap32(morphBits);
  std::memcpy(&initialMorphValue, &morphBits, sizeof(initialMorphValue));
  if (morphSkin != -1)
    SetMorphSkin("", morphSkin, initialMorphValue, false);
  return jointAnimator != nullptr && jointAnimator->Load(*file);
}

const idList<sourceSurface_t, 5>* idTreeAnimator::GetSourceSurfaces() const {
  return decl != nullptr && decl->model != nullptr
             ? &decl->model->sourceSurfaces
             : nullptr;
}

bool idTreeAnimator::CommitSubclass() {
  if (decl == nullptr || decl->model == nullptr || jointAnimator == nullptr)
    return false;
  referenceBounds = jointAnimator->frameBounds;
  if (r_showSkel.valueInteger > 1) {
    FreeSurfaces();
    return true;
  }

  const idMD6Model& model = *decl->model;
  if (surfaces.Num() != model.meshes.Num()) {
    InitializeSurfaces();
    InitializeSurfaceParms();
  }
  if (blendShapeBuffers.Num() != blendShapes.Num())
    InitializeMorphBuffers();
  if (blendShapes.Num() != 0 &&
      ((treeFlags & TREE_UPDATE_BLEND_SHAPES) != 0 ||
       (jointAnimator->animatorFlags & 0x40u) != 0)) {
    jointAnimator->animatorFlags &= static_cast<std::uint8_t>(~0x40u);
    treeFlags |= TREE_UPDATE_BLEND_SHAPES;
    currentBlendShapeBuffer ^= 1;
  }

  const idImage* roughnessMap = nullptr;
  bool usesWrinkleMaps = false;
  std::uint64_t mask = 1;
  const int meshCount = std::min(surfaces.Num(), model.meshes.Num());
  for (int meshIndex = 0; meshIndex < meshCount; ++meshIndex,
           mask = (mask << 1u) | (mask >> 63u)) {
    idRenderModelSurface& surface = surfaces[meshIndex];
    surface.material = nullptr;
    if ((mask & meshVisibilityMask) == 0)
      continue;
    idMD6Mesh* mesh = model.meshes[meshIndex];
    if (mesh == nullptr)
      continue;

    const idMaterial* material =
        static_cast<const idMaterial*>(g.customMaterial);
    surface.skinOffsets = {};
    if (material == nullptr) {
      if (g.customSkin >= 0 && g.customSkin < mesh->skinRemaps.Num()) {
        const skinRemap_t& remap = mesh->skinRemaps[g.customSkin];
        material = remap.mtr;
        surface.skinOffsets.x = remap.texOffset.x;
        surface.skinOffsets.y = remap.texOffset.y;
        surface.skinOffsets.z = remap.texOffset.x;
        surface.skinOffsets.w = remap.texOffset.y;
      } else {
        material = mesh->material;
      }
      if (morphSkin >= 0 && morphSkin < mesh->skinRemaps.Num()) {
        const skinRemap_t& remap = mesh->skinRemaps[morphSkin];
        surface.skinOffsets.z = remap.texOffset.x;
        surface.skinOffsets.w = remap.texOffset.y;
        if (remap.mtr != nullptr &&
            rpSkinSSSPostSpecular.ConstPtr() != nullptr &&
            remap.mtr->GetFloat(rpSkinSSSPostSpecular.ConstPtr()) > 0.5f &&
            rpSkinRoughnessMap.ConstPtr() != nullptr) {
          roughnessMap =
              remap.mtr->GetImage(rpSkinRoughnessMap.ConstPtr());
        }
      }
      const idMaterial* compatible = material != nullptr
                                         ? material->CompatibleGenericMaterial()
                                         : nullptr;
      if (material != compatible) {
        surface.skinOffsets.z -= surface.skinOffsets.x;
        surface.skinOffsets.w -= surface.skinOffsets.y;
        surface.skinOffsets.x = 0.0f;
        surface.skinOffsets.y = 0.0f;
      }
    }
    if (material == nullptr || material->Coverage() == MC_INVISIBLE)
      continue;

    surface.material = material;
    surface.geometry = mesh->tris;
    surface.geometryIsReference = true;
    surface.referenceMask = 0x2du;
    surface.stMap =
        mesh->stSets.Num() > 0 ? mesh->stSets[0].vb : nullptr;
    surface.parms = meshIndex < surfaceParms.Num()
                        ? surfaceParms[meshIndex]
                        : nullptr;
    jointAnimator->Reference(surface.joints, mesh->jointOffset,
                             mesh->numJoints);

    if ((treeFlags & TREE_UPDATE_BLEND_SHAPES) != 0 &&
        meshIndex < blendShapeBuffers.Num() &&
        meshIndex < blendShapes.Num()) {
      blendShapeBuffer_t& buffer = blendShapeBuffers[meshIndex];
      if (buffer.object != nullptr && buffer.size != 0) {
        surface.morphMap = buffer.object;
        surface.morphMultiplier = blendShapes[meshIndex].multiplier;
        buffer.object->Reference(morphMapBuffers[currentBlendShapeBuffer],
                                 buffer.offset, buffer.size);
        buffer.object->Update(blendShapes[meshIndex].map, buffer.size, 4);
      }
    }
    if (r_disableBlendShapes.valueInteger != 0) {
      surface.morphMap = nullptr;
      surface.morphMultiplier = 0.0f;
    }
    if (surface.material->MaterialUsesWrinkleMaps() &&
        wrinkleMapExpressions.Num() != 0) {
      usesWrinkleMaps = true;
    }
  }

  if (roughnessMap != nullptr && rpSkinRoughnessMap2.ConstPtr() != nullptr)
    gameParmBlock.SetImage(rpSkinRoughnessMap2.ConstPtr(), roughnessMap);

  if (usesWrinkleMaps) {
    float factors[16]{};
    const float* channels = jointAnimator->userChannels[1];
    const int numChannels = decl->model->skeleton != nullptr &&
                                    decl->model->skeleton->data != nullptr
                                ? decl->model->skeleton->data->numUserChannels
                                : 0;
    const int expressionCount =
        std::min(wrinkleMapExpressions.Num(), 16);
    for (int index = 0; index < expressionCount; ++index) {
      factors[index] = wrinkleMapExpressions[index].expr.HasExpr()
                           ? wrinkleMapExpressions[index].Eval(channels,
                                                               numChannels)
                           : 0.0f;
    }
    if (rp_wrinkleFactor0.ConstPtr() != nullptr) {
      gameParmBlock.SetParm(
          rp_wrinkleFactor0.ConstPtr(),
          idVec4{factors[0], factors[2], factors[4], factors[6]});
    }
    if (rp_wrinkleFactor1.ConstPtr() != nullptr) {
      gameParmBlock.SetParm(
          rp_wrinkleFactor1.ConstPtr(),
          idVec4{factors[1], factors[3], factors[5], factors[7]});
    }
    if (rp_wrinkleFactor2.ConstPtr() != nullptr) {
      gameParmBlock.SetParm(
          rp_wrinkleFactor2.ConstPtr(),
          idVec4{factors[8], factors[10], factors[12], factors[14]});
    }
    if (rp_wrinkleFactor3.ConstPtr() != nullptr) {
      gameParmBlock.SetParm(
          rp_wrinkleFactor3.ConstPtr(),
          idVec4{factors[9], factors[11], factors[13], factors[15]});
    }
  }

  treeFlags &= static_cast<std::uint8_t>(~TREE_UPDATE_BLEND_SHAPES);
  for (int decalIndex = decalData.Num() - 1; decalIndex >= 0; --decalIndex) {
    decalData_t& decal = decalData[decalIndex];
    idVec3 modelPosition = decal.relativePos;
    idMat3 modelAxis = decal.relativeAxis;
    if (decal.jointId.value >= 0) {
      idVec3 jointOrigin;
      idMat3 jointAxis;
      if (GetModelSpaceJointTransform(DRIVER_MODEL,
                                      idJointIndex_t{decal.jointId.value},
                                      jointOrigin, jointAxis)) {
        modelPosition = jointAxis * decal.relativePos + jointOrigin;
        modelAxis = decal.relativeAxis * jointAxis;
      }
    }
    const idVec3 worldPosition = ModelToWorld(modelPosition);
    const idMat3 worldAxis = modelAxis * g.axis;
    if (!SetDecalPosition(decal.handle, worldPosition, worldAxis))
      decalData.RemoveIndexFast(decalIndex);
  }

  if ((g.flags3 & 0x08u) != 0 && (g.flags4 & 0x20u) != 0) {
    idPropsCollection* props = decl->GetPropsCollection();
    if (props != nullptr) {
      const tagData_t& localLightTag = props->GetTag(
          idPropInfo::INFO_PROP_NAME,
          idPropInfo::LOCALLIGHTPOSITION_TAG_NAME);
      idVec3 localLightPosition;
      if (localLightTag.IsValid() &&
          jointAnimator->GetModelSpaceTagPosition(localLightTag,
                                                  localLightPosition)) {
        g.localLightPositionOverride = ScaleTranslation(localLightPosition);
      }
    }
  }

  if (r_showAnimatedBounds.valueInteger != 0) {
    const int display = std::abs(r_showAnimatedBounds.valueInteger);
    const bool depthTest = r_showAnimatedBounds.valueInteger < 0;
    if ((display & 1) != 0)
      DrawBounds(jointAnimator->frameBounds, idColor::colorYellow, depthTest);
    if ((display & 2) != 0)
      DrawBounds(jointAnimator->normalizedBounds, idColor::colorPink,
                 depthTest);
    if ((display & 4) != 0)
      DrawBounds(jointAnimator->translatedBounds, idColor::colorCyan,
                 depthTest);
  }
  return true;
}

decalHandle_t idTreeAnimator::AddDecalFromPoint(
    const decalParams_t* decalParms, int startTime, const idVec3& position,
    const idVec3& direction, idJointIndex_t jointId) {
  if (decalData.Num() >= decalData.size)
    return {-1, -1};
  decalData_t decal{};
  decal.handle = {-1, -1};
  decal.jointId.value = -1;
  InitDecalData(decal, decalParms, static_cast<unsigned>(startTime),
                position, direction);
  if (jointId.value >= 0) {
    idVec3 jointOrigin;
    idMat3 jointAxis;
    if (GetModelSpaceJointTransform(DRIVER_MODEL, jointId, jointOrigin,
                                    jointAxis)) {
      const idMat3 inverseJointAxis = Transpose(jointAxis);
      decal.relativePos =
          inverseJointAxis * (decal.relativePos - jointOrigin);
      decal.relativeAxis *= inverseJointAxis;
      decal.jointId.value = jointId.value;
    }
  }
  decalData.Append(decal);
  return decal.handle;
}

void idTreeAnimator::SerializeSnapshot(idSerializer* serializer,
                                       bool serializeParmBlock) {
  if (serializer == nullptr || serializer->msg == nullptr)
    return;
  bool skip = (treeFlags & TREE_SKIP_SERIALIZATION) != 0;
  if (serializer->writing)
    serializer->msg->WriteBits(skip ? 1 : 0, 1);
  else
    skip = serializer->msg->ReadBits(1) == 1;
  treeFlags = static_cast<std::uint8_t>(
      (treeFlags & ~TREE_SKIP_SERIALIZATION) |
      (skip ? TREE_SKIP_SERIALIZATION : 0));
  if (skip)
    return;

  idRenderModel::SerializeSnapshot(serializer, serializeParmBlock);
  bool removeOrigin = GetRemoveOriginTranslation();
  if (serializer->writing)
    serializer->msg->WriteBits(removeOrigin ? 1 : 0, 1);
  else {
    removeOrigin = serializer->msg->ReadBits(1) == 1;
    SetRemoveOriginTranslation(removeOrigin);
  }
  serializer->Serialize(&meshVisibilityMask);
  const std::int16_t previousSkin = morphSkin;
  if (serializer->writing)
    serializer->msg->WriteBits(morphSkin, -16);
  else
    morphSkin = static_cast<std::int16_t>(serializer->msg->ReadBits(-16));
  if (!serializer->writing && previousSkin == -1 && morphSkin != -1)
    SetMorphSkin(nullptr, morphSkin, 0.0f, false);
}
