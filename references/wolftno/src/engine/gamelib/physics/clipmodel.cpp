#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/physics/clip.h"

#include "engine/framework/common.h"
#include "engine/gamelib/aas2/aas2_interface.h"
#include "engine/gamelib/physics/tracemodelcache.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/jointgroup.h"
#include "engine/models/skeletalanimation/treeanimator.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/lib.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

class idCollisionModelManager {
public:
  idCollisionModel* LoadModel(const char* name);
};

extern idCollisionModelManager* collisionModelManager;

#if defined(WOLFTNO_CLIPMODEL_DIFFERENTIAL_HARNESS)
const idDeclRenderParm* ClipModelHarnessContentFlags();
#endif

namespace {

struct retailGame_t {
  const void* const* vtable;
};

template <typename Function, typename Type>
Function VirtualFunction(Type* object, std::size_t byteOffset) {
  return reinterpret_cast<Function>(
      (*reinterpret_cast<const void* const* const*>(object))[byteOffset / 8]);
}

retailGame_t* CommonGame() {
  using Function = retailGame_t* (*)(idCommon*);
  return VirtualFunction<Function>(common, 0x1F8)(common);
}

void* GameGetEntity(retailGame_t* game, int entityNumber) {
  using Function = void* (*)(retailGame_t*, int);
  return VirtualFunction<Function>(game, 0x450)(game, entityNumber);
}

const char* GameGetEntityName(retailGame_t* game, void* entity) {
  using Function = const char* (*)(retailGame_t*, void*);
  return VirtualFunction<Function>(game, 0x458)(game, entity);
}

__forceinline int ClipEntityNumber(const idClipModel* model) {
  return static_cast<std::int32_t>(model->flags << 15u) >> 17;
}

float MinimumRetail(float left, float right) {
  return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

float MaximumRetail(float left, float right) {
  return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

__forceinline void CollisionModelGetBounds(idCollisionModel* model,
                                           idBounds& modelBounds) {
  using Function = bool (*)(idCollisionModel*, idBounds&);
  VirtualFunction<Function>(model, 0x40)(model, modelBounds);
}

__forceinline int CollisionModelGetContents(idCollisionModel* model) {
  int modelContents = 0;
  using Function = bool (*)(idCollisionModel*, int&);
  VirtualFunction<Function>(model, 0x50)(model, modelContents);
  return modelContents;
}

__forceinline const char* CollisionModelGetName(idCollisionModel* model) {
  return *reinterpret_cast<const char* const*>(
      reinterpret_cast<const std::uint8_t*>(model) + 8u);
}

} // namespace

void idClipModel::Delete() {
  flags |= 2u;
  clip->DeleteClipModel(this);
}

idCollisionModel* idClipModel::NoTraceModelWarning() const {
  retailGame_t* const game = CommonGame();
  if (game != nullptr) {
    void* const entity = GameGetEntity(game, ClipEntityNumber(this));
    if (entity != nullptr) {
      const char* const name = GameGetEntityName(game, entity);
      if (name != nullptr) {
        idLib::Warning(
            "idClipModel::GetCollisionModel: clip model %d on entity '%s' "
            "is not a collision or trace model",
            bodyId, name);
        return nullptr;
      }
    }
  }

  idLib::Warning(
      "idClipModel::GetCollisionModel: clip model %d on entity %d is not a "
      "collision or trace model",
      bodyId, ClipEntityNumber(this));
  return nullptr;
}

void idClipModel::GetMassProperties(float density, float& mass,
                                    idVec3& centerOfMass,
                                    idMat3& inertiaTensor) const {
  const int count = traceModels.num;
  if (count == 0) {
    idLib::Error(
        "idClipModel::GetMassProperties: clip model %d on entity %d is not "
        "a trace model\n",
        bodyId, ClipEntityNumber(this));
  }

  if (count == 1) {
    clip->traceModelCache->GetMassProperties(
        traceModels.list[0], density, mass, centerOfMass, inertiaTensor);
    return;
  }

  mass = 0.0f;
  centerOfMass.x = 0.0f;
  centerOfMass.y = 0.0f;
  centerOfMass.z = 0.0f;
  for (idVec3& row : inertiaTensor.mat) {
    row.x = 0.0f;
    row.y = 0.0f;
    row.z = 0.0f;
  }

  for (int index = 0; index < count; ++index) {
    float traceMass;
    idVec3 traceCenter;
    idMat3 traceInertia;
    clip->traceModelCache->GetMassProperties(
        traceModels.list[index], density, traceMass, traceCenter,
        traceInertia);
    mass = traceMass + mass;
    centerOfMass.x = traceCenter.x * traceMass + centerOfMass.x;
    centerOfMass.y = traceCenter.y * traceMass + centerOfMass.y;
    centerOfMass.z = traceCenter.z * traceMass + centerOfMass.z;
  }

  const float inverseMass = 1.0f / mass;
  centerOfMass.x = inverseMass * centerOfMass.x;
  centerOfMass.y = inverseMass * centerOfMass.y;
  centerOfMass.z = inverseMass * centerOfMass.z;

  for (int index = 0; index < count; ++index) {
    float traceMass;
    idVec3 traceCenter;
    idMat3 traceInertia;
    clip->traceModelCache->GetMassProperties(
        traceModels.list[index], density, traceMass, traceCenter,
        traceInertia);
    const idVec3 translation{
        centerOfMass.x - traceCenter.x,
        centerOfMass.y - traceCenter.y,
        centerOfMass.z - traceCenter.z,
    };
    traceInertia.InertiaTranslateSelf(traceMass, traceCenter, translation);
    inertiaTensor.mat[0].x =
        traceInertia.mat[0].x + inertiaTensor.mat[0].x;
    inertiaTensor.mat[0].y =
        traceInertia.mat[0].y + inertiaTensor.mat[0].y;
    inertiaTensor.mat[0].z =
        traceInertia.mat[0].z + inertiaTensor.mat[0].z;
    inertiaTensor.mat[1].x =
        traceInertia.mat[1].x + inertiaTensor.mat[1].x;
    inertiaTensor.mat[1].y =
        traceInertia.mat[1].y + inertiaTensor.mat[1].y;
    inertiaTensor.mat[1].z =
        traceInertia.mat[1].z + inertiaTensor.mat[1].z;
    inertiaTensor.mat[2].x =
        traceInertia.mat[2].x + inertiaTensor.mat[2].x;
    inertiaTensor.mat[2].y =
        traceInertia.mat[2].y + inertiaTensor.mat[2].y;
    inertiaTensor.mat[2].z =
        traceInertia.mat[2].z + inertiaTensor.mat[2].z;
  }
}

void idClipModel::LinkIntoAASes(aasType_t aasType) {
  if (aasType == AAS_FILE_EXTENSION_MAX) {
    for (int index = 0; index < AAS_FILE_EXTENSION_MAX; ++index) {
      idAAS2* const aas = clip->aases[index];
      if (aas != nullptr) {
        aasLinks[index] = aas->LinkClipModel(this);
      }
    }
    return;
  }

  idAAS2* const aas = clip->aases[aasType];
  if (aas != nullptr) {
    aasLinks[aasType] = aas->LinkClipModel(this);
  }
}

void idClipModel::SetContents(int newContents) {
  constexpr int obstacleContents = 0x20000;
  if ((contents & obstacleContents) == (newContents & obstacleContents)) {
    contents = newContents;
    return;
  }

  if ((newContents & obstacleContents) != 0 && clipLinks != nullptr) {
    LinkIntoAASes(AAS_FILE_EXTENSION_MAX);
  } else {
    for (int index = 0; index < AAS_FILE_EXTENSION_MAX; ++index) {
      idAAS2* const aas = clip->aases[index];
      if (aas != nullptr) {
        aasLinks[index] = aas->UnLinkClipModel(aasLinks[index]);
      }
    }
  }
  contents = newContents;
}

void idClipModel::UpdateAbsBounds() {
  if (axis.mat[0].x == 1.0f && axis.mat[1].y == 1.0f &&
      axis.mat[2].z == 1.0f) {
    absBounds.b[0].x = bounds.b[0].x + origin.x;
    absBounds.b[0].y = bounds.b[0].y + origin.y;
    absBounds.b[0].z = bounds.b[0].z + origin.z;
    absBounds.b[1].x = bounds.b[1].x + origin.x;
    absBounds.b[1].y = bounds.b[1].y + origin.y;
    absBounds.b[1].z = bounds.b[1].z + origin.z;
  } else {
    absBounds.FromTransformedBounds(bounds, origin, axis);
  }

  absBounds.b[0].x -= 1.0f;
  absBounds.b[0].y -= 1.0f;
  absBounds.b[0].z -= 1.0f;
  absBounds.b[1].x += 1.0f;
  absBounds.b[1].y += 1.0f;
  absBounds.b[1].z += 1.0f;
}

void idClipModel::FreeModel() {
  for (int index = 0; index < traceModels.num; ++index) {
    clip->traceModelCache->FreeTraceModel(traceModels.list[index]);
  }
  traceModels.ClearFree();
}

bool idClipModel::LoadCollisionModel(
    const char* name, const idMaterial* overrideClipMaterial) {
  FreeModel();
  if (name != nullptr && name[0] != '\0') {
    collisionModel = collisionModelManager->LoadModel(name);
  }

  if (collisionModel == nullptr) {
    bounds.b[0].x = 0.0f;
    bounds.b[0].y = 0.0f;
    bounds.b[0].z = 0.0f;
    bounds.b[1].x = 0.0f;
    bounds.b[1].y = 0.0f;
    bounds.b[1].z = 0.0f;
    return false;
  }

  CollisionModelGetBounds(collisionModel, bounds);
  contents = CollisionModelGetContents(collisionModel);
  if (overrideClipMaterial != nullptr) {
#if defined(WOLFTNO_CLIPMODEL_DIFFERENTIAL_HARNESS)
    contents = overrideClipMaterial->GetInteger(
        ClipModelHarnessContentFlags());
#else
    contents = overrideClipMaterial->GetInteger(
        reinterpret_cast<const idDeclRenderParm*>(rp_contentFlags.r));
#endif
  }
  return true;
}

void idClipModel::LoadCollisionModel(idCollisionModel* model) {
  FreeModel();
  collisionModel = model;
  CollisionModelGetBounds(model, bounds);
  contents = CollisionModelGetContents(collisionModel);
}

void idClipModel::Init(idClip* clip_) {
  flags = 0x3FFE7FFDu;
  physicsId = 0;
  bodyId = 0;
  bodyMask = 0;
  touchCount = -1;
  contents = static_cast<int>(0x80000000u);
  origin.x = 0.0f;
  origin.y = 0.0f;
  origin.z = 0.0f;
  axis.mat[0] = {1.0f, 0.0f, 0.0f};
  axis.mat[1] = {0.0f, 1.0f, 0.0f};
  axis.mat[2] = {0.0f, 0.0f, 1.0f};
  std::memset(&absBounds, 0, sizeof(absBounds));
  std::memset(&bounds, 0, sizeof(bounds));
  collisionModel = nullptr;
  traceModels.ClearFree();
  children.ClearFree();
  sphereFeatures = nullptr;
  clip = clip_;
  clipLinks = nullptr;
  nextDeleted = nullptr;
  std::memset(aasLinks, 0, sizeof(aasLinks));
}

void idClipModel::Serialize(idSerializer& serializer) {
  if (serializer.writing) {
    serializer.msg->WriteBits(contents, 32);
  } else {
    SetContents(serializer.msg->ReadBits(32));
  }
}

void idClipModel::RelinkIntoAASes(aasType_t aasType) {
  if (aasType == AAS_FILE_EXTENSION_MAX) {
    std::memset(aasLinks, 0, sizeof(aasLinks));
  } else {
    aasLinks[aasType] = nullptr;
  }
  LinkIntoAASes(aasType);
}

idSphereFeatures::idSphereFeatures(idTreeAnimator* treeAnimator_)
    : treeAnimator(nullptr), activeFeatures() {
  activeFeatures.memTag = 9;
  int numFeatures = 0;
  idJointGroupCollection* const collection =
      treeAnimator_->GetJointGroupCollection();
  const int numGroups = collection->NumJointGroupsForType(
      idJointGroup::JOINTGROUP_HITTEST);
  for (int index = 0; index < numGroups; ++index) {
    const idJointGroup* const group = collection->GetJointGroupForIndex(
        idJointGroup::JOINTGROUP_HITTEST, index);
    if (group != nullptr) {
      numFeatures += group->joints.num;
    }
  }
  treeAnimator = treeAnimator_;
  activeFeatures.Alloc(static_cast<std::uint64_t>((numFeatures + 3) & ~3));
  std::memset(activeFeatures.buffer, 0xFF,
              (activeFeatures.bits + 7u) >> 3u);
}

idSphereFeatures::idSphereFeatures(
    const idSphereFeatures* sphereFeatures_)
    : treeAnimator(nullptr), activeFeatures() {
  activeFeatures.memTag = 9;
  treeAnimator = sphereFeatures_->treeAnimator;
  activeFeatures.Alloc(sphereFeatures_->activeFeatures.bits);
  std::memmove(activeFeatures.buffer,
               sphereFeatures_->activeFeatures.buffer,
               (activeFeatures.bits + 7u) >> 3u);
}

idSphereFeatures::~idSphereFeatures() {
  treeAnimator = nullptr;
  if (activeFeatures.buffer != nullptr) {
    std::memset(activeFeatures.buffer, 0,
                (activeFeatures.bits + 7u) >> 3u);
  }
}

void idClipModel::LoadTraceModel(const idTraceModel* traceModels_,
                                 int numTraceModels,
                                 const idMaterial* material) {
  FreeModel();
  contents = static_cast<int>(0x80000000u);
  bounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < numTraceModels; ++index) {
    const int traceModelIndex = clip->traceModelCache->AllocTraceModel(
        traceModels_[index], material);
    traceModels.Append(traceModelIndex);
    const idBounds& traceBounds = traceModels_[index].bounds;
    bounds.b[0].x = MinimumRetail(bounds.b[0].x, traceBounds.b[0].x);
    bounds.b[0].y = MinimumRetail(bounds.b[0].y, traceBounds.b[0].y);
    bounds.b[0].z = MinimumRetail(bounds.b[0].z, traceBounds.b[0].z);
    bounds.b[1].x = MaximumRetail(bounds.b[1].x, traceBounds.b[1].x);
    bounds.b[1].y = MaximumRetail(bounds.b[1].y, traceBounds.b[1].y);
    bounds.b[1].z = MaximumRetail(bounds.b[1].z, traceBounds.b[1].z);

    idCollisionModel* const model =
        clip->traceModelCache->cache.list[traceModels.list[index]]
            ->collisionModel;
    if (model != nullptr) {
      contents = CollisionModelGetContents(model) |
                 (contents & 0x7FFFFFFF);
    }
  }
}

void idClipModel::LoadTraceModels(int numTraceModels,
                                  const int* traceModelIndices) {
  FreeModel();
  contents = static_cast<int>(0x80000000u);
  bounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < numTraceModels; ++index) {
    idTraceModel* const traceModel =
        clip->traceModelCache->cache.list[traceModelIndices[index]]->trm;
    if (traceModel != nullptr) {
      traceModels.Append(traceModelIndices[index]);
      const idBounds& traceBounds = traceModel->bounds;
      bounds.b[0].x = MinimumRetail(bounds.b[0].x, traceBounds.b[0].x);
      bounds.b[0].y = MinimumRetail(bounds.b[0].y, traceBounds.b[0].y);
      bounds.b[0].z = MinimumRetail(bounds.b[0].z, traceBounds.b[0].z);
      bounds.b[1].x = MaximumRetail(bounds.b[1].x, traceBounds.b[1].x);
      bounds.b[1].y = MaximumRetail(bounds.b[1].y, traceBounds.b[1].y);
      bounds.b[1].z = MaximumRetail(bounds.b[1].z, traceBounds.b[1].z);

      idCollisionModel* const model =
          clip->traceModelCache->cache.list[traceModels.list[index]]
              ->collisionModel;
      if (model != nullptr) {
        contents = CollisionModelGetContents(model) |
                   (contents & 0x7FFFFFFF);
      }
    }
  }
}

