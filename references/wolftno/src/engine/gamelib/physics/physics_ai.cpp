#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#include "engine/gamelib/physics/physics_ai.h"

#include "engine/framework/common.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>
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

class idCollisionModelManager {
public:
  bool GetSlideMoveContactsResult(trace_t* trace,
                                  contactsResult_t* contacts,
                                  idCollisionQuery& query, bool peek);
};

extern idCollisionModelManager* collisionModelManager;
idRenderDebug* GetGameLibRenderDebug();

void operator delete[](void* memory, memTag_t tag) noexcept;

enum class physicsAIClipTag_t : int {
  ClipModel = TAG_CLIPMODEL,
};

void operator delete(void* memory, physicsAIClipTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4physicsAIClipTag_t@@@Z="                   \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 physicsAIClipTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

idCVar ai_debugCollision(
    "ai_debugCollision", "0", CVAR_INTEGER,
    "Draw AI Collision Info; -1 to draw all, or entity number for just that "
    "entity",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

constexpr std::uint64_t kCollisionResultOffsetMask = 0x3fffffffffffffull;
constexpr float kOverClip = 1.001f;
constexpr float kParallelNormalLimit = 0.99989998f;
constexpr float kMaximumImpulseVelocity = 2000.0f;

#define PHYSICS_AI_HAS_NONZERO(value)                                         \
  (((std::bit_cast<std::uint32_t>((value).x) |                               \
     std::bit_cast<std::uint32_t>((value).y) |                               \
     std::bit_cast<std::uint32_t>((value).z)) &                              \
    0x7fffffffu) != 0)

#define PHYSICS_AI_ABSOLUTE(value)                                            \
  std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) & 0x7fffffffu)

#define PHYSICS_AI_FAST_INVERSE_LENGTH(squaredLength)                         \
  _mm_cvtss_f32(_mm_div_ps(                                                   \
      _mm_set1_ps(1.0f),                                                      \
      _mm_sqrt_ps(_mm_max_ps(_mm_set_ss(squaredLength),                       \
                             idMath::vector_float_smallestNonDenorm))))

#define PHYSICS_AI_CANCEL_STEP_QUERY()                                        \
  do {                                                                        \
    idClip* const cancelClip = clip;                                           \
    const std::uint64_t cancelIndex = stepMoveQuery.index;                    \
    if (cancelIndex >= cancelClip->collisionQueryFirstSubmittedIndex &&       \
        cancelIndex <= cancelClip->collisionQueryLastAllocedIndex) {          \
      cancelClip->collisionQueries[cancelIndex & 0xfffu].type = 1;            \
    }                                                                         \
    stepMoveQuery.index = 0;                                                  \
  } while (false)

}  // namespace

idPhysics_AI::~idPhysics_AI() {
  if (clipModel_standing != nullptr) {
    clipModel_standing->Delete();
  }
  if (clipModel_crouched != nullptr) {
    clipModel_crouched->Delete();
  }
  clipModels[0] = nullptr;
}

void idPhysics_AI::Activate() { current.atRest = -1; }

bool idPhysics_AI::IsAtRest() const { return current.atRest >= 0; }

idVec3 idPhysics_AI::GetLinearVelocity(int) const { return current.velocity; }

idVec3 idPhysics_AI::GetPushedLinearVelocity(int) const {
  return current.pushVelocity;
}

void idPhysics_AI::SetContents(int contents, int id) {
  if (id < 0) {
    id = 0;
  }

  if (id != 0) {
    clipModels[id]->SetContents(contents);
    return;
  }

  if (clipModel_standing != nullptr) {
    clipModel_standing->SetContents(contents);
  }
  if (clipModel_crouched != nullptr) {
    clipModel_crouched->SetContents(contents);
  }
}

