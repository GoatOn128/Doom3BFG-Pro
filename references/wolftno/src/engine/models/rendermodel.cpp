#include "engine/models/rendermodel.h"

#include "engine/decls/declskins.h"
#include "engine/models/renderworld.h"
#include "engine/models/serializer.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/jobs/render/setupmodelmvp.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/networking/bitmsg.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

extern idCVar r_skipCommits;
extern idCVar r_showCommits;
extern idCVar r_checkBounds;

idCVar r_forceReferencesOnCommit(
    "r_forceReferencesOnCommit", "0", CVAR_BOOL,
    "Never skip the reference update");
idCVar r_showReferenceUpdates(
    "r_showReferenceUpdates", "0", CVAR_BOOL,
    "show rendermodels which have their references updated");

namespace {

idRenderModelRuntimeCallbacks runtimeCallbacks{};

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

void WriteBigBounds(idFile* file, const idBounds& value) {
  WriteBigVec3(file, value.b[0]);
  WriteBigVec3(file, value.b[1]);
}

bool ReadBigBounds(idFile* file, idBounds& value) {
  return ReadBigVec3(file, value.b[0]) && ReadBigVec3(file, value.b[1]);
}

void WriteString(idFile* file, const char* value) {
  const int length = value != nullptr ? static_cast<int>(std::strlen(value)) : 0;
  file->Write(&length, sizeof(length));
  if (length > 0)
    file->Write(value, static_cast<std::size_t>(length));
}

bool ReadString(idFile* file, idStr& value) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)) || length < 0 ||
      length > 1024 * 1024) {
    return false;
  }
  std::vector<char> text(static_cast<std::size_t>(length) + 1U, '\0');
  if (length > 0 &&
      !ReadExact(file, text.data(), static_cast<std::size_t>(length))) {
    return false;
  }
  value = text.data();
  return true;
}

void WriteBool(idFile* file, bool value) { file->Write(&value, 1); }
bool ReadBool(idFile* file, bool& value) { return ReadExact(file, &value, 1); }

void MarkCommit(idRenderModel& model, bool value = true) {
  if (model.world != nullptr && model.committed != nullptr &&
      runtimeCallbacks.markCommit != nullptr) {
    runtimeCallbacks.markCommit(model.world, model.GetIndex(), value);
  }
}

void SerializeFlag(idSerializer& serializer, std::uint8_t& byte,
                   std::uint8_t mask) {
  bool value = (byte & mask) != 0U;
  if (serializer.writing)
    serializer.msg->WriteBits(value ? 1 : 0, 1);
  else
    value = serializer.msg->ReadBits(1) == 1;
  byte = static_cast<std::uint8_t>((byte & ~mask) | (value ? mask : 0U));
}

bool SameFloatBits(float left, float right) {
  std::uint32_t a;
  std::uint32_t b;
  std::memcpy(&a, &left, sizeof(a));
  std::memcpy(&b, &right, sizeof(b));
  return a == b;
}

bool TransformChanged(const idRenderModel& model) {
  const idRenderModelParms& left = model.g;
  const idRenderModelParms& right = model.committed->r;
  for (int index = 0; index < 3; ++index) {
    if (!SameFloatBits(left.origin[index], right.origin[index]) ||
        !SameFloatBits(left.scale[index], right.scale[index]))
      return true;
    for (int column = 0; column < 3; ++column) {
      if (!SameFloatBits(left.axis[index][column], right.axis[index][column]))
        return true;
    }
    if (!SameFloatBits(model.referenceBounds.b[0][index],
                       model.committed->referenceBounds.b[0][index]) ||
        !SameFloatBits(model.referenceBounds.b[1][index],
                       model.committed->referenceBounds.b[1][index]))
      return true;
  }
  return false;
}

void SetFlagsFromSurfaces(idRenderModelCommitted& model) {
  model.committedFlags0 &= 0x03U;
  model.committedFlags1 &= 0xf6U;
  for (int index = 0; index < model.surfaces.Num(); ++index) {
    const idRenderModelSurface& surface = model.surfaces[index];
    const idMaterial* material = surface.material;
    if (material == nullptr)
      continue;
    if (material->coverage != MC_INVISIBLE)
      model.committedFlags0 |= 0x08U;
    if (material->coverage == MC_OPAQUE ||
        material->coverage == MC_PERFORATED_FOR_MODELFADE) {
      model.committedFlags1 |= 0x01U;
    }
    if ((material->flags0 & 0x01U) != 0U)
      model.committedFlags0 |= 0x10U;
    if (material->progs[1] != nullptr)
      model.committedFlags0 |= 0x20U;
    if (material->pageFlavor == ID_VTF_BAKE)
      model.committedFlags0 |= 0x80U;
    if ((material->flags0 & 0x20U) != 0U)
      model.committedFlags0 |= 0x04U;
    if (material->progs[3] != nullptr)
      model.committedFlags0 |= 0x40U;
    if ((material->flags1 & 0x10U) != 0U)
      model.committedFlags1 |= 0x08U;
  }
  if ((model.r.flags0 & 0x04U) != 0U &&
      runtimeCallbacks.isComboMap != nullptr &&
      runtimeCallbacks.isComboMap(&model)) {
    model.committedFlags0 &= static_cast<std::uint8_t>(~0x04U);
  }
  if ((model.committedFlags1 & 0x04U) != 0U)
    model.committedFlags0 &= static_cast<std::uint8_t>(~0x04U);
  if ((model.committedFlags1 & 0x02U) != 0U)
    model.committedFlags0 |= 0x04U;
}

} // namespace

