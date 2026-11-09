#define WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#include "engine/gamelib/physics/physics_staticmulti.h"

#include "engine/framework/common.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#include "shared/idlib/lib.h"

#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

namespace {

staticPState_t defaultState;

idMat3 Transpose(const idMat3& matrix) {
  idMat3 result;
  result[0] = {matrix[0].x, matrix[1].x, matrix[2].x};
  result[1] = {matrix[0].y, matrix[1].y, matrix[2].y};
  result[2] = {matrix[0].z, matrix[1].z, matrix[2].z};
  return result;
}

idVec3 WorldToLocal(const idVec3& worldOrigin,
                    const idVec3& masterOrigin,
                    const idMat3& masterAxis) {
  const float x = worldOrigin.x - masterOrigin.x;
  const float y = worldOrigin.y - masterOrigin.y;
  const float z = worldOrigin.z - masterOrigin.z;
  return {
      (masterAxis[0].y * y + masterAxis[0].x * x) +
          masterAxis[0].z * z,
      (masterAxis[1].y * y + masterAxis[1].x * x) +
          masterAxis[1].z * z,
      (masterAxis[2].y * y + masterAxis[2].x * x) +
          masterAxis[2].z * z,
  };
}

idVec3 LocalToWorld(const idVec3& localOrigin,
                    const idVec3& masterOrigin,
                    const idMat3& masterAxis) {
  return {
      ((localOrigin.y * masterAxis[1].x +
        localOrigin.x * masterAxis[0].x) +
       localOrigin.z * masterAxis[2].x) +
          masterOrigin.x,
      masterOrigin.y +
          ((localOrigin.y * masterAxis[1].y +
            localOrigin.x * masterAxis[0].y) +
           localOrigin.z * masterAxis[2].y),
      masterOrigin.z +
          ((localOrigin.y * masterAxis[1].z +
            localOrigin.x * masterAxis[0].z) +
           localOrigin.z * masterAxis[2].z),
  };
}

float MinimumRetail(float left, float right) {
  return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

float MaximumRetail(float left, float right) {
  return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

void AddBoundsRetail(idBounds& destination, const idBounds& source) {
  destination.b[0].x = MinimumRetail(destination.b[0].x, source.b[0].x);
  destination.b[0].y = MinimumRetail(destination.b[0].y, source.b[0].y);
  destination.b[0].z = MinimumRetail(destination.b[0].z, source.b[0].z);
  destination.b[1].x = MaximumRetail(destination.b[1].x, source.b[1].x);
  destination.b[1].y = MaximumRetail(destination.b[1].y, source.b[1].y);
  destination.b[1].z = MaximumRetail(destination.b[1].z, source.b[1].z);
}

} // namespace

template <>
__declspec(noinline) bool idList<staticPState_t, 77>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }

  idMem* const memory = &idMem::Singleton();
  staticPState_t* const replacement =
      static_cast<staticPState_t*>(memory->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : "
          "tag",
          sizeof(staticPState_t) * static_cast<std::size_t>(newSize),
          static_cast<memTag_t>(memTag), false, ALIGN_16, HEAP_DEFAULTHEAP));
  if (replacement == nullptr) {
    return false;
  }
  for (int index = 0; index < newSize; ++index) {
    ::new (static_cast<void*>(&replacement[index])) staticPState_t;
  }
  const int copyCount = std::min(num, newSize);
  for (int index = 0; index < copyCount; ++index) {
    replacement[index] = list[index];
  }
  if ((listStatic == 0 || listStatic == 2) && list != nullptr) {
    memory->Free(list, ALIGN_16);
  }
  list = replacement;
  size = newSize;
  num = copyCount;
  listStatic = 0;
  return true;
}

template <>
__declspec(noinline) bool idList<staticPState_t, 77>::SetNum(
    int newNum, const staticPState_t& initialValue) {
  if (newNum > size && !Resize(newNum)) {
    return false;
  }
  const int oldNum = num;
  const int boundedNum = std::min(newNum, size);
  for (int index = oldNum; index < boundedNum; ++index) {
    list[index] = initialValue;
  }
  num = boundedNum;
  return true;
}