void idPhysics_AI::Evolve(float timeStep) {
  float stepUp = maxStepHeight;
  float stepDown = 0.0f;

  switch (movementType) {
    case AI_MOVE_WALKING: {
      float upVelocity;
      if (gravityNormal.x == vec3_origin.x &&
          gravityNormal.y == vec3_origin.y &&
          gravityNormal.z == vec3_origin.z) {
        upVelocity = current.velocity.z;
      } else {
        upVelocity = -((current.velocity.y * gravityNormal.y +
                        gravityNormal.x * current.velocity.x) +
                       current.velocity.z * gravityNormal.z);
      }

      if (current.onGround &&
          (upVelocity <= 1.0f ||
           (swimmer && GetWaterLevel(0) != 0.0f))) {
        if (swimmer && GetWaterLevel(0) > 0.0f) {
          current.velocity.z =
              timeStep * gravityVector.z + current.velocity.z;
          current.velocity.y =
              timeStep * gravityVector.y + current.velocity.y;
          current.velocity.x =
              timeStep * gravityVector.x + current.velocity.x;
          const float vertical = current.velocity.z;
          current.velocity.z =
              vertical - GetWaterViscosity(0) * GetWaterLevel(0) * vertical;
          break;
        }

        const float gravityVelocity =
            (gravityNormal.x * current.velocity.x +
             current.velocity.y * gravityNormal.y) +
            current.velocity.z * gravityNormal.z;
        const float oldY = current.velocity.y;
        current.velocity.x -= gravityNormal.x * gravityVelocity;
        current.velocity.z -= gravityVelocity * gravityNormal.z;
        current.velocity.y =
            oldY - gravityVelocity * gravityNormal.y;
      } else {
        current.velocity.y =
            timeStep * gravityVector.y + current.velocity.y;
        current.velocity.z =
            timeStep * gravityVector.z + current.velocity.z;
        current.velocity.x =
            timeStep * gravityVector.x + current.velocity.x;
        if (swimmer) {
          const float vertical = current.velocity.z;
          current.velocity.z =
              vertical -
              GetWaterLevel(0) * (GetWaterViscosity(0) * 0.1f) * vertical;
        }
      }
      stepDown = maxStepHeight * 2.0f;
      break;
    }

    case AI_MOVE_FLYING:
      stepDown = maxStepHeight;
      break;

    case AI_MOVE_DROPPING:
      current.velocity.x =
          timeStep * gravityVector.x + current.velocity.x;
      current.velocity.z =
          timeStep * gravityVector.z + current.velocity.z;
      current.velocity.y =
          timeStep * gravityVector.y + current.velocity.y;
      if (maxDropVelocity > current.velocity.z) {
        current.velocity.z = maxDropVelocity;
      }
      stepDown = maxStepHeight;
      break;

    default:
      return;
  }

  current.velocity.ProjectOntoPlane(lastCollisionNormal, kOverClip);

  idVec3 delta{
      timeStep * current.velocity.x,
      timeStep * current.velocity.y,
      timeStep * current.velocity.z,
  };
  if (idMath::FLT_SMALLEST_NON_DENORMAL >= PHYSICS_AI_ABSOLUTE(delta.x)) {
    delta.x = 0.0f;
  }
  if (idMath::FLT_SMALLEST_NON_DENORMAL >= PHYSICS_AI_ABSOLUTE(delta.y)) {
    delta.y = 0.0f;
  }
  if (idMath::FLT_SMALLEST_NON_DENORMAL >= PHYSICS_AI_ABSOLUTE(delta.z)) {
    delta.z = 0.0f;
  }

  if (PHYSICS_AI_HAS_NONZERO(delta)) {
    idClipModel* const clipModel = clipModels[0];
    const idVec3 end{
        delta.x + clipModel->origin.x,
        delta.y + clipModel->origin.y,
        delta.z + clipModel->origin.z,
    };
    stepMoveQuery = clip->StepMoveContacts(
        nullptr, nullptr, clipModel->origin, end, gravityNormal, stepUp,
        stepDown, clipModel, clipModel->axis, clipMasks[0], entityNumber,
        true,
        "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_AI.cpp(123) "
        ": StepMoveContacts");
  }

  idClipModel* const secondary = clipModels[1];
  if (secondary != nullptr && (secondary->flags & 1u) != 0) {
    secondary->SetLinkedPosition(clipModels[0]->origin, clipModels[0]->axis);
  }
}

