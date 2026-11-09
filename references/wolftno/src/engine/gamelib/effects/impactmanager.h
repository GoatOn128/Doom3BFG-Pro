#pragma once

#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idDeclParticle;
class idDeclRenderParm;
class idEffectsModelManager;
class idRenderModelParticle;
class idSerializer;
struct particleMeshDist_t;
template <typename T> class idCodeResource;

class idImpactManager {
public:
  struct impactState_t {
    using idListAllocReturnsReferenceMarker = void;

    impactState_t()
        : pmodel(nullptr), startTime(0), endTime(0) {}

    idRenderModelParticle* pmodel;
    int startTime;
    int endTime;
  };

  struct impactBindState_t {
    using idListAllocReturnsReferenceMarker = void;

    impactBindState_t()
        : bindToParent(false), relOrg(vec3_origin), relRot(mat3_identity) {}

    bool bindToParent;
    std::uint8_t recoveredPadding[3];
    idVec3 relOrg;
    idMat3 relRot;
  };

  idImpactManager();
  ~idImpactManager();

  void Init(idEffectsModelManager* effectsModelManager, float diversity,
            int maxImpacts, int playerIndex);
  void Shutdown();
  void Update(const idVec3& parentOrigin, const idMat3& parentAxis,
              int localTime, int playerIndex);
  void UseImpact(const idVec3& origin, const idMat3& axis,
                 const idDeclParticle* particleDecl, int startTime,
                 bool bindToParent, const idVec3& parentOrigin,
                 const idMat3& parentAxis, int playerIndex, float distSqr,
                 const idColor& color, int numCycles,
                 const particleMeshDist_t* mesh);
  void StopImpacts();
  void Serialize(idSerializer& serializer);

  idList<impactState_t, 5> impacts;
  idList<impactBindState_t, 5> impactBinds;
  int next;
  idRandom2 random;
  bool initialized;
  std::uint8_t recoveredPadding[7];
  idEffectsModelManager* effectsModelManager;
};

static_assert(sizeof(idImpactManager::impactState_t) == 0x10,
              "impact state ABI drift");
static_assert(offsetof(idImpactManager::impactState_t, pmodel) == 0,
              "impact model offset drift");
static_assert(offsetof(idImpactManager::impactState_t, startTime) == 8,
              "impact start-time offset drift");
static_assert(offsetof(idImpactManager::impactState_t, endTime) == 0xC,
              "impact end-time offset drift");
static_assert(sizeof(idImpactManager::impactBindState_t) == 0x34,
              "impact bind state ABI drift");
static_assert(offsetof(idImpactManager::impactBindState_t, bindToParent) == 0,
              "impact bind flag offset drift");
static_assert(offsetof(idImpactManager::impactBindState_t, relOrg) == 4,
              "impact relative-origin offset drift");
static_assert(offsetof(idImpactManager::impactBindState_t, relRot) == 0x10,
              "impact relative-axis offset drift");
static_assert(sizeof(idImpactManager) == 0x48,
              "idImpactManager ABI drift");
static_assert(offsetof(idImpactManager, impacts) == 0,
              "impact list offset drift");
static_assert(offsetof(idImpactManager, impactBinds) == 0x18,
              "impact bind list offset drift");
static_assert(offsetof(idImpactManager, next) == 0x30,
              "impact next offset drift");
static_assert(offsetof(idImpactManager, random) == 0x34,
              "impact random offset drift");
static_assert(offsetof(idImpactManager, initialized) == 0x38,
              "impact initialized offset drift");
static_assert(offsetof(idImpactManager, effectsModelManager) == 0x40,
              "impact effects-manager offset drift");

extern class idCVar g_debugImpactMgr;
extern idCodeResource<idDeclRenderParm> rp_particleLOD;
