#pragma once

#include "engine/models/cloth/jobs/clothgen.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/bv/sphere.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/color.h"
#include "shared/idlib/math/plane.h"

class idParallelJobList;
class idRenderDebug;
class idCVar;

struct idClothSimDebugCallbacks {
  void (*debugBounds)(idRenderDebug*, const idColor&, const idBounds&,
                      const idVec3&, int, bool){};
  void (*debugText)(idRenderDebug*, const char*, const idVec3&, float,
                    const idColor&, int, bool){};
  void (*debugLine)(idRenderDebug*, const idColor&, const idVec3&,
                    const idVec3&, int, bool){};
  void (*debugSphere)(idRenderDebug*, const idColor&, const idSphere&, int,
                      int, bool){};
};

void SetClothSimDebugCallbacks(const idClothSimDebugCallbacks* callbacks);
extern idCVar g_showClothBounds;

class idClothSim {
public:
  static constexpr int MAX_CLOTH_COLLISIONS_SIZE = 1024;
  static constexpr int MAX_CLOTH_COLLISIONS = 100;

  explicit idClothSim(const idDeclCloth* clothSystem);
  idClothSim(int w, int h, float hGridSpacing, float vGridSpacing,
             const idDeclCloth* clothSystem, bool rect);
  ~idClothSim();

  void Reset();
  void DebugDraw(const idVec3& offset, const idMat3& axis,
                 idRenderDebug* renderDebug, int debugLevel);
  void SetOriginAxis(const idVec3& newOrigin, const idMat3& newAxis,
                     bool updateParticles);
  void SetAnchor(int index, bool tempAnchor);
  void SetAnchorPosition(int index, const idVec3& org);
  void SetAnchorAxis(int index, const idMat3& modelAxis);
  void AddCollisionSphere(const idSphere& collisionSphere,
                          bool isAPermanentSphere);
  void GetRopeParticleOriginAxis(int index, idList<idVec3, 5>& originList,
                                 idList<idMat3, 5>& axisList);
  void AddWeaponTrace(const idVec3& start, const idVec3& dir, float strength,
                      bool causesHoles);
  void Run(idParallelJobList* parallelJobList);
  void Swap();

  const idDeclCloth* clothDecl{};
  int width{};
  int height{};
  float hSpacing{};
  float vSpacing{};
  idVec3 origin{};
  idMat3 axis{};
  idVec3 windDirection{};
  idVec3 gravity{};
  int numIterations{1};
  float timeDelta{0.016000001f};
  idClothParticle* cloth{};
  int numClothParticles{};
  int currentDeferred{};
  idBounds* clothBoundsFromJob{};
  idBounds clothBounds{};
  clothParms_t* clothParms{};
  bool isRect{};
  std::uint8_t recoveredPadding[7]{};
  idList<idClothSpring, 81> springs;
  idList<idSphere, 81> permanentCollisionSpheres;
  idList<idSphere, 81> temporaryCollisionSpheres[2];
  idList<clothWeaponTrace_t, 81> weaponTraces[2];
  idPlane collisionPlane[2]{};

private:
  void GenerateGeneric();
  void GenerateRope();
  void GenerateSquare();
  void GenerateTriangle(const idDeclCloth* clothSystem);
  void GenerateRect(const idDeclCloth* clothSystem);
  int RopeParticleOffset(int index) const;
};

static_assert(sizeof(idClothSim) == 344, "idClothSim ABI drift");
static_assert(offsetof(idClothSim, springs) == 168, "idClothSim springs offset drift");
static_assert(offsetof(idClothSim, collisionPlane) == 312,
              "idClothSim collisionPlane offset drift");
