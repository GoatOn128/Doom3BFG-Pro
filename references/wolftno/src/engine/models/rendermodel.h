#pragma once

#include "engine/renderer/triangles.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/approximatelighting.h"
#include "engine/renderer/rendermodelparms.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idConstantBuffer;
class idJointBuffer;
class idMaterial;
class idParmBlock;
class idRenderModelUpdateTools;
class idRenderView;
class idRenderModel;
class idDeclRenderParm;
class idDeclSkins;
class idRenderDebug;
class idRenderWorldLocal;
class idSerializer;
struct idRenderModelCommitted;
struct decalParams_t;
struct sourceSurface_t;

struct decalHandle_t {
  int decalIndex;
  int spawnIndex;
};

struct renderJointIndex_t {
  std::int16_t value;
};

struct idRenderModelRuntimeCallbacks {
  void (*markCommit)(idRenderWorldLocal*, int, bool) = nullptr;
  void (*markDeleted)(idRenderWorldLocal*, int, bool) = nullptr;
  void (*clearOcclusion)(idRenderWorldLocal*, idRenderModel*) = nullptr;
  bool (*setDecalPosition)(idRenderWorldLocal*, decalHandle_t, const idVec3&,
                           const idMat3&) = nullptr;
  decalHandle_t (*addDecal)(idRenderWorldLocal*, const decalParams_t*,
                            unsigned, const idVec3&, const idVec3&,
                            renderJointIndex_t) = nullptr;
  bool (*getDecalAxis)(idRenderWorldLocal*, decalHandle_t, idMat3&) = nullptr;
  void (*removeDecal)(idRenderWorldLocal*, decalHandle_t) = nullptr;
  bool (*isDecalInvalid)(idRenderWorldLocal*, decalHandle_t) = nullptr;
  int (*gameTimeMilliseconds)() = nullptr;
  const idMaterial* (*findMaterial)(const char*) = nullptr;
  const idDeclSkins* (*findSkins)(const char*) = nullptr;
  void (*getResolutionScale)(float*, float*) = nullptr;
  int (*getGuiFrameCount)() = nullptr;
  bool (*isComboMap)(const idRenderModelCommitted*) = nullptr;
  void (*checkCommittedBounds)(idRenderModelCommitted*) = nullptr;
  const idDeclRenderParm* modelFadeParm = nullptr;
};

void SetRenderModelRuntimeCallbacks(
    const idRenderModelRuntimeCallbacks& callbacks);

struct idRenderModelParmLinks {
  std::uint8_t prefix[184];
  idRenderModel* imposterParent;
  idRenderModel* imposterNext;
  std::uint8_t viewport[16];
};

static_assert(offsetof(idRenderModelParmLinks, imposterParent) == 184,
              "idRenderModelParms imposterParent offset drift");
static_assert(offsetof(idRenderModelParmLinks, imposterNext) == 192,
              "idRenderModelParms imposterNext offset drift");
static_assert(sizeof(idRenderModelParmLinks) == 216,
              "idRenderModelParms ABI drift");

enum renderModelType_t : int {
  RENDERMODEL_DEFAULT = 0,
  RENDERMODEL_CLOTH = 1,
  RENDERMODEL_DETAIL = 2,
  RENDERMODEL_DETAILEDIT = 3,
  RENDERMODEL_DISCRETEANIMATION = 4,
  RENDERMODEL_MANYBOXES = 5,
  RENDERMODEL_FLARE = 6,
  RENDERMODEL_FLAREOCCLUSIONQUAD = 7,
  RENDERMODEL_FOLIAGE = 8,
  RENDERMODEL_FOLIAGEEDIT = 9,
  RENDERMODEL_GLASS = 10,
  RENDERMODEL_EFFECTS = 11,
  RENDERMODEL_PARTICLE = 12,
  RENDERMODEL_TREEANIMATOR = 13,
  RENDERMODEL_STATIC = 14,
  RENDERMODEL_TRANSPARENCY = 15,
  RENDERMODEL_WATERSURFACE = 16,
  RENDERMODEL_ZIPLINE = 17,
  RENDERMODEL_BEAM = 18,
  RENDERMODEL_DECAL = 19,
  RENDERMODEL_GUI = 20,
  RENDERMODEL_IMPOSTER = 21,
  RENDERMODEL_TREEIMPOSTER = 22,
  RENDERMODEL_OCEAN = 23,
  RENDERMODEL_OCEAN2 = 24,
  RENDERMODEL_LOD = 25,
  RENDERMODEL_CHAIN = 26,
  RENDERMODEL_CUTTABLEBEAMS = 27,
  RENDERMODEL_CUTTABLEDYNAMIC = 28,
  RENDERMODEL_CUTTABLESURFACE = 29,
  RENDERMODEL_GODRAYS = 30,
  RENDERMODEL_DUST = 31,
  RENDERMODEL_CABLE = 32,
  RENDERMODEL_TYPE_MAX = 33,
};