idPhysics_StaticMulti::idPhysics_StaticMulti()
    : clipModels(), mass(), current() {
  type = PHYSICS_STATICMULTI;
  hasMaster = false;
  isOrientated = false;

  defaultState.worldOrigin = vec3_origin;
  defaultState.worldAxis = mat3_identity;
  defaultState.localOrigin = vec3_origin;
  defaultState.localAxis = mat3_identity;

  current.SetNum(1, defaultState);
  clipModels.SetNum(1, nullptr);
  mass.SetNum(1, 1.0f);
  parent = nullptr;
}

idPhysics_StaticMulti::~idPhysics_StaticMulti() {
  for (int index = 0; index < clipModels.num; ++index) {
    if (clipModels.list[index] != nullptr) {
      clipModels.list[index]->Delete();
    }
  }
  if (parent != nullptr) {
    parent->Delete();
  }
}

void idPhysics_StaticMulti::GetImpactInfo(int, const idVec3&,
                                          impactInfo_t& info) const {
  std::memset(&info, 0, sizeof(info));
}

void idPhysics_StaticMulti::InitPhysics(idClip* clip_,
                                        idPhysicsCallbacks* callbacks_,
                                        int entityNumber_,
                                        bool enableBroadphase) {
  idPhysics::InitPhysics(clip_, callbacks_, entityNumber_);
  if (enableBroadphase && parent == nullptr) {
    void* const allocation = idMem::Singleton().AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
        sizeof(idClipModel), TAG_CLIPMODEL, false, ALIGN_16,
        HEAP_DEFAULTHEAP);
    parent = allocation != nullptr
                 ? ::new (allocation) idClipModel(clip, clip->clip8x8)
                 : nullptr;
  }
}

idClipModel* idPhysics_StaticMulti::GetClipModel(int id) const {
  if (id >= 0 && id < clipModels.num && clipModels.list[id] != nullptr) {
    return clipModels.list[id];
  }
  return clip->defaultClipModel;
}

float idPhysics_StaticMulti::GetMass(int id) const {
  if (id >= 0) {
    return mass.list[id];
  }
  float total = 0.0f;
  for (int index = 0; index < clipModels.num; ++index) {
    total = total + mass.list[index];
  }
  return total;
}

void idPhysics_StaticMulti::SetContents(int newContents, int id) {
  if (id >= 0 && id < clipModels.num) {
    if (clipModels.list[id] != nullptr) {
      clipModels.list[id]->SetContents(newContents);
    }
    if (parent != nullptr) {
      parent->SetContents(GetContents(-1));
    }
  } else if (id == -1) {
    for (int index = 0; index < clipModels.num; ++index) {
      if (clipModels.list[index] != nullptr) {
        clipModels.list[index]->SetContents(newContents);
      }
    }
    if (parent != nullptr) {
      parent->SetContents(newContents);
    }
  }
}

int idPhysics_StaticMulti::GetContents(int id) const {
  if (id >= 0 && id < clipModels.num) {
    return clipModels.list[id] != nullptr ? clipModels.list[id]->contents : 0;
  }
  int result = 0;
  if (id == -1) {
    for (int index = 0; index < clipModels.num; ++index) {
      if (clipModels.list[index] != nullptr) {
        result |= clipModels.list[index]->contents;
      }
    }
  }
  return result;
}

const idBounds& idPhysics_StaticMulti::GetBounds(int id) const {
  static idBounds aggregateBounds;
  if (id >= 0 && id < clipModels.num && clipModels.list[id] != nullptr) {
    return clipModels.list[id]->bounds;
  }
  if (id != -1) {
    return bounds_zero;
  }

  aggregateBounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  aggregateBounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < clipModels.num; ++index) {
    if (clipModels.list[index] != nullptr) {
      AddBoundsRetail(aggregateBounds, clipModels.list[index]->absBounds);
    }
  }
  const idVec3& aggregateOrigin = GetOrigin(parent != nullptr ? -1 : 0);
  aggregateBounds.b[0].x -= aggregateOrigin.x;
  aggregateBounds.b[0].y -= aggregateOrigin.y;
  aggregateBounds.b[0].z -= aggregateOrigin.z;
  aggregateBounds.b[1].x -= aggregateOrigin.x;
  aggregateBounds.b[1].y -= aggregateOrigin.y;
  aggregateBounds.b[1].z -= aggregateOrigin.z;
  return aggregateBounds;
}

