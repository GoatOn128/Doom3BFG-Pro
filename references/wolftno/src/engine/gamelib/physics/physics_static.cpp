#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#include "engine/gamelib/physics/physics_static.h"

#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"

#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstring>

namespace {

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
  const float deltaX = worldOrigin.x - masterOrigin.x;
  const float deltaY = worldOrigin.y - masterOrigin.y;
  const float deltaZ = worldOrigin.z - masterOrigin.z;
  return {
      (masterAxis[0].y * deltaY + masterAxis[0].x * deltaX) +
          masterAxis[0].z * deltaZ,
      (masterAxis[1].y * deltaY + masterAxis[1].x * deltaX) +
          masterAxis[1].z * deltaZ,
      (masterAxis[2].y * deltaY + masterAxis[2].x * deltaX) +
          masterAxis[2].z * deltaZ,
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

bool MatricesDiffer(const idMat3& left, const idMat3& right) {
  return left[0].x != right[0].x || left[0].y != right[0].y ||
         left[0].z != right[0].z || left[1].x != right[1].x ||
         left[1].y != right[1].y || left[1].z != right[1].z ||
         left[2].x != right[2].x || left[2].y != right[2].y ||
         left[2].z != right[2].z;
}

} // namespace

idPhysics_Static::idPhysics_Static() {
  type = PHYSICS_STATIC;
  clipModel = nullptr;
  current.worldOrigin = vec3_origin;
  current.worldAxis = mat3_identity;
  current.localOrigin = vec3_origin;
  current.localAxis = mat3_identity;
  hasMaster = false;
  isOrientated = false;
}

idPhysics_Static::~idPhysics_Static() {
  if (clipModel != nullptr) {
    clipModel->Delete();
  }
}

void idPhysics_Static::SetClipModel(idClipModel* model, float, int id,
                                    bool freeOld) {
  if (clipModel != nullptr && clipModel != model && freeOld) {
    clipModel->Delete();
  }
  clipModel = model;
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, id, current.worldOrigin,
                current.worldAxis);
  }
}

idClipModel* idPhysics_Static::GetClipModel(int) const {
  return clipModel != nullptr ? clipModel : clip->defaultClipModel;
}

int idPhysics_Static::GetNumClipModels() const {
  return clipModel != nullptr;
}

void idPhysics_Static::SetContents(int contents, int) {
  if (clipModel != nullptr) {
    clipModel->SetContents(contents);
  }
}

int idPhysics_Static::GetContents(int) const {
  return clipModel != nullptr ? clipModel->contents : 0;
}

const idBounds& idPhysics_Static::GetBounds(int) const {
  return clipModel != nullptr ? clipModel->bounds : bounds_zero;
}

const idBounds& idPhysics_Static::GetAbsBounds(int) const {
  if (clipModel != nullptr) {
    return clipModel->absBounds;
  }
  static idBounds absBounds;
  absBounds[1].x = current.worldOrigin.x;
  absBounds[1].y = current.worldOrigin.y;
  absBounds[1].z = current.worldOrigin.z;
  absBounds[0].x = absBounds[1].x;
  absBounds[0].z = absBounds[1].z;
  absBounds[0].y = absBounds[1].y;
  return absBounds;
}

const idVec3& idPhysics_Static::GetOrigin(int) const {
  return current.worldOrigin;
}

const idVec3& idPhysics_Static::GetLocalOrigin(int) const {
  return current.localOrigin;
}

const idVec3& idPhysics_Static::GetGravity() const { return vec3_down; }

void idPhysics_Static::GetImpactInfo(int, const idVec3&,
                                     impactInfo_t& info) const {
  std::memset(&info, 0, sizeof(info));
}

bool idPhysics_Static::Evaluate(int, int) {
  if (!hasMaster) {
    return false;
  }

  const idVec3 oldOrigin = current.worldOrigin;
  const idMat3 oldAxis = current.worldAxis;
  idVec3 masterOrigin;
  idMat3 masterAxis;
  callbacks->GetMasterTransform(masterOrigin, masterAxis);
  current.worldOrigin =
      LocalToWorld(current.localOrigin, masterOrigin, masterAxis);
  current.worldAxis =
      isOrientated ? current.localAxis * masterAxis : current.localAxis;
  LinkClip();

  return current.worldOrigin.x != oldOrigin.x ||
         current.worldOrigin.y != oldOrigin.y ||
         current.worldOrigin.z != oldOrigin.z ||
         MatricesDiffer(current.worldAxis, oldAxis);
}

void idPhysics_Static::SetOrigin(const idVec3& newOrigin, int) {
  current.worldOrigin = newOrigin;
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localOrigin = WorldToLocal(newOrigin, masterOrigin, masterAxis);
  } else {
    current.localOrigin = newOrigin;
  }
  LinkClip();
}

void idPhysics_Static::SetAxis(const idMat3& newAxis, int) {
  current.worldAxis = newAxis;
  if (hasMaster && isOrientated) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localAxis = newAxis * Transpose(masterAxis);
  } else {
    current.localAxis = newAxis;
  }
  LinkClip();
}

void idPhysics_Static::Translate(const idVec3& translation, int) {
  current.localOrigin.x = current.localOrigin.x + translation.x;
  current.localOrigin.y = translation.y + current.localOrigin.y;
  current.localOrigin.z = translation.z + current.localOrigin.z;
  current.worldOrigin.x = translation.x + current.worldOrigin.x;
  current.worldOrigin.y = translation.y + current.worldOrigin.y;
  current.worldOrigin.z = translation.z + current.worldOrigin.z;
  LinkClip();
}