void SetRenderModelRuntimeCallbacks(
    const idRenderModelRuntimeCallbacks& callbacks) {
  runtimeCallbacks = callbacks;
}

idRenderModelParms::idRenderModelParms()
    : origin{}, axis(mat3_identity), scale{1.0f, 1.0f, 1.0f},
      alignmentPadding{}, customMaterial(nullptr), customSkin(-1),
      groupMasterIndex(-1), lightGroupID(-1), allowSurfaceOnlyInViewID(0),
      suppressSurfaceInViewID(0), suppressShadowInViewID(0),
      suppressShadowInLightIndex(-1), firstVisibleFrameCount(2),
      fovScale(1.0f), modelDepthHack(0.0f),
      castDimShadows(DIM_SHADOW_DEFAULT), flags0(0), flags1(0),
      flags2(0), flags3(0), flags4(0), flagPadding{}, traceGroup(0),
      alignmentBeforeRadiosity{},
      radiosityScale(1.0f), staticReflectionStrength(1.0f),
      highlightColor(HIGHLIGHT_COLOR_NONE), dimShadowClipZ(0.0f),
      maxTexelDensity(1.0f), modelFade(1.0f), maxVisibleRange(0.0f),
      fadeVisibilityOver(512.0f), localLightPositionOverride{},
      modelDepthSort(MODEL_DEPTH_SORT_NORMAL), referenceModel(nullptr),
      imposterParent(nullptr), imposterNext(nullptr), viewport{} {
  flags2 &= static_cast<std::uint8_t>(~4U);
}

idRenderModelParms&
idRenderModelParms::operator=(const idRenderModelParms& other) {
  if (this == &other)
    return *this;
  origin = other.origin;
  axis = other.axis;
  scale = other.scale;
  customMaterial = other.customMaterial;
  customSkin = other.customSkin;
  groupMasterIndex = other.groupMasterIndex;
  lightGroupID = other.lightGroupID;
  allowSurfaceOnlyInViewID = other.allowSurfaceOnlyInViewID;
  suppressSurfaceInViewID = other.suppressSurfaceInViewID;
  suppressShadowInViewID = other.suppressShadowInViewID;
  suppressShadowInLightIndex = other.suppressShadowInLightIndex;
  firstVisibleFrameCount = other.firstVisibleFrameCount;
  fovScale = other.fovScale;
  modelDepthHack = other.modelDepthHack;
  castDimShadows = other.castDimShadows;
  flags0 = other.flags0;
  flags1 = other.flags1;
  flags2 = other.flags2;
  flags3 = other.flags3;
  flags4 = static_cast<std::uint8_t>((flags4 & 0x80U) |
                                     (other.flags4 & 0x7fU));
  traceGroup = other.traceGroup;
  radiosityScale = other.radiosityScale;
  staticReflectionStrength = other.staticReflectionStrength;
  highlightColor = other.highlightColor;
  dimShadowClipZ = other.dimShadowClipZ;
  maxTexelDensity = other.maxTexelDensity;
  modelFade = other.modelFade;
  maxVisibleRange = other.maxVisibleRange;
  fadeVisibilityOver = other.fadeVisibilityOver;
  localLightPositionOverride = other.localLightPositionOverride;
  modelDepthSort = other.modelDepthSort;
  referenceModel = other.referenceModel;
  imposterParent = other.imposterParent;
  imposterNext = other.imposterNext;
  viewport = other.viewport;
  return *this;
}

void idSerializer::SerializePacked(int* original) {
  if (writing) {
    std::uint32_t value = static_cast<std::uint32_t>(*original);
    do {
      const std::uint8_t byte = static_cast<std::uint8_t>(
          (value & 0x7fU) | (value > 0x7fU ? 0x80U : 0U));
      msg->WriteBits(byte, 8);
      value >>= 7;
    } while (value != 0U);
  } else {
    std::uint32_t value = 0;
    int shift = 0;
    while (shift < 32) {
      const std::uint8_t byte = static_cast<std::uint8_t>(msg->ReadBits(8));
      value |= static_cast<std::uint32_t>(byte & 0x7fU) << shift;
      shift += 7;
      if ((byte & 0x80U) == 0U)
        break;
    }
    *original = static_cast<int>(value);
  }
}