const idBounds& idPhysics_StaticMulti::GetAbsBounds(int id) const {
  if (id >= 0 && id < clipModels.num && clipModels.list[id] != nullptr) {
    return clipModels.list[id]->absBounds;
  }
  if (id != -1) {
    return bounds_zero;
  }

  absBounds.b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  absBounds.b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < clipModels.num; ++index) {
    if (clipModels.list[index] != nullptr) {
      AddBoundsRetail(absBounds, clipModels.list[index]->absBounds);
    }
  }
  return absBounds;
}

const idVec3& idPhysics_StaticMulti::GetOrigin(int id) const {
  if (id >= 0 && id < clipModels.num) {
    return current.list[id].worldOrigin;
  }
  if (parent != nullptr || clipModels.num == 0) {
    return vec3_origin;
  }
  return current.list[0].worldOrigin;
}

const idMat3& idPhysics_StaticMulti::GetAxis(int id) const {
  if (id >= 0 && id < clipModels.num) {
    return current.list[id].worldAxis;
  }
  if (parent != nullptr || clipModels.num == 0) {
    return mat3_identity;
  }
  return current.list[0].worldAxis;
}

const idVec3& idPhysics_StaticMulti::GetLocalOrigin(int id) const {
  if (id >= 0 && id < clipModels.num) {
    return current.list[id].localOrigin;
  }
  if (parent != nullptr || clipModels.num == 0) {
    return vec3_origin;
  }
  return current.list[0].localOrigin;
}

const idMat3& idPhysics_StaticMulti::GetLocalAxis(int id) const {
  if (id >= 0 && id < clipModels.num) {
    return current.list[id].localAxis;
  }
  if (parent != nullptr || clipModels.num == 0) {
    return mat3_identity;
  }
  return current.list[0].localAxis;
}

void idPhysics_StaticMulti::DisableClip() {
  for (int index = 0; index < clipModels.num; ++index) {
    if (clipModels.list[index] != nullptr) {
      clipModels.list[index]->flags &= ~1u;
    }
  }
  if (parent != nullptr) {
    parent->flags &= ~1u;
  }
}

void idPhysics_StaticMulti::EnableClip() {
  if (parent != nullptr) {
    parent->flags |= 1u;
  }
  for (int index = 0; index < clipModels.num; ++index) {
    if (clipModels.list[index] != nullptr) {
      clipModels.list[index]->flags |= 1u;
    }
  }
}

void idPhysics_StaticMulti::UnlinkClip() {
  for (int index = 0; index < clipModels.num; ++index) {
    if (clipModels.list[index] != nullptr) {
      clipModels.list[index]->Unlink();
    }
  }
  if (parent != nullptr) {
    parent->Unlink();
  }
}

const contactInfo_t& idPhysics_StaticMulti::GetContact(int) const {
  static contactInfo_t info;
  std::memset(&info, 0, sizeof(info));
  return info;
}

void idPhysics_StaticMulti::SetMaster(bool enable,
                                      const idVec3& masterOrigin,
                                      const idMat3& masterAxis,
                                      bindFlags_t flags) {
  if (enable) {
    if (!hasMaster) {
      const idMat3 inverseMasterAxis = Transpose(masterAxis);
      for (int index = 0; index < clipModels.num; ++index) {
        current.list[index].localOrigin =
            WorldToLocal(current.list[index].worldOrigin, masterOrigin,
                         masterAxis);
        if ((static_cast<int>(flags) & 1) != 0) {
          current.list[index].localAxis =
              current.list[index].worldAxis * inverseMasterAxis;
        } else {
          current.list[index].localAxis = current.list[index].worldAxis;
        }
      }
      hasMaster = true;
      isOrientated = (static_cast<int>(flags) & 1) != 0;
    }
  } else {
    hasMaster = false;
  }
}