void idPhysics_Static::Rotate(const idRotation& rotation, int) {
  current.worldOrigin = rotation * current.worldOrigin;
  current.worldAxis = current.worldAxis * rotation.ToMat3();
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localAxis = current.localAxis * rotation.ToMat3();
    current.localOrigin =
        WorldToLocal(current.worldOrigin, masterOrigin, masterAxis);
  } else {
    current.localAxis = current.worldAxis;
    current.localOrigin = current.worldOrigin;
  }
  LinkClip();
}

void idPhysics_Static::ClipTranslation(trace_t& results,
                                       const idVec3& translation,
                                       const idClipModel* model) const {
  const idVec3 end{
      translation.x + current.worldOrigin.x,
      current.worldOrigin.y + translation.y,
      current.worldOrigin.z + translation.z,
  };
  if (model != nullptr) {
    clip->TranslationModel(results, current.worldOrigin, end, clipModel,
                           current.worldAxis, 1, model, model->origin,
                           model->axis, nullptr);
  } else {
    clip->Translation(&results, current.worldOrigin, end, clipModel,
                      current.worldAxis, 1, entityNumber, false, nullptr);
  }
}

void idPhysics_Static::ClipRotation(trace_t& results,
                                    const idRotation& rotation,
                                    const idClipModel* model) const {
  if (model != nullptr) {
    clip->RotationModel(results, current.worldOrigin, rotation, clipModel,
                        current.worldAxis, 1, model, model->origin,
                        model->axis, nullptr);
  } else {
    clip->Rotation(&results, current.worldOrigin, rotation, clipModel,
                   current.worldAxis, 1, entityNumber, false, nullptr);
  }
}

int idPhysics_Static::ClipContents(const idClipModel* model,
                                   int clipMask) const {
  if (clipModel == nullptr) {
    return 0;
  }

  const int mask = clipMask != 0 ? clipMask : -1;
  alignas(trace_t) unsigned char contentsStorage[sizeof(trace_t)];
  trace_t& contents = *reinterpret_cast<trace_t*>(contentsStorage);
  if (model != nullptr) {
    clip->ContentsModel(contents, clipModel->origin, clipModel,
                        clipModel->axis, mask, model, model->origin,
                        model->axis, nullptr);
  } else {
    clip->Contents(&contents, clipModel->origin, clipModel, clipModel->axis,
                   mask, 0x1FFF, nullptr);
  }
  return static_cast<int>(contents.c.contentFlags);
}

void idPhysics_Static::DisableClip() {
  if (clipModel != nullptr) {
    clipModel->flags &= ~1u;
  }
}

void idPhysics_Static::EnableClip() {
  if (clipModel != nullptr) {
    clipModel->flags |= 1u;
  }
}

void idPhysics_Static::UnlinkClip() {
  if (clipModel != nullptr) {
    clipModel->Unlink();
  }
}

void idPhysics_Static::LinkClip() {
  if (clipModel != nullptr) {
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                    current.worldOrigin, current.worldAxis);
  }
}

const contactInfo_t& idPhysics_Static::GetContact(int) const {
  static contactInfo_t info;
  std::memset(&info, 0, sizeof(info));
  return info;
}

void idPhysics_Static::AddContactPhysics(idPhysics* phys) {
  bool found = false;
  for (int index = 0; index < contactPhysicsIds.num; ++index) {
    idPhysics* const contact =
        idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
    if (contact != nullptr) {
      if (contact == phys) {
        found = true;
      }
    } else {
      contactPhysicsIds.RemoveIndexFast(index);
      --index;
    }
  }
  if (!found) {
    *contactPhysicsIds.Alloc() = phys->physicsId;
  }
}

void idPhysics_Static::RemoveContactPhysics(idPhysics* phys) {
  for (int index = 0; index < contactPhysicsIds.num; ++index) {
    idPhysics* const contact =
        idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
    if (contact == nullptr || contact == phys) {
      contactPhysicsIds.RemoveIndexFast(index);
      --index;
    }
  }
}

int idPhysics_Static::GetNumContactPhysics() const {
  return contactPhysicsIds.num;
}

idPhysics* idPhysics_Static::GetContactPhysics(int index) const {
  return idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
}

void idPhysics_Static::ActivateContactPhysics() {
  for (int index = 0; index < contactPhysicsIds.num; ++index) {
    idPhysics* const contact =
        idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
    if (contact != nullptr) {
      contact->callbacks->ActivatePhysics(physicsId);
    } else {
      contactPhysicsIds.RemoveIndexFast(index);
      --index;
    }
  }
}

void idPhysics_Static::SetMaster(bool enable, const idVec3& masterOrigin,
                                 const idMat3& masterAxis,
                                 bindFlags_t flags) {
  if (enable) {
    if (!hasMaster) {
      current.localOrigin =
          WorldToLocal(current.worldOrigin, masterOrigin, masterAxis);
      if ((static_cast<int>(flags) & 1) != 0) {
        current.localAxis = current.worldAxis * Transpose(masterAxis);
      } else {
        current.localAxis = current.worldAxis;
      }
      isOrientated = (static_cast<int>(flags) & 1) != 0;
      hasMaster = true;
    }
  } else if (hasMaster) {
    hasMaster = false;
  }
}

void idPhysics_Static::SetLocalOrigin(const idVec3& newOrigin, int) {
  current.localOrigin = newOrigin;
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.worldOrigin = LocalToWorld(newOrigin, masterOrigin, masterAxis);
  } else {
    current.worldOrigin = newOrigin;
  }
  LinkClip();
}

void idPhysics_Static::SetLocalAxis(const idMat3& newAxis, int) {
  current.localAxis = newAxis;
  if (hasMaster && isOrientated) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.worldAxis = newAxis * masterAxis;
  } else {
    current.worldAxis = newAxis;
  }
  LinkClip();
}

int idPhysics_Static::GetBlockingEntityNum() const { return 0x1FFF; }
