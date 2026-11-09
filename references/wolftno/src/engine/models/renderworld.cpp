#include "engine/models/renderworld.h"

#include "engine/models/renderlight.h"
#include "engine/models/rendermodel.h"
#include "engine/models/serializer.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/network/snapshot.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/polytope.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstring>
#include <new>

template <typename T>
void* idListArrayNew(int num, memTag_t tag, bool zeroBuffer) {
  if (num <= 0)
    return nullptr;
  const char* location =
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag";
  void* allocation = idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * static_cast<std::size_t>(num), tag, zeroBuffer,
      ALIGN_16, HEAP_DEFAULTHEAP);
  if (allocation == nullptr)
    return nullptr;
  T* values = static_cast<T*>(allocation);
  for (int index = 0; index < num; ++index)
    new (&values[index]) T;
  return allocation;
}

template <typename T>
void* idListArrayResize(void* oldValues, int oldNum, int newNum, memTag_t tag,
                        bool zeroBuffer) {
  void* replacement =
      newNum > 0 ? idListArrayNew<T>(newNum, tag, zeroBuffer) : nullptr;
  if (newNum > 0 && replacement == nullptr)
    return nullptr;
  T* oldList = static_cast<T*>(oldValues);
  T* newList = static_cast<T*>(replacement);
  const int copyCount = std::min(oldNum, newNum);
  for (int index = 0; index < copyCount; ++index)
    newList[index] = oldList[index];
  for (int index = 0; index < oldNum; ++index)
    oldList[index].~T();
  if (oldList != nullptr)
    idMem::Singleton()->Free(oldList, ALIGN_16);
  return replacement;
}

template <typename T> void idListArrayDelete(void* values, int num) {
  T* list = static_cast<T*>(values);
  for (int index = 0; index < num; ++index)
    list[index].~T();
  if (list != nullptr)
    idMem::Singleton()->Free(list, ALIGN_16);
}

template void* idListArrayNew<dynamicEnvironment_t>(int, memTag_t, bool);
template void* idListArrayResize<dynamicEnvironment_t>(void*, int, int,
                                                       memTag_t, bool);
template void* idListArrayNew<debugPolygon_t>(int, memTag_t, bool);
template void idListArrayDelete<debugPolygon_t>(void*, int);

idCVar r_showUpdateInView("r_showUpdateInView", "0", CVAR_BOOL,
                          "show update in view statistics");

