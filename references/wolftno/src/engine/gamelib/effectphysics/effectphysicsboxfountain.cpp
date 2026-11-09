#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/effectphysics/effectphysicsboxfountain.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/discreteanimation/rendermodelmanyboxes.h"
#pragma warning(pop)
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/tracemodel.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

idCVar g_effectPhysicsBoxFountain(
    "g_effectPhysicsBoxFountain", "0", CVAR_INTEGER, "",
    static_cast<argCompletionRef_t>(nullptr));
idCVar g_effectPhysicsRespawnTime(
    "g_effectPhysicsRespawnTime", "200", CVAR_INTEGER, "",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

void RetireManyBoxes(idRenderModelManyBoxes* model) {
  if (model == nullptr) {
    return;
  }

  model->CommitThisFrame();
  using RenderModelFunction = void (*)(idRenderModel*);
  RenderModelFunction* const vtable =
      *reinterpret_cast<RenderModelFunction**>(model);
  if ((model->unlinked & 8u) == 0u) {
    model->CommitThisFrame();
    vtable[7](model);
    model->unlinked |= 8u;
  }
  vtable[8](model);
  model->deleteOnSync |= 4u;
}

}  // namespace

void idEffectPhysicsRigidBody::SetOrientation(const idMat3& orientation) {
  currentState.orientation = orientation;
  currentState.orientationFiltered = orientation;
  properties->broadPhase->SetObjectPosition(
      &SPObject, currentState.position, currentState.orientation, nullptr);
}

void idEffectPhysicsBoxFountain::Init(idClip* clip,
                                      idTraceModelCache* traceModelCache,
                                      int entityNumber) {
  broadPhase.Init(clip, traceModelCache);

  idBounds bounds;
  bounds[0] = {-16.0f, -16.0f, -16.0f};
  bounds[1] = {16.0f, 16.0f, 16.0f};

  alignas(idTraceModel) std::byte traceModelStorage[sizeof(idTraceModel)];
  idTraceModel& traceModel =
      *reinterpret_cast<idTraceModel*>(traceModelStorage);
  traceModel.InitBox();
  traceModel.SetupBox(bounds);

  const int traceModelIndex =
      broadPhase.traceModelCache->AllocTraceModel(traceModel, nullptr);
  properties.Setup(&broadPhase, traceModelIndex, 10.0f);

  startOrigin = {0.0f, 0.0f, 64.0f};
  spawnOrigin = {0.0f, 0.0f, 160.0f};
  broadPhase.AddObject(&spawnObject, spawnOrigin, mat3_identity, &bounds);
  spawnObject.entityNum = entityNumber;
}

idSPObject::~idSPObject() noexcept(false) = default;

idEffectPhysicsBoxFountain::~idEffectPhysicsBoxFountain() noexcept(false) {
  RetireManyBoxes(manyBoxes);
}

idSPObject::idSPObject() {
  id = 0;
  traceModelIndex = -1;
  origin = {0.0f, 0.0f, 0.0f};
  axis = mat3_identity;
  absBounds.b[0][0] = 0x7FFF;
  absBounds.b[0][1] = 0x7FFF;
  absBounds.b[0][2] = 0x7FFF;
  absBounds.b[1][0] = static_cast<short>(0x8000);
  absBounds.b[1][1] = static_cast<short>(0x8000);
  absBounds.b[1][2] = static_cast<short>(0x8000);
  motion.valid = false;
  motion.worldCollisionOnly = false;
  motion.ignore = false;
  motion.translation = {0.0f, 0.0f, 0.0f};
  motion.rotationVec = {0.0f, 0.0f, 0.0f};
  motion.rotationAngle = 0.0f;
  for (auto& side : index) {
    side[0] = 0;
    side[1] = 0;
    side[2] = 0;
  }
  entityNum = 0x1FFF;
  clipMask = 1;
}

idEffectPhysicsBoxFountain::idEffectPhysicsBoxFountain() {
  manyBoxes = nullptr;
  lastFireTime = 0;
  currentFireRigidBody = 0;
  startOrigin = {0.0f, 0.0f, 0.0f};
  spawnOrigin = {0.0f, 0.0f, 0.0f};
}