void idClipModel::LoadAnimator(idTreeAnimator* animatedModel) {
  FreeModel();
  collisionModel = collisionModelManager->LoadModel(animatedModel->decl->name.str);
  if (collisionModel != nullptr) {
    CollisionModelGetBounds(collisionModel, bounds);
    contents = CollisionModelGetContents(collisionModel);
    void* const allocation = idMem::Singleton().AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
        sizeof(idSphereFeatures), TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
    sphereFeatures = allocation != nullptr
                         ? ::new (allocation) idSphereFeatures(animatedModel)
                         : nullptr;
  }
}

idClipModel::idClipModel() : traceModels(), children(), collisionQuery() {
  Init(nullptr);
}

idClipModel::idClipModel(idClip* clip_, const char* name,
                         const idMaterial* overrideClipMaterial)
    : traceModels(), children(), collisionQuery() {
  Init(clip_);
  LoadCollisionModel(name, overrideClipMaterial);
}

idClipModel::idClipModel(idClip* clip_, idCollisionModel* model)
    : traceModels(), children(), collisionQuery() {
  Init(clip_);
  LoadCollisionModel(model);
}

idClipModel::idClipModel(idClip* clip_, const idTraceModel* traceModels_,
                         int numTraceModels, const idMaterial* material)
    : traceModels(), children(), collisionQuery() {
  Init(clip_);
  LoadTraceModel(traceModels_, numTraceModels, material);
}

