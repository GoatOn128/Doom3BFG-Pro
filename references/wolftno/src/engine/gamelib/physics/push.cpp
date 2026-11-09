#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#include "engine/gamelib/physics/push.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/color.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

extern bool forceIdListsToReallocateEveryAppend;
extern const idVec3 vec3_up;
idRenderDebug* GetGameLibRenderDebug();

idCVar g_showPushHits(
    "g_showPushHits", "0", CVAR_INTEGER,
    "Shows traces for pushers applying impulses on colliding objects",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

constexpr int PUSH_NO = 0;
constexpr int PUSH_OK = 1;
constexpr int PUSH_BLOCKED = 2;

float impulseMult = 10.0f;
float impulseMult_0 = 25.0f;

__forceinline int ClipEntityNumber(const idClipModel& clipModel) {
  return static_cast<std::int32_t>(clipModel.flags << 15) >> 17;
}

__forceinline void DisableClipModel(idClipModel* clipModel) {
  if (clipModel != nullptr) {
    clipModel->flags &= ~1u;
  }
}

__forceinline void EnableClipModel(idClipModel* clipModel) {
  if (clipModel != nullptr) {
    clipModel->flags |= 1u;
  }
}

__forceinline bool IsPlayerNoClip(const idPhysics* physics) {
  return physics->type == PHYSICS_PLAYER &&
         *reinterpret_cast<const int*>(
             reinterpret_cast<const std::uint8_t*>(physics) + 0x268) == 4;
}

__forceinline void ActivatePhysics(idPhysics* physics, int pusherPhysicsId) {
  using Function = void (*)(idPhysicsCallbacks*, int);
  idPhysicsCallbacks* const callbacks = physics->callbacks;
  void** const vtable = *reinterpret_cast<void***>(callbacks);
  reinterpret_cast<Function>(vtable[0x48 / sizeof(void*)])(
      callbacks, pusherPhysicsId);
}

__forceinline bool CrushPhysics(idPhysics* physics, int pusherPhysicsId) {
  using Function = bool (*)(idPhysicsCallbacks*, int);
  idPhysicsCallbacks* const callbacks = physics->callbacks;
  void** const vtable = *reinterpret_cast<void***>(callbacks);
  return reinterpret_cast<Function>(vtable[0x38 / sizeof(void*)])(
      callbacks, pusherPhysicsId);
}

__forceinline float InverseLength(float squaredLength) {
  const __m128 squared = _mm_set_ss(squaredLength);
  return _mm_cvtss_f32(_mm_div_ps(
      _mm_set1_ps(1.0f),
      _mm_sqrt_ps(
          _mm_max_ps(squared, idMath::vector_float_smallestNonDenorm))));
}

__forceinline float ClampedLength(float squaredLength) {
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(squaredLength), idMath::vector_float_smallestNonDenorm)));
}

__forceinline float LengthSquared(const idVec3& vector) {
  return (vector.y * vector.y + vector.x * vector.x) +
         vector.z * vector.z;
}

__forceinline void ClearContact(trace_t& trace) {
  std::memset(&trace.c, 0, sizeof(trace.c));
}

__forceinline void InvertContactPlane(trace_t& trace) {
  trace.c.normal.x = -trace.c.normal.x;
  trace.c.normal.y = -trace.c.normal.y;
  trace.c.normal.z = -trace.c.normal.z;
  trace.c.dist = -trace.c.dist;
}

__forceinline void SetBlockedResult(trace_t& results,
                                    const trace_t& pushResults,
                                    const idClipModel& clipModel,
                                    const idPhysics& blockingPhysics) {
  results = pushResults;
  results.fraction = 0.0f;
  results.endAxis = clipModel.axis;
  results.endpos = clipModel.origin;
  results.c.entityNum = blockingPhysics.entityNumber;
  results.c.physicsId = blockingPhysics.physicsId;
  results.c.bodyId = 0;
}

