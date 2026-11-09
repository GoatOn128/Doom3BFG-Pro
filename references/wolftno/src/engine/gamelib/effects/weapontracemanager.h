#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/staticlist.h"
#include "engine/gamelib/idspawnid_abi.h"

#include <cstddef>

class idDeclDamage;

struct weaponTrace_t {
  using idListAllocReturnsReferenceMarker = void;

  enum weaponTraceType_t {
    WEAPON_TRACE_BULLET = 0,
    WEAPON_TRACE_EXPLOSION = 1,
    WEAPON_TRACE_PROJECTILE = 2,
  };

  weaponTrace_t() : attacker(), projectile() {}

  idVec3 start;
  idVec3 end;
  idVec3 impactPoint;
  idVec3 dir;
  idSpawnId attacker;
  idSpawnId projectile;
  const idDeclDamage* damageDecl;
  float radius;
  weaponTraceType_t weaponTraceType;
};

class idWeaponTraceManager {
public:
  static constexpr int MAX_WEAPON_TRACES = 6;

  idWeaponTraceManager();
  ~idWeaponTraceManager();

  void Init();
  void Update();
  void ShutDown();
  void AddTrace(const idVec3& start, const idVec3& end, idSpawnId spawnId,
                const idDeclDamage* damage);
  void AddExplosion(const idVec3& origin, float radius, idSpawnId attacker,
                    const idDeclDamage* damage);
  void AddProjectile(const idVec3& origin, float radius, idSpawnId attacker,
                     idSpawnId projectile, const idDeclDamage* damage);
  const int GetCollisions(const idBounds& inBounds,
                          idList<weaponTrace_t, 5>& outTraces);

  idStaticList<weaponTrace_t, MAX_WEAPON_TRACES> weaponTraces[2];
  int currentDeferred;
};

template <>
bool idList<weaponTrace_t, 5>::Resize(int newSize);
template <>
weaponTrace_t& idList<weaponTrace_t, 5>::Alloc();

static_assert(sizeof(weaponTrace_t) == 0x48, "weapon trace ABI drift");
static_assert(offsetof(weaponTrace_t, start) == 0x00,
              "weapon trace start offset drift");
static_assert(offsetof(weaponTrace_t, end) == 0x0C,
              "weapon trace end offset drift");
static_assert(offsetof(weaponTrace_t, impactPoint) == 0x18,
              "weapon trace impact offset drift");
static_assert(offsetof(weaponTrace_t, dir) == 0x24,
              "weapon trace direction offset drift");
static_assert(offsetof(weaponTrace_t, attacker) == 0x30,
              "weapon trace attacker offset drift");
static_assert(offsetof(weaponTrace_t, projectile) == 0x34,
              "weapon trace projectile offset drift");
static_assert(offsetof(weaponTrace_t, damageDecl) == 0x38,
              "weapon trace damage offset drift");
static_assert(offsetof(weaponTrace_t, radius) == 0x40,
              "weapon trace radius offset drift");
static_assert(offsetof(weaponTrace_t, weaponTraceType) == 0x44,
              "weapon trace type offset drift");
using weaponTraceStaticList_t = idStaticList<weaponTrace_t, 6>;
static_assert(sizeof(weaponTraceStaticList_t) == 0x1C8,
              "weapon trace static-list ABI drift");
static_assert(offsetof(weaponTraceStaticList_t, staticList) == 0x18,
              "weapon trace static storage offset drift");
static_assert(sizeof(idWeaponTraceManager) == 0x398,
              "weapon trace manager ABI drift");
static_assert(offsetof(idWeaponTraceManager, weaponTraces) == 0x00,
              "weapon trace list offset drift");
static_assert(offsetof(idWeaponTraceManager, currentDeferred) == 0x390,
              "weapon trace deferred-index offset drift");

extern class idCVar debug_weaponTrace;
