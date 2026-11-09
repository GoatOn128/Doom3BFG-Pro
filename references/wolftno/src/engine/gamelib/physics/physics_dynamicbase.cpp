#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#define WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#include "engine/gamelib/physics/physics_dynamicbase.h"

#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/color.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#undef WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI
#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

struct contactsResult_t {
  int numContacts;
  std::uint8_t pad[12];
  contactInfo_t contacts[12];
};

static_assert(sizeof(contactsResult_t) == 0x3A0,
              "contactsResult_t ABI drift");
static_assert(offsetof(contactsResult_t, contacts) == 0x10,
              "contactsResult_t contact offset drift");

extern bool forceIdListsToReallocateEveryAppend;
idRenderDebug* GetGameLibRenderDebug();

namespace {

float InverseSquareRoot(float squaredLength) {
  const __m128 squared = _mm_set_ss(squaredLength);
  const __m128 clamped =
      _mm_max_ps(squared, idMath::vector_float_smallestNonDenorm);
  return _mm_cvtss_f32(_mm_div_ps(_mm_set_ss(1.0f), _mm_sqrt_ps(clamped)));
}

template <typename T, int Tag>
int NextListSize(const idList<T, Tag>& values, int required) {
  if (forceIdListsToReallocateEveryAppend) {
    return required;
  }
  if (values.granularity != 0) {
    const int rounded = required + values.granularity - 1;
    return rounded - rounded % values.granularity;
  }
  int size = 1;
  while (size < required) {
    size *= 2;
  }
  return size;
}

void RemoveFast(idList<int, 77>& values, int index) {
  if (index < 0 || index >= values.num) {
    return;
  }
  --values.num;
  if (index != values.num) {
    values.list[index] = values.list[values.num];
  }
}

idVec3 RotateVector(const idMat3& matrix, const idVec3& vector) {
  return {
      (vector.y * matrix[1].x + vector.x * matrix[0].x) +
          vector.z * matrix[2].x,
      (vector.y * matrix[1].y + vector.x * matrix[0].y) +
          vector.z * matrix[2].y,
      (vector.y * matrix[1].z + vector.x * matrix[0].z) +
          vector.z * matrix[2].z,
  };
}

} // namespace

template <>
idList<int, 77>::AllocResult idList<int, 77>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    Resize(NextListSize(*this, required));
  }
  if (num >= size) {
    return &list[size - 1];
  }
  return &list[num++];
}

idVec3& idVec3::Truncate(float length) {
  if (length == 0.0f) {
    x = 0.0f;
    y = 0.0f;
    z = 0.0f;
    return *this;
  }

  const float oldX = x;
  const float oldY = y;
  const float oldZ = z;
  const float squaredLength =
      (oldX * oldX + oldY * oldY) + oldZ * oldZ;
  if (squaredLength > length * length) {
    const float scale = InverseSquareRoot(squaredLength) * length;
    x = oldX * scale;
    y = oldY * scale;
    z = oldZ * scale;
  }
  return *this;
}

idPhysics_DynamicBase::idPhysics_DynamicBase() {
  type = PHYSICS_INVALID;
  clipMask = 0;
  gravityVector = {0.0f, 0.0f, -800.0f};
  gravityNormal = gravityVector;
  const float inverseLength = InverseSquareRoot(
      (gravityNormal.x * gravityNormal.x +
       gravityNormal.y * gravityNormal.y) +
      gravityNormal.z * gravityNormal.z);
  gravityNormal.x *= inverseLength;
  gravityNormal.y *= inverseLength;
  gravityNormal.z *= inverseLength;
  ClearContacts();
  waterLevel = 0.0f;
  waterViscosity = 0.0f;
}

idPhysics_DynamicBase::~idPhysics_DynamicBase() { ClearContacts(); }

void idPhysics_DynamicBase::SetClipModel(idClipModel*, float, int, bool) {}

idClipModel* idPhysics_DynamicBase::GetClipModel(int) const {
  return nullptr;
}

int idPhysics_DynamicBase::GetNumClipModels() const { return 0; }

void idPhysics_DynamicBase::SetMass(float, int) {}