idClipModel::idClipModel(idClip* clip_, int numTraceModels,
                         const int* traceModelIndices)
    : traceModels(), children(), collisionQuery() {
  Init(clip_);
  LoadTraceModels(numTraceModels, traceModelIndices);
}

idClipModel::idClipModel(idClip* clip_, idTreeAnimator* animatedModel)
    : traceModels(), children(), collisionQuery() {
  Init(clip_);
  LoadAnimator(animatedModel);
}

idClipModel::idClipModel(idClip* clip_, const idClipModel* model)
    : traceModels(), children(), collisionQuery() {
  flags = model->flags;
  physicsId = model->physicsId;
  bodyId = model->bodyId;
  bodyMask = model->bodyMask;
  origin = model->origin;
  axis = model->axis;
  absBounds = model->absBounds;
  bounds = model->bounds;
  contents = model->contents;
  collisionModel = nullptr;
  clip = clip_;
  if (model->collisionModel != nullptr) {
    collisionModel =
        collisionModelManager->LoadModel(
            CollisionModelGetName(model->collisionModel));
  }

  traceModels.SetNum(model->traceModels.num);
  for (int index = 0; index < model->traceModels.num; ++index) {
    traceModels.list[index] = clip->traceModelCache->CopyTraceModel(
        model->traceModels.list[index]);
  }
  clipLinks = nullptr;
  touchCount = -1;
  if (model->sphereFeatures != nullptr) {
    void* const allocation = idMem::Singleton().AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
        sizeof(idSphereFeatures), TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
    sphereFeatures =
        allocation != nullptr
            ? ::new (allocation) idSphereFeatures(model->sphereFeatures)
            : nullptr;
  } else {
    sphereFeatures = nullptr;
  }
  std::memset(aasLinks, 0, sizeof(aasLinks));
  nextDeleted = nullptr;
}