void idPhysics_AI::GetImpactInfo(int, const idVec3&,
                                 impactInfo_t& info) const {
  info.invMass = invMass;
  std::memset(&info.invInertiaTensor, 0, sizeof(info.invInertiaTensor));
  info.position = vec3_origin;
  info.velocity = current.velocity;
}

void idPhysics_AI::ApplyImpulse(int, const idVec3& point,
                                const idVec3& impulse) {
  if (noImpact) {
    return;
  }

  const float impulseSquared =
      (impulse.x * impulse.x + impulse.y * impulse.y) +
      impulse.z * impulse.z;
  const float impulseLength =
      PHYSICS_AI_FAST_INVERSE_LENGTH(impulseSquared) * impulseSquared;
  if (swimmer && impulse.z == impulseLength) {
    current.velocity.z =
        impulse.z * 1000.0f * invMass + current.velocity.z;
  } else {
    current.velocity.z =
        invMass * impulse.z + current.velocity.z;
    current.velocity.x =
        invMass * impulse.x + current.velocity.x;
    current.velocity.y =
        invMass * impulse.y + current.velocity.y;
  }

  const float velocitySquared =
      (current.velocity.x * current.velocity.x +
       current.velocity.y * current.velocity.y) +
      current.velocity.z * current.velocity.z;
  if (PHYSICS_AI_FAST_INVERSE_LENGTH(velocitySquared) * velocitySquared >
      kMaximumImpulseVelocity) {
    const float inverseLength =
        PHYSICS_AI_FAST_INVERSE_LENGTH(velocitySquared);
    const float x = current.velocity.x;
    current.velocity.y *= inverseLength;
    current.velocity.z *= inverseLength;
    current.velocity.y *= kMaximumImpulseVelocity;
    current.velocity.x = x * inverseLength * kMaximumImpulseVelocity;
    current.velocity.z *= kMaximumImpulseVelocity;
  }

  callbacks->ActivatePhysics(physicsId);
  static_cast<void>(point);
}

void idPhysics_AI::PutToRest() {
  current.atRest = 1;
  current.velocity = vec3_origin;
}

void idPhysics_AI::SaveState() {
  saved = current;
  saved.worldOrigin = clipModels[0]->origin;
}

void idPhysics_AI::RestoreState() {
  current = saved;
  LinkClip(current.worldOrigin, clipModels[0]->axis);
}

void idPhysics_AI::SetClipModel(idClipModel* model, float density, int id,
                                bool freeOld) {
  if (id == 0) {
    if (clipModel_standing != nullptr) {
      clipModel_standing->Delete();
      clipModel_standing = nullptr;
    }
    if (clipModel_crouched != nullptr) {
      clipModel_crouched->Delete();
      clipModel_crouched = nullptr;
    }
    clipModels[0] = nullptr;

    if (model != nullptr) {
      clipModel_standing = model;
      idBounds bounds = model->bounds;
      bounds[1].z = heightCrouched;

      alignas(idTraceModel) std::byte traceModelStorage[sizeof(idTraceModel)];
      idTraceModel& crouchTraceModel =
          *reinterpret_cast<idTraceModel*>(traceModelStorage);
      crouchTraceModel.InitBox();
      crouchTraceModel.SetupBox(bounds);

      clipModel_crouched =
          new (physicsAIClipTag_t::ClipModel)
              idClipModel(clip, &crouchTraceModel, 1, nullptr);
      clipModel_crouched->SetContents(clipModel_standing->contents);
      clipModel_crouched->Unlink();
      clipModel_crouched->origin.x += current.worldOrigin.x;
      clipModel_crouched->origin.y =
          current.worldOrigin.y + clipModel_crouched->origin.y;
      clipModel_crouched->origin.z =
          current.worldOrigin.z + clipModel_crouched->origin.z;
    }
  }

  idPhysics_Actor::SetClipModel(model, density, id, freeOld);
}

