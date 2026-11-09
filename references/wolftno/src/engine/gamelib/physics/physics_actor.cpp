#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#include "engine/gamelib/physics/physics_actor.h"

#include "engine/framework/common.h"

#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace {

constexpr int MIN_HISTORY_TIME_DESIRED = 256;
constexpr int MIN_HISTORY_TIME_DESIRED_0 = 256;

int ScaledGameMilliseconds() {
  idGame* const game = common->Game();
  using GetGameMsFunction = int (*)(idGame*, int);
  const auto vtable = *reinterpret_cast<void* const* const*>(game);
  return reinterpret_cast<GetGameMsFunction>(vtable[47])(game, 1);
}

int ClipModelIndex(int id) { return id > 0 ? id : 0; }

bool ClipModelEnabled(const idClipModel* model) {
  return model != nullptr && (model->flags & 1) != 0;
}

idVec3 SampleVelocity(const idVec3& first, const idVec3& second,
                      float scale) {
  return {(second.x - first.x) * scale,
          (second.y - first.y) * scale,
          (second.z - first.z) * scale};
}

} // namespace

int idPhysics_Actor::nextHistoryOffset;
int idPhysics_Actor::nextHistoryOffsetIncr = 51;
const float idPhysics_Actor::SAMPLE_RATE_TO_VELOCITY = 7.8125f;

idPhysics_Actor::idPhysics_Actor() {
  type = PHYSICS_INVALID;
  for (int index = 0; index < 2; ++index) {
    clipModels[index] = nullptr;
    clipMasks[index] = 0;
  }
  SetClipModelAxis();
  mass = 100.0f;
  invMass = 0.01f;
  hasMaster = false;
  masterYaw = 0.0f;
  masterDeltaYaw = 0.0f;
  wasHistoryInitialized = false;
  lastHistorySaveTime = 0;
  for (int index = 0; index < NUM_HISTORIES; ++index) {
    originHistory[index] = vec3_origin;
    yawHistory[index] = 0.0f;
  }
}

idPhysics_Actor::~idPhysics_Actor() {
  for (idClipModel*& model : clipModels) {
    if (model != nullptr) {
      model->Delete();
    }
  }
}

const idMat3& idPhysics_Actor::GetGravityAxis() const {
  return clipModelAxis;
}

float idPhysics_Actor::GetMasterDeltaYaw() const {
  return masterDeltaYaw;
}

void idPhysics_Actor::SetClipModel(idClipModel* model, float, int id,
                                   bool freeOld) {
  const int index = ClipModelIndex(id);
  if (freeOld && clipModels[index] != nullptr &&
      clipModels[index] != model) {
    clipModels[index]->Delete();
  }
  clipModels[index] = model;
  if (model != nullptr) {
    LinkClip();
  }
}

idClipModel* idPhysics_Actor::GetClipModel(int id) const {
  return clipModels[ClipModelIndex(id)];
}

int idPhysics_Actor::GetNumClipModels() const {
  int count = 0;
  for (const idClipModel* model : clipModels) {
    if (model != nullptr) {
      ++count;
    }
  }
  return count;
}

void idPhysics_Actor::SetMass(float newMass, int) {
  mass = newMass;
  invMass = 1.0f / newMass;
}

float idPhysics_Actor::GetMass(int) const { return mass; }

void idPhysics_Actor::SetContents(int contents, int id) {
  clipModels[ClipModelIndex(id)]->SetContents(contents);
}

int idPhysics_Actor::GetContents(int id) const {
  return clipModels[ClipModelIndex(id)]->contents;
}

void idPhysics_Actor::SetClipMask(int mask, int id) {
  if (id > 0) {
    clipMasks[id] = mask;
  } else {
    clipMask = mask;
    clipMasks[0] = mask;
  }
}

int idPhysics_Actor::GetClipMask(int id) const {
  return clipMasks[ClipModelIndex(id)];
}

bool idPhysics_Actor::IsPushable(int) const { return !hasMaster; }

void idPhysics_Actor::UnlinkClip() {
  for (idClipModel* model : clipModels) {
    if (model != nullptr) {
      model->Unlink();
    }
  }
}

void idPhysics_Actor::SetClipModelAxis() {
  if (gravityNormal.z == -1.0f || gravityNormal == vec3_origin) {
    clipModelAxis = mat3_identity;
  } else {
    clipModelAxis[2] = -gravityNormal;
    clipModelAxis[2].NormalVectors(clipModelAxis[0], clipModelAxis[1]);
    clipModelAxis[1] = -clipModelAxis[1];
  }
  LinkClip();
}

const idBounds& idPhysics_Actor::GetBounds(int id) const {
  return clipModels[ClipModelIndex(id)]->bounds;
}

const idBounds& idPhysics_Actor::GetAbsBounds(int id) const {
  return clipModels[ClipModelIndex(id)]->absBounds;
}