void idClipModel::Unlink() {
  while (clipLinks != nullptr) {
    clipLink_t* const link = clipLinks;
    clipLinks = link->nextLink;
    if (link->prevInSector != nullptr) {
      link->prevInSector->nextInSector = link->nextInSector;
    } else {
      link->sector->clipLinks = link->nextInSector;
    }
    if (link->nextInSector != nullptr) {
      link->nextInSector->prevInSector = link->prevInSector;
    }
    clip->clipLinkAllocator.Free(link);
  }

  if ((contents & 0x20000) != 0) {
    for (int index = 0; index < AAS_FILE_EXTENSION_MAX; ++index) {
      idAAS2* const aas = clip->aases[index];
      if (aas != nullptr) {
        aasLinks[index] = aas->UnLinkClipModel(aasLinks[index]);
      }
    }
  }
}

void idClipModel::Link_r(clipSector_t* node) {
  while (node->axis != -1) {
    const int splitAxis = node->axis;
    const float splitDistance = node->dist;
    if (absBounds.b[0][splitAxis] <= splitDistance) {
      if (splitDistance <= absBounds.b[1][splitAxis]) {
        Link_r(node->children[0]);
      }
      node = node->children[1];
    } else {
      node = node->children[0];
    }
  }

  clipLink_t* const link = clip->clipLinkAllocator.Alloc();
  link->clipModel = this;
  link->sector = node;
  link->nextInSector = node->clipLinks;
  link->prevInSector = nullptr;
  if (node->clipLinks != nullptr) {
    node->clipLinks->prevInSector = link;
  }
  node->clipLinks = link;
  link->nextLink = clipLinks;
  clipLinks = link;
}