idRenderModel::idRenderModel()
    : name(), debugName(nullptr), deferredOrigin{}, deferredAxis(mat3_identity),
      useDeferredPosition(0), deferredPositionInitialized(0), deleteOnSync(0),
      unlinked(8), forceUpdateInView(0), recoveredFlagPadding{},
      referenceBounds{}, gameParmBlock(), decalLastUpdateTime(0),
      decalUpdateTime(0), world(nullptr), committed(nullptr), surfaces(),
      decalData(), renderModelType(RENDERMODEL_DEFAULT), recoveredModelTail(0) {
  ::new (static_cast<void*>(&g)) idRenderModelParms();
  name.Set("<unnamed>");
  for (decalData_t& decal : decalData.staticList) {
    decal.handle = {-1, -1};
    decal.jointId.value = -1;
  }
  committed = new idRenderModelCommitted{};
  committed->index.index = static_cast<unsigned int>(-1);
  committed->maxVisibleRange.w = 262144.0f;
  committed->committedFlags0 = 0x20U;
  committed->nextAddAlwaysModel = committed;
  committed->nextDimShadowModel = committed;
  committed->nextApproxLightModel = committed;
  committed->name = name.str;
}

idRenderModel::~idRenderModel() {
  FreeSurfaces();
  RemoveDecals();
  if (world != nullptr && committed != nullptr) {
    if (runtimeCallbacks.markDeleted != nullptr)
      runtimeCallbacks.markDeleted(world, GetIndex(), false);
    if (runtimeCallbacks.markCommit != nullptr)
      runtimeCallbacks.markCommit(world, GetIndex(), false);
  }
  delete committed;
  committed = nullptr;
  g.~idRenderModelParms();
}

bool idRenderModel::UpdateInView(const idRenderView*, const idRenderView*,
                                 idRenderModelUpdateTools*) {
  return false;
}

bool idRenderModel::CommitSubclass() { return false; }

int idRenderModel::GetIndex() const {
  return committed != nullptr ? committed->index.index >> 8 : -1;
}

idRenderDebug* idRenderModel::GetRenderDebug() const {
  return world != nullptr ? world->GetRenderDebug() : nullptr;
}

void idRenderModel::SetViewport(int x, int y, int width, int height) {
  g.viewport.x1 = x;
  g.viewport.y1 = y;
  g.viewport.x2 = x + width - 1;
  g.viewport.y2 = y + height - 1;
}

void idRenderModel::SetParm(const idDeclRenderParm* parm,
                            const parmValue_t& value) {
  gameParmBlock.SetParm(parm,
                        idVec4{value.value[0], value.value[1], value.value[2],
                               value.value[3]});
}

void idRenderModel::SetParm(const idDeclRenderParm* parm, float scalar) {
  gameParmBlock.SetParm(parm, idVec4{scalar, scalar, scalar, scalar});
}

void idRenderModel::SetParm(const idDeclRenderParm* parm,
                            const idVec3& value) {
  gameParmBlock.SetParm(parm, idVec4{value.x, value.y, value.z, 0.0f});
}

void idRenderModel::SetParm(const idDeclRenderParm* parm,
                            const idVec4& value) {
  gameParmBlock.SetParm(parm, value);
}

bool idRenderModel::SetDecalPosition(decalHandle_t handle,
                                     const idVec3& position,
                                     const idMat3& axis) {
  return world != nullptr && runtimeCallbacks.setDecalPosition != nullptr &&
         runtimeCallbacks.setDecalPosition(world, handle, position, axis);
}

void idRenderModel::BoundsInAreas(const idBounds& bounds, int* areas,
                                  int* numAreas, int maxAreas) {
  if (world != nullptr)
    world->BoundsInAreas(&bounds, areas, numAreas, maxAreas);
}

void idRenderModel::ClearOcclusionQuery() {
  if (world != nullptr && runtimeCallbacks.clearOcclusion != nullptr)
    runtimeCallbacks.clearOcclusion(world, this);
}

const approximateLighting_t* idRenderModel::GetApproximateLighting() const {
  return committed != nullptr
             ? &committed->approximateLighting[
                   committed->CurrentApproximateLighting()]
             : nullptr;
}

void idRenderModel::SetForceApproximateLightingCalculation() {
  if (committed != nullptr)
    committed->committedFlags1 |= 2U;
}

bool idRenderModel::SetPreventApproximateLightingCalculation(
    bool allowLighting) {
  if (committed == nullptr)
    return false;
  const bool previous = (committed->committedFlags1 & 4U) != 0U;
  committed->committedFlags1 = static_cast<std::uint8_t>(
      (committed->committedFlags1 & ~4U) | (allowLighting ? 4U : 0U));
  return previous;
}

