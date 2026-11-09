#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_HASHINDEX_INVALID_ARRAY_ABI
#define WOLFTNO_RETAIL_HASHINDEX_MEMBER_ABI
#define WOLFTNO_RETAIL_HASHINDEX_OUTOFLINE_MUTATORS_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOCINDEX_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#include "engine/gamelib/effects/fxmanager_abi.h"

#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/decls/decltable.h"
#include "engine/framework/coderesource.h"
#include "engine/framework/common.h"
#include "engine/gamelib/effects/fxmodelrecycler_abi.h"
#include "engine/gamelib/effects/gamelibeffects.h"
#include "engine/gamelib/effects/tracermanager.h"
#include "engine/gamelib/effects/viewcallbacks_abi.h"
#include "engine/models/particles/declparticle.h"
#include "engine/models/particles/rendermodeleffects.h"
#include "engine/models/particles/rendermodelparticle.h"
#include "engine/models/rendermodeldecal.h"
#include "engine/models/rendermodelgodrays.h"
#include "engine/models/renderlight.h"
#include "engine/models/renderworld.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/treeanimator.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/sound/soundsystem.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/sys/sys_alloc.h"
#pragma warning(pop)

#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOCINDEX_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_OUTOFLINE_MUTATORS_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_MEMBER_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_INVALID_ARRAY_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

idGame* GetGame();
idRenderDebug* GetGameLibRenderDebug();
extern bool forceIdListsToReallocateEveryAppend;
extern idCVar g_EnableGore;
extern idTypeInfoTools* typeInfoTools;

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

template <typename Type>
void idListArrayDelete(void* pointer, int count);

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

idCVar g_skipFXManager("g_skipFXManager", "0", CVAR_INTEGER, "skip fx",
                       static_cast<argCompletionRef_t>(nullptr));
idCVar g_debugFX(
    "g_debugFX", "0", CVAR_INTEGER,
    "1 = fx start/stop debug, 2 = distance debug, 3+ = serialization",
    static_cast<argCompletionRef_t>(nullptr));
idCVar fx_verbose("fx_verbose", "0", CVAR_BOOL,
                  "print verbose warnings for fx",
                  static_cast<argCompletionRef_t>(nullptr));
idCVar g_showFxModelRecyclerStats(
    "g_showFxModelRecyclerStats", "0", CVAR_BOOL, "",
    static_cast<argCompletionRef_t>(nullptr));
idCVar fx_skipRangeCheck("fx_skipRangeCheck", "0", CVAR_BOOL, "",
                         static_cast<argCompletionRef_t>(nullptr));

enum class fxManagerRenderTag_t : int { RenderModel = TAG_RENDERMODEL };

void operator delete(void* memory, fxManagerRenderTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4fxManagerRenderTag_t@@@Z="                  \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 fxManagerRenderTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

namespace {

idCodeResource<idDeclRenderParm> rpParticleVel("particleVel");

constexpr memTag_t TAG_FX_RETAIL = static_cast<memTag_t>(59);

__forceinline float ClampUnitRetail(float value) {
  float result = 1.0f;
  if (value >= 0.0f) {
    if (value <= 1.0f) {
      result = value;
    }
  } else {
    result = 0.0f;
  }
  return result;
}

__forceinline float InverseLengthRetail(float squaredLength) {
  return 1.0f /
         _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
             _mm_set_ss(squaredLength),
             idMath::vector_float_smallestNonDenorm)));
}

__forceinline int* RetailInvalidHashIndex() {
  return idHashIndex::RetailInvalidIndexAddress();
}

__forceinline void ClearHash(idHashIndex& index) {
  if (index.hash != RetailInvalidHashIndex()) {
    std::memset(index.hash, 0xff,
                sizeof(int) * static_cast<std::size_t>(index.hashSize));
  }
}

template <typename Type, int Capacity>
__forceinline void ClearStaticList(idStaticList<Type, Capacity>& values) {
  if (values.listStatic == 0 || values.listStatic == 2) {
    if (values.list != nullptr) {
      idMem::Singleton().Free(values.list, ALIGN_16);
    }
    values.list = nullptr;
    values.size = 0;
  }
  values.num = 0;
}

__forceinline void AddHash(idHashIndex& index, int key, int value) {
  if (index.hash == RetailInvalidHashIndex()) {
    index.InternalAllocate(index.hashSize,
                           std::max(index.indexSize, value + 1));
  } else if (value >= index.indexSize) {
    index.InternalResizeIndex(value + 1);
  }
  const int bucket = key & index.hashMask;
  index.indexChain[value] = index.hash[bucket];
  index.hash[bucket] = value;
}

__forceinline void RemoveValue(idStaticList<int, 256>& values, int value) {
  for (int index = 0; index < values.num; ++index) {
    if (values.list[index] == value) {
      --values.num;
      for (int move = index; move < values.num; ++move) {
        values.list[move] = values.list[move + 1];
      }
      return;
    }
  }
}

__forceinline void RemoveValue(idStaticList<int, 64>& values, int value) {
  for (int index = 0; index < values.num; ++index) {
    if (values.list[index] == value) {
      --values.num;
      for (int move = index; move < values.num; ++move) {
        values.list[move] = values.list[move + 1];
      }
      return;
    }
  }
}

__forceinline void RetireRenderModel(idRenderModel*& model,
                                     bool commitFirst) {
  if (model == nullptr) {
    return;
  }
  if (commitFirst) {
    model->CommitThisFrame();
  }
  using Function = void (*)(idRenderModel*);
  Function* const vtable = *reinterpret_cast<Function**>(model);
  if ((model->unlinked & 8u) == 0u) {
    model->CommitThisFrame();
    vtable[7](model);
    model->unlinked |= 8u;
  }
  vtable[8](model);
  model->deleteOnSync |= 4u;
  model = nullptr;
}

__forceinline void UnlinkRenderModel(idRenderModel* model) {
  if (model == nullptr || (model->unlinked & 8u) != 0u) {
    return;
  }
  model->CommitThisFrame();
  model->UnlinkSubclass();
  model->unlinked |= 8u;
}

__forceinline void SetRenderModelOriginAndAxis(idRenderModel* model,
                                               const idVec3& origin,
                                               const idMat3& axis) {
  if ((model->useDeferredPosition &
       (model->deferredPositionInitialized >> 1u) & 1u) == 0u) {
    model->g.origin = origin;
    model->g.axis = axis;
  }
  model->deferredOrigin = origin;
  model->deferredAxis = axis;
}

__forceinline idVec3 TransformRowVector(const idVec3& value,
                                        const idMat3& axis) {
  return {
      (value.x * axis.mat[0].x + value.y * axis.mat[1].x) +
          value.z * axis.mat[2].x,
      (value.x * axis.mat[0].y + value.y * axis.mat[1].y) +
          value.z * axis.mat[2].y,
      (value.x * axis.mat[0].z + value.y * axis.mat[1].z) +
          value.z * axis.mat[2].z};
}

__forceinline bool AddRenderModel(idRenderWorld* world,
                                  idRenderModel* model) {
  using Function = bool (*)(idRenderWorld*, idRenderModel*);
  void** const vtable = *reinterpret_cast<void***>(world);
  return reinterpret_cast<Function>(vtable[0x80 / sizeof(void*)])(world,
                                                                  model);
}

__forceinline idRenderModel* AllocGameRenderModel(idGame* game,
                                                  const char* name) {
  using Function = idRenderModel* (*)(idGame*, const char*, bool);
  void** const vtable = *reinterpret_cast<void***>(game);
  return reinterpret_cast<Function>(vtable[0x78 / sizeof(void*)])(game, name,
                                                                  true);
}

__forceinline bool GameIsActive(idGame* game) {
  using Function = bool (*)(idGame*, bool);
  void** const vtable = *reinterpret_cast<void***>(game);
  return reinterpret_cast<Function>(vtable[0x30 / sizeof(void*)])(game, true);
}

__forceinline const idList<idViewCallbacks*, 108>&
GameViewCallbacks(idGame* game) {
  using RetailList = idList<idViewCallbacks*, 5>;
  using Function = const RetailList& (*)(idGame*);
  void** const vtable = *reinterpret_cast<void***>(game);
  const RetailList& values =
      reinterpret_cast<Function>(vtable[0x2D8 / sizeof(void*)])(game);
  return reinterpret_cast<const idList<idViewCallbacks*, 108>&>(values);
}

__forceinline unsigned NextRandom(idRandom2& random) {
  random.seed = 1664525u * random.seed + 1013904223u;
  return random.seed;
}

__forceinline float RandomUnit(idRandom2& random) {
  return static_cast<float>((NextRandom(random) >> 10) & 0x7fffu) *
         0.000030518509f;
}

template <typename Type>
__forceinline bool ResizeTrivialList(idList<Type, 108>& values,
                                     int newSize) {
  if (newSize <= 0) {
    if (values.listStatic == 0 || values.listStatic == 2) {
      if (values.list != nullptr) {
        idMem::Singleton().Free(values.list, ALIGN_16);
      }
      values.list = nullptr;
      values.size = 0;
    }
    values.num = 0;
    return true;
  }
  if (newSize == values.size || values.listStatic != 0) {
    return true;
  }
  if (values.size > 0 || newSize == values.granularity) {
    reinterpret_cast<idScenePoint*>(&values)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  values.list = static_cast<Type*>(idListArrayResize<Type>(
      values.list, values.size, newSize,
      static_cast<memTag_t>(values.memTag), false));
  if (values.list == nullptr) {
    return false;
  }
  values.size = newSize;
  if (values.num > newSize) {
    values.num = newSize;
  }
  return true;
}

} // namespace

const idList<idViewCallbacks*, 108> idFXManager::emptyFallBackList;

idPropsCollection* idTreeAnimator::GetPropsCollection() {
  if (decl == nullptr) {
    return nullptr;
  }
  return reinterpret_cast<idPropsCollection*>(
      const_cast<unsigned char*>(
          reinterpret_cast<const unsigned char*>(decl)) +
      0x240);
}

static idMat3 GetExplicitTableAxis(const idFXSingleAction& action,
                                   float fraction, float deltaTime) {
  idMat3 result = mat3_identity;
  if (deltaTime > 0.00000011920929f) {
    const float pitch =
        action.rotateTablePitch != nullptr
            ? action.rotateTablePitch->table->TableLookup(fraction, true)
            : 0.0f;
    const float yaw = action.rotateTableYaw != nullptr
                          ? action.rotateTableYaw->table->TableLookup(fraction,
                                                                     true)
                          : 0.0f;
    const float roll =
        action.rotateTableRoll != nullptr
            ? action.rotateTableRoll->table->TableLookup(fraction, true)
            : 0.0f;
    idAngles speed{deltaTime * pitch, deltaTime * yaw, deltaTime * roll};
    speed.Normalize360();
    result = speed.ToMat3();
  }
  return result;
}

const idList<idViewCallbacks*, 108>& idFXManager::GetViewCallbacks() {
  if (common->Game() == nullptr) {
    return emptyFallBackList;
  }
  idGame* const game = common->Game();
  if (!GameIsActive(game)) {
    return emptyFallBackList;
  }
  return GameViewCallbacks(common->Game());
}

idVec4 idFXManager::GetTableColor(const idFXSingleAction& action,
                                  float fraction) const {
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  float alpha = 1.0f;
  if (action.colorTableRGB != nullptr) {
    red = action.colorTableRGB->table->TableLookup(fraction, true);
    green = red;
    blue = red;
  }
  if (action.colorTableA != nullptr) {
    alpha = action.colorTableA->table->TableLookup(fraction, true);
  }
  return {red, green, blue, alpha};
}

bool idFXManager::IsStopped(int time) const {
  bool result = true;
  for (int index = 0; index < actionState.num; ++index) {
    result &= actionState.list[index].stopTime <= time;
  }
  return result;
}

bool idFXManager::GetWorldSpaceTagVelocity(
    const tagData_t& tag, int gameMillisecondsPerFrame, idVec3& velocity) {
  if (ta == nullptr) {
    return false;
  }

  const idMat3& worldAxis =
      (ta->useDeferredPosition & 1u) != 0 ? ta->deferredAxis : ta->g.axis;
  const float rate = 1.0f / (static_cast<float>(gameMillisecondsPerFrame) *
                             0.001f);
  idVec3 originDelta;
  idMat3 originRotation;
  ta->GetLastOriginDeltas(originRotation, originDelta);
  const float originX =
      ((originDelta.x * rate) * worldAxis.mat[0].x +
       (originDelta.y * rate) * worldAxis.mat[1].x) +
      (originDelta.z * rate) * worldAxis.mat[2].x;
  const float originY =
      ((originDelta.y * rate) * worldAxis.mat[1].y +
       (originDelta.x * rate) * worldAxis.mat[0].y) +
      (originDelta.z * rate) * worldAxis.mat[2].y;
  const float originZ =
      ((originDelta.y * rate) * worldAxis.mat[1].z +
       (originDelta.x * rate) * worldAxis.mat[0].z) +
      (originDelta.z * rate) * worldAxis.mat[2].z;

  const idJointHandle_t jointHandle(tag.parentJoint.value);
  const idJointIndex_t jointIndex = ta->GetJointIndex(jointHandle);
  idVec3 jointDelta;
  idMat3 jointRotation;
  const bool result = ta->GetModelSpaceJointFrameDeltas(
      &jointIndex, 1, &jointDelta, &jointRotation);
  const float jointY =
      (jointDelta.y * rate) * worldAxis.mat[1].y +
      (jointDelta.x * rate) * worldAxis.mat[0].y;
  const float jointZ =
      (jointDelta.y * rate) * worldAxis.mat[1].z +
      (jointDelta.x * rate) * worldAxis.mat[0].z;
  velocity.x = (((jointDelta.x * rate) * worldAxis.mat[0].x +
                 (jointDelta.y * rate) * worldAxis.mat[1].x) +
                (jointDelta.z * rate) * worldAxis.mat[2].x) +
               originX;
  velocity.y = (jointY + (jointDelta.z * rate) * worldAxis.mat[2].y) +
               originY;
  velocity.z = (jointZ + (jointDelta.z * rate) * worldAxis.mat[2].z) +
               originZ;
  return result;
}