float idPhysics_DynamicBase::GetMass(int) const { return 0.0f; }

void idPhysics_DynamicBase::SetContents(int, int) {}

int idPhysics_DynamicBase::GetContents(int) const { return 0; }

void idPhysics_DynamicBase::SetClipMask(int mask, int) { clipMask = mask; }

int idPhysics_DynamicBase::GetClipMask(int) const { return clipMask; }

const idBounds& idPhysics_DynamicBase::GetBounds(int) const {
  return bounds_zero;
}

const idBounds& idPhysics_DynamicBase::GetAbsBounds(int) const {
  return bounds_zero;
}

void idPhysics_DynamicBase::SetOrigin(const idVec3&, int) {}
void idPhysics_DynamicBase::SetAxis(const idMat3&, int) {}
void idPhysics_DynamicBase::Translate(const idVec3&, int) {}
void idPhysics_DynamicBase::Rotate(const idRotation&, int) {}

const idVec3& idPhysics_DynamicBase::GetOrigin(int) const {
  return vec3_origin;
}

const idMat3& idPhysics_DynamicBase::GetAxis(int) const {
  return mat3_identity;
}

const idVec3& idPhysics_DynamicBase::GetLocalOrigin(int) const {
  return vec3_origin;
}

const idMat3& idPhysics_DynamicBase::GetLocalAxis(int) const {
  return mat3_identity;
}

void idPhysics_DynamicBase::SetLinearVelocity(const idVec3&, int) {}
void idPhysics_DynamicBase::SetAngularVelocity(const idVec3&, int) {}

idVec3 idPhysics_DynamicBase::GetLinearVelocity(int) const {
  return vec3_origin;
}

idVec3 idPhysics_DynamicBase::GetAngularVelocity(int) const {
  return vec3_origin;
}

void idPhysics_DynamicBase::SetGravity(const idVec3& newGravity) {
  gravityVector = newGravity;
  gravityNormal = newGravity;
  const float inverseLength = InverseSquareRoot(
      (gravityNormal.x * gravityNormal.x +
       gravityNormal.y * gravityNormal.y) +
      gravityNormal.z * gravityNormal.z);
  gravityNormal.x *= inverseLength;
  gravityNormal.y *= inverseLength;
  gravityNormal.z *= inverseLength;
}

const idVec3& idPhysics_DynamicBase::GetGravity() const {
  return gravityVector;
}

const idVec3& idPhysics_DynamicBase::GetGravityNormal() const {
  return gravityNormal;
}

void idPhysics_DynamicBase::SetWaterLevel(float level, int, bool, int,
                                          float) {
  waterLevel = level;
}

float idPhysics_DynamicBase::GetWaterLevel(int) const { return waterLevel; }

void idPhysics_DynamicBase::SetWaterViscosity(float viscosity, int) {
  waterViscosity = viscosity;
}

float idPhysics_DynamicBase::GetWaterViscosity(int) const {
  return waterViscosity;
}

int idPhysics_DynamicBase::GetWaterEntNum() const { return 0; }
float idPhysics_DynamicBase::GetWaterSurfaceWrldHeight() const { return 0.0f; }
float idPhysics_DynamicBase::GetBuoyancyScale() const { return 0.0f; }

void idPhysics_DynamicBase::GetImpactInfo(int, const idVec3&,
                                          impactInfo_t& info) const {
  std::memset(&info, 0, sizeof(info));
}

void idPhysics_DynamicBase::ApplyImpulse(int, const idVec3&,
                                         const idVec3&) {}
void idPhysics_DynamicBase::ApplyForce(int, const idVec3&, const idVec3&) {}
void idPhysics_DynamicBase::Activate() {}
void idPhysics_DynamicBase::PutToRest() {}
bool idPhysics_DynamicBase::IsAtRest() const { return true; }
bool idPhysics_DynamicBase::IsPushable(int) const { return true; }
void idPhysics_DynamicBase::SaveState() {}
void idPhysics_DynamicBase::RestoreState() {}
bool idPhysics_DynamicBase::Evaluate(int, int) { return false; }
void idPhysics_DynamicBase::UpdateTime(int) {}

