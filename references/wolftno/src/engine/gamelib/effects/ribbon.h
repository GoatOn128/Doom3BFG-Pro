#pragma once

#include "shared/idlib/math/random.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idDeclRibbon;
class idRenderModelBeam;

enum ribbonState_t : int {
  RIBBON_ACTIVE = 0,
  RIBBON_DEACTIVATING = 1,
  RIBBON_INACTIVE = 2,
};

class idRibbon {
public:
  static constexpr int MAX_NUM_RIBBON_NODES = 128;

  explicit idRibbon(const idDeclRibbon* decl);

  void StartRibbon(int spawnTime, const idVec3& spawnOrigin);
  void RemoveOldNodes(int currentTime);
  void UpdatePosition(int currentTime, const idVec3& origin,
                      const idVec3& origin2, const idMat3& axis,
                      const idVec3& velocity, const idVec4& color);
  void UpdatePosition(int currentTime, const idVec3& origin,
                      const idMat3& axis, const idVec3& velocity,
                      const idVec4& color);
  void UpdateGeometry(int currentTime, idRenderModelBeam* beamModel,
                      const idMat3& axis, const idVec3& translate);
  int GetRemainingTime(int currentTime) const;

  ribbonState_t ribbonState;
  idVec3 spawnOrg;
  int spawnTime;
  int numActiveNodes;
  int currHeadIdx;
  float startVariance;
  int prevTime;
  idVec3 prevOrigin;
  idVec3 prevOrigin2;
  std::uint8_t recoveredPadding0[4];
  const idDeclRibbon* ribbonDecl;
  idVec4 globalColor;

private:
  struct ribbonNode_t {
    ribbonNode_t() { Clear(); }

    void Clear() {
      origin = {0.0f, 0.0f, 0.0f};
      origin2 = {0.0f, 0.0f, 0.0f};
      velocity = {0.0f, 0.0f, 0.0f};
      orientVec = {0.0f, 0.0f, 0.0f};
      color = {0.0f, 0.0f, 0.0f, 0.0f};
      spawnTime = 0;
      nodeLength = 0.0f;
    }

    idVec3 origin;
    idVec3 origin2;
    idVec3 velocity;
    idVec3 orientVec;
    idVec4 color;
    int spawnTime;
    float nodeLength;
  };

  void ResetNodes();
  void AddNode(int currentTime, const idVec3& origin,
               const idVec3& origin2, const idMat3& axis,
               const idVec3& velocity, const idVec4& color);
  void CalcTurbulence(const idVec3& origin, const idMat3& axis,
                      idVec3& turbulence) const;
  void CalcHelix(int currentTime, const idVec3& origin,
                 const idMat3& axis, const idVec3& velocity,
                 idVec3& helix) const;

  ribbonNode_t ribbonNodes[MAX_NUM_RIBBON_NODES];
  bool explicitDefineBothEdges;
  std::uint8_t recoveredTailPadding[7];

  static idRandom2 random;
};

static_assert(sizeof(idRibbon) == 0x2460, "idRibbon ABI drift");
static_assert(offsetof(idRibbon, spawnOrg) == 4,
              "ribbon spawn-origin offset drift");
static_assert(offsetof(idRibbon, prevOrigin) == 0x24,
              "ribbon previous-origin offset drift");
static_assert(offsetof(idRibbon, ribbonDecl) == 0x40,
              "ribbon declaration offset drift");
static_assert(offsetof(idRibbon, globalColor) == 0x48,
              "ribbon global-color offset drift");