const char* idFXManager::GetName() const {
  return ta != nullptr && ta->decl != nullptr ? ta->decl->name.str : "NULL";
}

bool idMat3::IsOrthoNormal(float epsilon) const {
  const float row0 = mat[0].x * mat[0].x + mat[0].y * mat[0].y +
                     mat[0].z * mat[0].z;
  const float row1 = mat[1].x * mat[1].x + mat[1].y * mat[1].y +
                     mat[1].z * mat[1].z;
  const float row2 = mat[2].x * mat[2].x + mat[2].y * mat[2].y +
                     mat[2].z * mat[2].z;
  const float column0 = mat[0].x * mat[0].x + mat[1].x * mat[1].x +
                        mat[2].x * mat[2].x;
  const float column1 = mat[0].y * mat[0].y + mat[1].y * mat[1].y +
                        mat[2].y * mat[2].y;
  const float column2 = mat[0].z * mat[0].z + mat[1].z * mat[1].z +
                        mat[2].z * mat[2].z;
  const float minimum = (1.0f - epsilon) * (1.0f - epsilon);
  const float maximum = (epsilon + 1.0f) * (epsilon + 1.0f);
  return row0 > minimum && row1 > minimum && row2 > minimum &&
         maximum > row0 && maximum > row1 && maximum > row2 &&
         column0 > minimum && column1 > minimum && column2 > minimum &&
         maximum > column0 && maximum > column1 && maximum > column2;
}

void idFXManager::SetFade(float value) {
  if (ta == nullptr || (ta->g.flags3 & 4u) != 0) {
    return;
  }
  ta->g.modelFade = value;
  const auto* const modelFade =
      static_cast<const idDeclRenderParm*>(rp_modelFade.r);
  if (value == 1.0f) {
    ta->g.flags1 &= ~0x20u;
    ta->gameParmBlock.ClearParm(modelFade);
  } else {
    ta->g.flags1 |= 0x20u;
    parmValue_t parmValue{};
    parmValue.value[0] = value;
    parmValue.value[1] = 1.0f;
    ta->gameParmBlock.SetParm(modelFade, parmValue);
  }
}

int idFXManager::StartSound(soundChannel_t channel,
                            const idSoundShader* shader) {
  if (soundInfo.emitter != nullptr) {
    if (channel == SND_CHANNEL_ANY) {
      channel = soundInfo.channel;
    }
    soundShaderParms_t parms;
    soundInfo.emitter->StartSound(channel, shader, &parms);
  }
  return 0;
}

void idFXManager::ApplyFade(const idFXSingleAction& declaration,
                            idFXAction& action, fxActionState_t& state,
                            int currentTime, float fraction) {
  if (declaration.fadeInTime == 0.0f && declaration.fadeOutTime == 0.0f) {
    if (declaration.type == FX_RENDERPARM && ta != nullptr) {
      ta->SetParm(declaration.renderParmParms.declRenderParm,
                  declaration.color);
    }
    return;
  }

  float fade = 1.0f;
  if (declaration.fadeInTime != 0.0f &&
      currentTime <= state.fadeInEndTime) {
    fade = (static_cast<float>(currentTime) -
            static_cast<float>(state.fadeInStartTime)) /
           (static_cast<float>(state.fadeInEndTime) -
            static_cast<float>(state.fadeInStartTime));
  }
  const bool canFadeOut = !declaration.looping || state.forceStop;
  if (declaration.fadeOutTime != 0.0f && canFadeOut &&
      currentTime >= state.fadeOutStartTime) {
    fade *= 1.0f -
            ((static_cast<float>(currentTime) -
              static_cast<float>(state.fadeOutStartTime)) /
             (static_cast<float>(state.stopTime) -
              static_cast<float>(state.fadeOutStartTime)));
  }

  if (action.rModel != nullptr) {
    idVec4 currentColor = action.rModel->gameParmBlock.GetVector(
        static_cast<const idDeclRenderParm*>(rp_Color.r), nullptr);
    currentColor.w = fade;
    action.rModel->SetParm(
        static_cast<const idDeclRenderParm*>(rp_Color.r), currentColor);
    action.rModel->CommitThisFrame();
  }
  if (action.rParticle != nullptr) {
    action.rParticle->SetParm(
        static_cast<const idDeclRenderParm*>(rp_coverage.r), fade);
    action.rParticle->CommitThisFrame();
  }
  if (action.rLight != nullptr) {
    const idVec4 tableColor = GetTableColor(declaration, fraction);
    const float intensity = declaration.lightParms.intensity;
    const idVec4 lightColor{
        declaration.color.x * tableColor.x * systemColor.x * intensity * fade,
        declaration.color.y * tableColor.y * systemColor.y * intensity * fade,
        declaration.color.z * tableColor.z * systemColor.z * intensity * fade,
        declaration.color.w * tableColor.w * systemColor.w * intensity * fade};
    action.rLight->SetParm(
        static_cast<const idDeclRenderParm*>(rp_lightColor.r), lightColor);
    action.rLight->CommitThisFrame();
  }
  if (action.rGodrays != nullptr) {
    action.rGodrays->fadeVal = ClampUnitRetail(fade);
    action.rGodrays->CommitThisFrame();
  }
  if (declaration.type == FX_SOUND &&
      declaration.soundParms.sound != nullptr && canFadeOut &&
      currentTime >= state.fadeOutStartTime && soundInfo.emitter != nullptr) {
    soundChannel_t channel = declaration.soundParms.channel;
    if (channel == SND_CHANNEL_ANY) {
      channel = soundInfo.channel;
    }
    const float over =
        static_cast<float>(state.stopTime - state.fadeOutStartTime) * 0.001f;
    soundInfo.emitter->FadeSound(channel, -60.0f, over, 0.0f, false);
  }
  if (declaration.type == FX_FLARE) {
    gameLibEffects->lensFlareManager->SetFlareFade(action.flareHandle, fade);
  }
  if (declaration.type == FX_RENDERPARM) {
    idVec4 currentColor = action.renderParmStartValue;
    currentColor.x =
        (declaration.color.x - currentColor.x) * fade + currentColor.x;
    currentColor.y =
        (declaration.color.y - currentColor.y) * fade + currentColor.y;
    currentColor.z =
        (declaration.color.z - currentColor.z) * fade + currentColor.z;
    currentColor.w =
        (declaration.color.w - currentColor.w) * fade + currentColor.w;
    if (ta != nullptr) {
      ta->SetParm(declaration.renderParmParms.declRenderParm, currentColor);
    }
  }
  if (declaration.type == FX_FADE_PARENT) {
    SetFade(fade);
  }
}

void idFXManager::StopAction(int index, int currentTime, const bool instant,
                             bool recycleResources) {
  idFXAction& action = actions.list[index];
  const idFXSingleAction& declaration = fxDecl->events.list[index];
  fxActionState_t& state = actionState.list[index];
  const idList<idViewCallbacks*, 108>& callbacks = GetViewCallbacks();
  if (state.startTime < 0) {
    return;
  }

  if (declaration.fadeOutTime > 0.0f && declaration.type != FX_SOUND &&
      !instant && !state.forceStop &&
      (state.stopTime - currentTime > 0 || declaration.looping)) {
    if (currentTime < state.fadeOutStartTime) {
      state.fadeOutStartTime = currentTime;
      state.stopTime = currentTime -
                       static_cast<int>(declaration.fadeOutTime * -1000.0f);
    }
    state.forceStop = true;
    return;
  }

  switch (declaration.type) {
  case FX_LIGHT:
    if (action.rLight != nullptr) {
      action.rLight->Unlink();
    }
    break;

  case FX_PARTICLE:
    if (action.rParticle != nullptr) {
      if (declaration.particleParms.isScreenPrt) {
        for (int callbackIndex = 0; callbackIndex < callbacks.num;
             ++callbackIndex) {
          idViewCallbacks* const callback = callbacks.list[callbackIndex];
          if (action.viewCallbacksID < 0 ||
              action.viewCallbacksID == callback->GetViewID()) {
            callback->RemoveScreenParticle(
                action.screenPrtHandle,
                static_cast<int>(declaration.fadeOutTime * 1000.0f));
            action.screenPrtHandle = -1;
            break;
          }
        }
      }
      UnlinkRenderModel(action.rParticle);
      if (recycleResources) {
        gameLibEffects->fxModelRecycler.RecycleParticleFxModel(
            declaration.particleParms.declPrt, action.rParticle);
        action.rParticle = nullptr;
      }
    }
    break;

  case FX_DECAL:
    if (action.decalHandle.decalIndex != -1) {
      gameLibEffects->decalModel->RemoveDecal(action.decalHandle);
      action.decalHandle.decalIndex = -1;
      action.decalHandle.spawnIndex = -1;
    }
    break;

  case FX_DECAL2:
    if (action.rModel != nullptr) {
      RetireRenderModel(action.rModel, true);
    }
    break;

  case FX_MODEL:
    if (action.rModel != nullptr) {
      UnlinkRenderModel(action.rModel);
      if (recycleResources) {
        gameLibEffects->fxModelRecycler.RecycleStaticFxModel(
            declaration.modelParms.modelName, action.rModel);
        action.rModel = nullptr;
      }
    }
    break;

  case FX_SOUND:
    if (declaration.soundParms.sound != nullptr &&
        (static_cast<int>(
             declaration.soundParms.sound->parms.soundShaderFlags) &
         0x400800) != 0) {
      soundChannel_t channel = declaration.soundParms.channel;
      if (channel == SND_CHANNEL_ANY) {
        channel = soundInfo.channel;
      }
      if (!state.forceStop && declaration.fadeOutTime > 0.0f) {
        if (soundInfo.emitter != nullptr) {
          soundInfo.emitter->FadeSound(channel, -60.0f,
                                       declaration.fadeOutTime, 0.0f, false);
        }
        if (currentTime < state.fadeOutStartTime) {
          state.fadeOutStartTime = currentTime;
          state.stopTime = currentTime - static_cast<int>(
                                            declaration.fadeOutTime *
                                            -1000.0f);
        }
        state.forceStop = true;
        return;
      }
      if (soundInfo.emitter != nullptr) {
        soundInfo.emitter->StopSound(channel);
      }
    }
    break;

  case FX_SCREEN_SHAKE:
    for (int callbackIndex = 0; callbackIndex < callbacks.num;
         ++callbackIndex) {
      idViewCallbacks* const callback = callbacks.list[callbackIndex];
      if (action.viewCallbacksID < 0 ||
          action.viewCallbacksID == callback->GetViewID()) {
        callback->ApplyScreenShake(0.0f, 1.0f, ang_zero, vec3_origin);
      }
    }
    break;

  case FX_ENV_OVERRIDE:
    for (int callbackIndex = 0; callbackIndex < callbacks.num;
         ++callbackIndex) {
      idViewCallbacks* const callback = callbacks.list[callbackIndex];
      if (action.viewCallbacksID < 0 ||
          action.viewCallbacksID == callback->GetViewID()) {
        callback->ApplyDynEnvOverride(nullptr, 0.0f);
      }
    }
    break;

  case FX_ENV_CHANGE:
    for (int callbackIndex = 0; callbackIndex < callbacks.num;
         ++callbackIndex) {
      idViewCallbacks* const callback = callbacks.list[callbackIndex];
      if (action.viewCallbacksID < 0 ||
          action.viewCallbacksID == callback->GetViewID()) {
        for (int parmIndex = 0;
             parmIndex < declaration.envParms.envRenderParm.num;
             ++parmIndex) {
          callback->ApplyDynEnvParmOverride(
              declaration.envParms.envRenderParm.list[parmIndex].parm,
              idVec4{}, true, declaration.fadeOutTime);
        }
      }
    }
    break;

  case FX_FLARE:
    gameLibEffects->lensFlareManager->StopFlare(action.flareHandle);
    break;

  case FX_RIBBON: {
    const int remaining = action.ribbonManager.GetRemainingTime(currentTime);
    action.ribbonManager.StopRibbon();
    if (!state.forceStop) {
      state.forceStop = true;
      state.stopTime = currentTime + std::max(500, remaining);
      return;
    }
    break;
  }

  case FX_GODRAY:
    if (action.rGodrays != nullptr) {
      UnlinkRenderModel(action.rGodrays);
      idRenderModel* model = action.rGodrays;
      RetireRenderModel(model, true);
      action.rGodrays = nullptr;
    }
    break;

  default:
    break;
  }

  if (declaration.triggered) {
    state.hidden = true;
  }
  state.started = false;
  state.startTime = -1;
  state.stopTime = 0;
  state.fadeInStartTime = -1;
  state.fadeInEndTime = 0;
  state.fadeOutStartTime = 0;
  state.forceStop = false;
  action.lastParticleDropPos = vec3_origin;
  action.randomAngles = ang_zero;
  action.startAxis = mat3_identity;
  action.startOrg = vec3_origin;
}

