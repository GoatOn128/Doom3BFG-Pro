#pragma once

#include "engine/gamelib/effectphysics/effectphysicspieceemitter.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idDeclBreakable;
class idRenderModelEffects;
class idRenderWorld;

class idEffectPhysicsDebrisEmitter {
public:
  static constexpr int EMIT_DEBRIS_BUFFER_SIZE = 10;

  struct emitDebris_t {
    idVec3 pos;
    idVec3 normal;
    int count;
  };

  void Init(const idDeclBreakable* breakableDebris, int entityNumber,
            idClip* clip, idRenderWorld* world,
            idRenderModelEffects* renderModelEffects);
  void EmitDebris(idVec3 pos, idVec3 velocity, idVec3 normal,
                  int count, int currentTime);
  void Update(const idVec3& origin, const idMat3& axis,
              const idVec3& drawScale3D, int currentTime,
              int gameMsPerFrame);

  idEffectPhysicsPieceEmitter piecePhysics;
  emitDebris_t emitBuffer[EMIT_DEBRIS_BUFFER_SIZE];
  int emitBufferPos;
  int serializeEmitCount;
  bool skipFirstFrame;
  bool active;
  std::uint8_t flagPadding[2];
  idRandom2 random;
};

static_assert(sizeof(idRandom2) == 0x04, "idRandom2 ABI drift");
static_assert(sizeof(idEffectPhysicsDebrisEmitter::emitDebris_t) == 0x1C,
              "debris replay record ABI drift");
static_assert(offsetof(idEffectPhysicsDebrisEmitter, emitBuffer) == 0xF8,
              "debris replay buffer ABI drift");
static_assert(offsetof(idEffectPhysicsDebrisEmitter, emitBufferPos) == 0x210,
              "debris buffer position ABI drift");
static_assert(offsetof(idEffectPhysicsDebrisEmitter, serializeEmitCount) ==
                  0x214,
              "debris serialization count ABI drift");
static_assert(offsetof(idEffectPhysicsDebrisEmitter, skipFirstFrame) == 0x218,
              "debris flags ABI drift");
static_assert(offsetof(idEffectPhysicsDebrisEmitter, random) == 0x21C,
              "debris random ABI drift");
static_assert(sizeof(idEffectPhysicsDebrisEmitter) == 0x220,
              "idEffectPhysicsDebrisEmitter ABI drift");