void idRenderModelUpdateTools::ApproximateLightingForBounds(
    idRenderModel* model, const idBounds* bounds,
    approximateLighting_t* lighting) {
  if (model != nullptr && model->world != nullptr)
    model->world->ApproximateLightingForBounds(bounds, lighting);
}

void idRenderModelUpdateTools::GetCurrentResolutionScale(float* x, float* y) {
  if (runtimeCallbacks.getResolutionScale != nullptr)
    runtimeCallbacks.getResolutionScale(x, y);
}

void idRenderModel::SetName(const char* modelName) {
  name.Set(modelName != nullptr ? modelName : "");
  if (committed != nullptr)
    committed->name = name.str;
}

void idRenderModel::FinishSurfaces() {
  const auto* customMaterial =
      static_cast<const idMaterial*>(g.customMaterial);
  if (customMaterial != nullptr) {
    for (int index = 0; index < surfaces.Num(); ++index) {
      const idMaterial* material = surfaces[index].material;
      if (material != nullptr && material->coverage != MC_INVISIBLE)
        surfaces[index].material = customMaterial;
    }
  }
  if (surfaces.Num() == 0)
    return;
  referenceBounds.Clear();
  for (int index = 0; index < surfaces.Num(); ++index) {
    if (surfaces[index].geometry == nullptr)
      continue;
    referenceBounds.AddPoint(surfaces[index].geometry->bounds.b[0]);
    referenceBounds.AddPoint(surfaces[index].geometry->bounds.b[1]);
  }
}

void idRenderModel::CommitThisFrame() {
  idRenderModel* model = this;
  for (;;) {
    model->unlinked &= static_cast<std::uint8_t>(~8U);
    MarkCommit(*model);
    auto* parent = static_cast<idRenderModel*>(model->g.imposterParent);
    if (parent == nullptr || parent == model)
      break;
    model = parent;
  }
}

void idRenderModel::SetupMVP(idRenderMatrix* mvpMatrix,
                             const idRenderView* view, float zNear,
                             float zFar, float guiOriginOffset) {
  idRenderMatrix modelMatrix{};
  idRenderMatrix::CreateFromOriginAxisScale(g.origin, g.axis, g.scale,
                                             modelMatrix);
  SetupModelMVPMatrix(mvpMatrix, &g, &modelMatrix, view, zNear, zFar,
                      guiOriginOffset);
}

const idDeclSkins* idRenderModel::GetSkins() const {
  idStr skinName(name.str != nullptr ? name.str : "");
  skinName.StripFileExtension();
  return runtimeCallbacks.findSkins != nullptr
             ? runtimeCallbacks.findSkins(skinName.data)
             : nullptr;
}

void idRenderModel::FreeSurfaces() {
  for (int index = 0; index < surfaces.Num(); ++index) {
    idRenderModelSurface& surface = surfaces[index];
    if (surface.geometry != nullptr && !surface.geometryIsReference &&
        (surface.referenceMask & 1U) == 0U) {
      delete surface.geometry;
      surface.geometry = nullptr;
    }
    if (surface.stMap != nullptr && (surface.referenceMask & 8U) == 0U) {
      delete surface.stMap;
      surface.stMap = nullptr;
    }
    if (surface.morphMap != nullptr && (surface.referenceMask & 4U) == 0U) {
      delete surface.morphMap;
      surface.morphMap = nullptr;
    }
    if (surface.joints != nullptr && (surface.referenceMask & 2U) == 0U) {
      delete surface.joints;
      surface.joints = nullptr;
    }
    if (surface.surfaceConstants != nullptr &&
        (surface.referenceMask & 0x10U) == 0U) {
      delete surface.surfaceConstants;
      surface.surfaceConstants = nullptr;
    }
    if (surface.parms != nullptr && (surface.referenceMask & 0x20U) == 0U) {
      delete surface.parms;
      surface.parms = nullptr;
    }
  }
  surfaces.ClearFree();
}

void idRenderModel::UnlinkSubclass() {
  auto* parent = static_cast<idRenderModel*>(g.imposterParent);
  if (parent == this) {
    for (auto* child = static_cast<idRenderModel*>(g.imposterNext);
         child != nullptr;
         child = static_cast<idRenderModel*>(child->g.imposterNext)) {
      if ((child->unlinked & 8U) == 0U) {
        child->unlinked &= static_cast<std::uint8_t>(~8U);
        MarkCommit(*child);
        auto* childParent =
            static_cast<idRenderModel*>(child->g.imposterParent);
        if (childParent != nullptr && childParent != child)
          childParent->CommitThisFrame();
        child->UnlinkSubclass();
        child->unlinked |= 8U;
      }
    }
  }
}