template <>
__declspec(noinline) bool idList<fxActionState_t, 108>::Resize(int newSize) {
  return ResizeTrivialList(*this, newSize);
}

template <>
__declspec(noinline) bool
idList<idFXModelRecycler::fxStaticModel_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        idMem::Singleton().Free(list, ALIGN_16);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  list = static_cast<idFXModelRecycler::fxStaticModel_t*>(
      idListArrayResize<idFXModelRecycler::fxStaticModel_t>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
__declspec(noinline) int
idList<idFXModelRecycler::fxPrtModel_t, 5>::AllocIndex() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = required + granularity - 1 -
                (required + granularity - 1) % granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  const int result = num;
  if (result >= size) {
    return size - 1;
  }
  ++num;
  return result;
}

template <>
__declspec(noinline) int
idList<idFXModelRecycler::fxStaticModel_t, 5>::AllocIndex() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = required + granularity - 1 -
                (required + granularity - 1) % granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  const int result = num;
  if (result >= size) {
    return size - 1;
  }
  ++num;
  return result;
}

template <>
__declspec(noinline) bool idList<tagData_t, 108>::Resize(int newSize) {
  return ResizeTrivialList(*this, newSize);
}

template <>
__declspec(noinline) int
idList<tagData_t, 108>::Append(const tagData_t& value) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize)) {
      return -1;
    }
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity - (size + granularity) % granularity;
    } else if (size <= 0x3fffffff) {
      newSize = size * 2;
      if (newSize == 0) {
        newSize = 1;
      }
    } else {
      newSize = 0x7fffffff;
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  if (num >= size) {
    return num - 1;
  }
  list[num].trans = value.trans;
  list[num].rot = value.rot;
  list[num].parentJoint = value.parentJoint;
  return num++;
}

