#pragma once

#include "shared/idlib/math/vector.h"
#include "shared/idlib/networking/bitmsg.h"

#include <cstddef>
#include <cstdint>

class idCVar;
class idMaterial;
class idRenderModelEffects;
class idSerializer;

struct tracerManagerRecord_t {
  int startTime;
  int lifeTime;
  float speed;
  idVec3 origin;
  idVec3 dir;
  float length;
  float height;
  std::uint32_t recoveredPadding0;
  const idMaterial* mat;
  idVec3 maxDist;
  bool ensureVisual;
  std::uint8_t recoveredPadding1[3];
  float fractionInsured;
  int playerIndex;
};

class idTracerManager {
public:
  static constexpr int MAX_TRACERS = 4;

  idTracerManager();

  void Init(idRenderModelEffects* effectsModel, int playerIndex);
  void AddTracer(const idMaterial* material, const idVec3& origin,
                 const idVec3& direction, const idVec3& maximumDistance,
                 float speed, float length, float height, bool ensureVisual,
                 float fractionInsured, int playerIndex = 0);
  void Update(int time);
  void Serialize(idSerializer* serializer);

  idRenderModelEffects* effectsModel;
  tracerManagerRecord_t tracerBuffer[MAX_TRACERS];
  int tracerBufferPos;
  int serializeTracerCount;
  bool catchingUp;
  std::uint8_t recoveredPadding[3];
  int localPlayerIndex;
};

extern idCVar net_tracerMgrDebug;

template <>
void idBitMsg::WriteQuantizedVector<idVec3, 32768, 12>(
    const idVec3& vector);
template <>
void idBitMsg::WriteQuantizedVector<idVec3, 1, 12>(
    const idVec3& vector);

static_assert(sizeof(tracerManagerRecord_t) == 0x50,
              "tracer manager record ABI drift");
static_assert(offsetof(tracerManagerRecord_t, speed) == 0x08,
              "tracer speed offset drift");
static_assert(offsetof(tracerManagerRecord_t, origin) == 0x0C,
              "tracer origin offset drift");
static_assert(offsetof(tracerManagerRecord_t, dir) == 0x18,
              "tracer direction offset drift");
static_assert(offsetof(tracerManagerRecord_t, length) == 0x24,
              "tracer length offset drift");
static_assert(offsetof(tracerManagerRecord_t, mat) == 0x30,
              "tracer material offset drift");
static_assert(offsetof(tracerManagerRecord_t, maxDist) == 0x38,
              "tracer maximum-distance offset drift");
static_assert(offsetof(tracerManagerRecord_t, ensureVisual) == 0x44,
              "tracer visual flag offset drift");
static_assert(offsetof(tracerManagerRecord_t, fractionInsured) == 0x48,
              "tracer insured-fraction offset drift");
static_assert(offsetof(tracerManagerRecord_t, playerIndex) == 0x4C,
              "tracer player offset drift");
static_assert(offsetof(idTracerManager, tracerBuffer) == 0x08,
              "tracer buffer offset drift");
static_assert(offsetof(idTracerManager, tracerBufferPos) == 0x148,
              "tracer buffer position offset drift");
static_assert(offsetof(idTracerManager, serializeTracerCount) == 0x14C,
              "tracer serialized-count offset drift");
static_assert(offsetof(idTracerManager, catchingUp) == 0x150,
              "tracer catch-up flag offset drift");
static_assert(offsetof(idTracerManager, localPlayerIndex) == 0x154,
              "tracer local-player offset drift");
static_assert(sizeof(idTracerManager) == 0x158,
              "idTracerManager ABI drift");