__forceinline void DebugCollision(const trace_t& trace) {
  if (g_showPushHits.valueInteger != 0 && trace.fraction == 0.0f) {
    idRenderDebug* const debug = GetGameLibRenderDebug();
    debug->DebugPoint(reinterpret_cast<const idVec4&>(idColor::colorYellow),
                      trace.c.point, 3000, false);
    const idVec3 end = trace.c.point + trace.c.normal * 10.0f;
    GetGameLibRenderDebug()->DebugArrow(
        reinterpret_cast<const idVec4&>(idColor::colorGreen), trace.c.point,
        end, 5.0f, 3000, false);
  }
}

__forceinline void DebugImpulse(const idVec4& color, const trace_t& trace,
                                const idVec3& impulse, float size) {
  if (g_showPushHits.valueInteger != 0) {
    GetGameLibRenderDebug()->DebugArrow(color, trace.c.point,
                                        trace.c.point + impulse, size, 3000,
                                        false);
  }
}

} // namespace

void idPush::RestorePushedPhysicsObjectState() {
  for (int index = 0; index < pushed.num; ++index) {
    pushed.list[index]->RestoreState();
  }
}

void idPush::SetPushedOnPushedPhysicsObjects(int deltaTime) {
  for (int index = 0; index < pushed.num; ++index) {
    pushed.list[index]->SetPushed(deltaTime);
  }
}

int idPush::GetPhysicsObjectsTouchingBounds(
    idPhysics** const physicsObjects, const idBounds& bounds, int flags,
    idPhysics* pusher) {
  idClipModel* clipModels[8192];
  const int numClipModels =
      clip->GetClipModelsTouchingBounds(bounds, -1, clipModels, 8192);
  int numPhysicsObjects = 0;

  for (int clipIndex = 0; clipIndex < numClipModels; ++clipIndex) {
    idPhysics* const physics =
        idPhysics::GetPhysicsForId(clipModels[clipIndex]->physicsId);
    if (physics == nullptr ||
        !physics->IsPushable(pusher->GetContents(-1)) ||
        (physics->GetClipMask(-1) & pusher->GetContents(-1)) == 0 ||
        IsPlayerNoClip(physics) ||
        ((flags & 4) != 0 && physics->type != PHYSICS_RIGIDBODY) ||
        ((flags & 8) != 0 && pusher->IsGroundEntity(physics->entityNumber))) {
      continue;
    }

    int index = 0;
    while (index < numPhysicsObjects && physicsObjects[index] != physics) {
      ++index;
    }
    if (index >= numPhysicsObjects) {
      physicsObjects[numPhysicsObjects++] = physics;
    }
  }
  return numPhysicsObjects;
}

bool idPush::RotatePhysicsObjectToAxial(idPhysics* physics,
                                        const idVec3& initialRotationPoint,
                                        const idClipModel*) {
  idVec3 rotationPoint = initialRotationPoint;
  for (int iteration = 0; iteration < 4; ++iteration) {
    const idMat3 axis = physics->GetAxis(0);
    idRotation rotation = axis.ToRotation();
    if (rotation.angle == 0.0f) {
      physics->SetAxis(mat3_identity, -1);
      return true;
    }

    rotation.origin = rotationPoint;
    rotation.angle = -rotation.angle;
    rotation.axisValid = false;
    trace_t trace;
    physics->ClipRotation(trace, rotation, nullptr);
    if (trace.fraction >= 1.0f) {
      physics->SetOrigin(trace.endpos, -1);
      physics->SetAxis(mat3_identity, -1);
      return true;
    }
    if (trace.fraction > 0.0f) {
      physics->SetOrigin(trace.endpos, -1);
      physics->SetAxis(trace.endAxis, -1);
    }
    rotationPoint = trace.c.point;
  }
  return false;
}

void idPush::InitSavingPushedPhysicsObjectState() {
  if (pushed.size < 0) {
    if (pushed.listStatic == 0 || pushed.listStatic == 2) {
      if (pushed.list != nullptr) {
        idMem::Singleton().Free(pushed.list, ALIGN_16);
      }
      pushed.list = nullptr;
      pushed.size = 0;
    }
    pushed.num = 0;
  }
  pushed.num = pushed.size > 0 ? 0 : pushed.size;
}