class idRenderModelSurface {
public:
  const idMaterial* material = nullptr;
  int materialNum = 0;
  std::uint64_t extraGLState = 0;
  idTriangles* geometry = nullptr;
  bool geometryIsReference = false;
  std::uint8_t referenceMask = 0;
  int binaryModelId = 0;
  idJointBuffer* joints = nullptr;
  idVertexBuffer* morphMap = nullptr;
  idVertexBuffer* stMap = nullptr;
  idConstantBuffer* surfaceConstants = nullptr;
  idParmBlock* parms = nullptr;
  idVec4 skinOffsets{};
  float morphMultiplier = 0.0f;
};

static_assert(sizeof(idRenderModelSurface) == 104,
              "idRenderModelSurface ABI drift");

// Full method bodies belong to models/rendermodel.cpp. This declaration exposes
// the recovered surface API needed by subclasses while that compiland is ported.
class idRenderModel {
public:
  struct decalData_t {
    decalHandle_t handle;
    idVec3 relativePos;
    idMat3 relativeAxis;
    renderJointIndex_t jointId;
    std::uint8_t recoveredPadding[2]{};
  };

  idRenderModel();
  virtual ~idRenderModel();

  virtual void Save(idFile* file) const;
  virtual bool Load(idFile* file);
  virtual bool UpdateInView(const idRenderView* currentView,
                            const idRenderView* nextView,
                            idRenderModelUpdateTools* updateTools);
  virtual bool CommitSubclass();
  virtual void UnlinkSubclass();
  virtual void DeleteSubclass();
  virtual const idList<sourceSurface_t, 5>* GetSourceSurfaces() const {
    return nullptr;
  }

  int GetIndex() const;
  idRenderDebug* GetRenderDebug() const;
  void SetViewport(int x, int y, int width, int height);
  void SetParm(const idDeclRenderParm* parm, const parmValue_t& value);
  void SetParm(const idDeclRenderParm* parm, float scalar);
  void SetParm(const idDeclRenderParm* parm, const idVec3& value);
  void SetParm(const idDeclRenderParm* parm, const idVec4& value);
  bool SetDecalPosition(decalHandle_t decalHandle,
                        const idVec3& decalWorldPosition,
                        const idMat3& decalWorldAxis);
  void BoundsInAreas(const idBounds& bounds, int* areas, int* numAreas,
                     int maxAreas);
  void ClearOcclusionQuery();
  const approximateLighting_t* GetApproximateLighting() const;
  void SetForceApproximateLightingCalculation();
  bool SetPreventApproximateLightingCalculation(bool allowLighting);
  void SetName(const char* modelName);
  void FinishSurfaces();
  void FreeSurfaces();
  void AddSurface(const idRenderModelSurface& surface);
  void CommitThisFrame();
  void SetupMVP(idRenderMatrix* mvpMatrix, const idRenderView* view,
                float zNear, float zFar, float guiOriginOffset);
  const idDeclSkins* GetSkins() const;
  void RemoveDecals();
  bool RemoveDecal(decalHandle_t decalHandle);
  void UpdateDecalReferences();
  void SerializeSnapshot(idSerializer* serializer, bool serializeParmBlock);
  decalHandle_t AddDecalFromPoint(const decalParams_t* decalParms,
                                  unsigned startTime, const idVec3& position,
                                  const idVec3& direction);
  void Commit();
  void SetMaxSurfaces(int maximum);
  void InitDecalData(decalData_t& decal, const decalParams_t* decalParms,
                     unsigned startTime, const idVec3& position,
                     const idVec3& direction);

  idAtomicString name;
  const char* debugName = nullptr;
  idVec3 deferredOrigin{};
  idMat3 deferredAxis{};
  std::uint8_t useDeferredPosition = 0;
  std::uint8_t deferredPositionInitialized = 0;
  std::uint8_t deleteOnSync = 0;
  std::uint8_t unlinked = 0;
  std::uint8_t forceUpdateInView = 0;
  std::uint8_t recoveredFlagPadding[3]{};
  idBounds referenceBounds{};
  union {
    std::uint8_t recoveredParms[216];
    idRenderModelParms g;
    idRenderModelParmLinks recoveredParmLinks;
  };
  idStaticParmBlock<32> gameParmBlock;
  int decalLastUpdateTime = 0;
  int decalUpdateTime = 0;
  idRenderWorldLocal* world = nullptr;
  idRenderModelCommitted* committed = nullptr;
  idList<idRenderModelSurface, 85> surfaces;
  idStaticList<decalData_t, 16> decalData;
  renderModelType_t renderModelType = RENDERMODEL_DEFAULT;
  std::uint32_t recoveredModelTail = 0;
};

static_assert(sizeof(decalHandle_t) == 8, "decal handle ABI drift");
static_assert(sizeof(idRenderModel::decalData_t) == 60,
              "decal data ABI drift");

static_assert(offsetof(idRenderModel, referenceBounds) == 80,
              "idRenderModel bounds offset drift");
static_assert(offsetof(idRenderModel, surfaces) == 1160,
              "idRenderModel surfaces offset drift");
static_assert(offsetof(idRenderModel, decalData) == 1184,
              "idRenderModel decal data offset drift");
static_assert(offsetof(idRenderModel, renderModelType) == 2168,
              "idRenderModel type offset drift");
static_assert(sizeof(idRenderModel) == 2176, "idRenderModel ABI drift");