template <>
__declspec(noinline) bool idList<idFXAction, 108>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        idListArrayDelete<idFXAction>(list, size);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }

  idFXAction* const oldList = list;
  const int oldSize = size;
  auto* replacement = static_cast<idFXAction*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
          sizeof(idFXAction) * static_cast<std::size_t>(newSize),
          static_cast<memTag_t>(memTag), false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (replacement != nullptr) {
    for (int index = 0; index < newSize; ++index) {
      ::new (static_cast<void*>(&replacement[index])) idFXAction;
    }
    idListArrayDelete<idFXAction>(oldList, oldSize);
  }
  list = replacement;
  if (replacement == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
idList<idFXAction, 108>::~idList() {
  if (listStatic == 0 || listStatic == 2) {
    if (list != nullptr) {
      idListArrayDelete<idFXAction>(list, size);
    }
    list = nullptr;
    size = 0;
  }
  num = 0;
}

idFXModelRecycler::idFXModelRecycler() { Init(); }

idFXModelRecycler::~idFXModelRecycler() { Shutdown(); }

void idFXModelRecycler::Init() {
  ClearStaticList(fxPrtModels);
  ClearStaticList(fxInactivePrtModels);
  ClearHash(activePrtModelHash);
  activePrtModelHash.memTag = TAG_FX_RETAIL;
  ClearHash(inactivePrtModelHash);
  inactivePrtModelHash.memTag = TAG_FX_RETAIL;
  ClearStaticList(fxStaticModels);
  ClearStaticList(fxInactiveStaticModels);
  ClearHash(activeStaticModelHash);
  activeStaticModelHash.memTag = TAG_FX_RETAIL;
  ClearHash(inactiveStaticModelHash);
  inactiveStaticModelHash.memTag = TAG_FX_RETAIL;
}

void idFXModelRecycler::Shutdown() {
  ClearHash(activePrtModelHash);
  ClearHash(inactivePrtModelHash);
  ClearStaticList(fxInactivePrtModels);
  for (int index = 0; index < fxPrtModels.num; ++index) {
    idRenderModel* model = fxPrtModels.list[index].pmodel;
    RetireRenderModel(model, true);
    fxPrtModels.list[index].pmodel =
        static_cast<idRenderModelParticle*>(model);
  }
  ClearStaticList(fxPrtModels);
  ClearHash(activeStaticModelHash);
  ClearHash(inactiveStaticModelHash);
  ClearStaticList(fxInactiveStaticModels);
  for (int index = 0; index < fxStaticModels.num; ++index) {
    RetireRenderModel(fxStaticModels.list[index].rmodel, true);
  }
  ClearStaticList(fxStaticModels);
}

void idFXModelRecycler::RecycleParticleFxModel(
    const idDeclParticle* particle, idRenderModelParticle* model) {
  if (model == nullptr || particle == nullptr) {
    idLib::Warning(
        "idFXModelRecycler::RecycleParticleFxModel : invalid model");
    return;
  }
  const int hash = idStr::Hash(particle->name.str);
  const int activeKey = hash & activePrtModelHash.hashMask;
  int index = activePrtModelHash.hash[
      activeKey & activePrtModelHash.lookupMask & activePrtModelHash.hashMask];
  while (index != -1) {
    if (fxPrtModels.list[index].pDecl == particle &&
        fxPrtModels.list[index].pmodel == model) {
      activePrtModelHash.Remove(activeKey, index);
      inactivePrtModelHash.Add(hash & inactivePrtModelHash.hashMask, index);
      fxInactivePrtModels.Append(index);
      break;
    }
    index = activePrtModelHash.indexChain[
        index & activePrtModelHash.lookupMask];
  }
  if (g_showFxModelRecyclerStats.valueInteger != 0) {
    idLib::Printf(
        "Recycling model %s, total models allocated %d inactive models "
        "spread %d active models spread %d\n",
        particle->name.str, fxPrtModels.num,
        inactivePrtModelHash.GetSpread(), activePrtModelHash.GetSpread());
  }
}

void idFXModelRecycler::RecycleStaticFxModel(
    const idAtomicString& name, idRenderModel* model) {
  if (model == nullptr) {
    idLib::Warning("idFXModelRecycler::RecycleStaticFxModel : invalid model");
    return;
  }
  const int hash = idStr::Hash(name.str);
  const int activeKey = hash & activeStaticModelHash.hashMask;
  int index = activeStaticModelHash.hash[
      activeKey & activeStaticModelHash.lookupMask &
      activeStaticModelHash.hashMask];
  while (index != -1) {
    if (idStr::Cmp(fxStaticModels.list[index].modelName.str, name.str) == 0 &&
        fxStaticModels.list[index].rmodel == model) {
      activeStaticModelHash.Remove(activeKey, index);
      AddHash(inactiveStaticModelHash, hash, index);
      fxInactiveStaticModels.Append(index);
      break;
    }
    index = activeStaticModelHash.indexChain[
        index & activeStaticModelHash.lookupMask];
  }
  if (g_showFxModelRecyclerStats.valueInteger != 0) {
    idLib::Printf(
        "Recycling model %s, total models allocated %d inactive models "
        "spread %d active models spread %d\n",
        name.str, fxStaticModels.num, inactiveStaticModelHash.GetSpread(),
        activeStaticModelHash.GetSpread());
  }
}

idRenderModelParticle* idFXModelRecycler::GetParticleFxModel(
    const idDeclParticle* particle, idRenderWorld* renderWorld) {
  const int hash = idStr::Hash(particle->name.str);
  const int inactiveKey = hash & inactivePrtModelHash.hashMask;
  int index = inactivePrtModelHash.hash[
      inactiveKey & inactivePrtModelHash.lookupMask &
      inactivePrtModelHash.hashMask];
  while (index != -1 &&
         (fxPrtModels.list[index].pDecl != particle ||
          fxPrtModels.list[index].pmodel == nullptr)) {
    index = inactivePrtModelHash.indexChain[
        index & inactivePrtModelHash.lookupMask];
  }
  if (index != -1) {
    RemoveValue(fxInactivePrtModels, index);
    inactivePrtModelHash.Remove(inactiveKey, index);
    AddHash(activePrtModelHash, hash, index);
    idRenderModelParticle*& model = fxPrtModels.list[index].pmodel;
    if (model != nullptr && model->stageCounts[0].num != particle->stages.num) {
      idRenderModel* retired = model;
      RetireRenderModel(retired, true);
      model = nullptr;
      model = new (fxManagerRenderTag_t::RenderModel)
          idRenderModelParticle(particle);
      AddRenderModel(renderWorld, model);
      if (model == nullptr) {
        idLib::Warning(
            "idFXModelRecycler::GetParticleFxModel: Could not allocate a "
            "rendermodel for '%s'",
            particle->name.str);
        return nullptr;
      }
      fxPrtModels.list[index].pDecl = particle;
    }
    if (g_showFxModelRecyclerStats.valueInteger != 0) {
      idLib::Printf(
          "Found model %s in inactive list, total models allocated %d "
          "inactive models spread %d active models spread %d\n",
          particle->name.str, fxPrtModels.num,
          inactivePrtModelHash.GetSpread(), activePrtModelHash.GetSpread());
    }
    return model;
  }

  int oldInactiveKey = 0;
  if (fxPrtModels.num >= fxPrtModels.size) {
    if (fxInactivePrtModels.num <= 0) {
      idLib::Warning(
          "idFXModelRecycler::GetParticleFxModel: Could not allocate model "
          "%s, out of slots",
          particle->name.str);
      return nullptr;
    }
    index = fxInactivePrtModels.list[0];
    oldInactiveKey =
        idStr::Hash(fxPrtModels.list[index].pDecl->name.str) &
        inactivePrtModelHash.hashMask;
    idRenderModel* retired = fxPrtModels.list[index].pmodel;
    RetireRenderModel(retired, true);
    fxPrtModels.list[index].pmodel = nullptr;
  } else {
    index = -1;
  }

  idRenderModelParticle* const model =
      new (fxManagerRenderTag_t::RenderModel) idRenderModelParticle(particle);
  AddRenderModel(renderWorld, model);
  if (model == nullptr) {
    idLib::Warning(
        "idFXModelRecycler::GetParticleFxModel: Could not allocate a "
        "rendermodel for '%s'",
        particle->name.str);
    return nullptr;
  }
  if (index == -1) {
    index = fxPrtModels.AllocIndex();
  } else {
    RemoveValue(fxInactivePrtModels, index);
    inactivePrtModelHash.Remove(oldInactiveKey, index);
  }
  fxPrtModels.list[index].pmodel = model;
  fxPrtModels.list[index].pDecl = particle;
  activePrtModelHash.Add(hash & activePrtModelHash.hashMask, index);
  if (g_showFxModelRecyclerStats.valueInteger != 0) {
    idLib::Printf(
        "Allocated new model %s for active list, total models allocated %d "
        "inactive models spread %d active models spread %d\n",
        particle->name.str, fxPrtModels.num,
        inactivePrtModelHash.GetSpread(), activePrtModelHash.GetSpread());
  }
  return fxPrtModels.list[index].pmodel;
}

idRenderModel* idFXModelRecycler::GetStaticFxModel(
    const idAtomicString& name) {
  const int hash = idStr::Hash(name.str);
  const int inactiveKey = hash & inactiveStaticModelHash.hashMask;
  int index = inactiveStaticModelHash.hash[
      inactiveKey & inactiveStaticModelHash.lookupMask &
      inactiveStaticModelHash.hashMask];
  while (index != -1 &&
         (idStr::Cmp(fxStaticModels.list[index].modelName.str, name.str) != 0 ||
          fxStaticModels.list[index].rmodel == nullptr)) {
    index = inactiveStaticModelHash.indexChain[
        index & inactiveStaticModelHash.lookupMask];
  }
  if (index != -1) {
    RemoveValue(fxInactiveStaticModels, index);
    inactiveStaticModelHash.Remove(inactiveKey, index);
    AddHash(activeStaticModelHash, hash, index);
    if (g_showFxModelRecyclerStats.valueInteger != 0) {
      idLib::Printf(
          "Found model %s in inactive list, total models allocated %d "
          "inactive models spread %d active models spread %d\n",
          name.str, fxStaticModels.num, inactiveStaticModelHash.GetSpread(),
          activeStaticModelHash.GetSpread());
    }
    return fxStaticModels.list[index].rmodel;
  }

  int oldInactiveKey = 0;
  if (fxStaticModels.num >= fxStaticModels.size) {
    if (fxInactiveStaticModels.num <= 0) {
      idLib::Warning(
          "idFXModelRecycler::GetStaticFxModel: Could not allocate model %s, "
          "out of slots",
          name.str);
      return nullptr;
    }
    index = fxInactiveStaticModels.list[0];
    oldInactiveKey =
        idStr::Hash(fxStaticModels.list[index].modelName.str) &
        inactivePrtModelHash.hashMask;
    RetireRenderModel(fxStaticModels.list[index].rmodel, true);
  } else {
    index = -1;
  }

  idRenderModel* model = nullptr;
  if (GetGame() != nullptr) {
    model = AllocGameRenderModel(GetGame(), name.str);
  }
  if (model == nullptr) {
    idLib::Warning(
        "idFXModelRecycler::GetStaticFxModel: Could not allocate a "
        "rendermodel for '%s'",
        name.str);
    return nullptr;
  }
  if (index == -1) {
    index = fxStaticModels.AllocIndex();
  } else {
    RemoveValue(fxInactiveStaticModels, index);
    inactiveStaticModelHash.Remove(oldInactiveKey, index);
  }
  fxStaticModels.list[index].rmodel = model;
  fxStaticModels.list[index].modelName.Set(name.str);
  activeStaticModelHash.Add(hash & activeStaticModelHash.hashMask, index);
  if (g_showFxModelRecyclerStats.valueInteger != 0) {
    idLib::Printf(
        "Allocated new model %s for active list, total models allocated %d "
        "inactive models spread %d active models spread %d\n",
        name.str, fxStaticModels.num, inactiveStaticModelHash.GetSpread(),
        activeStaticModelHash.GetSpread());
  }
  return fxStaticModels.list[index].rmodel;
}

int idFXManager::StartActions(const idVec3& origin, const idMat3& axis,
                              int currentTime, fxCondition_t condition,
                              fxExtraCondition_t extraCondition,
                              int explicitTagIndex) {
  if (fxDecl == nullptr) {
    idLib::Warning("idFXManager::StartActions - fxDecl is NULL!");
    return 0;
  }
  if (actions.num == 0 || actions.num != fxDecl->events.num) {
    return 0;
  }

  int startedCount = 0;
  for (int index = 0; index < fxDecl->events.num; ++index) {
    const idFXSingleAction& declaration = fxDecl->events.list[index];
    bool conditionMatches = false;
    for (int conditionIndex = 0;
         conditionIndex < declaration.startCondition.num;
         ++conditionIndex) {
      if (declaration.startCondition.list[conditionIndex] == condition) {
        conditionMatches = true;
        break;
      }
    }

    fxExtraCondition_t combined = FX_EXTRA_COND_NONE;
    if (declaration.extraCondition.num > 0) {
      if (declaration.extraCondition.list[0] == FX_EXTRA_COND_ANY) {
        combined = extraCondition;
      } else {
        for (int extraIndex = 0;
             extraIndex < declaration.extraCondition.num; ++extraIndex) {
          combined = static_cast<fxExtraCondition_t>(
              combined | declaration.extraCondition.list[extraIndex]);
        }
      }
    }
    if (!conditionMatches || combined != extraCondition) {
      continue;
    }

    if (g_debugFX.valueInteger == 1) {
      idLib::Printf(
          "Starting FX on %s, event %d, action type %s, start condition %s, "
          "owner %s\n",
          fxDecl->name.str, index,
          idDeclFX::FXActionTypeToName(declaration.type),
          idDeclFX::FXConditionToName(condition), owner.c_str());
    }
    StartAction(index, origin, axis, currentTime, explicitTagIndex);
    ++startedCount;
  }
  return startedCount;
}

void idFXManager::StopActions(int currentTime, fxCondition_t condition,
                              fxExtraCondition_t extraCondition, bool stopAll,
                              bool instant) {
  if (actions.num == 0 || fxDecl == nullptr ||
      actions.num != fxDecl->events.num) {
    return;
  }
  for (int index = 0; index < fxDecl->events.num; ++index) {
    const idFXSingleAction& declaration = fxDecl->events.list[index];
    bool conditionMatches = false;
    for (int conditionIndex = 0;
         conditionIndex < declaration.stopCondition.num; ++conditionIndex) {
      if (declaration.stopCondition.list[conditionIndex] == condition) {
        conditionMatches = true;
        break;
      }
    }
    fxExtraCondition_t combined = FX_EXTRA_COND_NONE;
    for (int extraIndex = 0; extraIndex < declaration.extraCondition.num;
         ++extraIndex) {
      combined = static_cast<fxExtraCondition_t>(
          combined | declaration.extraCondition.list[extraIndex]);
    }
    if (!stopAll && (!conditionMatches || combined != extraCondition)) {
      continue;
    }
    if (g_debugFX.valueInteger == 1 &&
        actionState.list[index].startTime > 0) {
      idLib::Printf(
          "Stopping FX on %s, event %d, action type %s, start condition %s, "
          "owner %s\n",
          fxDecl->name.str, index,
          idDeclFX::FXActionTypeToName(declaration.type),
          idDeclFX::FXConditionToName(condition), owner.c_str());
    }
    StopAction(index, currentTime, instant, true);
  }
}

void idFXManager::LocalStartFX(const idVec3& origin, const idMat3& axis,
                               int currentTime, fxCondition_t condition) {
  if (g_skipFXManager.valueInteger == 0 && initialized) {
    StartActions(origin, axis, currentTime, condition, FX_EXTRA_COND_NONE, -1);
  }
}

void idFXManager::LocalStopFX(int currentTime, fxCondition_t condition,
                              bool instant) {
  if (g_skipFXManager.valueInteger == 0 && initialized) {
    StopActions(currentTime, condition, FX_EXTRA_COND_NONE, false, instant);
  }
}

void idFXManager::LocalStopAllFX(int currentTime, bool instant) {
  if (g_skipFXManager.valueInteger == 0 && initialized) {
    StopActions(currentTime, FX_NONE, FX_EXTRA_COND_NONE, true, instant);
  }
}

void idFXManager::StopAllFX(int currentTime, bool instant) {
  if (g_skipFXManager.valueInteger != 0 || !initialized) {
    return;
  }
  if (common->IsServer()) {
    fxActionCall_t& call = actionBuffer[actionBufferPos % 8];
    call.condition = FX_NONE;
    call.extraCondition = FX_EXTRA_COND_NONE;
    call.time = currentTime;
    call.actionType = FXACTION_STOP_ALL;
    call.immediate = instant;
    StopActions(currentTime, FX_NONE, FX_EXTRA_COND_NONE, true, instant);
    ++actionBufferPos;
  } else {
    StopActions(currentTime, FX_NONE, FX_EXTRA_COND_NONE, true, instant);
  }
}

void idFXManager::StopFX(int currentTime, fxCondition_t condition,
                         bool instant) {
  if (g_skipFXManager.valueInteger != 0 || !initialized) {
    return;
  }
  if (common->IsServer()) {
    fxActionCall_t& call = actionBuffer[actionBufferPos % 8];
    call.time = currentTime;
    call.condition = condition;
    call.extraCondition = FX_EXTRA_COND_NONE;
    call.actionType = FXACTION_STOP;
    call.immediate = instant;
    StopActions(currentTime, condition, FX_EXTRA_COND_NONE, false, instant);
    ++actionBufferPos;
  } else {
    StopActions(currentTime, condition, FX_EXTRA_COND_NONE, false, instant);
  }
}

void idFXManager::StopFX(int currentTime, fxCondition_t condition,
                         fxExtraCondition_t extraCondition, bool instant) {
  if (g_skipFXManager.valueInteger != 0 || !initialized) {
    return;
  }
  if (common->IsServer()) {
    fxActionCall_t& call = actionBuffer[actionBufferPos % 8];
    call.time = currentTime;
    call.condition = condition;
    call.extraCondition = extraCondition;
    call.actionType = FXACTION_STOP;
    call.immediate = instant;
    StopActions(currentTime, condition, extraCondition, false, instant);
    ++actionBufferPos;
  } else {
    StopActions(currentTime, condition, extraCondition, false, instant);
  }
}

void idFXManager::StartAction(int index, const idVec3& origin,
                              const idMat3& parentAxis, int currentTime,
                              int explicitTagIndex) {
  const idFXSingleAction& declaration = fxDecl->events.list[index];
  idFXAction& action = actions.list[index];
  fxActionState_t& state = actionState.list[index];
  if ((state.startTime >= 0 && declaration.looping) ||
      (declaration.filterHighViolence && g_EnableGore.valueInteger == 0)) {
    return;
  }

  const idList<idViewCallbacks*, 108>& callbacks = GetViewCallbacks();
  if (declaration.particleParms.isScreenPrt &&
      action.screenPrtHandle >= 0 && action.rParticle != nullptr) {
    for (int callbackIndex = 0; callbackIndex < callbacks.num;
         ++callbackIndex) {
      idViewCallbacks* const callback = callbacks.list[callbackIndex];
      if (action.viewCallbacksID < 0 ||
          action.viewCallbacksID == callback->GetViewID()) {
        callback->RemoveScreenParticle(action.screenPrtHandle, 0);
        break;
      }
    }
  }

  state.startTime = currentTime;
  state.fadeInStartTime = currentTime;
  state.stopTime = currentTime + state.startDelay -
                   static_cast<int>(declaration.duration * -1000.0f);
  state.fadeInEndTime = currentTime + state.startDelay -
                        static_cast<int>(declaration.fadeInTime * -1000.0f);
  state.shouldTrigger = true;
  state.forceStop = false;
  state.started = false;
  state.fadeOutStartTime =
      currentTime + state.startDelay - static_cast<int>(
                                           (declaration.duration -
                                            declaration.fadeOutTime) *
                                           -1000.0f);
  action.lastParticleDropPos = origin;
  action.viewCallbacksID = viewCallbacksID;
  action.randomAngles.pitch =
      RandomUnit(random) * (declaration.rndRotY.y - declaration.rndRotY.x) +
      declaration.rndRotY.x;
  action.randomAngles.yaw =
      RandomUnit(random) * (declaration.rndRotZ.y - declaration.rndRotZ.x) +
      declaration.rndRotZ.x;
  action.randomAngles.roll =
      RandomUnit(random) * (declaration.rndRotX.y - declaration.rndRotX.x) +
      declaration.rndRotX.x;
  if (declaration.implicit && state.hidden) {
    state.startTime = -1;
    return;
  }

  action.tagIndex = 0;
  if (declaration.multiTagUseType == FX_MULTI_TAG_USE_EXPLICIT &&
      explicitTagIndex >= 0 && explicitTagIndex < action.tagData.num) {
    action.tagIndex = explicitTagIndex;
  } else if (declaration.multiTagUseType == FX_MULTI_TAG_USE_RND &&
             action.tagData.num > 1) {
    action.tagIndex = static_cast<int>(
        ((NextRandom(random) >> 10) & 0x7fffu) %
        static_cast<unsigned>(action.tagData.num));
  }

  idVec3 tagOrigin = origin;
  idMat3 tagAxis = parentAxis;
  if (action.tagData.num > 0) {
    const tagData_t tag = action.tagData.list[action.tagIndex];
    if (ta != nullptr) {
      ta->GetWorldSpaceTagTransform(tag, tagOrigin, tagAxis);
    }
  }

  idMat3 currentAxis = mat3_identity;
  switch (declaration.rotationType) {
  case FX_ROT_START_AXIS:
  case FX_ROT_TRACK_AXIS:
  case FX_ROT_TRACK_LOCAL_AXIS:
    currentAxis = tagAxis;
    break;
  case FX_ROT_START_AXIS_PARENT:
  case FX_ROT_TRACK_AXIS_PARENT:
    currentAxis = parentAxis;
    break;
  case FX_ROT_EXPLICIT_ANGLES:
    currentAxis = declaration.axis;
    break;
  case FX_ROT_EXPLICIT_TABLES:
    currentAxis = GetExplicitTableAxis(declaration, 0.0f, 0.0f);
    break;
  case FX_ROT_EXPLICIT_TABLES_LOCAL:
    currentAxis = GetExplicitTableAxis(declaration, 0.0f, 0.0f) * tagAxis;
    break;
  case FX_ROT_EXTERNALROT:
    currentAxis = externalRotation;
    break;
  default:
    break;
  }
  currentAxis =
      (action.randomAngles.ToMat3() * declaration.rotOffset) * currentAxis;
  if (!currentAxis.IsOrthoNormal(0.1f)) {
    return;
  }

  idVec3 currentOrigin = declaration.originType == FX_ORG_EXTERNALPOS
                             ? externalPosition
                             : tagOrigin;
  currentOrigin += TransformRowVector(declaration.offset, currentAxis);
  action.startAxis = currentAxis;
  action.startOrg = currentOrigin;

  switch (declaration.type) {
  case FX_LIGHT: {
    if (action.rLight == nullptr && rw != nullptr) {
      action.rLight = rw->AllocRenderLight(true);
      if (action.rLight != nullptr) {
        action.rLight->g.lightType = LIGHT_POINT;
        action.rLight->g.lightClass = LIGHT_BLENDED_ONLY;
        action.rLight->g.lightRadius = declaration.lightParms.radius;
        action.rLight->g.shader = declaration.lightParms.lightMtr;
        action.rLight->g.noShadows = declaration.noshadows;
        action.rLight->g.allowLightOnlyInViewID = allowSurfaceOnlyInViewID;
        action.rLight->g.suppressLightInViewID = suppressSurfaceInViewID;
      }
    }
    if (action.rLight != nullptr) {
      const idVec4 tableColor = GetTableColor(declaration, 0.0f);
      const float intensity = declaration.lightParms.intensity;
      const idVec4 lightColor{
          tableColor.x * declaration.color.x * systemColor.x * intensity,
          tableColor.y * declaration.color.y * systemColor.y * intensity,
          tableColor.z * declaration.color.z * systemColor.z * intensity,
          tableColor.w * declaration.color.w * systemColor.w * intensity};
      action.rLight->g.origin = currentOrigin;
      action.rLight->g.axis = currentAxis;
      action.rLight->SetParm(
          static_cast<const idDeclRenderParm*>(rp_lightColor.r), lightColor);
      if (state.startDelay != 0) {
        action.rLight->Unlink();
      } else {
        action.rLight->CommitThisFrame();
      }
    }
    return;
  }

  case FX_PARTICLE:
    if (!declaration.particleParms.useSmokeSystem) {
      if (action.rParticle == nullptr && rw != nullptr &&
          declaration.particleParms.declPrt != nullptr) {
        action.rParticle = gameLibEffects->fxModelRecycler.GetParticleFxModel(
            declaration.particleParms.declPrt, rw);
      }
      if (action.rParticle != nullptr) {
        SetRenderModelOriginAndAxis(action.rParticle, currentOrigin,
                                    currentAxis);
        action.rParticle->g.scale =
            {declaration.size, declaration.size, declaration.size};
        const idVec4 particleColor{
            systemColor.x * declaration.color.x,
            systemColor.y * declaration.color.y,
            systemColor.z * declaration.color.z,
            systemColor.w * declaration.color.w};
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_Color.r), particleColor);
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_Diversity.r),
            RandomUnit(random));
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_TimeStop.r), 0.0f);
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_TimeOffset.r),
            static_cast<float>(currentTime) * 0.001f);
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_NoShadows.r),
            declaration.noshadows ? 1.0f : 0.0f);
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rpParticleVel.r),
            vec3_origin);
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_coverage.r), 1.0f);
        action.rParticle->g.fadeVisibilityOver =
            declaration.fadeStartDistance;
        action.rParticle->g.maxVisibleRange = declaration.distance;
        action.rParticle->g.allowSurfaceOnlyInViewID =
            allowSurfaceOnlyInViewID;
        action.rParticle->g.suppressSurfaceInViewID =
            suppressSurfaceInViewID;
        if (declaration.particleParms.isScreenPrt || state.startDelay != 0) {
          UnlinkRenderModel(action.rParticle);
        } else {
          action.rParticle->CommitThisFrame();
        }
      }
    }
    return;

  case FX_DECAL2:
    if (action.rModel == nullptr) {
      action.rModel = AllocGameRenderModel(GetGame(), "_unitCube");
      if (action.rModel != nullptr) {
        if (action.rModel->g.customMaterial !=
            declaration.decalParms.decalMtr) {
          action.rModel->g.customMaterial = declaration.decalParms.decalMtr;
          action.rModel->FinishSurfaces();
        }
        action.rModel->g.flags0 |= 0x80u;
      }
    }
    if (action.rModel != nullptr) {
      SetRenderModelOriginAndAxis(action.rModel, currentOrigin, currentAxis);
      action.rModel->g.scale = {declaration.size, declaration.size,
                                declaration.decalParms.depth};
      action.rModel->g.allowSurfaceOnlyInViewID = allowSurfaceOnlyInViewID;
      action.rModel->g.suppressSurfaceInViewID = suppressSurfaceInViewID;
      if (state.startDelay != 0) {
        UnlinkRenderModel(action.rModel);
      } else {
        action.rModel->CommitThisFrame();
      }
    }
    return;

  case FX_MODEL:
    if (rw != nullptr && action.rModel == nullptr) {
      action.rModel = gameLibEffects->fxModelRecycler.GetStaticFxModel(
          declaration.modelParms.modelName);
      if (action.rModel != nullptr &&
          action.rModel->g.customMaterial !=
              declaration.modelParms.customMaterial) {
        action.rModel->g.customMaterial = declaration.modelParms.customMaterial;
        action.rModel->FinishSurfaces();
      }
    }
    if (action.rModel != nullptr) {
      const idVec4 modelColor{
          systemColor.x * declaration.color.x,
          systemColor.y * declaration.color.y,
          systemColor.z * declaration.color.z,
          systemColor.w * declaration.color.w};
      action.rModel->SetParm(
          static_cast<const idDeclRenderParm*>(rp_Color.r), modelColor);
      SetRenderModelOriginAndAxis(action.rModel, currentOrigin, currentAxis);
      action.rModel->g.flags0 =
          static_cast<std::uint8_t>((action.rModel->g.flags0 & 0x7fu) |
                                    (declaration.noshadows ? 0x80u : 0u));
      action.rModel->g.scale =
          {declaration.size, declaration.size, declaration.size};
      action.rModel->g.allowSurfaceOnlyInViewID = allowSurfaceOnlyInViewID;
      action.rModel->g.suppressSurfaceInViewID = suppressSurfaceInViewID;
      if (state.startDelay != 0) {
        UnlinkRenderModel(action.rModel);
      } else {
        action.rModel->CommitThisFrame();
      }
    }
    return;

  case FX_SOUND:
    if (state.startDelay == 0 && declaration.soundParms.sound != nullptr) {
      state.started = true;
      const idSoundShader* shader = declaration.soundParms.sound;
      if (remote && shader->remoteSound != nullptr) {
        shader = shader->remoteSound;
      }
      StartSound(declaration.soundParms.channel, shader);
      if (declaration.fadeInTime != 0.0f && soundInfo.emitter != nullptr) {
        soundChannel_t channel = declaration.soundParms.channel;
        if (channel == SND_CHANNEL_ANY) {
          channel = soundInfo.channel;
        }
        soundInfo.emitter->SetVolume(channel, -60.0f);
        soundInfo.emitter->FadeSound(channel, 0.0f,
                                     declaration.fadeInTime, 0.0f, false);
      }
      if (soundInfo.emitter != nullptr) {
        soundInfo.emitter->UpdateEmitter(currentOrigin, currentAxis,
                                         vec3_origin, 0, false);
      }
      ApplyFade(declaration, action, state, currentTime, 0.0f);
    }
    return;

  case FX_FLARE:
    if (declaration.flareParms.declFlare != nullptr) {
      if (action.flareHandle.value.handleId == -1) {
        action.flareHandle = gameLibEffects->lensFlareManager->AddFlare(
            gameLibEffects->world, declaration.flareParms.declFlare,
            vec3_origin, vec3_origin,
            declaration.flareParms.isAutosprited);
        gameLibEffects->lensFlareManager->SetupRenderModels(
            action.flareHandle, declaration.size);
      }
      const idVec4 flareColor{
          systemColor.x * declaration.color.x,
          systemColor.y * declaration.color.y,
          systemColor.z * declaration.color.z,
          systemColor.w * declaration.color.w};
      gameLibEffects->lensFlareManager->StartFlare(
          action.flareHandle, currentOrigin, currentAxis, flareColor,
          declaration.fadeStartDistance,
          fmaxf(0.0f, declaration.distance));
      if (declaration.colorTableRGB != nullptr ||
          declaration.colorTableA != nullptr) {
        gameLibEffects->lensFlareManager->SetFlareColor(action.flareHandle,
                                                        flareColor);
      }
      gameLibEffects->lensFlareManager->UpdatePosition(
          action.flareHandle, currentOrigin, currentAxis,
          declaration.originType == FX_ORG_TRACK_POS,
          declaration.rotationType == FX_ROT_TRACK_AXIS ||
              declaration.rotationType == FX_ROT_TRACK_AXIS_PARENT);
      state.started = true;
    }
    return;

  case FX_GODRAY:
    if (declaration.godrayParms.godrayMaterial != nullptr &&
        action.rGodrays == nullptr) {
      const idVec3 godrayColor{
          declaration.godrayParms.color.r *
              declaration.godrayParms.color.a *
              declaration.godrayParms.colorScale,
          declaration.godrayParms.color.g *
              declaration.godrayParms.color.a *
              declaration.godrayParms.colorScale,
          declaration.godrayParms.color.b *
              declaration.godrayParms.color.a *
              declaration.godrayParms.colorScale};
      action.rGodrays = static_cast<idRenderModelGodRays*>(
          AllocGameRenderModel(GetGame(), "_godrays"));
      if (action.rGodrays != nullptr) {
        action.rGodrays->SetParms(
            declaration.godrayParms.godrayMaterial, false, currentOrigin,
            godrayColor, declaration.godrayParms.size,
            declaration.godrayParms.sourceSize);
      }
    }
    if (action.rGodrays != nullptr) {
      if ((action.rGodrays->useDeferredPosition &
           (action.rGodrays->deferredPositionInitialized >> 1u) & 1u) ==
          0u) {
        action.rGodrays->g.origin = currentOrigin;
      }
      action.rGodrays->deferredOrigin = currentOrigin;
      action.rGodrays->fadeVal = 0.0f;
      action.rGodrays->CommitThisFrame();
    }
    return;

  default:
    return;
  }
}