void idPhysics_StaticMulti::Link(int id, int entityNumber_, int physicsId_,
                                 int bodyId, const idVec3& newOrigin,
                                 const idMat3& newAxis) {
  if (parent != nullptr) {
    idClipModel* const child = clipModels.list[id];
    if (child->clipLinks != nullptr) {
      child->Unlink();
    }
    child->flags &= 0xFFFE0003u;
    child->flags |= 4u * (static_cast<unsigned int>(entityNumber_) & 0x7FFFu);
    child->physicsId = physicsId_;
    child->bodyId = bodyId;
    child->SetLinkedPosition(newOrigin, newAxis);
    const idBounds aggregateBounds = GetBounds(-1);
    parent->Link(entityNumber_, physicsId_, bodyId, GetOrigin(-1),
                 GetAxis(-1), aggregateBounds);
  } else {
    clipModels.list[id]->Link(entityNumber_, physicsId_, bodyId,
                              newOrigin, newAxis);
  }
}

bool idPhysics_StaticMulti::Evaluate(int, int) {
  if (!hasMaster) {
    return false;
  }
  idVec3 masterOrigin;
  idMat3 masterAxis;
  callbacks->GetMasterTransform(masterOrigin, masterAxis);
  for (int bodyId = 0; bodyId < clipModels.num; ++bodyId) {
    current.list[bodyId].worldOrigin =
        LocalToWorld(current.list[bodyId].localOrigin, masterOrigin,
                     masterAxis);
    if (isOrientated) {
      current.list[bodyId].worldAxis =
          current.list[bodyId].localAxis * masterAxis;
    } else {
      current.list[bodyId].worldAxis = current.list[bodyId].localAxis;
    }
    if (clipModels.list[bodyId] != nullptr) {
      Link(bodyId, entityNumber, physicsId, bodyId,
           current.list[bodyId].worldOrigin,
           current.list[bodyId].worldAxis);
    }
  }
  return true;
}

void idPhysics_StaticMulti::SetOrigin(const idVec3& newOrigin, int id) {
  if (id >= 0 && id < clipModels.num) {
    current.list[id].worldOrigin = newOrigin;
    if (hasMaster) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      current.list[id].localOrigin =
          WorldToLocal(newOrigin, masterOrigin, masterAxis);
    } else {
      current.list[id].localOrigin = newOrigin;
    }
    if (clipModels.list[id] != nullptr) {
      Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
           current.list[id].worldAxis);
    }
  } else if (id == -1) {
    const idVec3 translation{
        newOrigin.x - current.list[0].worldOrigin.x,
        newOrigin.y - current.list[0].worldOrigin.y,
        newOrigin.z - current.list[0].worldOrigin.z,
    };
    Translate(translation, -1);
  }
}

void idPhysics_StaticMulti::SetAxis(const idMat3& newAxis, int id) {
  if (id >= 0 && id < clipModels.num) {
    current.list[id].worldAxis = newAxis;
    if (hasMaster && isOrientated) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      current.list[id].localAxis = newAxis * Transpose(masterAxis);
    } else {
      current.list[id].localAxis = newAxis;
    }
    if (clipModels.list[id] != nullptr) {
      Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
           current.list[id].worldAxis);
    }
  } else if (id == -1) {
    const idMat3 delta = Transpose(current.list[0].worldAxis) * newAxis;
    idRotation rotation = delta.ToRotation();
    rotation.origin = current.list[0].worldOrigin;
    Rotate(rotation, -1);
  }
}