void idPhysics_DynamicBase::ClipTranslation(trace_t& results,
                                            const idVec3&,
                                            const idClipModel*) const {
  std::memset(&results, 0, sizeof(results));
}

void idPhysics_DynamicBase::ClipRotation(trace_t& results,
                                         const idRotation&,
                                         const idClipModel*) const {
  std::memset(&results, 0, sizeof(results));
}

int idPhysics_DynamicBase::ClipContents(const idClipModel*, int) const {
  return 0;
}

void idPhysics_DynamicBase::DisableClip() {}
void idPhysics_DynamicBase::EnableClip() {}
void idPhysics_DynamicBase::UnlinkClip() {}
void idPhysics_DynamicBase::LinkClip() {}
bool idPhysics_DynamicBase::EvaluateContacts() { return false; }

int idPhysics_DynamicBase::GetNumContacts() const { return contacts.num; }

const contactInfo_t& idPhysics_DynamicBase::GetContact(int index) const {
  return contacts.list[index];
}

void idPhysics_DynamicBase::ClearContacts() {
  for (int index = 0; index < contacts.num; ++index) {
    idPhysics* const physics =
        idPhysics::GetPhysicsForId(contacts.list[index].physicsId);
    if (physics != nullptr) {
      physics->RemoveContactPhysics(this);
    }
  }

  if (contacts.size < 0) {
    if (contacts.listStatic == 0 || contacts.listStatic == 2) {
      if (contacts.list != nullptr) {
        idMem::Singleton().Free(contacts.list, ALIGN_16);
      }
      contacts.list = nullptr;
      contacts.size = 0;
    }
    contacts.num = 0;
  }
  contacts.num = contacts.size > 0 ? 0 : contacts.size;
}

void idPhysics_DynamicBase::AddContactPhysics(idPhysics* physics) {
  bool found = false;
  for (int index = 0; index < contactPhysicsIds.num; ++index) {
    idPhysics* const contact =
        idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
    if (contact != nullptr) {
      if (contact == physics) {
        found = true;
      }
    } else {
      RemoveFast(contactPhysicsIds, index);
      --index;
    }
  }
  if (!found) {
    *contactPhysicsIds.Alloc() = physics->physicsId;
  }
}

void idPhysics_DynamicBase::RemoveContactPhysics(idPhysics* physics) {
  for (int index = 0; index < contactPhysicsIds.num; ++index) {
    idPhysics* const contact =
        idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
    if (contact == nullptr || contact == physics) {
      RemoveFast(contactPhysicsIds, index);
      --index;
    }
  }
}

int idPhysics_DynamicBase::GetNumContactPhysics() const {
  return contactPhysicsIds.num;
}

idPhysics* idPhysics_DynamicBase::GetContactPhysics(int index) const {
  return idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
}

void idPhysics_DynamicBase::ActivateContactPhysics() {
  for (int index = 0; index < contactPhysicsIds.num; ++index) {
    idPhysics* const contact =
        idPhysics::GetPhysicsForId(contactPhysicsIds.list[index]);
    if (contact != nullptr) {
      contact->callbacks->ActivatePhysics(physicsId);
    } else {
      RemoveFast(contactPhysicsIds, index);
      --index;
    }
  }
}

bool idPhysics_DynamicBase::IsGroundNormal(const idVec3& normal) const {
  return (gravityNormal.y * normal.y + gravityNormal.x * normal.x) +
             gravityNormal.z * normal.z <
         -0.1f;
}

bool idPhysics_DynamicBase::HasGroundContacts() const {
  for (int index = 0; index < contacts.num; ++index) {
    const idVec3& normal = contacts.list[index].normal;
    if (((-gravityNormal.x * normal.x) +
         (-gravityNormal.y * normal.y)) +
            (-gravityNormal.z * normal.z) >
        0.0f) {
      return true;
    }
  }
  return false;
}

bool idPhysics_DynamicBase::IsGroundEntity(int entityNum) const {
  for (int index = 0; index < contacts.num; ++index) {
    const contactInfo_t& contact = contacts.list[index];
    if (contact.entityNum == entityNum &&
        (contact.normal.y * gravityNormal.y +
         contact.normal.x * gravityNormal.x) +
                gravityNormal.z * contact.normal.z <
            -0.1f) {
      return true;
    }
  }
  return false;
}