void idFXManager::UpdateActions(const idVec3& origin,
                                const idMat3& parentAxis,
                                const idVec3& velocity, int currentTime,
                                int gameMillisecondsPerFrame,
                                int serverCurrentTime, float fovScale,
                                float depthHack) {
  if (fxDecl == nullptr) {
    return;
  }

  struct viewCallbackInfo_t {
    idVec3 viewOrigin;
    idMat3 viewAxis;
    float distanceSquared;
    bool inRange;
    unsigned char padding[3];
  };
  static_assert(sizeof(viewCallbackInfo_t) == 0x38);

  const idList<idViewCallbacks*, 108>& callbacks = GetViewCallbacks();
  viewCallbackInfo_t viewInfo[16];
  for (int index = 0; index < fxDecl->events.num; ++index) {
    if (index >= actions.num) {
      continue;
    }
    idFXAction& action = actions.list[index];
    fxActionState_t& state = actionState.list[index];
    const idFXSingleAction& declaration = fxDecl->events.list[index];
    const int actionStartTime = state.startTime + state.startDelay;
    if (state.startTime < 0 || currentTime < actionStartTime ||
        state.hidden) {
      continue;
    }

    const float fraction =
        (static_cast<float>(currentTime) -
         static_cast<float>(actionStartTime)) /
        (static_cast<float>(state.stopTime) -
         static_cast<float>(actionStartTime));
    if (fraction >= 1.0f) {
      if (!declaration.looping || state.forceStop) {
        if (!declaration.restart || state.forceStop) {
          StopAction(index, currentTime, false, true);
        } else {
          RestartAction(index, currentTime);
        }
      } else {
        state.startTime = currentTime;
        state.stopTime = currentTime -
                         static_cast<int>(declaration.duration * -1000.0f);
        state.fadeOutStartTime = currentTime - static_cast<int>(
                                                   (declaration.duration -
                                                    declaration.fadeOutTime) *
                                                   -1000.0f);
      }
      continue;
    }

    idVec3 tagOrigin = origin;
    idMat3 tagAxis = parentAxis;
    idVec3 tagVelocity = velocity;
    if (action.tagData.num > 0) {
      const tagData_t tag = action.tagData.list[action.tagIndex];
      if (ta != nullptr) {
        ta->GetWorldSpaceTagTransform(tag, tagOrigin, tagAxis);
      }
      if (!tagAxis.IsOrthoNormal(0.0001f)) {
        continue;
      }
      if (declaration.particleParms.trackVelocity) {
        GetWorldSpaceTagVelocity(tag, gameMillisecondsPerFrame, tagVelocity);
      }
    }

    idMat3 currentAxis = parentAxis;
    switch (declaration.rotationType) {
    case FX_ROT_START_AXIS:
    case FX_ROT_START_AXIS_PARENT:
      currentAxis = action.startAxis;
      break;
    case FX_ROT_TRACK_AXIS:
      currentAxis = tagAxis;
      break;
    case FX_ROT_TRACK_AXIS_PARENT:
      currentAxis = parentAxis;
      break;
    case FX_ROT_TRACK_LOCAL_AXIS:
      currentAxis = action.startAxis * parentAxis;
      break;
    case FX_ROT_EXPLICIT_ANGLES:
      currentAxis = declaration.axis;
      break;
    case FX_ROT_EXPLICIT_TABLES:
      currentAxis = GetExplicitTableAxis(
          declaration, fraction,
          static_cast<float>(currentTime - actionStartTime) * 0.001f);
      break;
    case FX_ROT_EXPLICIT_TABLES_LOCAL:
      currentAxis =
          GetExplicitTableAxis(
              declaration, fraction,
              static_cast<float>(currentTime - actionStartTime) * 0.001f) *
          tagAxis;
      break;
    case FX_ROT_EXTERNALROT:
      currentAxis = externalRotation;
      break;
    default:
      break;
    }
    if (declaration.rotationType >= FX_ROT_TRACK_AXIS &&
        declaration.rotationType <= FX_ROT_EXTERNALROT) {
      currentAxis =
          (action.randomAngles.ToMat3() * declaration.rotOffset) * currentAxis;
    }

    idVec3 currentOrigin = origin;
    switch (declaration.originType) {
    case FX_ORG_START_POS:
      currentOrigin = action.startOrg;
      break;
    case FX_ORG_TRACK_POS:
      currentOrigin = tagOrigin;
      break;
    case FX_ORG_TRACK_LOCAL_POS:
      currentOrigin = TransformRowVector(action.startOrg, parentAxis) + origin;
      break;
    case FX_ORG_EXTERNALPOS:
      currentOrigin = externalPosition;
      break;
    default:
      break;
    }
    if (declaration.originType >= FX_ORG_TRACK_POS &&
        declaration.originType <= FX_ORG_EXTERNALPOS) {
      currentOrigin += TransformRowVector(declaration.offset, currentAxis);
    }

    if (g_debugFX.valueInteger == 1) {
      GetGameLibRenderDebug()->DebugAxis(currentOrigin, currentAxis, 0, false);
    }

    bool anyViewInRange = false;
    const float maximumDistanceSquared =
        declaration.distance * declaration.distance;
    for (int callbackIndex = 0; callbackIndex < callbacks.num;
         ++callbackIndex) {
      callbacks.list[callbackIndex]->GetViewPosition(
          viewInfo[callbackIndex].viewOrigin,
          viewInfo[callbackIndex].viewAxis);
      const idVec3 difference =
          viewInfo[callbackIndex].viewOrigin - currentOrigin;
      viewInfo[callbackIndex].distanceSquared = difference * difference;
      viewInfo[callbackIndex].inRange =
          declaration.distance == -1.0f ||
          maximumDistanceSquared > viewInfo[callbackIndex].distanceSquared;
      anyViewInRange |= viewInfo[callbackIndex].inRange;
    }
    if (fx_skipRangeCheck.valueInteger != 0) {
      anyViewInRange = true;
    }

    if (declaration.fire.str[0] != '\0' && state.shouldTrigger) {
      for (int targetIndex = 0; targetIndex < fxDecl->events.num;
           ++targetIndex) {
        if (idStr::Icmp(fxDecl->events.list[targetIndex].name.str,
                        declaration.fire.str) == 0 &&
            actionState.list[targetIndex].hidden) {
          fxActionState_t& targetState = actionState.list[targetIndex];
          targetState.startDelay = 0;
          targetState.hidden = false;
          targetState.startTime = currentTime;
          targetState.shouldTrigger = true;
          targetState.forceStop = false;
          StartAction(targetIndex, currentOrigin, currentAxis, currentTime,
                      -1);
        }
      }
      state.shouldTrigger = false;
    }

    switch (declaration.type) {
    case FX_LIGHT:
      if (action.rLight != nullptr) {
        if (action.rLight->unlinked ||
            declaration.originType != FX_ORG_START_POS ||
            declaration.rotationType >= FX_ROT_TRACK_AXIS) {
          action.rLight->g.origin = currentOrigin;
          action.rLight->g.axis = currentAxis;
        }
        const idVec4 tableColor = GetTableColor(declaration, fraction);
        const float intensity = declaration.lightParms.intensity;
        const idVec4 lightColor{
            tableColor.x * declaration.color.x * systemColor.x * intensity,
            tableColor.y * declaration.color.y * systemColor.y * intensity,
            tableColor.z * declaration.color.z * systemColor.z * intensity,
            tableColor.w * declaration.color.w * systemColor.w * intensity};
        action.rLight->SetParm(
            static_cast<const idDeclRenderParm*>(rp_lightColor.r),
            lightColor);
        action.rLight->CommitThisFrame();
        ApplyFade(declaration, action, state, currentTime, fraction);
      }
      break;

    case FX_PARTICLE:
      if (!declaration.particleParms.useSmokeSystem) {
        if (action.rParticle == nullptr &&
            declaration.particleParms.declPrt != nullptr) {
          action.rParticle =
              gameLibEffects->fxModelRecycler.GetParticleFxModel(
                  declaration.particleParms.declPrt, rw);
        }
        if (action.rParticle == nullptr) {
          break;
        }
        action.rParticle->g.allowSurfaceOnlyInViewID =
            allowSurfaceOnlyInViewID;
        action.rParticle->g.suppressSurfaceInViewID =
            suppressSurfaceInViewID;
        if (declaration.particleParms.isScreenPrt) {
          if (!state.started) {
            for (int callbackIndex = 0; callbackIndex < callbacks.num;
                 ++callbackIndex) {
              if (g_debugFX.valueInteger == 2) {
                GetGameLibRenderDebug()->DebugAxis(
                    idColor::colorRed, currentOrigin, currentAxis, 10000,
                    false);
              }
              if (viewInfo[callbackIndex].inRange &&
                  (action.viewCallbacksID < 0 ||
                   action.viewCallbacksID ==
                       callbacks.list[callbackIndex]->GetViewID())) {
                action.screenPrtHandle =
                    callbacks.list[callbackIndex]->AddScreenParticle(
                        action.rParticle,
                        declaration.particleParms.screenPrtExcludeAngle);
                break;
              }
            }
            state.started = true;
          }
          break;
        }

        const idVec4 tableColor = GetTableColor(declaration, fraction);
        const idVec4 particleColor{
            tableColor.x * declaration.color.x * systemColor.x,
            tableColor.y * declaration.color.y * systemColor.y,
            tableColor.z * declaration.color.z * systemColor.z,
            tableColor.w * declaration.color.w * systemColor.w};
        const bool staticTransform =
            declaration.originType == FX_ORG_START_POS &&
            declaration.rotationType <= FX_ROT_START_AXIS_PARENT;
        const bool wasUnlinked = (action.rParticle->unlinked & 8u) != 0u;
        if (wasUnlinked) {
          action.rParticle->SetParm(
              static_cast<const idDeclRenderParm*>(rp_Diversity.r),
              RandomUnit(random));
          action.rParticle->SetParm(
              static_cast<const idDeclRenderParm*>(rp_TimeStop.r), 0.0f);
          action.rParticle->SetParm(
              static_cast<const idDeclRenderParm*>(rp_TimeOffset.r),
              static_cast<float>(currentTime) * 0.001f);
          action.rParticle->SetParm(
              static_cast<const idDeclRenderParm*>(rp_NoShadows.r),
              declaration.noshadows ? 1.0f : 0.0f);
        }
        idVec3 particleVelocity = vec3_origin;
        if (!staticTransform && declaration.particleParms.trackVelocity) {
          particleVelocity =
              tagVelocity * declaration.particleParms.velocityScale;
          if (g_debugFX.valueInteger != 0) {
            const float velocityLengthSquared =
                particleVelocity * particleVelocity;
            const float inverseVelocityLength =
                InverseLengthRetail(velocityLengthSquared);
            const idVec3 debugEnd =
                currentOrigin +
                particleVelocity * (inverseVelocityLength * 15.0f);
            GetGameLibRenderDebug()->DebugArrow(
                idColor::colorOrange, currentOrigin, debugEnd, 4.0f, 0,
                false);
          }
        }
        if (wasUnlinked || !staticTransform) {
          action.rParticle->SetParm(
              static_cast<const idDeclRenderParm*>(rpParticleVel.r),
              particleVelocity);
          SetRenderModelOriginAndAxis(action.rParticle,
                                      currentOrigin + particleVelocity,
                                      currentAxis);
        }
        action.rParticle->g.fovScale = fovScale;
        action.rParticle->g.modelDepthHack = depthHack;
        action.rParticle->SetParm(
            static_cast<const idDeclRenderParm*>(rp_Color.r), particleColor);
        if (declaration.colorTableRGB != nullptr ||
            declaration.colorTableA != nullptr) {
          action.rParticle->SetParm(
              static_cast<const idDeclRenderParm*>(rp_Color.r), particleColor);
        }
        action.rParticle->CommitThisFrame();
        ApplyFade(declaration, action, state, currentTime, fraction);
      } else {
        if (!anyViewInRange) {
          break;
        }
        if (!state.started) {
          action.lastParticleDropPos = currentOrigin;
          state.started = true;
        }
        idVec3 smokeVelocity = vec3_origin;
        if (declaration.particleParms.trackVelocity) {
          smokeVelocity =
              tagVelocity * declaration.particleParms.velocityScale;
          if (g_debugFX.valueInteger != 0) {
            const idVec3 debugVelocity = smokeVelocity;
            smokeVelocity =
                smokeVelocity *
                (1.0f / static_cast<float>(gameMillisecondsPerFrame));
            const float velocityLengthSquared =
                debugVelocity * debugVelocity;
            const float inverseVelocityLength =
                InverseLengthRetail(velocityLengthSquared);
            const idVec3 debugEnd =
                currentOrigin +
                debugVelocity * (inverseVelocityLength * 15.0f);
            GetGameLibRenderDebug()->DebugArrow(
                idColor::colorOrange, currentOrigin, debugEnd, 4.0f, 0,
                false);
          }
          if (smokeVelocity * smokeVelocity <
              declaration.particleParms.minVelocity *
                  declaration.particleParms.minVelocity) {
            break;
          }
        }

        const idVec3 movement = currentOrigin - action.lastParticleDropPos;
        const float movementLengthSquared = movement * movement;
        const float inverseMovementLength =
            InverseLengthRetail(movementLengthSquared);
        const float movementLength =
            inverseMovementLength * movementLengthSquared;
        const idVec3 movementDirection = movement * inverseMovementLength;
        if (declaration.particleParms.trailSpacing > 0.0f &&
            movementLength > 1.1920929e-7f) {
          if (movementLength <= declaration.particleParms.trailSpacing) {
            break;
          }
          const int count = static_cast<int>(
              movementLength / declaration.particleParms.trailSpacing);
          for (int particleIndex = 1; particleIndex <= count;
               ++particleIndex) {
            const float travel = static_cast<float>(particleIndex) *
                                 declaration.particleParms.trailSpacing;
            const idVec3 particleOrigin =
                action.lastParticleDropPos + movementDirection * travel;
            const std::uint32_t color = 0xffffffffu;
            gameLibEffects->effectsModel->AddParticles(
                declaration.particleParms.declPrt, currentTime,
                gameMillisecondsPerFrame, RandomUnit(random), particleOrigin,
                currentAxis, smokeVelocity, color, false);
          }
          const float consumed = static_cast<float>(count) *
                                 declaration.particleParms.trailSpacing;
          action.lastParticleDropPos += movementDirection * consumed;
        } else if (declaration.particleParms.smokeSystemRate == 0 ||
                   serverCurrentTime %
                           declaration.particleParms.smokeSystemRate ==
                       0) {
          const std::uint32_t color = 0xffffffffu;
          gameLibEffects->effectsModel->AddParticles(
              declaration.particleParms.declPrt, currentTime,
              gameMillisecondsPerFrame, RandomUnit(random), currentOrigin,
              currentAxis, smokeVelocity, color, false);
        }
      }
      break;

    case FX_DECAL:
      if (!state.started && declaration.decalParms.decalMtr != nullptr &&
          gameLibEffects->decalModel != nullptr) {
        state.started = true;
        decalParams_t parameters;
        parameters.decalMat = declaration.decalParms.decalMtr;
        parameters.depth = declaration.decalParms.depth;
        parameters.size = {declaration.size, declaration.size};
        parameters.angle = declaration.decalParms.angle;
        parameters.lifeTime =
            static_cast<int>(declaration.duration * 1000.0f);
        parameters.fadeOutTime =
            static_cast<int>(declaration.fadeOutTime * 1000.0f);
        parameters.fadeInTime =
            static_cast<int>(declaration.fadeInTime * 1000.0f);
        parameters.quad = true;
        parameters.overlap = false;
        parameters.overlapRadius = 0.0f;
        parameters.cost = 0;
        if (action.decalHandle.decalIndex != -1) {
          gameLibEffects->decalModel->RemoveDecal(action.decalHandle);
        }
        action.decalHandle = gameLibEffects->decalModel->AddDecalFromPoint(
            &parameters, currentTime, currentOrigin, currentAxis.mat[2]);
      }
      break;

    case FX_DECAL2:
      if (action.rModel != nullptr) {
        const bool wasUnlinked = (action.rModel->unlinked & 8u) != 0u;
        bool changed = wasUnlinked;
        if (changed || declaration.originType != FX_ORG_START_POS ||
            declaration.rotationType >= FX_ROT_TRACK_AXIS) {
          SetRenderModelOriginAndAxis(action.rModel, currentOrigin,
                                      currentAxis);
          changed = true;
        }
        if (wasUnlinked ||
            std::fabs(action.rModel->g.fovScale - fovScale) >
                1.1920929e-7f ||
            std::fabs(action.rModel->g.modelDepthHack - depthHack) >
                1.1920929e-7f) {
          action.rModel->g.fovScale = fovScale;
          action.rModel->g.modelDepthHack = depthHack;
          changed = true;
        }
        if (declaration.customRenderParm != nullptr) {
          idVec3 tableValues = vec3_origin;
          if (declaration.customTable1 != nullptr) {
            tableValues.x = declaration.customTable1->table->TableLookup(
                fraction, true);
          }
          if (declaration.customTable2 != nullptr) {
            tableValues.y = declaration.customTable2->table->TableLookup(
                fraction, true);
          }
          action.rModel->SetParm(declaration.customRenderParm, tableValues);
          action.rModel->CommitThisFrame();
        } else if (changed) {
          action.rModel->CommitThisFrame();
        }
        ApplyFade(declaration, action, state, currentTime, fraction);
      }
      break;

    case FX_MODEL:
      if (action.rModel == nullptr) {
        action.rModel = gameLibEffects->fxModelRecycler.GetStaticFxModel(
            declaration.modelParms.modelName);
      }
      if (action.rModel != nullptr) {
        if ((action.rModel->unlinked & 8u) != 0u ||
            declaration.originType != FX_ORG_START_POS ||
            declaration.rotationType >= FX_ROT_TRACK_AXIS) {
          SetRenderModelOriginAndAxis(action.rModel, currentOrigin,
                                      currentAxis);
        }
        const idVec4 modelColor{
            systemColor.x * declaration.color.x,
            systemColor.y * declaration.color.y,
            systemColor.z * declaration.color.z,
            systemColor.w * declaration.color.w};
        action.rModel->SetParm(
            static_cast<const idDeclRenderParm*>(rp_Color.r), modelColor);
        action.rModel->g.fovScale = fovScale;
        action.rModel->g.modelDepthHack = depthHack;
        if (declaration.customRenderParm != nullptr) {
          idVec3 tableValues = vec3_origin;
          if (declaration.customTable1 != nullptr) {
            tableValues.x = declaration.customTable1->table->TableLookup(
                fraction, true);
          }
          if (declaration.customTable2 != nullptr) {
            tableValues.y = declaration.customTable2->table->TableLookup(
                fraction, true);
          }
          action.rModel->SetParm(declaration.customRenderParm, tableValues);
          action.rModel->CommitThisFrame();
        }
        action.rModel->CommitThisFrame();
        ApplyFade(declaration, action, state, currentTime, fraction);
      }
      break;

    case FX_SOUND:
      if (declaration.soundParms.sound != nullptr) {
        const bool dynamic =
            declaration.originType != FX_ORG_START_POS ||
            declaration.rotationType >= FX_ROT_TRACK_AXIS;
        if (!state.started) {
          state.started = true;
          const idSoundShader* shader = declaration.soundParms.sound;
          if (remote && shader->remoteSound != nullptr) {
            shader = shader->remoteSound;
          }
          StartSound(declaration.soundParms.channel, shader);
          if (declaration.fadeInTime != 0.0f &&
              soundInfo.emitter != nullptr) {
            soundChannel_t channel = declaration.soundParms.channel;
            if (channel == SND_CHANNEL_ANY) {
              channel = soundInfo.channel;
            }
            soundInfo.emitter->SetVolume(channel, -60.0f);
            soundInfo.emitter->FadeSound(channel, 0.0f,
                                         declaration.fadeInTime, 0.0f, false);
          }
          if (soundInfo.emitter != nullptr) {
            soundInfo.emitter->UpdateEmitter(currentOrigin, currentAxis,
                                             vec3_origin, 0, false);
          }
        } else if (dynamic && soundInfo.emitter != nullptr) {
          soundInfo.emitter->UpdateEmitter(currentOrigin, currentAxis,
                                           vec3_origin, 0, false);
        }
        ApplyFade(declaration, action, state, currentTime, fraction);
      }
      break;

    case FX_SCREEN_SHAKE:
      for (int callbackIndex = 0; callbackIndex < callbacks.num;
           ++callbackIndex) {
        if (!viewInfo[callbackIndex].inRange ||
            (action.viewCallbacksID >= 0 &&
             action.viewCallbacksID !=
                 callbacks.list[callbackIndex]->GetViewID())) {
          continue;
        }
        float distanceScale = 1.0f;
        if (declaration.fadeStartDistance > 0.0f) {
          const float minimumDistanceSquared =
              declaration.fadeStartDistance * declaration.fadeStartDistance;
          distanceScale =
              1.0f - (viewInfo[callbackIndex].distanceSquared -
                      minimumDistanceSquared) /
                         (maximumDistanceSquared - minimumDistanceSquared);
          distanceScale = ClampUnitRetail(distanceScale);
        }
        if (g_debugFX.valueInteger == 1 && !state.started) {
          idLib::Printf("Starting screen shake: %s - %s - Owner: %s \n",
                        fxDecl->name.str, declaration.name.str,
                        owner.c_str());
        }
        callbacks.list[callbackIndex]->ApplyScreenShake(
            declaration.screenShakeParms.magnitude * distanceScale,
            declaration.screenShakeParms.controllerShakeScale,
            declaration.screenShakeParms.maxAngles,
            declaration.screenShakeParms.maxOffset);
      }
      state.started = true;
      break;

    case FX_CONTROLLER_SHAKE:
      if (!state.started) {
        for (int callbackIndex = 0; callbackIndex < callbacks.num;
             ++callbackIndex) {
          if (!viewInfo[callbackIndex].inRange ||
              (action.viewCallbacksID >= 0 &&
               action.viewCallbacksID !=
                   callbacks.list[callbackIndex]->GetViewID())) {
            continue;
          }
          float distanceScale = 1.0f;
          if (declaration.fadeStartDistance > 0.0f) {
            const float minimumDistanceSquared =
                declaration.fadeStartDistance *
                declaration.fadeStartDistance;
            distanceScale =
                1.0f - (viewInfo[callbackIndex].distanceSquared -
                        minimumDistanceSquared) /
                           (maximumDistanceSquared - minimumDistanceSquared);
            distanceScale = ClampUnitRetail(distanceScale);
          }
          if (g_debugFX.valueInteger == 1) {
            idLib::Printf(
                "Starting controller shake: %s - %s - Owner: %s \n",
                fxDecl->name.str, declaration.name.str, owner.c_str());
          }
          callbacks.list[callbackIndex]->ApplyControllerShake(
              static_cast<int>(
                  declaration.controllerShakeParms.highDuration),
              static_cast<int>(
                  declaration.controllerShakeParms.lowDuration),
              declaration.controllerShakeParms.highMag * distanceScale,
              declaration.controllerShakeParms.lowMag * distanceScale,
              declaration.controllerShakeParms.ampTable);
        }
        state.started = true;
      }
      break;

    case FX_RENDERPARM:
      if (!state.started) {
        action.renderParmStartValue =
            ta != nullptr
                ? ta->gameParmBlock.GetVector(
                      declaration.renderParmParms.declRenderParm, nullptr)
                : idVec4{};
        state.started = true;
      }
      ApplyFade(declaration, action, state, currentTime, fraction);
      break;

    case FX_ENV_OVERRIDE:
      if (!state.started) {
        for (int callbackIndex = 0; callbackIndex < callbacks.num;
             ++callbackIndex) {
          if (viewInfo[callbackIndex].inRange &&
              (action.viewCallbacksID < 0 ||
               action.viewCallbacksID ==
                   callbacks.list[callbackIndex]->GetViewID())) {
            callbacks.list[callbackIndex]->ApplyDynEnvOverride(
                declaration.envParms.declEnv, declaration.fadeInTime);
          }
        }
        state.started = true;
      }
      break;

    case FX_ENV_CHANGE:
      if (!state.started) {
        for (int callbackIndex = 0; callbackIndex < callbacks.num;
             ++callbackIndex) {
          if (!viewInfo[callbackIndex].inRange ||
              (action.viewCallbacksID >= 0 &&
               action.viewCallbacksID !=
                   callbacks.list[callbackIndex]->GetViewID())) {
            continue;
          }
          for (int parmIndex = 0;
               parmIndex < declaration.envParms.envRenderParm.num;
               ++parmIndex) {
            const idFXRenderParm& parameter =
                declaration.envParms.envRenderParm.list[parmIndex];
            callbacks.list[callbackIndex]->ApplyDynEnvParmOverride(
                parameter.parm, parameter.value, false,
                declaration.fadeInTime);
          }
        }
        state.started = true;
      }
      break;

    case FX_FLARE:
      if (action.flareHandle.value.handleId != -1) {
        idVec3 flareOrigin = currentOrigin;
        const idMat3 flareAxis = currentAxis;
        if (declaration.originType == FX_ORG_TRACK_POS) {
          flareOrigin += TransformRowVector(declaration.flareParms.position,
                                            currentAxis);
        }
        if (!state.started) {
          const idVec4 flareColor{
              declaration.color.x * systemColor.x,
              declaration.color.y * systemColor.y,
              declaration.color.z * systemColor.z,
              declaration.color.w * systemColor.w};
          gameLibEffects->lensFlareManager->StartFlare(
              action.flareHandle, flareOrigin, currentAxis, flareColor,
              declaration.fadeStartDistance,
              fmaxf(0.0f, declaration.distance));
          state.started = true;
        }
        if (declaration.colorTableRGB != nullptr ||
            declaration.colorTableA != nullptr) {
          const idVec4 tableColor = GetTableColor(declaration, fraction);
          const idVec4 flareColor{
              declaration.color.x * systemColor.x * tableColor.x,
              declaration.color.y * systemColor.y * tableColor.y,
              declaration.color.z * systemColor.z * tableColor.z,
              declaration.color.w * systemColor.w * tableColor.w};
          gameLibEffects->lensFlareManager->SetFlareColor(action.flareHandle,
                                                          flareColor);
        }
        ApplyFade(declaration, action, state, currentTime, fraction);
        gameLibEffects->lensFlareManager->UpdatePosition(
            action.flareHandle, flareOrigin, flareAxis,
            declaration.originType == FX_ORG_TRACK_POS,
            declaration.rotationType == FX_ROT_TRACK_AXIS ||
                declaration.rotationType == FX_ROT_TRACK_AXIS_PARENT);
      }
      break;

    case FX_RADIAL_BLUR:
      if (!state.started) {
        const bool unlimitedRange = declaration.distance < 0.0f;
        for (int callbackIndex = 0; callbackIndex < callbacks.num;
             ++callbackIndex) {
          if ((!unlimitedRange &&
               maximumDistanceSquared <=
                   viewInfo[callbackIndex].distanceSquared) ||
              (action.viewCallbacksID >= 0 &&
               action.viewCallbacksID !=
                   callbacks.list[callbackIndex]->GetViewID())) {
            continue;
          }
          const float distanceScale =
              unlimitedRange
                  ? 1.0f
                  : 1.0f - fminf(
                               viewInfo[callbackIndex].distanceSquared /
                                   maximumDistanceSquared,
                               1.0f);
          callbacks.list[callbackIndex]->ApplyRadialBlur(
              currentOrigin, 0.015f, declaration.duration, distanceScale,
              0.0f, declaration.radialBlurParms.maxScale);
        }
        state.started = true;
      }
      break;

    case FX_RIBBON:
      if (!action.ribbonManager.IsInitialized()) {
        action.ribbonManager.Init(
            declaration.ribbonParms.declRibbon, gameLibEffects->beamModel,
            &gameLibEffects->ribbonModelManager);
      }
      if (!state.started) {
        action.ribbonManager.StartRibbon(currentTime, currentOrigin);
        state.started = true;
      }
      action.ribbonManager.UpdateRibbon(
          currentTime, currentOrigin, currentAxis, vec3_origin,
          idVec4{1.0f, 1.0f, 1.0f, 1.0f}, vec3_origin);
      break;

    case FX_FADE_PARENT:
      state.started = true;
      ApplyFade(declaration, action, state, currentTime, fraction);
      [[fallthrough]];

    case FX_GODRAY:
      state.started = true;
      ApplyFade(declaration, action, state, currentTime, fraction);
      if (action.rGodrays != nullptr) {
        if ((action.rGodrays->useDeferredPosition &
             (action.rGodrays->deferredPositionInitialized >> 1u) & 1u) ==
            0u) {
          action.rGodrays->g.origin = currentOrigin;
        }
        action.rGodrays->deferredOrigin = currentOrigin;
      }
      break;

    default:
      break;
    }
  }
}