namespace {

idRenderWorldRuntimeCallbacks runtimeCallbacks;

float BoundsVolume(const idBounds& bounds) {
  if (bounds.b[0].x >= bounds.b[1].x || bounds.b[0].y >= bounds.b[1].y ||
      bounds.b[0].z >= bounds.b[1].z)
    return 0.0f;
  return (bounds.b[1].x - bounds.b[0].x) *
         (bounds.b[1].y - bounds.b[0].y) *
         (bounds.b[1].z - bounds.b[0].z);
}

void UpdateEvent(idRenderWorldLocal* world, renderWorldUpdateEvent_t event,
                 idRenderModelUpdateTools* tools = nullptr) {
  if (runtimeCallbacks.updateEvent != nullptr)
    runtimeCallbacks.updateEvent(world, event, tools);
}

template <typename LinkMember>
void UnlinkCommittedModel(idRenderModelCommitted*& head,
                          idRenderModelCommitted* model,
                          LinkMember linkMember) {
  idRenderModelCommitted*& next = model->*linkMember;
  if (next == model)
    return;
  idRenderModelCommitted** link = &head;
  while (*link != nullptr && *link != model)
    link = &((*link)->*linkMember);
  if (*link == model)
    *link = next;
  next = model;
}

void WriteFloatBits(idBitMsg& message, float value) {
  int bits{};
  std::memcpy(&bits, &value, sizeof(bits));
  message.WriteBits(bits, 32);
}

float ReadFloatBits(idBitMsg& message) {
  const int bits = message.ReadBits(32);
  float value{};
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

} // namespace

void SetRenderWorldRuntimeCallbacks(
    const idRenderWorldRuntimeCallbacks& callbacks) {
  runtimeCallbacks = callbacks;
}

void idRenderWorldCommitted::UnlinkDimShadow(idRenderModelCommitted* model) {
  UnlinkCommittedModel(dimShadowModels, model,
                       &idRenderModelCommitted::nextDimShadowModel);
}

idStr idRenderWorldUtil::CreateMegaModelName(const char* mapName,
                                             const char* modelName,
                                             const char* entityName) {
  idStr result;
  if (idStr::Find(modelName, ".decl", true, 0, -1) != -1 ||
      idStr::Find(modelName, ".breakabledecl", true, 0, -1) != -1 ||
      idStr::Find(modelName, ".modelloddecl", true, 0, -1) != -1) {
    result = modelName;
  } else if (idStr::Find(modelName, ".md6", true, 0, -1) != -1) {
    result.Format("%s/megamodel_%s.decl", mapName, entityName);
  } else if (idStr::Find(modelName, ".break", true, 0, -1) != -1) {
    result.Format("%s/megabreakable_%s.breakabledecl", mapName, entityName);
  } else if (idStr::Find(modelName, ".modellod", true, 0, -1) != -1) {
    result.Format("%s/megalod_%s.modelloddecl", mapName, entityName);
  } else {
    result.Format("%s/_combo/megamodel_%s.bmodel", mapName, entityName);
  }
  return result;
}

idRenderWorld::~idRenderWorld() = default;
idRenderDebug::~idRenderDebug() = default;
debugPolygon_t::~debugPolygon_t() = default;
idRenderDebugLocal::idRenderDebugLocal() = default;
idRenderDebugLocal::~idRenderDebugLocal() = default;

debugText_t& debugText_t::operator=(const debugText_t& other) {
  if (this != &other) {
    std::memcpy(text, other.text, sizeof(text));
    origin = other.origin;
    scale = other.scale;
    color = other.color;
    viewAxis = other.viewAxis;
    align = other.align;
    lineWidth = other.lineWidth;
    orientToRenderViewAxis = other.orientToRenderViewAxis;
    depthTest = other.depthTest;
    lifeTime = other.lifeTime;
  }
  return *this;
}

void PrintUpdateInViewStatistics(const idRenderModelUpdateTools* updateTools) {
  if (updateTools == nullptr)
    return;
  const bool showParticleInfo = runtimeCallbacks.showParticleInfo != nullptr &&
                                runtimeCallbacks.showParticleInfo();
  if ((updateTools->pc.showStats || showParticleInfo) &&
      runtimeCallbacks.printUpdateStatistics != nullptr) {
    runtimeCallbacks.printUpdateStatistics(
        updateTools->pc.c_numParticleVertices >> 2);
  }
}

const char* idRenderWorldLocal::GetName() const { return mapName.data; }

bool idRenderWorldLocal::IsComboMap() const { return committedWorld->isComboMap; }

idRenderModel* idRenderWorldLocal::StampModel() {
  if (stampModel == nullptr)
    stampModel = AllocBaseRenderModel();
  return stampModel;
}

idRenderModelTransparency* idRenderWorldLocal::TransparencyRenderModel() {
  return transparencyRenderModel;
}

idRenderModelDecal* idRenderWorldLocal::DecalModel() {
  return decalRenderModel;
}

void idRenderWorldLocal::SetBinaryModelState(const char* binaryModelName,
                                             bool isVisible) {
  if (runtimeCallbacks.setBinaryModelState != nullptr)
    runtimeCallbacks.setBinaryModelState(committedWorld, binaryModelName,
                                         isVisible, false);
}

idRenderDebug* idRenderWorldLocal::GetRenderDebug() { return debugData; }

void idRenderWorldLocal::SetEnvironmentEffects(
    const dynamicEnvironmentHandle_t& handle, const idDeclEnv* envEffectsDecl) {
  const int index = FindEnvironment(handle);
  if (index >= 0)
    committedWorld->dynamicEnvironments[index].env = envEffectsDecl;
}

int idRenderWorldLocal::FindEnvironment(
    const dynamicEnvironmentHandle_t& handle) const {
  for (int index = 0; index < committedWorld->dynamicEnvironments.Num(); ++index) {
    if (committedWorld->dynamicEnvironments[index].handle.value == handle.value)
      return index;
  }
  return -1;
}

const idDeclEnv* idRenderWorldLocal::FindEnvironmentDecl(
    const idVec3& point) const {
  for (int index = 0; index < committedWorld->dynamicEnvironments.Num(); ++index) {
    const dynamicEnvironment_t& environment =
        committedWorld->dynamicEnvironments[index];
    const bool inBounds =
        point.x >= environment.bounds.b[0].x &&
        point.y >= environment.bounds.b[0].y &&
        point.z >= environment.bounds.b[0].z &&
        point.x <= environment.bounds.b[1].x &&
        point.y <= environment.bounds.b[1].y &&
        point.z <= environment.bounds.b[1].z;
    if (inBounds && idPolytope::ConvexPolytopeContainsPoint(
                        environment.planes.ptr, environment.numPlanes, point,
                        0.0f)) {
      return environment.env;
    }
  }
  return nullptr;
}

idRenderView* idRenderWorldLocal::RenderViewForIndex(int index) const {
  if (index >= 0 && index < renderViews.Num())
    return renderViews[index];
  idLib::Printf("idRenderWorld::RenderViewForIndex: invalid index %i [0, %i]\n",
                index, renderViews.Num());
  return nullptr;
}

idRenderModelFoliage* idRenderWorldLocal::FoliageRenderModel() {
  if (foliageRenderModel == nullptr && runtimeCallbacks.allocFoliageModel != nullptr) {
    foliageRenderModel = runtimeCallbacks.allocFoliageModel();
    AddRenderModel(reinterpret_cast<idRenderModel*>(foliageRenderModel));
  }
  return foliageRenderModel;
}

idRenderModelDetail* idRenderWorldLocal::DetailRenderModel() {
  if (detailRenderModel == nullptr && runtimeCallbacks.allocDetailModel != nullptr) {
    detailRenderModel = runtimeCallbacks.allocDetailModel();
    AddRenderModel(reinterpret_cast<idRenderModel*>(detailRenderModel));
  }
  return detailRenderModel;
}

void idRenderWorldLocal::MarkAllLightsAsSerialized() {
  for (int index = 0; index < renderLights.Num(); ++index) {
    idRenderLight* light = renderLights[index];
    if (light != nullptr && !light->skipAutoReplication)
      MarkLightSerialized(light, true, false);
  }
}

void idRenderWorldLocal::ResetSnapshots() {
  for (int index = 0; index < renderLights.Num(); ++index) {
    if (renderLights[index] != nullptr)
      renderLights[index]->needWriteToSnapshot = true;
  }
}

void idRenderWorldLocal::UpdateInView(const idRenderView* currentRenderView,
                                      const idRenderView* nextRenderView) {
  UpdateEvent(this, renderWorldUpdateEvent_t::WaitJobs);
  UpdateEvent(this, renderWorldUpdateEvent_t::AddJobTimeRange);
  UpdateEvent(this, renderWorldUpdateEvent_t::GenerateTransparencyList);
  UpdateEvent(this, renderWorldUpdateEvent_t::StartTransparency);
  UpdateEvent(this, renderWorldUpdateEvent_t::StartEffects);
  UpdateEvent(this, renderWorldUpdateEvent_t::StartFlare);
  UpdateEvent(this, renderWorldUpdateEvent_t::StartFlareOcclusion);
  UpdateEvent(this, renderWorldUpdateEvent_t::StartCuttableBeams);

  if (currentRenderView != nullptr && nextRenderView != nullptr) {
    idRenderModelUpdateTools updateTools{};
    updateTools.renderModelJobList = renderModelJobList;
    updateTools.transparencyModel = transparencyRenderModel;
    updateTools.globalShadowMap = runtimeCallbacks.globalShadowMap != nullptr
                                      ? runtimeCallbacks.globalShadowMap(committedWorld)
                                      : committedWorld->globalShadows.shadowMap;
    updateTools.pc.showStats = r_showUpdateInView.valueInteger != 0;
    updateTools.pc.c_numParticleVertices = 0;
    UpdateEvent(this, renderWorldUpdateEvent_t::GenerateVisibleInfluences,
                &updateTools);

    for (int index = 0; index < renderModels.Num(); ++index) {
      idRenderModel* model = renderModels[index];
      if (model == nullptr || (model->unlinked & 8U) != 0U)
        continue;
      const bool occlusionAllows =
          runtimeCallbacks.occlusionAllowsUpdate == nullptr ||
          runtimeCallbacks.occlusionAllowsUpdate(this, model, index,
                                                  currentRenderView,
                                                  nextRenderView);
      if ((model->forceUpdateInView & 0x10U) == 0U &&
          (model->g.flags2 & 0x08U) == 0U && !occlusionAllows) {
        continue;
      }
      model->UpdateInView(currentRenderView, nextRenderView, &updateTools);
      auto* referenceModel =
          const_cast<idRenderModel*>(static_cast<const idRenderModel*>(
              model->g.referenceModel));
      if (referenceModel != nullptr && (model->g.flags1 & 0x20U) == 0U)
        referenceModel->UpdateInView(currentRenderView, nextRenderView,
                                     &updateTools);
    }
    PrintUpdateInViewStatistics(&updateTools);
    UpdateEvent(this, renderWorldUpdateEvent_t::AddTransparencySortJob,
                &updateTools);
  }

  UpdateEvent(this, renderWorldUpdateEvent_t::SubmitJobs);
  UpdateEvent(this, renderWorldUpdateEvent_t::EndCuttableBeams);
  UpdateEvent(this, renderWorldUpdateEvent_t::EndEffects);
  UpdateEvent(this, renderWorldUpdateEvent_t::EndFlare);
  UpdateEvent(this, renderWorldUpdateEvent_t::OceanSwap);
}

void idRenderWorldLocal::UpdateDeferredPositions() {
  for (int index = 0; index < renderModels.Num(); ++index) {
    if (!commitModels.ptr[index])
      continue;
    idRenderModel* model = renderModels[index];
    if (model == nullptr)
      continue;
    if ((model->useDeferredPosition & 1U) != 0U) {
      model->g.origin = model->deferredOrigin;
      model->g.axis = model->deferredAxis;
    }
    model->deferredPositionInitialized |= 2U;
  }
}

void idRenderWorldLocal::ClearOcclusionQueryForModel(idRenderModel* model) {
  for (int index = 0; index < renderViews.Num(); ++index) {
    if (runtimeCallbacks.clearModelOcclusion != nullptr)
      runtimeCallbacks.clearModelOcclusion(this, model, index);
  }
}

idRenderModel* idRenderWorldLocal::AllocBaseRenderModel() {
  idRenderModel* model = new idRenderModel;
  AddRenderModel(model);
  return model;
}

idRenderModel* idRenderWorldLocal::AllocStaticModel(const char* modelName,
                                                    bool makeDefault) {
  if (modelName == nullptr || *modelName == '\0')
    return nullptr;
  idStr canonical(modelName);
  canonical.MakeNameCanonical();
  if (runtimeCallbacks.allocStaticModel == nullptr)
    return nullptr;
  idRenderModel* model =
      runtimeCallbacks.allocStaticModel(canonical.data, makeDefault);
  if (model != nullptr)
    AddRenderModel(model);
  return model;
}

void idRenderWorldLocal::RemoveEnvironment(
    const dynamicEnvironmentHandle_t& handle) {
  const int index = FindEnvironment(handle);
  if (index >= 0)
    committedWorld->dynamicEnvironments.RemoveIndex(index);
}

dynamicEnvironmentHandle_t idRenderWorldLocal::AddEnvironment(
    const idBounds& bounds, const idDeclEnv* envEffectsDecl,
    const idPlane* planes, int numPlanes, float volume) {
  dynamicEnvironmentHandle_t result;
  if (envEffectsDecl == nullptr || numPlanes > 16)
    return result;
  idStaticList<dynamicEnvironment_t, 128>& environments =
      committedWorld->dynamicEnvironments;
  if (environments.Num() == 128) {
    idLib::Warning("MAX_DYNAMIC_ENVIRONMENT exceeded");
    return result;
  }

  dynamicEnvironment_t environment{};
  environment.env = envEffectsDecl;
  environment.bounds = bounds;
  environment.volume = volume == 0.0f ? BoundsVolume(bounds) : volume;
  environment.handle.value = currentDynamicEnvHandle++;
  if (planes != nullptr && numPlanes > 0) {
    std::memcpy(environment.planes.ptr, planes,
                static_cast<std::size_t>(numPlanes) * sizeof(idPlane));
    environment.numPlanes = numPlanes;
  } else {
    idPolytope::ConvexPolytopeFromBounds(bounds, environment.planes.ptr);
    environment.numPlanes = 6;
  }

  int insertionIndex = 0;
  while (insertionIndex < environments.Num() &&
         environments[insertionIndex].volume <= environment.volume) {
    ++insertionIndex;
  }
  environments.Insert(environment, insertionIndex);
  result.value = environment.handle.value;
  return result;
}

void idRenderWorldLocal::SetEnvironmentBounds(
    const dynamicEnvironmentHandle_t& handle, const idBounds& bounds) {
  const int index = FindEnvironment(handle);
  if (index < 0)
    return;
  dynamicEnvironment_t environment =
      committedWorld->dynamicEnvironments[index];
  environment.bounds = bounds;
  idPolytope::ConvexPolytopeFromBounds(bounds, environment.planes.ptr);
  environment.numPlanes = 6;
  const float newBoundsVolume = BoundsVolume(bounds);
  committedWorld->dynamicEnvironments.RemoveIndex(index);

  int insertionIndex = 0;
  while (insertionIndex < committedWorld->dynamicEnvironments.Num() &&
         BoundsVolume(committedWorld->dynamicEnvironments[insertionIndex]
                          .bounds) <= newBoundsVolume) {
    ++insertionIndex;
  }
  committedWorld->dynamicEnvironments.Insert(environment, insertionIndex);
}

void idRenderWorldLocal::SetNumRenderViews(int num) {
  renderViews.SetNum(num);
}

idRenderLight* idRenderWorldLocal::AllocRenderLight(bool skipAutoReplication) {
  idRenderLight* light = new idRenderLight;
  renderLights.SetNum(8192, nullptr);
  light->skipAutoReplication = skipAutoReplication;
  const int first = skipAutoReplication ? 7692 : 0;
  const int last = skipAutoReplication ? 8192 : 7692;
  int slot = first;
  while (slot < last && renderLights[slot] != nullptr)
    ++slot;
  if (slot == last) {
    int active = 0;
    for (int index = 0; index < renderLights.Num(); ++index) {
      idLib::Printf(renderLights[index] == nullptr ? "%4i: FREED\n"
                                                   : "%4i: ACTIVE\n",
                    index);
      active += renderLights[index] != nullptr ? 1 : 0;
    }
    delete light;
    idLib::Error(
        "Max lights allocated when trying to find a news spot. Active: %d",
        active);
  }
  renderLights[slot] = light;
  light->world = this;
  light->committed->world = committedWorld;
  light->committed->index.index =
      static_cast<unsigned>((slot << 8) | (1 << (slot & 7)));
  return light;
}

bool idRenderWorldLocal::AddRenderModel(idRenderModel* model) {
  if (model == nullptr)
    idLib::Error("%s failed - attempted to add an invalid (NULL) model",
                 "idRenderWorldLocal::AddRenderModel");
  int index = 0;
  while (index < renderModels.Num() && renderModels[index] != nullptr)
    ++index;
  if (index == renderModels.Num()) {
    if (renderModels.Num() == renderModels.size) {
      index = renderModels.size - 1;
      idLib::Warning("RegisterRenderModel: index %d already in use", index);
    } else {
      index = renderModels.Append(nullptr);
    }
  }
  if (renderModels[index] != nullptr)
    idLib::Warning("RegisterRenderModel: index %d already in use", index);
  renderModels[index] = model;
  model->world = this;
  model->committed->name = model->name.str;
  model->committed->world = committedWorld;
  model->committed->index.index =
      static_cast<unsigned>((index << 8) | (1 << (index & 7)));
  return true;
}

void idRenderWorldLocal::MarkLightSerialized(idRenderLight* light,
                                             bool serialized,
                                             bool deleted) {
  if (light->skipAutoReplication) {
    serialized = false;
    deleted = true;
  }
  serializedLights.SetNum(renderLights.Num(), nullptr);
  const int index = light->GetIndex();
  if (serialized) {
    serializedLights[index] = light;
    light->needWriteToSnapshot = true;
  } else {
    if (serializedLights[index] != nullptr)
      serializedLights[index] = nullptr;
    if (deleted && runtimeCallbacks.markSnapshotObjectDeleted != nullptr)
      runtimeCallbacks.markSnapshotObjectDeleted(index + 10051);
  }
}

void idRenderWorldLocal::WriteToSnapshot(idSnapShot& snapshot,
                                         const idSnapShot* oldSnapshot) const {
  auto* mutableThis = const_cast<idRenderWorldLocal*>(this);
  std::uint8_t buffer[8000]{};
  idBitMsg message;
  message.InitWrite(buffer, static_cast<int>(sizeof(buffer)));
  message.WriteBits(0, 8);
  snapshot.S_AddObject(10050, 0xFFFFFFFFU, message);

  for (int index = 0; index < serializedLights.Num(); ++index) {
    idRenderLight* light = serializedLights[index];
    if (light == nullptr)
      continue;
    const int objectNumber = light->GetIndex() + 10051;
    if (light->skipAutoReplication) {
      mutableThis->MarkLightSerialized(light, false, false);
      continue;
    }

    bool writeObject = oldSnapshot == nullptr || light->needWriteToSnapshot;
    if (!writeObject) {
      if (runtimeCallbacks.allPeersHaveStaleSnapshotObject != nullptr &&
          runtimeCallbacks.allPeersHaveStaleSnapshotObject(objectNumber)) {
        mutableThis->MarkLightSerialized(light, false, false);
        continue;
      }
      writeObject = !snapshot.CopyObject(*oldSnapshot, objectNumber, true);
    }
    if (!writeObject)
      continue;

    light->needWriteToSnapshot = false;
    message.InitWrite(buffer, static_cast<int>(sizeof(buffer)));
    idSerializer serializer{true, {}, &message};
    light->SerializeSnapshot(&serializer);
    snapshot.S_AddObject(objectNumber, 0xFFFFFFFFU, message);
  }

  message.InitWrite(buffer, static_cast<int>(sizeof(buffer)));
  message.WriteBits(committedWorld->dynamicEnvironments.Num(), 8);
  for (int index = 0;
       index < committedWorld->dynamicEnvironments.Num(); ++index) {
    const dynamicEnvironment_t& environment =
        committedWorld->dynamicEnvironments[index];
    if (runtimeCallbacks.writeEnvironmentResource != nullptr)
      runtimeCallbacks.writeEnvironmentResource(message, environment.env);
    for (int bound = 0; bound < 2; ++bound) {
      WriteFloatBits(message, environment.bounds.b[bound].x);
      WriteFloatBits(message, environment.bounds.b[bound].y);
      WriteFloatBits(message, environment.bounds.b[bound].z);
    }
  }
  snapshot.S_AddObject(15050, 0xFFFFFFFFU, message);
}

void idRenderWorldLocal::ReadFromSnapshot(idSnapShot& snapshot,
                                          const idSnapShot* oldSnapshot) {
  (void)oldSnapshot;
  int objectIndex = snapshot.FindObjectIndexByID(10050);
  const int environmentsIndex = snapshot.FindObjectIndexByID(15050);
  if (objectIndex == -1 || environmentsIndex == -1)
    return;

  while (objectIndex < environmentsIndex) {
    idBitMsg message;
    const int objectNumber =
        snapshot.GetObjectMsgByIndex(objectIndex, message, false);
    if (objectNumber != 10050) {
      const int lightIndex = objectNumber - 10051;
      renderLights.SetNum(std::max(renderLights.Num(), lightIndex + 1),
                          nullptr);
      idRenderLight* light = renderLights[lightIndex];
      if (light != nullptr && light->skipAutoReplication)
        idLib::Error("Light skipping replication in invalid slot");

      if (message.GetSize() != 0) {
        if (light == nullptr) {
          light = new idRenderLight;
          renderLights[lightIndex] = light;
          light->world = this;
          light->committed->world = committedWorld;
          light->committed->index.index = static_cast<unsigned>(
              (lightIndex << 8) | (1 << (lightIndex & 7)));
        }
        if (!light->deleteOnSync) {
          const int changeCount =
              snapshot.ObjectChangedCountByIndex(objectIndex);
          if (changeCount != light->serializedChangeCount) {
            light->serializedChangeCount = changeCount;
            idSerializer serializer{false, {}, &message};
            light->SerializeSnapshot(&serializer);
          }
        }
      } else if (light != nullptr) {
        light->Delete();
      }
    }
    ++objectIndex;
  }

  idBitMsg environmentMessage;
  snapshot.GetObjectMsgByIndex(environmentsIndex, environmentMessage, false);
  const int numEnvironments = environmentMessage.ReadBits(8);
  committedWorld->dynamicEnvironments.SetNum(numEnvironments);
  for (int index = 0; index < numEnvironments; ++index) {
    dynamicEnvironment_t& environment =
        committedWorld->dynamicEnvironments[index];
    environment.env = runtimeCallbacks.readEnvironmentResource != nullptr
                          ? runtimeCallbacks.readEnvironmentResource(
                                environmentMessage)
                          : nullptr;
    for (int bound = 0; bound < 2; ++bound) {
      environment.bounds.b[bound].x = ReadFloatBits(environmentMessage);
      environment.bounds.b[bound].y = ReadFloatBits(environmentMessage);
      environment.bounds.b[bound].z = ReadFloatBits(environmentMessage);
    }
  }
}

idRenderWorldCommitted* idRenderWorldLocal::GetCommittedWorld() const {
  return committedWorld;
}

void idRenderWorldLocal::Commit(bool isHeadless) {
  const bool renderCommit = !isHeadless;

  for (int index = 0; index < renderModels.Num(); ++index) {
    if (!deletedModels.ptr[index])
      continue;
    delete renderModels[index];
    renderModels[index] = nullptr;
    deletedModels.ptr[index] = false;
    commitModels.ptr[index] = false;
  }

  committedWorld->committedRenderModels.SetNum(renderModels.Num(), nullptr);
  for (int index = 0; index < renderModels.Num(); ++index) {
    if (!commitModels.ptr[index])
      continue;
    idRenderModel* model = renderModels[index];
    idRenderModelCommitted* committed = model->committed;
    commitModels.ptr[index] = false;
    committedWorld->postCommitModels.ptr[index] = renderCommit;
    committedWorld->committedRenderModels[index] = committed;

    const bool unlinked = (model->unlinked & 8U) != 0U;
    const bool deleteOnSync = (model->deleteOnSync & 4U) != 0U;
    if (unlinked || deleteOnSync) {
      ClearOcclusionQueryForModel(model);
      if (runtimeCallbacks.unlinkModelFromTree != nullptr)
        runtimeCallbacks.unlinkModelFromTree(committedWorld, committed);
      UnlinkCommittedModel(committedWorld->addAlwaysModels, committed,
                           &idRenderModelCommitted::nextAddAlwaysModel);
      committedWorld->UnlinkDimShadow(committed);
      UnlinkCommittedModel(committedWorld->approxLightModels, committed,
                           &idRenderModelCommitted::nextApproxLightModel);
      committedWorld->postCommitModels.ptr[index] = false;
    }

    if (deleteOnSync) {
      committedWorld->committedRenderModels[index] = nullptr;
      if (renderCommit) {
        deletedModels.ptr[index] = true;
        committedWorld->postCommitModels.ptr[index] = false;
      } else {
        delete renderModels[index];
        renderModels[index] = nullptr;
      }
    } else if (unlinked) {
      committed->r.flags1 ^=
          (committed->r.flags1 ^ model->g.flags1) & 0x20U;
      committedWorld->postCommitModels.ptr[index] = false;
    } else if (renderCommit) {
      model->Commit();
      auto* referenceModel =
          const_cast<idRenderModel*>(static_cast<const idRenderModel*>(
              model->g.referenceModel));
      if (referenceModel != nullptr && (model->g.flags1 & 0x20U) == 0U)
        referenceModel->Commit();
    }
  }

  committedWorld->committedRenderLights.SetNum(renderLights.Num(), nullptr);
  for (int index = 0; index < renderLights.Num(); ++index) {
    if (!commitLights.ptr[index])
      continue;
    idRenderLight* light = renderLights[index];
    idRenderLightCommitted* committed = light->committed;
    commitLights.ptr[index] = false;
    committedWorld->postCommitLights.ptr[index] = renderCommit;
    committedWorld->committedRenderLights[index] = committed;
    light->needWriteToSnapshot = true;
    if (light->unlinked || light->deleteOnSync) {
      if (runtimeCallbacks.unlinkLightFromTree != nullptr)
        runtimeCallbacks.unlinkLightFromTree(committedWorld, committed);
      committedWorld->postCommitLights.ptr[index] = false;
      if (light->deleteOnSync) {
        committedWorld->committedRenderLights[index] = nullptr;
        renderLights[index] = nullptr;
        delete light;
      }
    } else if (renderCommit) {
      light->Commit();
    }
  }

  if (renderCommit && committedWorld->committedDebugData != nullptr &&
      debugData != nullptr) {
    idRenderDebugLocal* committedDebug = committedWorld->committedDebugData;
    if (committedDebug->debugLines.Num() != 0 ||
        debugData->debugLines.Num() != 0)
      committedDebug->debugLines = debugData->debugLines;
    if (committedDebug->debugText.Num() != 0 || debugData->debugText.Num() != 0)
      committedDebug->debugText = debugData->debugText;
    if (committedDebug->debugPolygons.Num() != 0 ||
        debugData->debugPolygons.Num() != 0)
      committedDebug->debugPolygons = debugData->debugPolygons;
    if (committedDebug->debugBounds.Num() != 0 ||
        debugData->debugBounds.Num() != 0)
      committedDebug->debugBounds = debugData->debugBounds;
  }
}

idRenderWorldLocal::idRenderWorldLocal() {
  mapName.Clear();
  everInitialized = false;
  specificationCompleteHasBeenCalled = false;
  currentDynamicEnvHandle = 0;
  renderModels.Clear();
  renderLights.Clear();
  serializedLights.Clear();
  renderViews.SetNum(2);
  for (int index = 0; index < 2; ++index) {
    renderViews[index] = runtimeCallbacks.allocRenderView != nullptr
                             ? runtimeCallbacks.allocRenderView(index)
                             : nullptr;
    if (renderViews[index] != nullptr)
      renderViews[index]->viewIndex = index;
  }
  std::memset(deletedModels.ptr, 0, sizeof(deletedModels.ptr));
  std::memset(commitModels.ptr, 0, sizeof(commitModels.ptr));
  std::memset(commitLights.ptr, 0, sizeof(commitLights.ptr));
  m_pSharedTileMemory = nullptr;
  renderModelJobList = runtimeCallbacks.allocModelJobList != nullptr
                           ? runtimeCallbacks.allocModelJobList(this)
                           : nullptr;
  debugData = new idRenderDebugLocal;
  committedWorld = runtimeCallbacks.allocCommittedWorld != nullptr
                       ? runtimeCallbacks.allocCommittedWorld()
                       : nullptr;
  virtualTextures.ClearFree();
}

idRenderWorldLocal::~idRenderWorldLocal() {
  if (runtimeCallbacks.freeRenderWorld != nullptr)
    runtimeCallbacks.freeRenderWorld(this);

  for (int index = 0; index < renderLights.Num(); ++index) {
    idRenderLight* light = renderLights[index];
    renderLights[index] = nullptr;
    delete light;
  }
  for (int index = 0; index < renderModels.Num(); ++index) {
    if (renderModels[index] != nullptr)
      renderModels[index]->RemoveDecals();
  }
  for (int index = 0; index < renderModels.Num(); ++index) {
    idRenderModel* model = renderModels[index];
    renderModels[index] = nullptr;
    delete model;
  }
  renderViews.SetNum(2);
  for (int index = 0; index < 2; ++index) {
    if (renderViews[index] != nullptr &&
        runtimeCallbacks.freeRenderView != nullptr)
      runtimeCallbacks.freeRenderView(renderViews[index]);
    renderViews[index] = nullptr;
  }
  stampModel = nullptr;
  foliageEditRenderModel = nullptr;
  foliageRenderModel = nullptr;
  detailEditRenderModel = nullptr;
  detailRenderModel = nullptr;
  transparencyRenderModel = nullptr;
  decalRenderModel = nullptr;
  if (runtimeCallbacks.oceanDestroy != nullptr)
    runtimeCallbacks.oceanDestroy(this);
  if (renderModelJobList != nullptr &&
      runtimeCallbacks.freeModelJobList != nullptr)
    runtimeCallbacks.freeModelJobList(renderModelJobList);
  renderModelJobList = nullptr;
  delete debugData;
  debugData = nullptr;
  if (runtimeCallbacks.freeCommittedWorld != nullptr)
    runtimeCallbacks.freeCommittedWorld(committedWorld);
  committedWorld = nullptr;
  mapName = "<FREED>";
}

idRenderWorld* idRenderWorld::Alloc() { return new idRenderWorldLocal; }

// The recovered compiland's idList template and cleanup records are generated
// by the concrete list operations above; they are not handwritten functions.