void idRenderModel::DeleteSubclass() {
  auto* parent = static_cast<idRenderModel*>(g.imposterParent);
  if (parent == nullptr)
    return;
  if (parent == this) {
    auto* child = static_cast<idRenderModel*>(g.imposterNext);
    while (child != nullptr) {
      auto* next = static_cast<idRenderModel*>(child->g.imposterNext);
      child->g.imposterParent = nullptr;
      child->g.imposterNext = nullptr;
      if ((child->unlinked & 8U) == 0U) {
        child->unlinked &= static_cast<std::uint8_t>(~8U);
        MarkCommit(*child);
        child->UnlinkSubclass();
        child->unlinked |= 8U;
      }
      child = next;
    }
  } else {
    auto* previous = parent;
    auto* current = static_cast<idRenderModel*>(parent->g.imposterNext);
    while (current != nullptr && current != this) {
      previous = current;
      current = static_cast<idRenderModel*>(current->g.imposterNext);
    }
    if (current == this)
      previous->g.imposterNext = current->g.imposterNext;
    g.imposterParent = nullptr;
    g.imposterNext = nullptr;
  }
}

void idRenderModel::RemoveDecals() {
  if (world != nullptr && runtimeCallbacks.removeDecal != nullptr) {
    for (int index = 0; index < decalData.Num(); ++index)
      runtimeCallbacks.removeDecal(world, decalData[index].handle);
  }
  decalData.ClearFree();
}

void idRenderModel::InitDecalData(decalData_t& decal,
                                  const decalParams_t* decalParms,
                                  unsigned startTime,
                                  const idVec3& position,
                                  const idVec3& direction) {
  decal.handle = {-1, -1};
  decal.jointId.value = -1;
  if (world == nullptr || runtimeCallbacks.addDecal == nullptr)
    return;
  decal.handle = runtimeCallbacks.addDecal(
      world, decalParms, startTime, position, direction,
      renderJointIndex_t{static_cast<std::int16_t>(-1)});
  idMat3 initialAxis = mat3_identity;
  if (runtimeCallbacks.getDecalAxis != nullptr)
    runtimeCallbacks.getDecalAxis(world, decal.handle, initialAxis);

  const bool deferred = (useDeferredPosition & 1U) != 0U;
  const idVec3& origin = deferred ? deferredOrigin : g.origin;
  const idMat3& axis = deferred ? deferredAxis : g.axis;
  const idVec3 delta = position - origin;
  decal.relativePos = {delta * axis.mat[0], delta * axis.mat[1],
                       delta * axis.mat[2]};
  idMat3 transpose{};
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column)
      transpose.mat[row][column] = axis.mat[column][row];
  }
  decal.relativeAxis = initialAxis * transpose;
}

bool idRenderModel::RemoveDecal(decalHandle_t handle) {
  if (world == nullptr)
    return false;
  if (handle.decalIndex == -1) {
    const bool removed = decalData.Num() > 0;
    RemoveDecals();
    return removed;
  }
  for (int index = 0; index < decalData.Num(); ++index) {
    if (decalData[index].handle.decalIndex != handle.decalIndex)
      continue;
    const decalHandle_t removedHandle = decalData[index].handle;
    decalData.RemoveIndexFast(index);
    if (runtimeCallbacks.removeDecal != nullptr)
      runtimeCallbacks.removeDecal(world, removedHandle);
    return true;
  }
  return false;
}

void idRenderModel::UpdateDecalReferences() {
  decalLastUpdateTime = runtimeCallbacks.gameTimeMilliseconds != nullptr
                            ? runtimeCallbacks.gameTimeMilliseconds()
                            : decalLastUpdateTime;
  if (decalLastUpdateTime - decalUpdateTime <= 1000 || world == nullptr ||
      runtimeCallbacks.isDecalInvalid == nullptr) {
    return;
  }
  int index = 0;
  while (index < decalData.Num()) {
    if (runtimeCallbacks.isDecalInvalid(world, decalData[index].handle))
      decalData.RemoveIndexFast(index);
    else
      ++index;
  }
  decalUpdateTime = decalLastUpdateTime;
}

decalHandle_t idRenderModel::AddDecalFromPoint(
    const decalParams_t* decalParms, unsigned startTime,
    const idVec3& position, const idVec3& direction) {
  UpdateDecalReferences();
  decalData_t decal{};
  decal.handle = {-1, -1};
  decal.jointId.value = -1;
  if (decalData.Num() < decalData.size) {
    InitDecalData(decal, decalParms, startTime, position, direction);
    if (decal.handle.decalIndex != -1)
      decalData.Append(decal);
  }
  return decal.handle;
}

