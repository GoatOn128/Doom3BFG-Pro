#pragma once

#include "engine/gamelib/physics/clip.h"
#include "shared/idlib/bv/boundsshort.h"
#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

class idBounds;
class idClip;
class idCollisionModel;
#if defined(WOLFTNO_RETAIL_PIECEEMITTER_IDJOINTMAT_CLASS_ABI)
class idJointMat;
#else
struct idJointMat;
#endif
class idTraceModelCache;

class idPositionedCollisionModel {
public:
  idCollisionModel* model;
  idJointMat* modelJoints;
  bool* sphereActives;
  idVec3 modelOrigin;
  idMat3 modelAxis;
  int modelEntityNum;
  int modelPhysicsId;
  int modelBodyId;
  int modelContentsOverride;
  float modelScale;
  std::uint32_t recoveredPadding;
  idCollisionQuery modelQuery;
};

struct idSPObjectMotion {
  bool valid;
  bool worldCollisionOnly;
  bool ignore;
  std::uint8_t padding;
  idVec3 translation;
  idVec3 rotationVec;
  float rotationAngle;
};

class idSPObject {
public:
  __declspec(noinline) idSPObject();
  __declspec(noinline) ~idSPObject() noexcept(false);

  int id;
  int traceModelIndex;
  int entityNum;
  int clipMask;
  idVec3 origin;
  idMat3 axis;
  idBoundsShort absBounds;
  idSPObjectMotion motion;
  std::uint32_t queryPadding;
  idCollisionQuery query;
  int index[2][3];
  idList<short, 59> overlap;
};

class idEffectPhysicsBroadPhase {
public:
  class idSPMarker {
  public:
    using idListAllocReturnsReferenceMarker = void;

    short value;
    unsigned short data;
  };

  idEffectPhysicsBroadPhase();
  ~idEffectPhysicsBroadPhase();

  void Init(idClip* clip, idTraceModelCache* traceModelCache);
  void AddObject(idSPObject* object, const idVec3& origin,
                 const idMat3& axis, const idBounds* bounds);
  void RemoveObject(idSPObject* object);
  void SetObjectPosition(idSPObject* object, const idVec3& origin,
                         const idMat3& axis, const idBounds* bounds);
  void ApplyMotion(idSPObject* object, const idVec3& translation,
                   const idRotation& rotation);
  bool GetMotionResult(trace_t* result, idSPObject* object);
  void SubmitQueries(bool worldCollisionModelOnly);

  idClip* clip;
  idTraceModelCache* traceModelCache;
  idList<idSPObject*, 59> objects;
  idList<idSPMarker, 59> markers[3];

private:
  void AddOverlappingPair(idSPObject* first, idSPObject* second);
  void RemoveOverlappingPair(idSPObject* first, idSPObject* second);
  int GetPositionedModels(const idSPObject* object,
                          idPositionedCollisionModel* models,
                          int maxModels) const;
  void MoveObject(idSPObject* object);

  friend class EffectPhysicsBroadPhaseValidationAccess;
};

static_assert(sizeof(idPositionedCollisionModel) == 0x68,
              "positioned collision-model ABI drift");
static_assert(offsetof(idPositionedCollisionModel, modelOrigin) == 0x18,
              "positioned model origin offset drift");
static_assert(offsetof(idPositionedCollisionModel, modelAxis) == 0x24,
              "positioned model axis offset drift");
static_assert(offsetof(idPositionedCollisionModel, modelEntityNum) == 0x48,
              "positioned model entity offset drift");
static_assert(offsetof(idPositionedCollisionModel, modelQuery) == 0x60,
              "positioned model query offset drift");

static_assert(sizeof(idSPObjectMotion) == 0x20,
              "spatial motion ABI drift");
static_assert(offsetof(idSPObjectMotion, translation) == 0x04,
              "spatial translation offset drift");
static_assert(offsetof(idSPObjectMotion, rotationAngle) == 0x1C,
              "spatial angle offset drift");
static_assert(sizeof(idSPObject) == 0xA8, "spatial object ABI drift");
static_assert(offsetof(idSPObject, motion) == 0x4C,
              "spatial motion offset drift");
static_assert(offsetof(idSPObject, query) == 0x70,
              "spatial query offset drift");
static_assert(offsetof(idSPObject, overlap) == 0x90,
              "spatial overlap offset drift");
static_assert(sizeof(idEffectPhysicsBroadPhase) == 0x70,
              "effect broad phase ABI drift");
static_assert(offsetof(idEffectPhysicsBroadPhase, objects) == 0x10,
              "effect broad phase object-list offset drift");
static_assert(offsetof(idEffectPhysicsBroadPhase, markers) == 0x28,
              "effect broad phase marker offset drift");