void idClipModel::Link() {
  if (clipLinks != nullptr) {
    Unlink();
  }
  if (bounds.b[0].x <= bounds.b[1].x) {
    UpdateAbsBounds();
    Link_r(clip->clipSectors.list);
    if ((contents & 0x20000) != 0) {
      LinkIntoAASes(AAS_FILE_EXTENSION_MAX);
    }
  }
}

void idClipModel::Link(int newEntityNumber, int newPhysicsId, int newBodyId,
                       const idVec3& newOrigin, const idMat3& newAxis) {
  flags &= 0xFFFE0003u;
  physicsId = newPhysicsId;
  bodyId = newBodyId;
  flags |= 4u * (static_cast<unsigned int>(newEntityNumber) & 0x7FFFu);
  origin = newOrigin;
  axis = newAxis;
  Link();
}

void idClipModel::Link(int newEntityNumber, int newPhysicsId, int newBodyId,
                       const idVec3& newOrigin, const idMat3& newAxis,
                       const idBounds& newBounds) {
  flags &= 0xFFFE0003u;
  physicsId = newPhysicsId;
  bodyId = newBodyId;
  flags |= 4u * (static_cast<unsigned int>(newEntityNumber) & 0x7FFFu);
  origin = newOrigin;
  axis = newAxis;
  bounds = newBounds;
  Link();
}