int idFXManager::InternalStartFX(const fxActionCall_t& actionCall) {
  if (!initialized) {
    return 0;
  }
  const int result = StartActions(
      actionCall.org, actionCall.axis, actionCall.time, actionCall.condition,
      actionCall.extraCondition, actionCall.tagIdx);
  ++actionBufferPos;
  return result;
}

int idFXManager::StartFX(const idVec3& origin, const idMat3& axis,
                         int currentTime, fxCondition_t condition,
                         fxExtraCondition_t extraCondition, int tagIndex) {
  if (g_skipFXManager.valueInteger != 0 || !initialized) {
    return 0;
  }
  if (!common->IsServer()) {
    return StartActions(origin, axis, currentTime, condition, extraCondition,
                        tagIndex);
  }
  fxActionCall_t& call = actionBuffer[actionBufferPos % 8];
  call.org = origin;
  call.axis = axis;
  call.time = currentTime;
  call.condition = condition;
  call.actionType = FXACTION_START;
  call.extraCondition = extraCondition;
  call.immediate = false;
  call.tagIdx = tagIndex;
  call.viewCallbacksID = viewCallbacksID;
  return InternalStartFX(call);
}

int idFXManager::StartFX(const idVec3& origin, const idMat3& axis,
                         int currentTime, fxCondition_t condition) {
  return StartFX(origin, axis, currentTime, condition, FX_EXTRA_COND_NONE, -1);
}