void idRenderModel::SerializeSnapshot(idSerializer* serializer,
                                      bool serializeParmBlock) {
  if (serializer == nullptr || serializer->msg == nullptr)
    return;
  idFile_BitMsg file(serializer->msg);
  const float oldFade = g.modelFade;
  serializer->SerializeQ<1, 8>(&g.modelFade);
  if (serializeParmBlock) {
    if (serializer->writing)
      gameParmBlock.Save(&file);
    else
      gameParmBlock.Load(&file);
  } else if (!serializer->writing &&
             std::fabs(oldFade - g.modelFade) > 0.01f &&
             (g.flags4 & 4U) == 0U) {
    if (g.modelFade == 1.0f) {
      g.flags2 &= static_cast<std::uint8_t>(~0x20U);
      if (runtimeCallbacks.modelFadeParm != nullptr)
        gameParmBlock.ClearParm(runtimeCallbacks.modelFadeParm);
    } else {
      g.flags2 |= 0x20U;
      if (runtimeCallbacks.modelFadeParm != nullptr) {
        gameParmBlock.SetParm(runtimeCallbacks.modelFadeParm,
                              idVec4{g.modelFade, 1.0f, 0.0f, 0.0f});
      }
    }
  }

  if (serializer->writing) {
    idMaterial::resourceList.WriteResourceToMsg(
        static_cast<const idMaterial*>(g.customMaterial), serializer->msg);
  } else {
    g.customMaterial = idMaterial::resourceList.ReadResourceFromMsg(
        serializer->msg);
  }
  serializer->Serialize(&g.scale);
  serializer->SerializePacked(&g.customSkin);
  serializer->SerializePacked(&g.allowSurfaceOnlyInViewID);
  serializer->SerializePacked(&g.suppressSurfaceInViewID);
  serializer->SerializePacked(&g.suppressShadowInViewID);
  serializer->SerializePacked(&g.suppressShadowInLightIndex);
  serializer->SerializeUQ<255, 16>(&g.fovScale);
  serializer->SerializeQ<128, 16>(&g.modelDepthHack);
  SerializeFlag(*serializer, g.flags0, 0x80U);

  bool casts = g.castDimShadows != DIM_SHADOW_DEFAULT;
  if (serializer->writing)
    serializer->msg->WriteBits(casts ? 1 : 0, 1);
  else {
    casts = serializer->msg->ReadBits(1) == 1;
    g.castDimShadows = static_cast<dimShadow_t>(casts ? 1 : 0);
  }
  SerializeFlag(*serializer, g.flags1, 0x10U);
  SerializeFlag(*serializer, g.flags1, 0x20U);
  SerializeFlag(*serializer, g.flags1, 0x80U);
  SerializeFlag(*serializer, g.flags2, 0x02U);
  SerializeFlag(*serializer, g.flags2, 0x01U);
  SerializeFlag(*serializer, g.flags2, 0x08U);
  SerializeFlag(*serializer, g.flags2, 0x10U);
  SerializeFlag(*serializer, g.flags2, 0x20U);
  SerializeFlag(*serializer, g.flags2, 0x40U);
  SerializeFlag(*serializer, g.flags3, 0x01U);
  SerializeFlag(*serializer, g.flags0, 0x20U);

  bool snapshotUnlinked = (unlinked & 8U) != 0U;
  if (serializer->writing) {
    serializer->msg->WriteBits(snapshotUnlinked ? 1 : 0, 1);
  } else {
    snapshotUnlinked = serializer->msg->ReadBits(1) == 1;
    if (snapshotUnlinked) {
      if ((unlinked & 8U) == 0U) {
        unlinked &= static_cast<std::uint8_t>(~8U);
        MarkCommit(*this);
        auto* parent = static_cast<idRenderModel*>(g.imposterParent);
        if (parent != nullptr && parent != this)
          parent->CommitThisFrame();
        UnlinkSubclass();
        unlinked |= 8U;
      }
    } else {
      unlinked &= static_cast<std::uint8_t>(~8U);
      MarkCommit(*this);
      auto* parent = static_cast<idRenderModel*>(g.imposterParent);
      if (parent != nullptr && parent != this)
        parent->CommitThisFrame();
    }
  }
}