bool idPhysics_DynamicBase::IsGroundClipModel(int entityNum, int id) const {
  for (int index = 0; index < contacts.num; ++index) {
    const contactInfo_t& contact = contacts.list[index];
    if (contact.entityNum == entityNum && contact.bodyId == id &&
        (contact.normal.y * gravityNormal.y +
         contact.normal.x * gravityNormal.x) +
                gravityNormal.z * contact.normal.z <
            -0.98480773f) {
      return true;
    }
  }
  return false;
}

void idPhysics_DynamicBase::AddContactPhysicsForContacts() {
  for (int index = 0; index < contacts.num; ++index) {
    idPhysics* const physics =
        idPhysics::GetPhysicsForId(contacts.list[index].physicsId);
    if (physics != nullptr && physics != this) {
      physics->AddContactPhysics(this);
    }
  }
}

void idPhysics_DynamicBase::AddGroundContacts(const idClipModel* model,
                                              int mask) {
  contactsResult_t result{};
  clip->Contacts(&result, model->origin, gravityNormal, 0.5f, model,
                 model->axis, mask, entityNumber, nullptr);
  for (int index = 0; index < result.numContacts; ++index) {
    if (contacts.list == nullptr) {
      const int initialSize =
          contacts.granularity > 0 ? contacts.granularity : 1;
      if (!contacts.Resize(initialSize)) {
        continue;
      }
    }
    if (contacts.num == contacts.size) {
      int newSize;
      if (forceIdListsToReallocateEveryAppend) {
        newSize = contacts.size + 1;
      } else if (contacts.granularity != 0) {
        newSize = contacts.size + contacts.granularity;
        newSize -= newSize % contacts.granularity;
      } else if (contacts.size <= 0x3FFFFFFF) {
        newSize = contacts.size * 2;
        if (newSize == 0) {
          newSize = 1;
        }
      } else {
        newSize = 0x7FFFFFFF;
      }
      if (!contacts.Resize(newSize)) {
        continue;
      }
    }
    if (contacts.num < contacts.size) {
      std::memcpy(&contacts.list[contacts.num], &result.contacts[index],
                  sizeof(contactInfo_t));
      ++contacts.num;
    }
  }
}

void idPhysics_DynamicBase::SetPushed(int) {}

idVec3 idPhysics_DynamicBase::GetPushedLinearVelocity(int) const {
  return vec3_origin;
}

idVec3 idPhysics_DynamicBase::GetPushedAngularVelocity(int) const {
  return vec3_origin;
}

void idPhysics_DynamicBase::SetMaster(bool, const idVec3&, const idMat3&,
                                      bindFlags_t) {}
void idPhysics_DynamicBase::SetLocalOrigin(const idVec3&, int) {}
void idPhysics_DynamicBase::SetLocalAxis(const idMat3&, int) {}
int idPhysics_DynamicBase::GetBlockingEntityNum() const { return 0x1FFF; }
int idPhysics_DynamicBase::GetLinearEndTime() const { return 0; }
int idPhysics_DynamicBase::GetAngularEndTime() const { return 0; }

bool idPhysics_DynamicBase::IsOutsideWorld() const {
  const idBounds& world = clip->GetWorldBounds();
  const idBounds& object = GetAbsBounds(-1);
  const idVec3 worldMinimum{world[0].x - 1024.0f,
                            world[0].y - 1024.0f,
                            world[0].z - 1024.0f};
  const idVec3 worldMaximum{world[1].x + 1024.0f,
                            world[1].y + 1024.0f,
                            world[1].z + 1024.0f};
  return !(worldMinimum.x <= object[1].x &&
           worldMinimum.y <= object[1].y &&
           worldMinimum.z <= object[1].z &&
           object[0].x <= worldMaximum.x &&
           object[0].y <= worldMaximum.y &&
           object[0].z <= worldMaximum.z);
}