void idFXManager::RestartAction(int index, int currentTime) {
  idFXAction& action = actions.list[index];
  const idFXSingleAction& declaration = fxDecl->events.list[index];
  fxActionState_t& state = actionState.list[index];
  const idVec3 lastOrigin = action.startOrg;
  const idMat3 lastAxis = action.startAxis;
  StopAction(index, currentTime, true, false);

  const float minimumDelay = declaration.delay.x * 1000.0f;
  const int delay = static_cast<int>(
      RandomUnit(random) * (declaration.delay.y - declaration.delay.x) *
          1000.0f +
      minimumDelay);
  action.tagIndex = 0;
  if (declaration.multiTagUseType == FX_MULTI_TAG_USE_RND &&
      action.tagData.num > 1) {
    action.tagIndex = static_cast<int>(
        ((NextRandom(random) >> 10) & 0x7fffu) %
        static_cast<unsigned>(action.tagData.num));
  }
  action.randomAngles.pitch =
      RandomUnit(random) * (declaration.rndRotY.y - declaration.rndRotY.x) +
      declaration.rndRotY.x;
  action.randomAngles.yaw =
      RandomUnit(random) * (declaration.rndRotZ.y - declaration.rndRotZ.x) +
      declaration.rndRotZ.x;
  action.randomAngles.roll =
      RandomUnit(random) * (declaration.rndRotX.y - declaration.rndRotX.x) +
      declaration.rndRotX.x;

  state.startTime = currentTime;
  state.startDelay = delay;
  state.fadeInStartTime = currentTime;
  state.stopTime = delay + currentTime -
                   static_cast<int>(declaration.duration * -1000.0f);
  state.fadeInEndTime = delay + currentTime -
                        static_cast<int>(declaration.fadeInTime * -1000.0f);
  state.shouldTrigger = true;
  state.forceStop = false;
  state.started = false;
  state.fadeOutStartTime =
      delay + currentTime - static_cast<int>(
                                (declaration.duration -
                                 declaration.fadeOutTime) *
                                -1000.0f);
  action.lastParticleDropPos = vec3_origin;
  action.screenPrtHandle = -1;
  action.startAxis = lastAxis;
  action.startOrg = lastOrigin;
}