const idVec3& idPhysics_Actor::GetOrigin(int id) const {
  return clipModels[ClipModelIndex(id)]->origin;
}

const idMat3& idPhysics_Actor::GetAxis(int id) const {
  return clipModels[ClipModelIndex(id)]->axis;
}

void idPhysics_Actor::SetGravity(const idVec3& newGravity) {
  if (newGravity != gravityVector) {
    idPhysics_DynamicBase::SetGravity(newGravity);
    gravityNormal.FixDegenerateNormal();
    SetClipModelAxis();
  }
}

void idPhysics_Actor::DisableClip(actorClipModel_t modelType) {
  idClipModel* const model = clipModels[modelType];
  if (model != nullptr) {
    model->flags &= ~1u;
  }
}

void idPhysics_Actor::EnableClip(actorClipModel_t modelType) {
  idClipModel* const model = clipModels[modelType];
  if (model != nullptr) {
    model->flags |= 1u;
  }
}

void idPhysics_Actor::DisableClip() {
  idClipModel* const model = clipModels[0];
  if (model != nullptr) {
    model->flags &= ~1u;
  }
}

void idPhysics_Actor::EnableClip() {
  idClipModel* const model = clipModels[0];
  if (model != nullptr) {
    model->flags |= 1u;
  }
}

void idPhysics_Actor::LinkClip() {
  for (idClipModel* model : clipModels) {
    if (model != nullptr) {
      model->Link(entityNumber, physicsId, 0, model->origin, clipModelAxis);
    }
  }
}

void idPhysics_Actor::LinkClip(const idVec3& newOrigin,
                               const idMat3& newAxis) {
  clipModelAxis = newAxis;
  for (idClipModel* model : clipModels) {
    if (model != nullptr) {
      model->Link(entityNumber, physicsId, 0, newOrigin, newAxis);
    }
  }
}

bool idPhysics_Actor::EvaluateContacts() {
  ClearContacts();
  for (int index = 0; index < 2; ++index) {
    if (clipModels[index] != nullptr && clipMasks[index] != 0) {
      AddGroundContacts(clipModels[index], clipMasks[index]);
    }
  }
  AddContactPhysicsForContacts();
  return contacts.num != 0;
}

void idPhysics_Actor::RememberHistorySample() {
  const int currentTime = ScaledGameMilliseconds();
  if (!wasHistoryInitialized) {
    const idVec3 origin = GetOrigin(0);
    for (int index = 0; index < NUM_HISTORIES; ++index) {
      originHistory[index] = origin;
      yawHistory[index] = masterYaw;
    }
    lastHistorySaveTime =
        (currentTime & NOT_HISTORY_SAMPLE_RATE_MASK) + nextHistoryOffset;
    if (lastHistorySaveTime >= currentTime &&
        lastHistorySaveTime > HISTORY_SAMPLE_RATE) {
      lastHistorySaveTime -= HISTORY_SAMPLE_RATE;
    }
    nextHistoryOffset += nextHistoryOffsetIncr;
    if (nextHistoryOffset >= HISTORY_SAMPLE_RATE) {
      nextHistoryOffset -= HISTORY_SAMPLE_RATE;
      nextHistoryOffsetIncr += nextHistoryOffsetIncr / 10;
      if (nextHistoryOffsetIncr >= 118) {
        nextHistoryOffsetIncr /= 3;
      }
    }
    wasHistoryInitialized = true;
    return;
  }

  const int nextSaveTime = lastHistorySaveTime + HISTORY_SAMPLE_RATE;
  if (currentTime < nextSaveTime) {
    return;
  }
  lastHistorySaveTime = nextSaveTime;
  if (nextSaveTime < currentTime - HISTORY_SAMPLE_RATE) {
    for (int index = 0; index < NUM_HISTORIES; ++index) {
      originHistory[index] = GetOrigin(0);
      yawHistory[index] = masterYaw;
    }
    lastHistorySaveTime &= HISTORY_SAMPLE_RATE_MASK;
    lastHistorySaveTime |= currentTime & NOT_HISTORY_SAMPLE_RATE_MASK;
    if (lastHistorySaveTime > currentTime) {
      lastHistorySaveTime -= HISTORY_SAMPLE_RATE;
    }
  } else {
    const int index =
        (nextSaveTime / HISTORY_SAMPLE_RATE) & NUM_HISTORIES_MASK;
    originHistory[index] = GetOrigin(0);
    yawHistory[index] = masterYaw;
  }
}