void idPhysics_DynamicBase::DrawVelocity(int id, float linearScale,
                                         float angularScale) const {
  idVec3 linearVelocity = GetLinearVelocity(id);
  idVec3 linearDirection{
      linearVelocity.x * linearScale,
      linearVelocity.y * linearScale,
      linearVelocity.z * linearScale,
  };

  alignas(idVec3) std::uint8_t originStorage[sizeof(idVec3)];
  idVec3& origin = *reinterpret_cast<idVec3*>(originStorage);
  if ((linearDirection.y * linearDirection.y +
       linearDirection.x * linearDirection.x) +
          linearDirection.z * linearDirection.z >
      0.010000001f) {
    linearDirection.Truncate(10.0f);
    origin = GetOrigin(id);
    const idVec3 end{origin.x + linearDirection.x,
                     origin.y + linearDirection.y,
                     origin.z + linearDirection.z};
    GetGameLibRenderDebug()->DebugArrow(idColor::colorRed, origin, end, 1.0f,
                                        0, false);
  }

  const idVec3 angularVelocity = GetAngularVelocity(id);
  const float squaredLength =
      (angularVelocity.y * angularVelocity.y +
       angularVelocity.x * angularVelocity.x) +
      angularVelocity.z * angularVelocity.z;
  const float inverseLength = InverseSquareRoot(squaredLength);
  const idVec3 angularAxis{angularVelocity.x * inverseLength,
                           angularVelocity.y * inverseLength,
                           angularVelocity.z * inverseLength};
  float angularDistance = inverseLength * squaredLength * angularScale;
  if (angularDistance <= 0.1f) {
    return;
  }
  if (angularDistance < 60.0f) {
    angularDistance = 60.0f;
  } else if (angularDistance > 360.0f) {
    angularDistance = 360.0f;
  }

  const idMat3& objectAxis = GetAxis(id);
  idVec3 reference = objectAxis[2];
  float dot = (reference.x * angularAxis.x +
               reference.y * angularAxis.y) +
              reference.z * angularAxis.z;
  const float absoluteDot =
      static_cast<float>(std::fabs(static_cast<double>(dot)));
  if (absoluteDot > 0.99000001f) {
    reference = objectAxis[0];
  }
  dot = (reference.y * angularAxis.y +
         reference.x * angularAxis.x) +
        reference.z * angularAxis.z;
  idVec3 perpendicular{
      reference.x - reference.x * dot,
      reference.y - reference.y * dot,
      reference.z - reference.z * dot,
  };
  const float perpendicularInverseLength = InverseSquareRoot(
      (perpendicular.y * perpendicular.y +
       perpendicular.x * perpendicular.x) +
      perpendicular.z * perpendicular.z);
  perpendicular.x = perpendicular.x * perpendicularInverseLength * 4.0f;
  perpendicular.y = perpendicular.y * perpendicularInverseLength * 4.0f;
  perpendicular.z = perpendicular.z * perpendicularInverseLength * 4.0f;

  idVec3 previous{perpendicular.x + origin.x,
                  perpendicular.y + origin.y,
                  perpendicular.z + origin.z};
  for (float angle = 20.0f; angularDistance > angle; angle += 20.0f) {
    idRotation rotation;
    rotation.origin = vec3_origin;
    rotation.vec = angularAxis;
    rotation.angle = -angle;
    rotation.axis = mat3_identity;
    rotation.axisValid = false;
    const idVec3 rotated = RotateVector(rotation.ToMat3(), perpendicular);
    const idVec3 end{rotated.x + origin.x,
                     rotated.y + origin.y,
                     rotated.z + origin.z};
    GetGameLibRenderDebug()->DebugLine(idColor::colorBlue, previous, end, 1,
                                       false);
    previous = end;
  }

  idRotation rotation;
  rotation.origin = vec3_origin;
  rotation.vec = angularAxis;
  rotation.angle = -angularDistance;
  rotation.axis = mat3_identity;
  rotation.axisValid = false;
  const idVec3 rotated = RotateVector(rotation.ToMat3(), perpendicular);
  const idVec3 end{rotated.x + origin.x,
                   rotated.y + origin.y,
                   rotated.z + origin.z};
  GetGameLibRenderDebug()->DebugArrow(idColor::colorBlue, previous, end, 1.0f,
                                      0, false);
}