void idClipModel::EnableSphereFeature(int sphereFeature) {
  if (sphereFeatures != nullptr &&
      sphereFeature < static_cast<int>(sphereFeatures->activeFeatures.bits)) {
    sphereFeatures->activeFeatures
        .buffer[static_cast<std::uint64_t>(sphereFeature) >> 3u] |=
        static_cast<unsigned char>(1u << (sphereFeature & 7));
  }
}

void idClipModel::DisableSphereFeature(int sphereFeature) {
  if (sphereFeatures != nullptr &&
      sphereFeature < static_cast<int>(sphereFeatures->activeFeatures.bits)) {
    sphereFeatures->activeFeatures
        .buffer[static_cast<std::uint64_t>(sphereFeature) >> 3u] &=
        static_cast<unsigned char>(~(1u << (sphereFeature & 7)));
  }
}

idClipModel::~idClipModel() {
  FreeModel();
  collisionModel = nullptr;
  clip = nullptr;
  clipLinks = nullptr;
  nextDeleted = nullptr;
  if (sphereFeatures != nullptr) {
    idSphereFeatures* const allocation = sphereFeatures;
    sphereFeatures->~idSphereFeatures();
    idMem::Singleton().Free(allocation, ALIGN_16);
    sphereFeatures = nullptr;
  }
}

static_assert(sizeof(clipLink_t) == 0x28, "clip-link ABI drift");
static_assert(sizeof(clipSector_t) == 0x20, "clip-sector ABI drift");
static_assert(sizeof(idSphereFeatures) == 0x20,
              "sphere-feature ABI drift");
static_assert(offsetof(idSphereFeatures, activeFeatures) == 0x08,
              "sphere-feature bit-array offset drift");