idVec3 idPhysics_Actor::GetOriginHistory(int timeBack) const {
  if (timeBack <= 0 || !wasHistoryInitialized) {
    return GetOrigin(0);
  }
  if (timeBack > MAX_TIME_BACK) {
    timeBack = MAX_TIME_BACK;
  }
  const int currentTime = ScaledGameMilliseconds();
  const int requestedTime = currentTime - timeBack;
  if (requestedTime <= MIN_HISTORY_TIME_DESIRED) {
    return GetOrigin(0);
  }

  const int saveOffset = lastHistorySaveTime & HISTORY_SAMPLE_RATE_MASK;
  const int lastSaveIndex = lastHistorySaveTime / HISTORY_SAMPLE_RATE;
  idVec3 sample2 = GetOrigin(0);
  int sample1Time = lastHistorySaveTime;
  int sample2Time = currentTime;
  idVec3 sample1;

  if (requestedTime < lastHistorySaveTime) {
    if (requestedTime > lastHistorySaveTime - 2032) {
      const int historyIndex =
          (requestedTime - saveOffset) / HISTORY_SAMPLE_RATE;
      sample1Time = saveOffset + historyIndex * HISTORY_SAMPLE_RATE;
      sample2Time = sample1Time + HISTORY_SAMPLE_RATE;
      sample1 = originHistory[historyIndex & NUM_HISTORIES_MASK];
      sample2 = originHistory[(historyIndex + 1) & NUM_HISTORIES_MASK];
    } else {
      sample1Time = requestedTime;
      sample1 = originHistory[(lastSaveIndex + 2) & NUM_HISTORIES_MASK];
    }
  } else {
    sample1 = originHistory[lastSaveIndex & NUM_HISTORIES_MASK];
  }

  if (sample1Time >= sample2Time || requestedTime >= sample2Time) {
    return sample2;
  }
  if (requestedTime > sample1Time) {
    const float fraction =
        static_cast<float>(requestedTime - sample1Time) /
        static_cast<float>(sample2Time - sample1Time);
    return {((sample2.x - sample1.x) * fraction) + sample1.x,
            ((sample2.y - sample1.y) * fraction) + sample1.y,
            ((sample2.z - sample1.z) * fraction) + sample1.z};
  }
  return sample1;
}

idVec3 idPhysics_Actor::GetVelocityHistory(int timeBack) const {
  if (timeBack <= 0 || !wasHistoryInitialized) {
    return GetLinearVelocity(0);
  }
  if (timeBack > MAX_TIME_BACK) {
    timeBack = MAX_TIME_BACK;
  }
  const int currentTime = ScaledGameMilliseconds();
  const int requestedTime = currentTime - timeBack;
  if (requestedTime <= MIN_HISTORY_TIME_DESIRED_0) {
    return GetLinearVelocity(0);
  }

  int sample1Time = lastHistorySaveTime - HALF_HISTORY_SAMPLE_RATE;
  int historyIndex =
      (lastHistorySaveTime / HISTORY_SAMPLE_RATE) & NUM_HISTORIES_MASK;
  int sample2Time;
  idVec3 sample1Velocity;
  idVec3 sample2Velocity;
  if (requestedTime < sample1Time) {
    const int historyCount =
        (sample1Time - requestedTime + HISTORY_SAMPLE_RATE_MASK) /
        HISTORY_SAMPLE_RATE;
    historyIndex -= historyCount;
    sample1Time -= historyCount * HISTORY_SAMPLE_RATE;
    sample2Time = sample1Time + HISTORY_SAMPLE_RATE;
    const idVec3& previous =
        originHistory[(historyIndex - 1) & NUM_HISTORIES_MASK];
    const idVec3& current =
        originHistory[historyIndex & NUM_HISTORIES_MASK];
    const idVec3& next =
        originHistory[(historyIndex + 1) & NUM_HISTORIES_MASK];
    sample1Velocity =
        SampleVelocity(previous, current, SAMPLE_RATE_TO_VELOCITY);
    sample2Velocity =
        SampleVelocity(current, next, SAMPLE_RATE_TO_VELOCITY);
  } else {
    const idVec3& previous =
        originHistory[(historyIndex - 1) & NUM_HISTORIES_MASK];
    const idVec3& current =
        originHistory[historyIndex & NUM_HISTORIES_MASK];
    sample1Velocity =
        SampleVelocity(previous, current, SAMPLE_RATE_TO_VELOCITY);
    sample2Velocity = GetLinearVelocity(0);
    sample2Time = currentTime;
  }

  const int interval = sample2Time - sample1Time;
  const float fraction =
      interval != 0
          ? static_cast<float>(requestedTime - sample1Time) /
                static_cast<float>(interval)
          : 1.0f;
  return {((sample2Velocity.x - sample1Velocity.x) * fraction) +
              sample1Velocity.x,
          ((sample2Velocity.y - sample1Velocity.y) * fraction) +
              sample1Velocity.y,
          ((sample2Velocity.z - sample1Velocity.z) * fraction) +
              sample1Velocity.z};
}