void idRenderModel::Save(idFile* file) const {
  if (file == nullptr)
    return;
  WriteBigVec3(file, deferredOrigin);
  WriteBigMat3(file, deferredAxis);
  WriteBool(file, (useDeferredPosition & 1U) != 0U);
  WriteBool(file, (deferredPositionInitialized & 2U) != 0U);
  WriteBool(file, (unlinked & 8U) != 0U);
  WriteBigBounds(file, referenceBounds);
  const auto* customMaterial =
      static_cast<const idMaterial*>(g.customMaterial);
  WriteString(file,
              customMaterial != nullptr ? customMaterial->GetName() : "");
  WriteBig32(file, static_cast<std::uint32_t>(g.customSkin));
  WriteBigVec3(file, g.origin);
  WriteBigMat3(file, g.axis);
  WriteBigVec3(file, g.scale);
  WriteBig32(file, static_cast<std::uint32_t>(g.groupMasterIndex));
  WriteBig32(file, static_cast<std::uint32_t>(g.allowSurfaceOnlyInViewID));
  WriteBig32(file, static_cast<std::uint32_t>(g.suppressSurfaceInViewID));
  WriteBig32(file, static_cast<std::uint32_t>(g.suppressShadowInViewID));
  WriteBig32(file, static_cast<std::uint32_t>(g.suppressShadowInLightIndex));
  WriteBigFloat(file, g.fovScale);
  WriteBigFloat(file, g.modelDepthHack);
  WriteBig32(file, static_cast<std::uint32_t>(g.castDimShadows));
  WriteBigFloat(file, g.dimShadowClipZ);

  const std::pair<const std::uint8_t*, std::uint8_t> flags[] = {
      {&g.flags0, 0x01U}, {&g.flags0, 0x80U}, {&g.flags1, 0x04U},
      {&g.flags1, 0x08U}, {&g.flags1, 0x10U}, {&g.flags3, 0x01U},
      {&g.flags3, 0x02U}, {&g.flags3, 0x04U}, {&g.flags3, 0x08U},
      {&g.flags3, 0x10U}, {&g.flags3, 0x20U}, {&g.flags1, 0x20U},
      {&g.flags4, 0x08U}, {&g.flags4, 0x10U}, {&g.flags1, 0x80U},
      {&g.flags2, 0x02U}, {&g.flags2, 0x01U}, {&g.flags2, 0x08U},
      {&g.flags2, 0x10U}, {&g.flags2, 0x20U}, {&g.flags2, 0x40U},
      {&g.flags1, 0x40U}};
  for (const auto& entry : flags)
    WriteBool(file, (*entry.first & entry.second) != 0U);
  WriteBig32(file, static_cast<std::uint32_t>(g.highlightColor));
  WriteBigFloat(file, g.modelFade);
  WriteBigFloat(file, g.maxVisibleRange);
  WriteBigFloat(file, g.fadeVisibilityOver);
  WriteBool(file, (g.flags0 & 0x20U) != 0U);
  gameParmBlock.Save(file);
}

bool idRenderModel::Load(idFile* file) {
  if (!ReadBigVec3(file, deferredOrigin) ||
      !ReadBigMat3(file, deferredAxis)) {
    return false;
  }
  bool useDeferred = false;
  bool deferredInitialized = false;
  bool isUnlinked = true;
  bool forceUpdate = false;
  if (!ReadBool(file, useDeferred) || !ReadBool(file, deferredInitialized) ||
      !ReadBool(file, isUnlinked) || !ReadBool(file, forceUpdate)) {
    return false;
  }
  useDeferredPosition = static_cast<std::uint8_t>(useDeferred ? 1U : 0U);
  deferredPositionInitialized =
      static_cast<std::uint8_t>(deferredInitialized ? 2U : 0U);
  unlinked = static_cast<std::uint8_t>(isUnlinked ? 8U : 0U);
  forceUpdateInView = static_cast<std::uint8_t>(forceUpdate ? 0x10U : 0U);
  if (!ReadBigBounds(file, referenceBounds))
    return false;

  idStr materialName;
  if (!ReadString(file, materialName))
    return false;
  g.customMaterial =
      materialName.IsEmpty() || runtimeCallbacks.findMaterial == nullptr
          ? nullptr
          : runtimeCallbacks.findMaterial(materialName.data);
  std::uint32_t integer = 0;
  if (!ReadBig32(file, integer))
    return false;
  g.customSkin = static_cast<int>(integer);
  if (!ReadBigVec3(file, g.origin) || !ReadBigMat3(file, g.axis) ||
      !ReadBigVec3(file, g.scale)) {
    return false;
  }
  int* integerFields[] = {
      &g.groupMasterIndex,          &g.allowSurfaceOnlyInViewID,
      &g.suppressSurfaceInViewID,  &g.suppressShadowInViewID,
      &g.suppressShadowInLightIndex};
  for (int* field : integerFields) {
    if (!ReadBig32(file, integer))
      return false;
    *field = static_cast<int>(integer);
  }
  if (!ReadBigFloat(file, g.fovScale) ||
      !ReadBigFloat(file, g.modelDepthHack) || !ReadBig32(file, integer) ||
      !ReadBigFloat(file, g.dimShadowClipZ)) {
    return false;
  }
  g.castDimShadows = static_cast<dimShadow_t>(integer);

  struct FlagTarget {
    std::uint8_t* byte;
    std::uint8_t mask;
  };
  const FlagTarget flagTargets[] = {
      {&g.flags0, 0x01U}, {&g.flags0, 0x80U}, {&g.flags1, 0x04U},
      {&g.flags1, 0x08U}, {&g.flags1, 0x10U}, {&g.flags3, 0x01U},
      {&g.flags3, 0x02U}, {&g.flags3, 0x04U}, {&g.flags3, 0x08U},
      {&g.flags3, 0x10U}, {&g.flags3, 0x20U}, {&g.flags1, 0x20U},
      {&g.flags4, 0x08U}, {&g.flags4, 0x10U}, {&g.flags1, 0x80U},
      {&g.flags2, 0x02U}, {&g.flags2, 0x01U}, {&g.flags2, 0x08U},
      {&g.flags2, 0x10U}, {&g.flags2, 0x20U}, {&g.flags2, 0x40U},
      {&g.flags1, 0x40U}};
  for (const FlagTarget& target : flagTargets) {
    bool value = false;
    if (!ReadBool(file, value))
      return false;
    *target.byte = static_cast<std::uint8_t>(
        (*target.byte & ~target.mask) | (value ? target.mask : 0U));
  }
  if (!ReadBig32(file, integer))
    return false;
  g.highlightColor = static_cast<highlightColor_t>(integer);
  if (!ReadBigFloat(file, g.modelFade) ||
      !ReadBigFloat(file, g.maxVisibleRange) ||
      !ReadBigFloat(file, g.fadeVisibilityOver)) {
    return false;
  }
  bool augment = false;
  if (!ReadBool(file, augment))
    return false;
  g.flags0 = static_cast<std::uint8_t>((g.flags0 & ~0x20U) |
                                       (augment ? 0x20U : 0U));
  return gameParmBlock.Load(file);
}