int idPush::RecursiveTryRotatePushPhysicsObject(
    trace_t& results, trace_t& trace, idPhysics* pusher, int flags,
    const idRotation& rotation, float& mass) {
  idStaticList<idPhysics*, 8192> checked;
  int pushResult = PUSH_BLOCKED;

  for (;;) {
    idPhysics* const physics = idPhysics::GetPhysicsForId(trace.c.physicsId);
    if ((pusher->GetNumClipModels() != 1 &&
         pusher->type != PHYSICS_PARAMETRICDOOR) ||
        physics == nullptr ||
        !physics->IsPushable(pusher->GetContents(-1)) ||
        (pusher->GetContents(-1) & physics->GetClipMask(-1)) == 0 ||
        IsPlayerNoClip(physics) ||
        ((flags & 4) != 0 && physics->type != PHYSICS_RIGIDBODY)) {
      pushResult = PUSH_BLOCKED;
      InvertContactPlane(results);
      break;
    }

    checked.Append(physics);
    physics->DisableClip();
    pushResult = TryRotatePushPhysicsObject(
        results, physics, pusher->GetClipModel(0), flags,
        pusher->GetOrigin(0), pusher->GetAxis(0), rotation, mass);
    if (pushResult == PUSH_NO) {
      break;
    }
    if (pushResult == PUSH_BLOCKED) {
      InvertContactPlane(results);
      break;
    }

    idClipModel* const pusherClipModel = pusher->GetClipModel(0);
    DisableClipModel(pusherClipModel);
    pusher->ClipRotation(trace, rotation, nullptr);
    EnableClipModel(pusherClipModel);
    if (trace.fraction >= 1.0f) {
      break;
    }
  }

  for (int index = 0; index < checked.num; ++index) {
    checked.list[index]->EnableClip();
  }
  return pushResult;
}

int idPush::RecursiveTryTranslatePushPhysicsObject(
    trace_t& results, trace_t& trace, idPhysics* pusher, int flags,
    const idVec3& translation, float& mass) {
  idStaticList<idPhysics*, 8192> checked;
  int pushResult = PUSH_BLOCKED;

  for (;;) {
    idPhysics* const physics = idPhysics::GetPhysicsForId(trace.c.physicsId);
    if ((pusher->GetNumClipModels() != 1 &&
         pusher->type != PHYSICS_PARAMETRICDOOR) ||
        physics == nullptr ||
        !physics->IsPushable(pusher->GetContents(-1)) ||
        (pusher->GetContents(-1) & physics->GetClipMask(-1)) == 0 ||
        IsPlayerNoClip(physics) ||
        ((flags & 4) != 0 && physics->type != PHYSICS_RIGIDBODY)) {
      pushResult = PUSH_BLOCKED;
      InvertContactPlane(results);
      break;
    }

    checked.Append(physics);
    physics->DisableClip();
    const idVec3 newOrigin = pusher->GetOrigin(0) + translation;
    pushResult = TryTranslatePushPhysicsObject(
        results, physics, pusher->GetClipModel(0), flags, newOrigin,
        translation, mass);
    if (pushResult == PUSH_NO) {
      break;
    }
    if (pushResult == PUSH_BLOCKED) {
      InvertContactPlane(results);
      break;
    }

    idClipModel* const pusherClipModel = pusher->GetClipModel(0);
    DisableClipModel(pusherClipModel);
    pusher->ClipTranslation(trace, translation, nullptr);
    EnableClipModel(pusherClipModel);
    if (trace.fraction >= 1.0f) {
      break;
    }
  }

  for (int index = 0; index < checked.num; ++index) {
    checked.list[index]->EnableClip();
  }
  return pushResult;
}

