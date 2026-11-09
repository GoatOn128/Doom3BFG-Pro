#pragma once

#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

class idDeclRibbon;
class idRibbon;

struct ribbonHandle_t {
  __forceinline ribbonHandle_t() : type(-1), index(-1), id(-1) {}

  int type;
  int index;
  int id;
};

static_assert(sizeof(ribbonHandle_t) == 0xC, "ribbon handle ABI drift");

class idRibbonModelManager {
public:
  static constexpr int MAX_RIBBON_EFFECT_TYPES = 4;
  static constexpr int MAX_RIBBON_EFFECTS_FOR_EACH_TYPE = 16;

  struct ribbonEffects_t {
    using idListAllocReturnsReferenceMarker = void;

    ribbonEffects_t() : ribbons(), handles(), next(0) {}
    ~ribbonEffects_t() noexcept(false) = default;

    idStaticList<idRibbon*, MAX_RIBBON_EFFECTS_FOR_EACH_TYPE> ribbons;
    idStaticList<int, MAX_RIBBON_EFFECTS_FOR_EACH_TYPE> handles;
    int next;
  };

  idRibbonModelManager();
  ~idRibbonModelManager() noexcept(false);

  void Init();
  __declspec(noinline) void Shutdown();
  __declspec(noinline) void
  CreateRibbonEffectModelType(const idDeclRibbon* ribbonDecl);
  __declspec(noinline) ribbonHandle_t
  AllocRibbonEffectModel(const idDeclRibbon* ribbonDecl);

  idStaticList<ribbonEffects_t, MAX_RIBBON_EFFECT_TYPES> ribbonEffects;
  int handleId;
};

static_assert(sizeof(idRibbonModelManager::ribbonEffects_t) == 0xF8,
              "ribbon effect slot ABI drift");
static_assert(offsetof(idRibbonModelManager::ribbonEffects_t, ribbons) == 0,
              "ribbon pointer-list offset drift");
static_assert(offsetof(idRibbonModelManager::ribbonEffects_t, handles) == 0x98,
              "ribbon handle-list offset drift");
static_assert(offsetof(idRibbonModelManager::ribbonEffects_t, next) == 0xF0,
              "ribbon next-index offset drift");
static_assert(sizeof(idRibbonModelManager) == 0x400,
              "ribbon model manager ABI drift");
static_assert(offsetof(idRibbonModelManager, handleId) == 0x3F8,
              "ribbon model manager handle offset drift");