void idFXManager::EnumerateTags(
    const idList<idAtomicString, 108>& names,
    idList<tagData_t, 108>& tags) {
  if (ta == nullptr) {
    return;
  }
  idPropsCollection* const props = ta->GetPropsCollection();
  for (int index = 0; index < names.num; ++index) {
    const tagData_t tag =
        props->GetTag(idPropInfo::INFO_PROP_NAME, names.list[index].str);
    tags.Append(tag);
    idLib::WarningIf(
        fx_verbose.valueInteger != 0 && !tag.IsValid(),
        "idFXManager::CreateAction cannot find tag %s for %s",
        names.list[index].str, fxDecl->name.str);
  }
}

void idFXManager::CreateAction(int index, idRenderWorld*) {
  if (index >= actions.num) {
    return;
  }
  idFXAction& action = actions.list[index];
  const idFXSingleAction& declaration = fxDecl->events.list[index];
  fxActionState_t& state = actionState.list[index];
  state.startTime = -1;
  state.forceStop = false;
  state.started = false;
  state.shouldTrigger = true;
  const float minimumDelay = declaration.delay.x * 1000.0f;
  state.startDelay = static_cast<int>(
      RandomUnit(random) * (declaration.delay.y - declaration.delay.x) *
          1000.0f +
      minimumDelay);
  action.lastParticleDropPos = vec3_origin;
  action.randomAngles = ang_zero;
  action.screenPrtHandle = -1;
  if (declaration.triggered) {
    state.hidden = true;
  }
  action.tagIndex = 0;
  if (declaration.tagNames.num > 0) {
    EnumerateTags(declaration.tagNames, action.tagData);
  }
  if (declaration.type == FX_MODEL) {
    staticModelManager->FindStaticModel(declaration.modelParms.modelName.str,
                                        true);
  }
}

void idFXManager::ResetTreeAnimator(idTreeAnimator* treeAnimator) {
  ta = treeAnimator;
  if (fxDecl == nullptr || actions.num == 0 ||
      actions.num != fxDecl->events.num) {
    return;
  }
  for (int index = 0; index < fxDecl->events.num; ++index) {
    idList<tagData_t, 108>& tags = actions.list[index].tagData;
    if (tags.size < 0) {
      if (tags.listStatic == 0 || tags.listStatic == 2) {
        if (tags.list != nullptr) {
          idMem::Singleton().Free(tags.list, ALIGN_16);
        }
        tags.list = nullptr;
        tags.size = 0;
      }
      tags.num = 0;
    }
    tags.num = tags.size > 0 ? 0 : tags.size;
    const idFXSingleAction& declaration = fxDecl->events.list[index];
    if (declaration.tagNames.num > 0) {
      EnumerateTags(declaration.tagNames, tags);
    }
  }
}

void idFXManager::Serialize(idSerializer& serializer) {
  int newActionBufferPos = actionBufferPos;
  if (serializer.writing) {
    serializer.msg->WriteBits(newActionBufferPos, 32);
  } else {
    newActionBufferPos = serializer.msg->ReadBits(32);
  }
  if (serializer.writing) {
    serializer.msg->WriteBits(allowSurfaceOnlyInViewID, 32);
  } else {
    allowSurfaceOnlyInViewID = serializer.msg->ReadBits(32);
  }
  if (serializer.writing) {
    serializer.msg->WriteBits(suppressSurfaceInViewID, 32);
  } else {
    suppressSurfaceInViewID = serializer.msg->ReadBits(32);
  }

  for (int index = 0; index < 8; ++index) {
    fxActionCall_t& call = actionBuffer[index];
    if (serializer.writing) {
      serializer.msg->WriteQuantizedVector<idVec3, 32768, 12>(call.org);
    } else {
      call.org.x = static_cast<float>(serializer.msg->ReadBits(-12)) *
                   16.007816f;
      call.org.y = static_cast<float>(serializer.msg->ReadBits(-12)) *
                   16.007816f;
      call.org.z = static_cast<float>(serializer.msg->ReadBits(-12)) *
                   16.007816f;
    }
    serializer.SerializeQ(&call.axis, 15);

    int condition = call.condition;
    if (serializer.writing) {
      serializer.msg->WriteBits(condition, 9);
    } else {
      condition = serializer.msg->ReadBits(9);
    }
    call.condition = static_cast<fxCondition_t>(condition);

    int actionType = call.actionType;
    if (serializer.writing) {
      serializer.msg->WriteBits(actionType, 2);
    } else {
      actionType = serializer.msg->ReadBits(2);
    }
    call.actionType = static_cast<fxActionCallType_t>(actionType);

    int extraCondition = call.extraCondition;
    if (serializer.writing) {
      serializer.msg->WriteBits(extraCondition, 19);
    } else {
      extraCondition = serializer.msg->ReadBits(19);
    }
    call.extraCondition = static_cast<fxExtraCondition_t>(extraCondition);

    if (serializer.writing) {
      serializer.msg->WriteBits(call.tagIdx, 32);
    } else {
      call.tagIdx = serializer.msg->ReadBits(32);
    }
    if (serializer.writing) {
      serializer.msg->WriteBits(call.viewCallbacksID, 32);
    } else {
      call.viewCallbacksID = serializer.msg->ReadBits(32);
    }
    int immediate = call.immediate ? 1 : 0;
    if (serializer.writing) {
      serializer.msg->WriteBits(immediate, 1);
    } else {
      immediate = serializer.msg->ReadBits(1) == 1;
    }
    call.immediate = immediate != 0;
  }

  const int difference = newActionBufferPos - actionBufferPos;
  serializeActionCount = difference;
  if (difference > 8) {
    idLib::Warning(
        "Too many FX Actions serialized [actionBufferPos %d -> "
        "newActionBufferPos %d ] (%d), dropping all but last %d! %s",
        actionBufferPos, newActionBufferPos, difference, 8,
        fxDecl != nullptr ? fxDecl->name.str : "NULL");
    serializeActionCount = 8;
    actionBufferPos = newActionBufferPos - 8;
    if (g_debugFX.valueInteger > 2) {
      for (int offset = 0; offset < 8; ++offset) {
        const int eventNumber = actionBufferPos - offset;
        const fxActionCall_t& call = actionBuffer[eventNumber % 8];
        const idEnumValueInfo* const conditionInfo =
            typeInfoTools->FindEnumValueInfo("fxCondition_t", call.condition,
                                             true);
        idLib::Printf("Action[%d] condition: %s actionType: %d \n",
                      eventNumber, conditionInfo->name.data,
                      call.actionType);
      }
      idLib::Printf("\n");
    }
  } else if (difference < 0) {
    idLib::Warning("Client got ahead of FX serialization. %s",
                   fxDecl != nullptr ? fxDecl->name.str : "NULL");
    serializeActionCount = 0;
    actionBufferPos = newActionBufferPos;
  }
}

void idFXManager::FreeActions() {
  for (int index = 0; index < actions.num; ++index) {
    idFXAction& action = actions.list[index];
    if (action.rLight != nullptr) {
      action.rLight->Delete();
      action.rLight = nullptr;
    }
    gameLibEffects->lensFlareManager->RemoveFlare(action.flareHandle);
    action.ribbonManager.Shutdown();
    if (action.tagData.listStatic == 0 || action.tagData.listStatic == 2) {
      if (action.tagData.list != nullptr) {
        idMem::Singleton().Free(action.tagData.list, ALIGN_16);
      }
      action.tagData.list = nullptr;
      action.tagData.size = 0;
    }
    action.tagData.num = 0;
  }

  if (actions.listStatic == 0 || actions.listStatic == 2) {
    if (actions.list != nullptr) {
      idListArrayDelete<idFXAction>(actions.list, actions.size);
    }
    actions.list = nullptr;
    actions.size = 0;
  }
  actions.num = 0;
  if (actionState.listStatic == 0 || actionState.listStatic == 2) {
    if (actionState.list != nullptr) {
      idMem::Singleton().Free(actionState.list, ALIGN_16);
    }
    actionState.list = nullptr;
    actionState.num = 0;
    actionState.size = 0;
  } else {
    actionState.num = 0;
  }
}

void idFXManager::Shutdown() {
  if (!initialized) {
    return;
  }
  StopAllFX(-1, true);
  initialized = false;
  for (int index = 0; index < actions.num; ++index) {
    idFXAction& action = actions.list[index];
    const idFXSingleAction& declaration = fxDecl->events.list[index];
    if (action.rParticle != nullptr) {
      UnlinkRenderModel(action.rParticle);
      gameLibEffects->fxModelRecycler.RecycleParticleFxModel(
          declaration.particleParms.declPrt, action.rParticle);
      action.rParticle = nullptr;
    }
    if (action.rModel != nullptr) {
      UnlinkRenderModel(action.rModel);
      gameLibEffects->fxModelRecycler.RecycleStaticFxModel(
          declaration.modelParms.modelName, action.rModel);
      action.rModel = nullptr;
    }
  }
  FreeActions();
  ta = nullptr;
}

idFXManager::idFXManager()
    : initialized(false), fxDecl(nullptr), gameLibEffects(nullptr), ta(nullptr),
      rw(nullptr), systemColor{1.0f, 1.0f, 1.0f, 1.0f}, random{0},
      soundInfo(), actions(), actionState(), remote(false),
      allowSurfaceOnlyInViewID(0), suppressSurfaceInViewID(0),
      viewCallbacksID(-1), owner(), declChangeId(-1), actionBufferPos(0),
      serializeActionCount(0) {
  if (actions.listStatic == 0 || actions.listStatic == 2) {
    if (actions.list != nullptr) {
      idListArrayDelete<idFXAction>(actions.list, actions.size);
    }
    actions.list = nullptr;
    actions.size = 0;
  }
  actions.num = 0;
  actionState.ClearFree();
  externalPosition = vec3_origin;
  std::memset(&externalRotation, 0, sizeof(externalRotation));
  hasExternalPositionAndRotation = false;
  std::memset(actionBuffer, 0, sizeof(actionBuffer));
}

idFXManager::~idFXManager() { Shutdown(); }

void idFXManager::Init(const idDeclFX* declaration,
                       idRenderWorld* renderWorld,
                       fxEmitterSound_t* emitterInfo,
                       idGameLibEffects* effects, float diversity,
                       idTreeAnimator* treeAnimator, const idStr ownerName) {
  if (declaration == nullptr || g_skipFXManager.valueInteger != 0) {
    return;
  }
  fxDecl = declaration;
  gameLibEffects = effects;
  ta = treeAnimator;
  rw = renderWorld;
  random.seed = static_cast<std::uint32_t>(
      static_cast<int>(diversity * 65535.0f));

  const int actionCount = declaration->events.num;
  if (actionCount <= actions.size || actions.Resize(actionCount)) {
    actions.num = std::min(actionCount, actions.size);
  }
  if (actionCount <= actionState.size ||
      actionState.Resize(actionCount)) {
    actionState.num = std::min(actionCount, actionState.size);
  }
  declChangeId = fxDecl->changeId;
  if (emitterInfo != nullptr) {
    soundInfo.emitter = emitterInfo->emitter;
    soundInfo.channel = emitterInfo->channel;
    soundInfo.padding = emitterInfo->padding;
  }
  for (int index = 0; index < fxDecl->events.num; ++index) {
    CreateAction(index, rw);
  }
  owner = ownerName;
  initialized = true;
}

void idFXManager::Update(const idVec3& origin, const idMat3& axis,
                         const idVec3& velocity, int currentTime,
                         int gameMillisecondsPerFrame, int serverCurrentTime,
                         float attenuation, float shakeScale) {
  if (g_skipFXManager.valueInteger != 0 || !initialized) {
    return;
  }
  while (serializeActionCount > 0) {
    if (g_debugFX.valueInteger > 3) {
      idLib::Printf(
          "%s %s Catching up on serialized event. serializeActionCount:%d "
          "\n",
          fxDecl != nullptr ? fxDecl->name.str : "NULL", GetName(),
          serializeActionCount);
    }
    --serializeActionCount;
    fxActionCall_t& call = actionBuffer[actionBufferPos % 8];
    call.time = currentTime;
    viewCallbacksID = call.viewCallbacksID;
    if (call.actionType == FXACTION_START) {
      if (initialized) {
        StartActions(call.org, call.axis, call.time, call.condition,
                     call.extraCondition, call.tagIdx);
        ++actionBufferPos;
      }
    } else if (call.actionType == FXACTION_STOP ||
               call.actionType == FXACTION_STOP_ALL) {
      StopActions(call.time, call.condition, call.extraCondition,
                  call.actionType == FXACTION_STOP_ALL, call.immediate);
      ++actionBufferPos;
    }
  }
  UpdateActions(origin, axis, velocity, currentTime,
                gameMillisecondsPerFrame, serverCurrentTime, attenuation,
                shakeScale);
}