void idPush::SavePhysicsObjectState(idPhysics* physics) {
  for (int index = 0; index < pushed.num; ++index) {
    if (pushed.list[index] == physics) {
      return;
    }
  }

  const int required = pushed.num + 1;
  if (required > pushed.size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (pushed.granularity != 0) {
      newSize = required + pushed.granularity - 1;
      newSize -= newSize % pushed.granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    pushed.Resize(newSize);
  }

  int index = pushed.num;
  if (index >= pushed.size) {
    index = pushed.size - 1;
  } else {
    ++pushed.num;
  }
  pushed.list[index] = physics;
  physics->SaveState();
}

int idPush::TryRotatePushPhysicsObject(
    trace_t& results, idPhysics* check, idClipModel* clipModel, int flags,
    const idVec3& newOrigin, const idMat3& newAxis,
    const idRotation& rotation, float& mass) {
  float angle = rotation.angle;
  results.fraction = 1.0f;
  results.endpos = newOrigin;
  results.endAxis = newAxis;
  ClearContact(results);
  mass += check->GetMass(-1);

  idVec3 rotationPoint;
  if (check->IsGroundClipModel(ClipEntityNumber(*clipModel),
                               clipModel->bodyId)) {
    DisableClipModel(clipModel);
    trace_t trace;
    check->ClipRotation(trace, rotation, nullptr);
    EnableClipModel(clipModel);
    if (trace.fraction < 1.0f) {
      idRotation reverseRotation = rotation;
      reverseRotation.angle = (trace.fraction - 1.0f) * rotation.angle;
      reverseRotation.axisValid = false;
      check->ClipRotation(results, reverseRotation, clipModel);
      if (results.fraction < 1.0f &&
          RecursiveTryRotatePushPhysicsObject(results, trace, check, flags,
                                              rotation, mass) ==
              PUSH_BLOCKED) {
        return PUSH_BLOCKED;
      }
    }
    rotationPoint = check->GetOrigin(0);
  } else {
    idRotation reverseRotation = rotation;
    reverseRotation.angle = -rotation.angle;
    reverseRotation.axis = mat3_identity;
    reverseRotation.axisValid = false;
    check->ClipRotation(results, reverseRotation, clipModel);
    if (results.fraction >= 1.0f) {
      return PUSH_NO;
    }
    if ((flags & 2) != 0) {
      InvertContactPlane(results);
      return PUSH_BLOCKED;
    }

    rotationPoint = results.c.point;
    angle = (1.0f - results.fraction) * rotation.angle;
    idRotation checkRotation = rotation;
    checkRotation.angle = angle;
    checkRotation.axisValid = false;
    DisableClipModel(clipModel);
    trace_t trace;
    check->ClipRotation(trace, checkRotation, nullptr);
    EnableClipModel(clipModel);
    if (trace.fraction < 1.0f) {
      DebugCollision(trace);
      if (RecursiveTryRotatePushPhysicsObject(results, trace, check, flags,
                                              checkRotation, mass) ==
          PUSH_BLOCKED) {
        return PUSH_BLOCKED;
      }
    }
  }

  SavePhysicsObjectState(check);
  idRotation checkRotation = rotation;
  checkRotation.angle = angle;
  checkRotation.axisValid = false;
  static volatile float shit;
  static volatile unsigned int initialized;
  if ((initialized & 1u) == 0) {
    shit = angle;
    initialized |= 1u;
  }
  checkRotation.RotatePoint(rotationPoint);
  check->Rotate(checkRotation, -1);

  const idMat3 oldAxis = clipModel->axis;
  clipModel->Link(ClipEntityNumber(*clipModel), clipModel->physicsId,
                  clipModel->bodyId, newOrigin, newAxis);
  if (check->type == PHYSICS_AI &&
      !RotatePhysicsObjectToAxial(check, rotationPoint, clipModel)) {
    return PUSH_BLOCKED;
  }
  if (check->type == PHYSICS_PLAYER &&
      check->IsGroundClipModel(ClipEntityNumber(*clipModel),
                               clipModel->bodyId)) {
    float* const deltaViewAngles = reinterpret_cast<float*>(
        reinterpret_cast<std::uint8_t*>(check) + 0x258);
    deltaViewAngles[1] +=
        idMath::AngleDelta(oldAxis.mat[0].ToYaw(), newAxis.mat[0].ToYaw());
    deltaViewAngles[0] += idMath::AngleDelta(oldAxis.mat[0].ToPitch(),
                                             newAxis.mat[0].ToPitch());
  }
  return PUSH_OK;
}

int idPush::TryTranslatePushPhysicsObject(
    trace_t& results, idPhysics* check, idClipModel* clipModel, int flags,
    const idVec3& newOrigin, const idVec3& translation, float& mass) {
  idVec3 checkTranslation = translation;
  results.fraction = 1.0f;
  results.endpos = newOrigin;
  results.endAxis = clipModel->axis;
  ClearContact(results);
  mass += check->GetMass(-1);

  if (check->IsGroundClipModel(ClipEntityNumber(*clipModel),
                               clipModel->bodyId)) {
    DisableClipModel(clipModel);
    trace_t trace;
    check->ClipTranslation(trace, translation, nullptr);
    EnableClipModel(clipModel);
    if (trace.fraction < 1.0f) {
      const idVec3 reverseTranslation =
          translation * (trace.fraction - 1.0f);
      check->ClipTranslation(results, reverseTranslation, clipModel);
      if (results.fraction < 1.0f &&
          RecursiveTryTranslatePushPhysicsObject(results, trace, check, flags,
                                                 translation, mass) ==
              PUSH_BLOCKED) {
        return PUSH_BLOCKED;
      }
    }
  } else {
    check->ClipTranslation(results, -translation, clipModel);
    if (results.fraction >= 1.0f) {
      return PUSH_NO;
    }
    if ((flags & 2) != 0) {
      InvertContactPlane(results);
      return PUSH_BLOCKED;
    }

    checkTranslation = translation * (1.0f - results.fraction);
    DisableClipModel(clipModel);
    trace_t trace;
    check->ClipTranslation(trace, checkTranslation, nullptr);
    EnableClipModel(clipModel);
    if (trace.fraction < 1.0f) {
      DebugCollision(trace);
      if (RecursiveTryTranslatePushPhysicsObject(
              results, trace, check, flags, checkTranslation, mass) ==
          PUSH_BLOCKED) {
        return PUSH_BLOCKED;
      }
    }
  }

  SavePhysicsObjectState(check);
  check->Translate(checkTranslation, -1);
  return PUSH_OK;
}

float idPush::ClipTranslationalPush(trace_t& results, idPhysics* pusher,
                                    int flags, const idVec3& newOrigin,
                                    const idVec3& translation) {
  idClipModel* const clipModel = pusher->GetClipModel(0);
  results.fraction = 1.0f;
  results.endpos = newOrigin;
  results.endAxis = clipModel->axis;
  ClearContact(results);
  float mass = 0.0f;
  if (translation == vec3_origin) {
    return mass;
  }

  idVec3 direction = translation;
  const float inverseLength = InverseLength(LengthSquared(direction));
  direction.x = direction.x * inverseLength * impulseMult;
  direction.y = direction.y * inverseLength * impulseMult;
  direction.z = (direction.z * inverseLength + 1.0f) * impulseMult;

  const idBounds bounds = clipModel->bounds;
  if (bounds.b[0].x >= bounds.b[1].x) {
    return mass;
  }
  idBounds pushBounds;
  pushBounds.FromBoundsTranslation(bounds, clipModel->origin, clipModel->axis,
                                   translation);

  const bool wasEnabled = (clipModel->flags & 1u) != 0;
  DisableClipModel(clipModel);
  idPhysics* check[8192];
  const int numChecks =
      GetPhysicsObjectsTouchingBounds(check, pushBounds, flags, pusher);

  idVec3 clipTranslation;
  idVec3 clipOrigin;
  if ((flags & 1) != 0) {
    for (int index = 0; index < numChecks; ++index) {
      check[index]->DisableClip();
    }
    const idVec3 end = clipModel->origin + translation;
    clip->Translation(&results, clipModel->origin, end, clipModel,
                      clipModel->axis, pusher->GetClipMask(-1), 8191, false,
                      nullptr);
    for (int index = 0; index < numChecks; ++index) {
      check[index]->EnableClip();
    }
    if (results.fraction == 0.0f) {
      if (wasEnabled) {
        EnableClipModel(clipModel);
      }
      return mass;
    }
    clipTranslation = results.endpos - clipModel->origin;
    clipOrigin = results.endpos;
  } else {
    clipTranslation = translation;
    clipOrigin = newOrigin;
  }

  EnableClipModel(clipModel);
  const idVec3 oldOrigin = clipModel->origin;
  for (int index = 0; index < numChecks; ++index) {
    idPhysics* const physics = check[index];
    physics->DisableClip();
    trace_t pushResults;
    const int pushResult = TryTranslatePushPhysicsObject(
        pushResults, physics, clipModel, flags, clipOrigin, clipTranslation,
        mass);
    physics->EnableClip();

    if (pushResult == PUSH_OK || (flags & 0x40) != 0) {
      clipModel->Link(ClipEntityNumber(*clipModel), clipModel->physicsId,
                      clipModel->bodyId, newOrigin, clipModel->axis);
      physics->EvaluateContacts();
      clipModel->Link(ClipEntityNumber(*clipModel), clipModel->physicsId,
                      clipModel->bodyId, oldOrigin, clipModel->axis);

      if ((flags & 0x40) != 0) {
        const idVec3 move = newOrigin - oldOrigin;
        const idVec3 physicsOffset = physics->GetOrigin(0) - oldOrigin;
        idVec3 normal{
            move.y * vec3_up.z - move.z * vec3_up.y,
            move.z * vec3_up.x - move.x * vec3_up.z,
            move.x * vec3_up.y - move.y * vec3_up.x,
        };
        normal.z *= 0.0f;
        const float inverseNormalLength = InverseLength(LengthSquared(normal));
        normal *= -inverseNormalLength;
        const float moveLength = ClampedLength(LengthSquared(move));
        idVec3 impulse = move * 1.1f;
        if ((normal * physicsOffset) < 0.0f) {
          impulse -= normal * (moveLength * 2.0f);
        } else {
          impulse += normal * (moveLength * 2.0f);
        }
        DebugImpulse(reinterpret_cast<const idVec4&>(idColor::colorGreen),
                     pushResults, impulse, impulseMult / 5.0f);
        physics->ApplyImpulse(clipModel->bodyId, clipModel->origin,
                              impulse * physics->GetMass(-1));
      } else if ((flags & 0x20) != 0) {
        DebugImpulse(reinterpret_cast<const idVec4&>(idColor::colorGreen),
                     pushResults, direction, impulseMult / 5.0f);
        physics->ApplyImpulse(clipModel->bodyId, clipModel->origin,
                              direction * physics->GetMass(-1));
      } else {
        ActivatePhysics(physics, pusher->physicsId);
      }
    }

    if (pushResult == PUSH_BLOCKED &&
        ((flags & 0x10) == 0 ||
         !CrushPhysics(physics, pusher->physicsId))) {
      SetBlockedResult(results, pushResults, *clipModel, *physics);
      if (!wasEnabled) {
        DisableClipModel(clipModel);
      }
      return mass;
    }
  }

  if (!wasEnabled) {
    DisableClipModel(clipModel);
  }
  return mass;
}

float idPush::ClipRotationalPush(trace_t& results, idPhysics* pusher,
                                 int flags, const idMat3& newAxis,
                                 const idRotation& rotation) {
  idClipModel* const clipModel = pusher->GetClipModel(0);
  float mass = 0.0f;
  results.fraction = 1.0f;
  results.endpos = clipModel->origin;
  results.endAxis = newAxis;
  ClearContact(results);
  if (rotation.angle == 0.0f) {
    return mass;
  }

  const idBounds bounds = clipModel->bounds;
  if (bounds.b[0].x >= bounds.b[1].x) {
    return mass;
  }
  idBounds pushBounds;
  pushBounds.FromBoundsRotation(bounds, clipModel->origin, clipModel->axis,
                                rotation);

  const bool wasEnabled = (clipModel->flags & 1u) != 0;
  DisableClipModel(clipModel);
  idPhysics* check[8192];
  const int numChecks =
      GetPhysicsObjectsTouchingBounds(check, pushBounds, flags, pusher);

  idRotation clipRotation = rotation;
  idMat3 clipAxis;
  if ((flags & 1) != 0) {
    for (int index = 0; index < numChecks; ++index) {
      check[index]->DisableClip();
    }
    clip->Rotation(&results, clipModel->origin, rotation, clipModel,
                   clipModel->axis, pusher->GetClipMask(-1), 8191, false,
                   nullptr);
    for (int index = 0; index < numChecks; ++index) {
      check[index]->EnableClip();
    }
    if (results.fraction == 0.0f) {
      if (wasEnabled) {
        EnableClipModel(clipModel);
      }
      return mass;
    }
    clipRotation.angle = results.fraction * rotation.angle;
    clipRotation.axis = mat3_identity;
    clipRotation.axisValid = false;
    clipAxis = results.endAxis;
  } else {
    clipAxis = newAxis;
  }

  EnableClipModel(clipModel);
  const idMat3 oldAxis = clipModel->axis;
  for (int index = 0; index < numChecks; ++index) {
    idPhysics* const physics = check[index];
    physics->DisableClip();
    trace_t pushResults;
    const int pushResult = TryRotatePushPhysicsObject(
        pushResults, physics, clipModel, flags, clipModel->origin, clipAxis,
        clipRotation, mass);
    physics->EnableClip();

    if (pushResult == PUSH_OK) {
      clipModel->Link(ClipEntityNumber(*clipModel), clipModel->physicsId,
                      clipModel->bodyId, clipModel->origin, newAxis);
      physics->EvaluateContacts();
      clipModel->Link(ClipEntityNumber(*clipModel), clipModel->physicsId,
                      clipModel->bodyId, clipModel->origin, oldAxis);
      const idVec3 normal = pushResults.c.normal * impulseMult_0;
      if ((flags & 0x20) != 0 && pushResults.fraction < 1.0f) {
        DebugImpulse(reinterpret_cast<const idVec4&>(idColor::colorYellow),
                     pushResults, normal, impulseMult_0 / 5.0f);
        physics->ApplyImpulse(pushResults.c.bodyId, pushResults.c.point,
                              normal * physics->GetMass(-1));
      } else {
        ActivatePhysics(physics, pusher->physicsId);
      }
    } else if (pushResult == PUSH_BLOCKED &&
               ((flags & 0x10) == 0 ||
                !CrushPhysics(physics, pusher->physicsId))) {
      SetBlockedResult(results, pushResults, *clipModel, *physics);
      if (!wasEnabled) {
        DisableClipModel(clipModel);
      }
      return mass;
    }
  }

  if (!wasEnabled) {
    DisableClipModel(clipModel);
  }
  return mass;
}

float idPush::ClipPush(trace_t& results, idPhysics* pusher, int flags,
                       const idVec3& oldOrigin, const idMat3& oldAxis,
                       idVec3& newOrigin, idMat3& newAxis) {
  float mass = 0.0f;
  results.fraction = 1.0f;
  results.endpos = newOrigin;
  results.endAxis = newAxis;
  ClearContact(results);

  const idVec3 translation = newOrigin - oldOrigin;
  if (!(translation == vec3_origin)) {
    mass += ClipTranslationalPush(results, pusher, flags, newOrigin,
                                  translation);
    if (results.fraction < 1.0f) {
      newOrigin = oldOrigin;
      newAxis = oldAxis;
      return mass;
    }
  } else {
    newOrigin = oldOrigin;
  }

  idMat3 transpose;
  transpose.mat[0] = {oldAxis.mat[0].x, oldAxis.mat[1].x,
                      oldAxis.mat[2].x};
  transpose.mat[1] = {oldAxis.mat[0].y, oldAxis.mat[1].y,
                      oldAxis.mat[2].y};
  transpose.mat[2] = {oldAxis.mat[0].z, oldAxis.mat[1].z,
                      oldAxis.mat[2].z};
  idRotation rotation = (transpose * newAxis).ToRotation();
  rotation.origin = newOrigin;
  rotation.Normalize180();
  rotation.axisValid = false;
  rotation.ToMat3();
  if (rotation.angle == 0.0f) {
    newAxis = oldAxis;
    return mass;
  }

  newAxis = oldAxis * rotation.ToMat3();
  newAxis.OrthoNormalizeSelf();
  newAxis.mat[0].FixDenormals();
  newAxis.mat[1].FixDenormals();
  newAxis.mat[2].FixDenormals();
  newAxis.mat[0].FixDegenerateNormal();
  newAxis.mat[1].FixDegenerateNormal();
  newAxis.mat[2].FixDegenerateNormal();
  pusher->GetClipModel(0)->SetLinkedPosition(newOrigin, oldAxis);
  mass += ClipRotationalPush(results, pusher, flags, newAxis, rotation);
  if (results.fraction < 1.0f) {
    newOrigin = oldOrigin;
    newAxis = oldAxis;
  }
  return mass;
}