void idRenderModel::AddSurface(const idRenderModelSurface& surface) {
  surfaces.Append(surface);
}

void idRenderModel::Commit() {
  if (r_skipCommits.valueInteger != 0 || committed == nullptr)
    return;
  if (r_showCommits.valueInteger > 1) {
    idLib::Printf("model commit: index %i, %s\n", GetIndex(), name.str);
  }
  bool updateReferences = r_forceReferencesOnCommit.valueInteger != 0 ||
                          committed->references.Num() == 0 ||
                          TransformChanged(*this);
  if (r_showReferenceUpdates.valueInteger != 0) {
    idLib::Printf("updating references for %s (%d): %s\n", name.str,
                  GetIndex(), updateReferences ? "TRUE" : "FALSE");
  }
  const bool customMaterialChanged =
      g.customMaterial != committed->r.customMaterial;
  committed->renderParmBlock = gameParmBlock;
  committed->r = g;
  auto* referenceModel = const_cast<idRenderModel*>(
      static_cast<const idRenderModel*>(g.referenceModel));
  committed->referenceModel =
      referenceModel != nullptr ? referenceModel->committed : nullptr;
  committed->guiFrameCount = runtimeCallbacks.getGuiFrameCount != nullptr
                                 ? runtimeCallbacks.getGuiFrameCount()
                                 : 0;
  committed->approximateState =
      (committed->approximateState & 3U) |
      (static_cast<std::uint32_t>(renderModelType) << 2U);
  idRenderMatrix::CreateFromOriginAxisScale(
      committed->r.origin, committed->r.axis, committed->r.scale,
      committed->modelMatrix);
  if (!idRenderMatrix::Inverse(committed->modelMatrix,
                               committed->inverseModelMatrix)) {
    committed->r.axis = mat3_identity;
    idRenderMatrix::CreateFromOriginAxis(committed->r.origin,
                                         committed->r.axis,
                                         committed->modelMatrix);
    idRenderMatrix::Inverse(committed->modelMatrix,
                            committed->inverseModelMatrix);
  }
  if (CommitSubclass())
    updateReferences = true;
  if ((useDeferredPosition & 1U) != 0U) {
    g.origin = deferredOrigin;
    g.axis = deferredAxis;
  }
  deferredPositionInitialized |= 2U;
  if (customMaterialChanged)
    FinishSurfaces();
  committed->surfaces.SetNum(surfaces.Num());
  for (int index = 0; index < surfaces.Num(); ++index)
    committed->surfaces[index] = surfaces[index];
  SetFlagsFromSurfaces(*committed);
  committed->referenceBounds = referenceBounds;
  if (r_checkBounds.valueInteger != 0 &&
      runtimeCallbacks.checkCommittedBounds != nullptr) {
    runtimeCallbacks.checkCommittedBounds(committed);
  }
  if (world != nullptr && (committed->r.flags0 & 8U) == 0U) {
    committed->committedFlags0 = static_cast<std::uint8_t>(
        (committed->committedFlags0 & ~2U) |
        (updateReferences ? 2U : 0U));
  }
}

void idRenderModel::SetMaxSurfaces(int maximum) {
  if (maximum > surfaces.size)
    surfaces.ResizeForPort(maximum);
  surfaces.listStatic = 2;
  if (committed != nullptr) {
    if (maximum > committed->surfaces.size)
      committed->surfaces.ResizeForPort(maximum);
    committed->surfaces.listStatic = 2;
  }
}
