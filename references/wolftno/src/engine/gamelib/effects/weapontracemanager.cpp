#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#include "engine/gamelib/effects/weapontracemanager.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <cstdint>
#include <xmmintrin.h>

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

extern bool forceIdListsToReallocateEveryAppend;
idRenderDebug* GetGameLibRenderDebug();

idCVar debug_weaponTrace(
    "debug_weaponTrace", "0", CVAR_BOOL, "f : %.1f degrees\n",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

template <typename Type, int Tag>
__forceinline void ClearListExact(idList<Type, Tag>& list) {
  if (list.listStatic == 0 || list.listStatic == 2) {
    if (list.list != nullptr) {
      idMem::Singleton().Free(list.list, ALIGN_16);
    }
    list.list = nullptr;
    list.size = 0;
  }
  list.num = 0;
}

__forceinline float InverseLengthExact(float x, float y, float z) {
  const float squaredXY = y * y + x * x;
  const float squaredLength = squaredXY + z * z;
  const __m128 length = _mm_set_ss(squaredLength);
  const __m128 clamped =
      _mm_max_ps(length, idMath::vector_float_smallestNonDenorm);
  const __m128 inverse =
      _mm_div_ps(_mm_set1_ps(1.0f), _mm_sqrt_ps(clamped));
  return _mm_cvtss_f32(inverse);
}

__forceinline void SetDirection(weaponTrace_t& trace) {
  const float x = trace.end.x - trace.start.x;
  const float y = trace.end.y - trace.start.y;
  const float z = trace.end.z - trace.start.z;
  const float inverseLength = InverseLengthExact(x, y, z);
  trace.dir.y = y * inverseLength;
  trace.dir.z = z * inverseLength;
  trace.dir.x = inverseLength * x;
}

} // namespace

template <>
__declspec(noinline) bool idList<weaponTrace_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearListExact(*this);
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }

  weaponTrace_t* const replacement = static_cast<weaponTrace_t*>(
      idListArrayResize<weaponTrace_t>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
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
__declspec(noinline) weaponTrace_t& idList<weaponTrace_t, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      const int rounded = required + granularity - 1;
      newSize = rounded - rounded % granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    Resize(newSize);
  }

  const int index = num;
  if (index >= size) {
    return list[size - 1];
  }
  ++num;
  return list[index];
}

idWeaponTraceManager::idWeaponTraceManager() : currentDeferred(0) {
  ShutDown();
}

void idWeaponTraceManager::Init() {
  ClearListExact(weaponTraces[0]);
  ClearListExact(weaponTraces[1]);
  currentDeferred = 0;
}

void idWeaponTraceManager::ShutDown() {
  ClearListExact(weaponTraces[0]);
  ClearListExact(weaponTraces[1]);
  currentDeferred = 0;
}

void idWeaponTraceManager::Update() {
  currentDeferred ^= 1;
  ClearListExact(weaponTraces[currentDeferred]);
}

void idWeaponTraceManager::AddTrace(const idVec3& start, const idVec3& end,
                                    idSpawnId spawnId,
                                    const idDeclDamage* damage) {
  idList<weaponTrace_t, 5>& traces = weaponTraces[currentDeferred];
  if (traces.num < MAX_WEAPON_TRACES) {
    weaponTrace_t& trace = traces.Alloc();
    trace.start = start;
    trace.end = end;
    trace.projectile.value = 0x1FFF;
    trace.attacker = spawnId;
    trace.damageDecl = damage;
    trace.radius = 0.0f;
    trace.weaponTraceType = weaponTrace_t::WEAPON_TRACE_BULLET;
  }
}

void idWeaponTraceManager::AddExplosion(const idVec3& origin, float radius,
                                        idSpawnId attacker,
                                        const idDeclDamage* damage) {
  idList<weaponTrace_t, 5>& traces = weaponTraces[currentDeferred];
  if (traces.num < MAX_WEAPON_TRACES) {
    weaponTrace_t& trace = traces.Alloc();
    trace.start = origin;
    trace.radius = radius;
    trace.attacker = attacker;
    trace.projectile.value = 0x1FFF;
    trace.damageDecl = damage;
    trace.weaponTraceType = weaponTrace_t::WEAPON_TRACE_EXPLOSION;
  }
}