void idPhysics_StaticMulti::Translate(const idVec3& translation, int id) {
  if (id >= 0 && id < clipModels.num) {
    current.list[id].localOrigin.x += translation.x;
    current.list[id].localOrigin.y += translation.y;
    current.list[id].localOrigin.z += translation.z;
    current.list[id].worldOrigin.x += translation.x;
    current.list[id].worldOrigin.y += translation.y;
    current.list[id].worldOrigin.z += translation.z;
    if (clipModels.list[id] != nullptr) {
      Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
           current.list[id].worldAxis);
    }
  } else if (id == -1) {
    for (int bodyId = 0; bodyId < clipModels.num; ++bodyId) {
      current.list[bodyId].localOrigin.x += translation.x;
      current.list[bodyId].localOrigin.y += translation.y;
      current.list[bodyId].localOrigin.z += translation.z;
      current.list[bodyId].worldOrigin.x += translation.x;
      current.list[bodyId].worldOrigin.y += translation.y;
      current.list[bodyId].worldOrigin.z += translation.z;
      if (clipModels.list[bodyId] != nullptr) {
        Link(bodyId, entityNumber, physicsId, bodyId,
             current.list[bodyId].worldOrigin,
             current.list[bodyId].worldAxis);
      }
    }
  }
}

void idPhysics_StaticMulti::Rotate(const idRotation& rotation, int id) {
  if (id >= 0 && id < clipModels.num) {
    current.list[id].worldOrigin = rotation * current.list[id].worldOrigin;
    current.list[id].worldAxis *= rotation.ToMat3();
    if (hasMaster) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      current.list[id].localAxis *= rotation.ToMat3();
      current.list[id].localOrigin =
          WorldToLocal(current.list[id].worldOrigin, masterOrigin,
                       masterAxis);
    } else {
      current.list[id].localAxis = current.list[id].worldAxis;
      current.list[id].localOrigin = current.list[id].worldOrigin;
    }
    if (clipModels.list[id] != nullptr) {
      Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
           current.list[id].worldAxis);
    }
  } else if (id == -1) {
    for (int bodyId = 0; bodyId < clipModels.num; ++bodyId) {
      current.list[bodyId].worldOrigin =
          rotation * current.list[bodyId].worldOrigin;
      current.list[bodyId].worldAxis *= rotation.ToMat3();
      current.list[bodyId].worldAxis.OrthoNormalizeSelf();
      if (hasMaster) {
        idVec3 masterOrigin;
        idMat3 masterAxis;
        callbacks->GetMasterTransform(masterOrigin, masterAxis);
        current.list[bodyId].localAxis *= rotation.ToMat3();
        current.list[bodyId].localOrigin =
            WorldToLocal(current.list[bodyId].worldOrigin, masterOrigin,
                         masterAxis);
      } else {
        current.list[bodyId].localAxis = current.list[bodyId].worldAxis;
        current.list[bodyId].localOrigin = current.list[bodyId].worldOrigin;
      }
      if (clipModels.list[bodyId] != nullptr) {
        Link(bodyId, entityNumber, physicsId, bodyId,
             current.list[bodyId].worldOrigin,
             current.list[bodyId].worldAxis);
      }
    }
  }
}

void idPhysics_StaticMulti::LinkClip() {
  for (int bodyId = 0; bodyId < clipModels.num; ++bodyId) {
    if (clipModels.list[bodyId] != nullptr) {
      Link(bodyId, entityNumber, physicsId, bodyId,
           current.list[bodyId].worldOrigin,
           current.list[bodyId].worldAxis);
    }
  }
}

void idPhysics_StaticMulti::SetLocalOrigin(const idVec3& newOrigin,
                                           int id) {
  if (id >= 0 && id < clipModels.num) {
    current.list[id].localOrigin = newOrigin;
    if (hasMaster) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      current.list[id].worldOrigin =
          LocalToWorld(newOrigin, masterOrigin, masterAxis);
    } else {
      current.list[id].worldOrigin = newOrigin;
    }
    if (clipModels.list[id] != nullptr) {
      Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
           current.list[id].worldAxis);
    }
  } else if (id == -1) {
    idVec3 desiredWorld = newOrigin;
    if (hasMaster) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      desiredWorld = LocalToWorld(newOrigin, masterOrigin, masterAxis);
    }
    const idVec3 translation{
        desiredWorld.x - current.list[0].worldOrigin.x,
        desiredWorld.y - current.list[0].worldOrigin.y,
        desiredWorld.z - current.list[0].worldOrigin.z,
    };
    Translate(translation, -1);
  }
}