void idPhysics_AI::SetOrigin(const idVec3& newOrigin, int) {
  PHYSICS_AI_CANCEL_STEP_QUERY();
  LinkClip(newOrigin, clipModels[0]->axis);
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const float x = newOrigin.x - masterOrigin.x;
    const float y = newOrigin.y - masterOrigin.y;
    const float z = newOrigin.z - masterOrigin.z;
    current.localOrigin = {
        (masterAxis.mat[0].y * y + masterAxis.mat[0].x * x) +
            masterAxis.mat[0].z * z,
        (masterAxis.mat[1].y * y + masterAxis.mat[1].x * x) +
            masterAxis.mat[1].z * z,
        (masterAxis.mat[2].y * y + masterAxis.mat[2].x * x) +
            masterAxis.mat[2].z * z,
    };
  }
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_AI::SetAxis(const idMat3& newAxis, int) {
  PHYSICS_AI_CANCEL_STEP_QUERY();
  LinkClip(clipModels[0]->origin, newAxis);
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_AI::SetLinearVelocity(const idVec3& newLinearVelocity, int) {
  current.velocity = newLinearVelocity;
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_AI::SetPushed(int deltaTime) {
  idClipModel* const clipModel = clipModels[0];
  const float inverseTime =
      1.0f / (static_cast<float>(deltaTime) * 0.001f);
  current.pushVelocity.x =
      inverseTime * (clipModel->origin.x - saved.worldOrigin.x);
  current.pushVelocity.y =
      inverseTime * (clipModel->origin.y - saved.worldOrigin.y);
  current.pushVelocity.z =
      inverseTime * (clipModel->origin.z - saved.worldOrigin.z);
}

void idPhysics_AI::SetMaster(bool enable, const idVec3& masterOrigin,
                             const idMat3& masterAxis, bindFlags_t) {
  PHYSICS_AI_CANCEL_STEP_QUERY();
  if (enable) {
    if (!hasMaster) {
      const idVec3& world = clipModels[0]->origin;
      const float x = world.x - masterOrigin.x;
      const float y = world.y - masterOrigin.y;
      const float z = world.z - masterOrigin.z;
      current.localOrigin = {
          (masterAxis.mat[0].y * y + masterAxis.mat[0].x * x) +
              masterAxis.mat[0].z * z,
          (masterAxis.mat[1].y * y + masterAxis.mat[1].x * x) +
              masterAxis.mat[1].z * z,
          (masterAxis.mat[2].y * y + masterAxis.mat[2].x * x) +
              masterAxis.mat[2].z * z,
      };
      hasMaster = true;
      masterYaw = masterAxis.mat[0].ToYaw();
    }
    ClearContacts();
  } else if (hasMaster) {
    hasMaster = false;
    callbacks->ActivatePhysics(physicsId);
  }
}

void idPhysics_AI::SetLocalOrigin(const idVec3& newOrigin, int) {
  PHYSICS_AI_CANCEL_STEP_QUERY();
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localOrigin = newOrigin;
    const idVec3 worldOrigin{
        ((newOrigin.y * masterAxis.mat[1].x +
          newOrigin.x * masterAxis.mat[0].x) +
         newOrigin.z * masterAxis.mat[2].x) +
            masterOrigin.x,
        ((newOrigin.y * masterAxis.mat[1].y +
          newOrigin.x * masterAxis.mat[0].y) +
         newOrigin.z * masterAxis.mat[2].y) +
            masterOrigin.y,
        ((newOrigin.y * masterAxis.mat[1].z +
          newOrigin.x * masterAxis.mat[0].z) +
         newOrigin.z * masterAxis.mat[2].z) +
            masterOrigin.z,
    };
    LinkClip(worldOrigin, clipModels[0]->axis);
  } else {
    LinkClip(newOrigin, clipModels[0]->axis);
  }
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_AI::SetLocalAxis(const idMat3& newAxis, int) {
  PHYSICS_AI_CANCEL_STEP_QUERY();
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const idMat3 worldAxis = newAxis * masterAxis;
    LinkClip(clipModels[0]->origin, worldAxis);
  } else {
    LinkClip(clipModels[0]->origin, newAxis);
  }
  callbacks->ActivatePhysics(physicsId);
}

idPhysics_AI::idPhysics_AI() {
  stepMoveQuery.index = 0;
  contactsQuery.index = 0;
  lastCollisionNormal = vec3_origin;
  lastCollisionPoint = vec3_origin;
  stuckCollisionNormal = vec3_origin;
  stuckCollisionPoint = vec3_origin;
  clipModel_standing = nullptr;
  clipModel_crouched = nullptr;
  maxStepHeight = 18.0f;
  minFloorCosine = 0.49999994f;
  maxDropVelocity = -500.0f;
  heightCrouched = 48.0f;
  movementType = AI_MOVE_WALKING;
  noImpact = false;
  masterControlledVelocity = false;
  swimmer = false;
  crouched = false;
  canSetCrouchedPhysics = false;
  fullContact = false;
  type = PHYSICS_AI;
  std::memset(&current, 0, sizeof(current));
  current.atRest = -1;
  saved = current;
}

void idPhysics_AI::ResolveCollisions() {
  lastCollisionNormal = vec3_origin;
  lastCollisionPoint = vec3_origin;

  idClip* const ownerClip = clip;
  const std::uint64_t index = stepMoveQuery.index;
  if (index < ownerClip->collisionQueryFirstSubmittedIndex ||
      index > ownerClip->collisionQueryLastSubmittedIndex) {
    return;
  }

  idClip::idClipCollisionQuery& collisionQuery =
      ownerClip->collisionQueries[index & 0xfffu];
  const std::uint64_t resultOffset =
      collisionQuery.query.offset & kCollisionResultOffsetMask;
  if (resultOffset < idCollisionQuery::finalResultsFirstSubmittedOffset ||
      resultOffset >= idCollisionQuery::finalResultsLastSubmittedOffset) {
    return;
  }

  const std::uint64_t savedIndex = stepMoveQuery.index;
  stepMoveQuery.index = 0;
  trace_t traceResult;
  contactsResult_t contactsResult;
  collisionModelManager->GetSlideMoveContactsResult(
      &traceResult, &contactsResult,
      ownerClip->collisionQueries[savedIndex & 0xfffu].query, false);

  lastCollisionNormal = traceResult.c.normal;
  lastCollisionPoint = traceResult.c.point;
  if (PHYSICS_AI_HAS_NONZERO(lastCollisionNormal)) {
    const idVec3& gravity = GetGravityNormal();
    const float gravityDot =
        (gravity.y * lastCollisionNormal.y +
         lastCollisionNormal.x * gravity.x) +
        gravity.z * lastCollisionNormal.z;
    if (gravityDot > -kParallelNormalLimit &&
        gravityDot < kParallelNormalLimit) {
      stuckCollisionNormal = lastCollisionNormal;
      stuckCollisionPoint = lastCollisionPoint;
    }
  }

  if (ai_debugCollision.valueInteger < 0 ||
      ai_debugCollision.valueInteger == entityNumber) {
    if (traceResult.fraction < 1.0f &&
        PHYSICS_AI_HAS_NONZERO(lastCollisionNormal)) {
      GetGameLibRenderDebug()->DebugPoint(
          idColor::colorYellow, traceResult.c.point, 100, false);
      const idVec3 arrowEnd{
          traceResult.c.normal.x * 24.0f + traceResult.c.point.x,
          traceResult.c.normal.y * 24.0f + traceResult.c.point.y,
          traceResult.c.normal.z * 24.0f + traceResult.c.point.z,
      };
      GetGameLibRenderDebug()->DebugArrow(
          idColor::colorMagenta, traceResult.c.point, arrowEnd, 2.0f, 100,
          false);
    }

    for (int i = 0; i < contactsResult.numContacts; ++i) {
      idGame* const game = common->Game();
      const auto gameTable =
          *reinterpret_cast<const void* const* const*>(game);
      using GetToolsFunction = const idTypeInfoTools* (*)(idGame*);
      const idTypeInfoTools* const tools =
          reinterpret_cast<GetToolsFunction>(
              const_cast<void*>(gameTable[93]))(game);
      ownerClip->ShowCollisionPoint(contactsResult.contacts[i], tools);
    }
  }

  fullContact = contactsResult.numContacts >= 4;
  UpdateCollisionResidency(traceResult.c);
  ClearContacts();
  for (int i = 0; i < contactsResult.numContacts; ++i) {
    contacts.Append(contactsResult.contacts[i]);
  }
  AddContactPhysicsForContacts();
  ActivateContactPhysics();
  current.onGround = false;

  if (!PHYSICS_AI_HAS_NONZERO(gravityNormal) || contacts.num <= 0) {
    return;
  }

  alignas(trace_t) std::byte groundTraceStorage[sizeof(trace_t)];
  trace_t& groundTrace = *reinterpret_cast<trace_t*>(groundTraceStorage);
  groundTrace.endpos = clipModels[0]->origin;
  groundTrace.endAxis = clipModels[0]->axis;
  groundTrace.c = contacts.list[0];

  float normalX = contacts.list[0].normal.x;
  float normalY = contacts.list[0].normal.y;
  float normalZ = contacts.list[0].normal.z;
  for (int i = 1; i < contacts.num; ++i) {
    normalX += contacts.list[i].normal.x;
    normalY += contacts.list[i].normal.y;
    normalZ += contacts.list[i].normal.z;
  }

  const float squaredNormal =
      (normalX * normalX + normalY * normalY) + normalZ * normalZ;
  const float inverseNormalLength =
      PHYSICS_AI_FAST_INVERSE_LENGTH(squaredNormal);
  normalX *= inverseNormalLength;
  normalY *= inverseNormalLength;
  normalZ *= inverseNormalLength;
  groundTrace.c.normal = {normalX, normalY, normalZ};

  const float floorDot =
      (normalX * gravityNormal.x + normalY * gravityNormal.y) +
      normalZ * gravityNormal.z;
  if (!(-minFloorCosine > floorDot)) {
    return;
  }

  current.onGround = true;
  callbacks->Collide(physicsId, groundTrace, current.velocity);
  idPhysics* const groundPhysics =
      idPhysics::GetPhysicsForId(groundTrace.c.physicsId);
  if (groundPhysics == nullptr) {
    return;
  }

  impactInfo_t info;
  groundPhysics->GetImpactInfo(groundTrace.c.bodyId, groundTrace.c.point,
                               info);
  if (PHYSICS_AI_ABSOLUTE(info.invMass) >
      idMath::FLT_SMALLEST_NON_DENORMAL) {
    const float scale = 1.0f / (info.invMass * 10.0f);
    const idVec3 impulse{
        scale * current.velocity.x,
        scale * current.velocity.y,
        scale * current.velocity.z,
    };
    groundPhysics->callbacks->ApplyImpulse(
        physicsId, 0, groundTrace.c.point, impulse);
  }
}

bool idPhysics_AI::Evaluate(int timeStepMSec, int) {
  RememberHistorySample();
  const float timeStep = static_cast<float>(timeStepMSec) * 0.001f;

  idClipModel* const selectedClipModel =
      crouched && canSetCrouchedPhysics ? clipModel_crouched
                                       : clipModel_standing;
  idClipModel* const oldClipModel = clipModels[0];
  if (selectedClipModel != oldClipModel) {
    const idVec3 oldOrigin = oldClipModel->origin;
    const idMat3 oldAxis = oldClipModel->axis;
    oldClipModel->Unlink();
    clipModels[0] = selectedClipModel;
    selectedClipModel->Link(entityNumber, physicsId, 0, oldOrigin, oldAxis);
  }

  if (hasMaster) {
    const idVec3 oldOrigin = clipModels[0]->origin;
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const idVec3& local = current.localOrigin;
    const idVec3 newOrigin{
        ((local.y * masterAxis.mat[1].x +
          local.x * masterAxis.mat[0].x) +
         local.z * masterAxis.mat[2].x) +
            masterOrigin.x,
        ((local.y * masterAxis.mat[1].y +
          local.x * masterAxis.mat[0].y) +
         local.z * masterAxis.mat[2].y) +
            masterOrigin.y,
        ((local.y * masterAxis.mat[1].z +
          local.x * masterAxis.mat[0].z) +
         local.z * masterAxis.mat[2].z) +
            masterOrigin.z,
    };
    LinkClip(newOrigin, clipModels[0]->axis);
    const float inverseTime = 1.0f / timeStep;
    current.velocity.x = (newOrigin.x - oldOrigin.x) * inverseTime;
    current.velocity.y = (newOrigin.y - oldOrigin.y) * inverseTime;
    current.velocity.z = (newOrigin.z - oldOrigin.z) * inverseTime;
    masterDeltaYaw = masterYaw;
    masterYaw = masterAxis.mat[0].ToYaw();
    masterDeltaYaw = masterYaw - masterDeltaYaw;
    return newOrigin.x != oldOrigin.x || newOrigin.y != oldOrigin.y ||
           newOrigin.z != oldOrigin.z;
  }

  if (current.atRest >= 0) {
    return false;
  }

  ResolveCollisions();
  Evolve(timeStep);
  if (IsOutsideWorld()) {
    callbacks->DeactivatePhysics(physicsId);
  }
  return true;
}

void idPhysics_AI::Translate(const idVec3& translation, int) {
  ResolveCollisions();
  idClipModel* const clipModel = clipModels[0];
  const idVec3 translatedOrigin{
      clipModel->origin.x + translation.x,
      clipModel->origin.y + translation.y,
      clipModel->origin.z + translation.z,
  };
  LinkClip(translatedOrigin, clipModel->axis);
  callbacks->ActivatePhysics(physicsId);
  if (hasMaster) {
    current.localOrigin.x += translation.x;
    current.localOrigin.y += translation.y;
    current.localOrigin.z = translation.z + current.localOrigin.z;
  }
}

void idPhysics_AI::Rotate(const idRotation& rotation, int) {
  ResolveCollisions();
  idClipModel* const clipModel = clipModels[0];
  const idVec3 rotatedOrigin = rotation * clipModel->origin;
  const idMat3 rotatedAxis = clipModel->axis * rotation.ToMat3();
  LinkClip(rotatedOrigin, rotatedAxis);
  callbacks->ActivatePhysics(physicsId);
  if (hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const idVec3& world = clipModels[0]->origin;
    const float x = world.x - masterOrigin.x;
    const float y = world.y - masterOrigin.y;
    const float z = world.z - masterOrigin.z;
    current.localOrigin = {
        (masterAxis.mat[0].y * y + masterAxis.mat[0].x * x) +
            masterAxis.mat[0].z * z,
        (masterAxis.mat[1].y * y + masterAxis.mat[1].x * x) +
            masterAxis.mat[1].z * z,
        (masterAxis.mat[2].y * y + masterAxis.mat[2].x * x) +
            masterAxis.mat[2].z * z,
    };
  }
}