void idWeaponTraceManager::AddProjectile(const idVec3& origin, float radius,
                                         idSpawnId attacker,
                                         idSpawnId projectile,
                                         const idDeclDamage* damage) {
  idList<weaponTrace_t, 5>& traces = weaponTraces[currentDeferred];
  if (traces.num < MAX_WEAPON_TRACES) {
    weaponTrace_t& trace = traces.Alloc();
    trace.start = origin;
    trace.radius = radius;
    trace.attacker = attacker;
    trace.weaponTraceType = weaponTrace_t::WEAPON_TRACE_PROJECTILE;
    trace.projectile = projectile;
    trace.damageDecl = damage;
  }
}

const int idWeaponTraceManager::GetCollisions(
    const idBounds& inBounds, idList<weaponTrace_t, 5>& outTraces) {
  ClearListExact(outTraces);

  const idVec4 green{0.0f, 1.0f, 0.0f, 1.0f};
  if (debug_weaponTrace.valueInteger != 0) {
    const idVec4 white{1.0f, 1.0f, 1.0f, 1.0f};
    GetGameLibRenderDebug()->DebugBounds(
        white, inBounds, vec3_origin, 0, false);
  }

  int index = 0;
  const int deferred = currentDeferred ^ 1;
  if (weaponTraces[deferred].num <= 0) {
    return outTraces.num;
  }

  do {
    if (outTraces.num == outTraces.size) {
      return outTraces.num;
    }

    weaponTrace_t* const source =
        &weaponTraces[deferred].list[index];
    switch (source->weaponTraceType) {
    case weaponTrace_t::WEAPON_TRACE_BULLET:
      if (inBounds.LineIntersection(source->start, source->end)) {
        weaponTrace_t& output = outTraces.Alloc();
        output.start = source->start;
        output.end = source->end;
        output.impactPoint = source->impactPoint;
        SetDirection(output);
        output.radius = 0.0f;
        output.attacker = source->attacker;
        output.projectile = source->projectile;
        output.weaponTraceType = weaponTrace_t::WEAPON_TRACE_BULLET;
        output.damageDecl = source->damageDecl;
        if (debug_weaponTrace.valueInteger != 0) {
          GetGameLibRenderDebug()->DebugLine(
              green, source->start, source->end, 3000, false);
        }
      }
      break;

    case weaponTrace_t::WEAPON_TRACE_EXPLOSION: {
      const idSphere sphere{source->start, source->radius};
      if (inBounds.SphereIntersection(sphere)) {
        weaponTrace_t& output = outTraces.Alloc();
        output.start = source->start;
        output.end.x = (inBounds.b[1].x + inBounds.b[0].x) * 0.5f;
        output.end.z = (inBounds.b[1].z + inBounds.b[0].z) * 0.5f;
        output.end.y = (inBounds.b[1].y + inBounds.b[0].y) * 0.5f;
        output.impactPoint = output.end;
        SetDirection(output);
        output.radius = source->radius;
        output.attacker = source->attacker;
        output.projectile = source->projectile;
        output.weaponTraceType = weaponTrace_t::WEAPON_TRACE_EXPLOSION;
        output.damageDecl = source->damageDecl;
        if (debug_weaponTrace.valueInteger != 0) {
          idRenderDebug* const debug = GetGameLibRenderDebug();
          const idSphere debugSphere{source->start, source->radius};
          debug->DebugSphere(green, debugSphere, 12, 3000, false);
        }
      }
      break;
    }

    case weaponTrace_t::WEAPON_TRACE_PROJECTILE: {
      const idSphere sphere{source->start, source->radius};
      if (inBounds.SphereIntersection(sphere)) {
        weaponTrace_t& output = outTraces.Alloc();
        output.start = source->start;
        output.end = source->start;
        output.impactPoint = source->start;
        output.dir.x = 0.0f;
        output.dir.y = 0.0f;
        output.dir.z = 0.0f;
        output.radius = source->radius;
        output.attacker = source->attacker;
        output.projectile = source->projectile;
        output.weaponTraceType = weaponTrace_t::WEAPON_TRACE_PROJECTILE;
        output.damageDecl = source->damageDecl;
        if (debug_weaponTrace.valueInteger != 0) {
          idRenderDebug* const debug = GetGameLibRenderDebug();
          const idSphere debugSphere{source->start, source->radius};
          debug->DebugSphere(green, debugSphere, 12, 0, false);
        }
      }
      break;
    }

    default:
      break;
    }
    ++index;
  } while (index < weaponTraces[deferred].num);

  return outTraces.num;
}