void idPhysics_StaticMulti::SetLocalAxis(const idMat3& newAxis, int id) {
  if (id >= 0 && id < clipModels.num) {
    current.list[id].localAxis = newAxis;
    if (hasMaster && isOrientated) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      current.list[id].worldAxis = newAxis * masterAxis;
    } else {
      current.list[id].worldAxis = newAxis;
    }
    if (clipModels.list[id] != nullptr) {
      Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
           current.list[id].worldAxis);
    }
  } else if (id == -1) {
    idMat3 desiredWorld = newAxis;
    if (hasMaster) {
      idVec3 masterOrigin;
      idMat3 masterAxis;
      callbacks->GetMasterTransform(masterOrigin, masterAxis);
      desiredWorld = newAxis * masterAxis;
    }
    const idMat3 delta =
        Transpose(current.list[0].worldAxis) * desiredWorld;
    idRotation rotation = delta.ToRotation();
    rotation.origin = current.list[0].worldOrigin;
    Rotate(rotation, -1);
  }
}

void idClipModel::RemoveChild(idClipModel* clipModel) {
  for (int index = 0; index < children.num; ++index) {
    if (children.list[index] == clipModel) {
      children.RemoveIndex(index);
      return;
    }
  }
}

void idPhysics_StaticMulti::ClipTranslation(
    trace_t& results, const idVec3&, const idClipModel*) const {
  std::memset(&results, 0, sizeof(results));
  idLib::Warning("idPhysics_StaticMulti::ClipTranslation called");
}

void idPhysics_StaticMulti::ClipRotation(
    trace_t& results, const idRotation&, const idClipModel*) const {
  std::memset(&results, 0, sizeof(results));
  idLib::Warning("idPhysics_StaticMulti::ClipRotation called");
}

int idPhysics_StaticMulti::ClipContents(const idClipModel* model,
                                        int clipMask) const {
  std::uint32_t contents = 0;
  for (int index = 0; index < clipModels.num; ++index) {
    idClipModel* const clipModel = clipModels.list[index];
    if (clipModel == nullptr) {
      continue;
    }
    alignas(trace_t) unsigned char storage[sizeof(trace_t)];
    trace_t& result = *reinterpret_cast<trace_t*>(storage);
    const int mask = clipMask != 0 ? clipMask : -1;
    if (model != nullptr) {
      clip->ContentsModel(result, clipModel->origin, clipModel,
                          clipModel->axis, mask, model, model->origin,
                          model->axis, nullptr);
    } else {
      clip->Contents(&result, clipModel->origin, clipModel, clipModel->axis,
                     mask, 0x1FFF, nullptr);
    }
    contents |= result.c.contentFlags;
  }
  return static_cast<int>(contents);
}

void idPhysics_StaticMulti::SetClipModel(idClipModel* model, float density,
                                         int id, bool freeOld) {
  if (id >= clipModels.num) {
    current.SetNum(std::max(current.num, id + 1), defaultState);
    clipModels.SetNum(std::max(clipModels.num, id + 1), nullptr);
    mass.SetNum(std::max(mass.num, id + 1), 1.0f);
  }

  idClipModel* const oldModel = clipModels.list[id];
  if (oldModel != nullptr && oldModel != model && freeOld) {
    if (parent != nullptr) {
      parent->RemoveChild(oldModel);
    }
    oldModel->Delete();
  }
  clipModels.list[id] = model;
  if (model != nullptr) {
    if (model->traceModels.num != 0) {
      idVec3 centerOfMass;
      idMat3 inertiaTensor;
      model->GetMassProperties(density, mass.list[id], centerOfMass,
                               inertiaTensor);
    } else {
      mass.list[id] = 1.0f;
    }
    if (parent != nullptr) {
      parent->children.Append(model);
    }
    Link(id, entityNumber, physicsId, id, current.list[id].worldOrigin,
         current.list[id].worldAxis);
  }

  int lastModel = clipModels.num - 1;
  while (lastModel >= 1 && clipModels.list[lastModel] == nullptr) {
    --lastModel;
  }
  const int newCount = lastModel + 1;
  current.SetNum(newCount);
  clipModels.SetNum(newCount);
  mass.SetNum(newCount);
}
