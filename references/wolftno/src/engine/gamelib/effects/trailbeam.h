#pragma once

#include "shared/idlib/math/vector.h"

#include <cstddef>

class idDeclTrailBeam;
class idRenderModelBeam;
class idRenderModelEffects;

class idTrailBeam {
public:
  idTrailBeam();

  void Init(idRenderModelBeam* beamModel,
            idRenderModelEffects* effectModel,
            const idDeclTrailBeam* decl);
  void StartTrail(int startTime, const idVec3& start,
                  const idVec3& end);
  void StopTrail();
  void Update(int currentTime, int gameMsPerFrame);

  idRenderModelBeam* beamModel;
  idRenderModelEffects* effectModel;
  const idDeclTrailBeam* decl;
  idVec3 start;
  idVec3 end;
  int timestamp;
  float updateStartLength;
  float updateEndLength;
  std::uint32_t recoveredPadding;
};

static_assert(offsetof(idTrailBeam, start) == 0x18,
              "trail-beam start offset drift");
static_assert(offsetof(idTrailBeam, timestamp) == 0x30,
              "trail-beam timestamp offset drift");
static_assert(offsetof(idTrailBeam, updateStartLength) == 0x34,
              "trail-beam particle cursor offset drift");
static_assert(sizeof(idTrailBeam) == 0x40, "idTrailBeam ABI drift");