void idPhysics_Actor::ClipRotation(trace_t& results,
                                   const idRotation& rotation,
                                   const idClipModel* model) const {
  idClipModel* const standing = clipModels[0];
  if (model != nullptr) {
    clip->RotationModel(results, standing->origin, rotation, standing,
                        standing->axis, clipMasks[0], model, model->origin,
                        model->axis, nullptr);
    if (results.fraction > 0.0f) {
      idClipModel* const crouching = clipModels[1];
      if (crouching != nullptr && clipMasks[1] != 0 &&
          ClipModelEnabled(crouching)) {
        alignas(trace_t) unsigned char storage[sizeof(trace_t)];
        trace_t& secondary = *reinterpret_cast<trace_t*>(storage);
        clip->RotationModel(secondary, crouching->origin, rotation,
                            crouching, crouching->axis, clipMasks[1], model,
                            model->origin, model->axis, nullptr);
        if (results.fraction > secondary.fraction) {
          results = secondary;
        }
      }
    }
    return;
  }

  clip->Rotation(&results, standing->origin, rotation, standing,
                 standing->axis, clipMasks[0], entityNumber, false, nullptr);
  if (results.fraction > 0.0f) {
    idClipModel* const crouching = clipModels[1];
    if (crouching != nullptr && clipMasks[1] != 0 &&
        ClipModelEnabled(crouching)) {
      alignas(trace_t) unsigned char storage[sizeof(trace_t)];
      trace_t& secondary = *reinterpret_cast<trace_t*>(storage);
      clip->Rotation(&secondary, crouching->origin, rotation, crouching,
                     crouching->axis, clipMasks[1], entityNumber, false,
                     nullptr);
      if (results.fraction > secondary.fraction) {
        results = secondary;
      }
    }
  }
}

int idPhysics_Actor::ClipContents(const idClipModel* model,
                                  int contentsMask) const {
  const int mask = contentsMask != 0 ? contentsMask : clipMask;
  unsigned int contents = 0;
  for (int index = 0; index < 2; ++index) {
    idClipModel* const actorModel = clipModels[index];
    if (actorModel == nullptr || clipMasks[index] == 0 ||
        !ClipModelEnabled(actorModel)) {
      continue;
    }
    alignas(trace_t) unsigned char storage[sizeof(trace_t)];
    trace_t& trace = *reinterpret_cast<trace_t*>(storage);
    if (model != nullptr) {
      clip->ContentsModel(trace, actorModel->origin, actorModel,
                          actorModel->axis, mask, model, model->origin,
                          model->axis, nullptr);
    } else {
      clip->Contents(&trace, actorModel->origin, actorModel,
                     actorModel->axis, mask, 0x1fff, nullptr);
    }
    contents |= trace.c.contentFlags;
  }
  return static_cast<int>(contents);
}

void idPhysics_Actor::ClipTranslation(trace_t& results,
                                      const idVec3& translation,
                                      const idClipModel* model) const {
  idClipModel* const standing = clipModels[0];
  idVec3 end{standing->origin.x + translation.x,
             standing->origin.y + translation.y,
             standing->origin.z + translation.z};
  if (model != nullptr) {
    clip->TranslationModel(results, standing->origin, end, standing,
                           standing->axis, clipMasks[0], model,
                           model->origin, model->axis, nullptr);
    if (results.fraction > 0.0f) {
      idClipModel* const crouching = clipModels[1];
      if (crouching != nullptr && clipMasks[1] != 0 &&
          ClipModelEnabled(crouching)) {
        end = {standing->origin.x + translation.x,
               standing->origin.y + translation.y,
               standing->origin.z + translation.z};
        alignas(trace_t) unsigned char storage[sizeof(trace_t)];
        trace_t& secondary = *reinterpret_cast<trace_t*>(storage);
        clip->TranslationModel(secondary, standing->origin, end, standing,
                               standing->axis, clipMasks[0], model,
                               model->origin, model->axis, nullptr);
        if (results.fraction > secondary.fraction) {
          results = secondary;
        }
      }
    }
    return;
  }

  clip->Translation(&results, standing->origin, end, standing,
                    standing->axis, clipMasks[0], entityNumber, false,
                    nullptr);
  if (results.fraction > 0.0f) {
    idClipModel* const crouching = clipModels[1];
    if (crouching != nullptr && clipMasks[1] != 0 &&
        ClipModelEnabled(crouching)) {
      end = {standing->origin.x + translation.x,
             standing->origin.y + translation.y,
             standing->origin.z + translation.z};
      alignas(trace_t) unsigned char storage[sizeof(trace_t)];
      trace_t& secondary = *reinterpret_cast<trace_t*>(storage);
      clip->Translation(&secondary, standing->origin, end, crouching,
                        crouching->axis, clipMasks[1], entityNumber, false,
                        nullptr);
      if (results.fraction > secondary.fraction) {
        results = secondary;
      }
    }
  }
}
