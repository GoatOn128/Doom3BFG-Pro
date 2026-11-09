#pragma once

#include "engine/renderer/approximatelighting.h"
#include "engine/renderer/ambientlighting.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/globalshadows.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/render.h"
#include "engine/renderer/rendermodelparms.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/bv/box.h"
#include "shared/idlib/bv/cylinder.h"
#include "shared/idlib/bv/sphere.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/geometry/winding.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/math/curve.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idDeclEnv;
#if defined(WOLFTNO_RETAIL_IDBOX_CLASS_ABI)
class idBox;
#else
struct idBox;
#endif
struct idCylinder;
class idMegaTexture;
class idLexer;
class idOceanSharedTileMemory;
class idParallelJobList;
class idRenderDebugLocal;
class idRenderLight;
struct idRenderLightCommitted;
class idRenderModel;
struct idRenderModelCommitted;
class idRenderModelDecal;
class idRenderModelDetail;
class idRenderModelDetailEdit;
class idRenderModelFoliage;
class idRenderModelFoliageEdit;
class idRenderModelSurface;
class idRenderModelTransparency;
class idRenderModelUpdateTools;
class idRenderWorldLocal;
class idSnapShot;
class idSphere;
class idStaticModel;
class idTraceOpts;
class idVirtualTexture;
struct approximateLightingParms_t;
struct areaNode_t;
struct frustumCorners_t;
struct idRenderAssetRemap;
struct worldArea_t;
template <typename T> class idCurve_Spline;

enum invalidDynamicEnvHandle_t : int { INVALID_DYNAMIC_ENV_HANDLE = -1 };

// The game layer owns the primary idHandle template. This explicit engine
// specialization preserves the retail decorated signatures without defining
// that primary template a second time.
template <typename T, typename InvalidEnum, int InvalidValue> class idHandle;
template <> class idHandle<int, invalidDynamicEnvHandle_t, -1> {
public:
  int value{-1};
  bool operator==(const idHandle& other) const { return value == other.value; }
  bool operator!=(const idHandle& other) const { return value != other.value; }
};
using dynamicEnvironmentHandle_t =
    idHandle<int, invalidDynamicEnvHandle_t, -1>;

struct dynamicEnvironment_t {
  const idDeclEnv* env{};
  idBounds bounds{};
  dynamicEnvironmentHandle_t handle{};
  idArray<idPlane, 16> planes{};
  int numPlanes{};
  float volume{};
};

struct debugLine_t {
  idVec4 rgb{};
  idVec3 start{};
  idVec3 end{};
  bool depthTest{};
  std::uint8_t recoveredPadding[3]{};
  int lifeTime{};
};

struct debugText_t {
  debugText_t& operator=(const debugText_t& other);
  char text[128]{};
  idVec3 origin{};
  float scale{};
  idVec4 color{};
  idMat3 viewAxis{};
  std::uint8_t align{};
  std::uint8_t lineWidth{};
  bool orientToRenderViewAxis{};
  bool depthTest{};
  int lifeTime{};
};

struct debugPolygon_t {
  debugPolygon_t() = default;
  debugPolygon_t(const debugPolygon_t& other) { *this = other; }
  debugPolygon_t& operator=(const debugPolygon_t& other) {
    if (this == &other)
      return *this;
    rgb = other.rgb;
    winding = other.winding;
    depthTest = other.depthTest;
    lifeTime = other.lifeTime;
    return *this;
  }
  ~debugPolygon_t();
  idVec4 rgb{};
  idWinding winding{};
  bool depthTest{};
  std::uint8_t recoveredPadding[3]{};
  int lifeTime{};
};

struct debugBounds_t {
  idVec4 rgb{};
  idBounds bounds{};
  idVec3 origin{};
  idMat3 axis{};
  bool depthTest{};
  std::uint8_t recoveredPadding[3]{};
  int lifeTime{};
};

class idRenderDebug {
public:
  virtual ~idRenderDebug();
  virtual void DebugClear(int time) = 0;
  virtual void DebugLine(const idVec4& color, const idVec3& start,
                         const idVec3& end, int lifetime, bool depthTest) = 0;
  virtual void DebugArrow(const idVec4& color, const idVec3& start,
                          const idVec3& end, float size, int lifetime,
                          bool depthTest) = 0;
  virtual void DebugArrow2(const idVec4& color, const idVec3& start,
                           const idVec3& end, float size, int lifetime,
                           bool depthTest, const idVec3& cameraPosition) = 0;
  virtual void DebugArrow3(const idVec4& color, const idVec3& start,
                           const idVec3& end, float size, int lifetime,
                           bool depthTest) = 0;
  virtual void DebugPyramid(const idVec4& color, const idVec3& base,
                            const idVec3& up, const idVec3& forward,
                            float height, float width, int lifetime,
                            bool depthTest) = 0;
  virtual void DebugWinding(const idVec4& color, const idWinding& winding,
                            const idVec3& origin, const idMat3& axis,
                            int lifetime, bool depthTest) = 0;
  virtual void DebugCircle(const idVec4& color, const idVec3& origin,
                           const idVec3& direction, float radius,
                           int numSteps, int lifetime, bool depthTest) = 0;
  virtual void DebugCylinder(const idVec4& color,
                             const idCylinder& cylinder,
                             const idVec3& direction, int numSteps,
                             int lifetime, bool depthTest) = 0;
  virtual void DebugArc(const idVec4& color, const idVec3& origin,
                        const idVec3& direction, float radius,
                        float startAngle, float endAngle,
                        float anglesPerSegment, int lifetime, bool depthTest,
                        bool drawSectorEdges) = 0;
  virtual void DebugShadedArc(
      const idVec4& color, const idVec3& origin, const idVec3& direction,
      const idVec3& up, float radius, float startAngle, float endAngle,
      float anglesPerSegment, int lifetime, bool depthTest) = 0;
  virtual void DebugSphere(const idVec4& color, const idSphere& sphere,
                           int numSteps, int lifetime, bool depthTest) = 0;
  virtual void DebugBounds(const idVec4& color, const idBounds& bounds,
                           const idVec3& origin, int lifetime,
                           bool depthTest) = 0;
  virtual void DebugOrientedBounds(const idVec4& color,
                                   const idBounds& bounds,
                                   const idVec3& origin,
                                   const idMat3& axis, int lifetime,
                                   bool depthTest) = 0;
  virtual void DebugBox(const idVec4& color, const idBox& box,
                        int lifetime, bool depthTest) = 0;
  virtual void DebugCone(const idVec4& color, const idVec3& apex,
                         const idVec3& direction, float radius,
                         int lifetime, bool depthTest) = 0;
  virtual void DebugCone(const idVec4& color, const idVec3& apex,
                         const idVec3& direction, float radius1,
                         float radius2, int lifetime, bool depthTest) = 0;
  virtual void DebugAxis(const idVec3& origin, const idMat3& axis,
                         int lifetime, bool depthTest) = 0;
  virtual void DebugAxis(const idVec4& color, const idVec3& origin,
                         const idMat3& axis, int lifetime, bool depthTest) = 0;
  virtual void DebugAxisScaled(const idVec3& origin, const idMat3& axis,
                               float scale, int lifetime, bool depthTest) = 0;
  virtual void DebugPoint(const idVec4& color, const idVec3& point,
                          int lifetime, bool depthTest) = 0;
  virtual void DebugFilledPolygon(const idVec4& color,
                                  const idWinding& winding, int lifetime,
                                  bool depthTest) = 0;
  virtual void DebugFilledBounds(const idVec4& color,
                                 const idBounds& bounds,
                                 const idVec3& origin, int lifetime,
                                 bool depthTest) = 0;
  virtual void DebugText(const char* text, const idVec3& origin, float scale,
                         const idVec4& color, const idMat3& viewAxis,
                         int align, int lifetime, bool depthTest,
                         bool bold) = 0;
  virtual void DebugText(const char* text, const idVec3& origin, float scale,
                         const idVec4& color, int align, int lifetime,
                         bool depthTest, bool bold) = 0;
  virtual void DebugSpline(const idVec4& color,
                           idCurve_Spline<idVec3>* spline, float step,
                           bool drawKnots, int lifetime, bool depthTest) = 0;
};

class idRenderDebugLocal final : public idRenderDebug {
public:
  idRenderDebugLocal();
  ~idRenderDebugLocal() override;
  void DebugClear(int) override;
  void DebugLine(const idVec4&, const idVec3&, const idVec3&, int,
                 bool) override;
  void DebugArrow(const idVec4&, const idVec3&, const idVec3&, float, int,
                  bool) override;
  void DebugArrow2(const idVec4&, const idVec3&, const idVec3&, float, int,
                   bool, const idVec3&) override;
  void DebugArrow3(const idVec4&, const idVec3&, const idVec3&, float, int,
                   bool) override;
  void DebugPyramid(const idVec4&, const idVec3&, const idVec3&,
                    const idVec3&, float, float, int, bool) override;
  void DebugWinding(const idVec4&, const idWinding&, const idVec3&,
                    const idMat3&, int, bool) override;
  void DebugCircle(const idVec4&, const idVec3&, const idVec3&, float, int,
                   int, bool) override;
  void DebugCylinder(const idVec4&, const idCylinder&, const idVec3&, int,
                     int, bool) override;
  void DebugArc(const idVec4&, const idVec3&, const idVec3&, float, float,
                float, float, int, bool, bool) override;
  void DebugShadedArc(const idVec4&, const idVec3&, const idVec3&,
                      const idVec3&, float, float, float, float, int,
                      bool) override;
  void DebugSphere(const idVec4&, const idSphere&, int, int, bool) override;
  void DebugBounds(const idVec4&, const idBounds&, const idVec3&, int,
                   bool) override;
  void DebugOrientedBounds(const idVec4&, const idBounds&, const idVec3&,
                           const idMat3&, int, bool) override;
  void DebugBox(const idVec4&, const idBox&, int, bool) override;
  void DebugCone(const idVec4&, const idVec3&, const idVec3&, float, int,
                 bool) override;
  void DebugCone(const idVec4&, const idVec3&, const idVec3&, float, float,
                 int, bool) override;
  void DebugAxis(const idVec3&, const idMat3&, int, bool) override;
  void DebugAxis(const idVec4&, const idVec3&, const idMat3&, int,
                 bool) override;
  void DebugAxisScaled(const idVec3&, const idMat3&, float, int,
                       bool) override;
  void DebugPoint(const idVec4&, const idVec3&, int, bool) override;
  void DebugFilledPolygon(const idVec4&, const idWinding&, int,
                          bool) override;
  void DebugFilledBounds(const idVec4&, const idBounds&, const idVec3&, int,
                         bool) override;
  void DebugText(const char*, const idVec3&, float, const idVec4&,
                 const idMat3&, int, int, bool, bool) override;
  void DebugText(const char*, const idVec3&, float, const idVec4&, int, int,
                 bool, bool) override;
  void DebugSpline(const idVec4&, idCurve_Spline<idVec3>*, float, bool, int,
                   bool) override;
  idList<debugLine_t, 3> debugLines;
  idList<debugText_t, 3> debugText;
  idList<debugPolygon_t, 3> debugPolygons;
  idList<debugBounds_t, 3> debugBounds;
};

struct localTrace_t {
  float fraction;
  idVec3 point;
  idVec3 normal;
  int jointIndex;
  int indexes[3];
};

struct modelTrace_t {
  localTrace_t tr;
  std::uint32_t padding;
  const idRenderModelSurface* surf;
  idRenderModel* entity;
};

class idRenderWorldCommitted {
public:
  idRenderWorldCommitted();
  ~idRenderWorldCommitted();
  void PostCommitData(const idRenderView* renderView, int renderTime);
  int GetNumCommittedModels() const;
  int GetNumCommittedLights() const;
  int GetNumAddAlwaysModels() const;
  void ApproximateLightingForBounds(const idBounds*, approximateLighting_t*);
  void ApproximateAmbientForBounds(const idBounds*, approximateLighting_t*);
  void BoundsInAreas(const idBounds*, int*, int*, int);
  void UnlinkDimShadow(idRenderModelCommitted* model);
  void LinkLightIntoTree(idRenderLightCommitted* light,
                         const idRenderMatrix& frustumTransform,
                         const idBounds& frustumBounds);
  void UnlinkLightFromTree(idRenderLightCommitted* light);
  void LinkModelIntoTree(idRenderModelCommitted* model,
                         const idRenderMatrix& frustumTransform,
                         const idBounds& frustumBounds);
  void UnlinkModelFromTree(idRenderModelCommitted* model);
  void LinkApproximateLighting(idRenderModelCommitted* model);
  void SetBinaryModelState(const char* binaryModelName, bool isVisible,
                           bool makingDefinition);

#if defined(WOLF_RENDERER_RENDERWORLDCOMMITTED_TEST_ACCESS)
public:
#else
private:
#endif
  void BoundsInAreas_r(int nodeNum, const idBounds* bounds, int* areas,
                       int* numAreas, int maxAreas);
  void ShowApproximateLighting(idRenderModelCommitted* model,
                               const approximateLightingParms_t* parms);
  void ApproximateLightingForModel(const idRenderView* view,
                                   idRenderModelCommitted* model,
                                   int renderTime,
                                   idParallelJobList* jobList);
  void AddModelRefToArea(idRenderModelCommitted* model, worldArea_t* area);
  void AddLightRefToArea(idRenderLightCommitted* light, worldArea_t* area);
  void PushFrustumIntoTree_r(idRenderModelCommitted* model,
                             idRenderLightCommitted* light,
                             const frustumCorners_t* corners, int nodeNum);

public:

  const char* mapName{};
  bool isComboMap{};
  bool isToolsWorld{};
  bool hasWorldGeometry{};
  bool hasWorldSurfaceOffsets{};
  std::uint8_t recoveredAreaPadding[4]{};
  areaNode_t* areaNodes{};
  idBounds* areaNodeBounds{};
  int* areaNodeOcclusionIndex{};
  int numAreaNodes{};
  std::uint8_t recoveredWorldAreaPadding[4]{};
  worldArea_t* worldAreas{};
  idBounds* worldAreaSubspaceBounds{};
  idBounds* worldAreaGeometryBounds{};
  int numWorldAreas{};
  std::uint8_t recoveredBinaryPadding[4]{};
  idStaticList<idStr, 1024> binaryModelNames;
  idHashIndex binaryModelHash;
  idStaticList<bool, 1024> binaryModelState;
  idStaticList<dynamicEnvironment_t, 128> dynamicEnvironments;
  const idDeclEnv* testEnv{};
  idAmbientLighting ambientLighting{};
  idGlobalShadows globalShadows{};
  idStaticList<idRenderModelCommitted*, 8192> committedRenderModels;
  idStaticList<idRenderLightCommitted*, 8192> committedRenderLights;
  idRenderModelCommitted* addAlwaysModels{};
  idRenderModelCommitted* dimShadowModels{};
  idRenderModelCommitted* approxLightModels{};
  idRenderModelCommitted* occlusionBaseModel{};
  idRenderModelCommitted* transparencyRenderModel{};
  idArray<bool, 8192> postCommitModels{};
  idArray<bool, 8192> postCommitLights{};
  approximateLightingParms_t* approximateLightingParms{};
  int numApproximateLightingParms{};
  int numWantedApproximateLightingParms{};
  const idRenderLightCommitted** approximateLightingLights{};
  int numApproximateLightingLights{};
  std::uint8_t recoveredAssetPadding[4]{};
  idRenderAssetRemap* assetRemappings{};
  idRenderDebugLocal* committedDebugData{};
  idParallelJobList* rendererJobList{};
};

class idRenderWorld {
public:
  virtual ~idRenderWorld();
  virtual const char* GetName() const;
  virtual bool IsComboMap() const;
  virtual bool InitFromMap(const char*, bool, bool);
  virtual void SpecificationComplete(bool);
  virtual dynamicEnvironmentHandle_t AddEnvironment(
      const idBounds&, const idDeclEnv*, const idPlane*, int, float);
  virtual void RemoveEnvironment(const dynamicEnvironmentHandle_t&);
  virtual void SetEnvironmentEffects(const dynamicEnvironmentHandle_t&, const idDeclEnv*);
  virtual void SetEnvironmentBounds(const dynamicEnvironmentHandle_t&, const idBounds&);
  virtual int FindEnvironment(const dynamicEnvironmentHandle_t&) const;
  virtual const idDeclEnv* FindEnvironmentDecl(const idVec3&) const;
  virtual idOceanSharedTileMemory* GetOceanSharedTileMemory();
  virtual void Ocean_Init();
  virtual idRenderLight* AllocRenderLight(bool);
  virtual idRenderModel* AllocBaseRenderModel();
  virtual idRenderModel* AllocStaticModel(const char*, bool);
  virtual bool AddRenderModel(idRenderModel*);
  virtual idRenderModel* StampModel();
  virtual idRenderModelFoliageEdit* FoliageEditRenderModel();
  virtual idRenderModelFoliage* FoliageRenderModel();
  virtual idRenderModelDetailEdit* DetailEditRenderModel();
  virtual idRenderModelDetail* DetailRenderModel();
  virtual idRenderModelTransparency* TransparencyRenderModel();
  virtual idRenderModelDecal* DecalModel();
  virtual void SetBinaryModelState(const char*, bool);
  virtual void ApproximateLightingForBounds(const idBounds*, approximateLighting_t*);
  virtual void ApproximateAmbientForBounds(const idBounds*, approximateLighting_t*);
  virtual void SetNumRenderViews(int);
  virtual idRenderView* RenderViewForIndex(int) const;
  virtual void UpdateInView(const idRenderView*, const idRenderView*);
  virtual void Commit(bool);
  virtual void UpdateDeferredPositions();
  virtual void MarkLightSerialized(idRenderLight*, bool, bool);
  virtual void MarkAllLightsAsSerialized();
  virtual void WriteToSnapshot(idSnapShot&, const idSnapShot*) const;
  virtual void ReadFromSnapshot(idSnapShot&, const idSnapShot*);
  virtual void ResetSnapshots();
  virtual idRenderDebug* GetRenderDebug();
  virtual bool InitForTools(const char*);
  virtual void TestEnvironment(const idDeclEnv*);
  virtual const idDeclEnv* GetTestEnvironment() const;
  virtual int NumEnvironments() const;
  virtual void ClearEnvironments();
  virtual const idParmBlock* GetCurrentBlendedParms();
  virtual const idDeclEnv* GetEnvironmentDeclForCurrentViewPoint() const;
  virtual int GetNumRenderModels() const;
  virtual idRenderModel* RenderModelForIndex(int) const;
  virtual int GetNumRenderLights() const;
  virtual idRenderLight* RenderLightForIndex(int) const;
  virtual int GetNumVirtualTextures() const;
  virtual idVirtualTexture* VirtualTextureForIndex(int) const;
  virtual int GetNumMegaTextures() const;
  virtual idMegaTexture* MegaTextureForIndex(int) const;
  virtual const idStaticModel* GetWorldStaticModel() const;
  virtual const idRenderModel* GetWorldRenderModel() const;
  virtual void GetStaticWorldBounds(idBounds*);
  virtual idRenderWorldCommitted* GetCommittedWorld() const;
  virtual bool Trace(modelTrace_t*, const idVec3*, const idVec3*, const idTraceOpts*);
  static idRenderWorld* Alloc();
};

enum class renderWorldUpdateEvent_t {
  WaitJobs, AddJobTimeRange, GenerateTransparencyList, StartTransparency,
  StartEffects, StartFlare, StartFlareOcclusion, StartCuttableBeams,
  GenerateVisibleInfluences, AddTransparencySortJob, SubmitJobs,
  EndCuttableBeams, EndEffects, EndFlare, OceanSwap,
};

struct idRenderWorldRuntimeCallbacks {
  idRenderView* (*allocRenderView)(int viewIndex) = nullptr;
  void (*freeRenderView)(idRenderView*) = nullptr;
  idParallelJobList* (*allocModelJobList)(idRenderWorldLocal*) = nullptr;
  void (*freeModelJobList)(idParallelJobList*) = nullptr;
  idRenderWorldCommitted* (*allocCommittedWorld)() = nullptr;
  void (*freeCommittedWorld)(idRenderWorldCommitted*) = nullptr;
  void (*freeRenderWorld)(idRenderWorldLocal*) = nullptr;
  void (*oceanDestroy)(idRenderWorldLocal*) = nullptr;
  void (*setBinaryModelState)(idRenderWorldCommitted*, const char*, bool, bool) = nullptr;
  idRenderModelFoliage* (*allocFoliageModel)() = nullptr;
  idRenderModelDetail* (*allocDetailModel)() = nullptr;
  idRenderModel* (*allocStaticModel)(const char*, bool) = nullptr;
  void (*printUpdateStatistics)(int particleQuads) = nullptr;
  bool (*showParticleInfo)() = nullptr;
  void (*updateEvent)(idRenderWorldLocal*, renderWorldUpdateEvent_t,
                      idRenderModelUpdateTools*) = nullptr;
  const void* (*globalShadowMap)(idRenderWorldCommitted*) = nullptr;
  bool (*occlusionAllowsUpdate)(idRenderWorldLocal*, idRenderModel*, int,
                                const idRenderView*, const idRenderView*) = nullptr;
  void (*clearModelOcclusion)(idRenderWorldLocal*, idRenderModel*, int) = nullptr;
  void (*unlinkModelFromTree)(idRenderWorldCommitted*, idRenderModelCommitted*) = nullptr;
  void (*unlinkLightFromTree)(idRenderWorldCommitted*, idRenderLightCommitted*) = nullptr;
  void (*markSnapshotObjectDeleted)(int objectNumber) = nullptr;
  bool (*allPeersHaveStaleSnapshotObject)(int objectNumber) = nullptr;
  void (*writeEnvironmentResource)(idBitMsg&, const idDeclEnv*) = nullptr;
  const idDeclEnv* (*readEnvironmentResource)(idBitMsg&) = nullptr;
  const idDeclEnv* (*findEnvironmentDecl)(const char*) = nullptr;
  const idDeclEnv* (*buildEnvironmentDecl)(
      const char* areaName, const char* envDeclPath, const char* worldName,
      const char* userEnvMap, const idVec3&, const idColor&, const idVec2&,
      const idVec2&) = nullptr;
  void (*beginVirtualTextureSetup)(const char* strippedMapName) = nullptr;
  idVirtualTexture* (*virtualTextureForPageSource)(int pageSource) = nullptr;
  void (*preloadVirtualTextures)(idVirtualTexture* const* textures,
                                 int numTextures) = nullptr;
  idStaticModel* (*createStaticModel)() = nullptr;
  idStaticModel* (*loadStaticModel)(const char* name, bool makeDefault,
                                    bool forceReload) = nullptr;
  void (*finishStaticModel)(idStaticModel*, int flags) = nullptr;
  void (*registerStaticModel)(idStaticModel*) = nullptr;
  void (*releaseTemporaryStaticModel)(idStaticModel*) = nullptr;
  void (*loadGlobalShadows)(idGlobalShadows*, const char* mapName) = nullptr;
  void (*loadAmbientLighting)(idAmbientLighting*, const char* mapName) = nullptr;
  bool (*loadFoliageModel)(idRenderWorldLocal*, const char* fileName) = nullptr;
};

void SetRenderWorldRuntimeCallbacks(const idRenderWorldRuntimeCallbacks& callbacks);
void SetRenderWorldLoadRuntimeCallbacks(
    const idRenderWorldRuntimeCallbacks& callbacks);
void PrintUpdateInViewStatistics(const idRenderModelUpdateTools* updateTools);

class idRenderWorldUtil {
public:
  static idStr CreateMegaModelName(const char* mapName, const char* modelName,
                                   const char* entityName);
};

class idRenderWorldLocal : public idRenderWorld {
public:
  idRenderWorldLocal();
  ~idRenderWorldLocal() override;
  const char* GetName() const override;
  bool IsComboMap() const override;
  bool InitFromMap(const char*, bool, bool) override;
  void SpecificationComplete(bool) override;
  dynamicEnvironmentHandle_t AddEnvironment(const idBounds&, const idDeclEnv*,
      const idPlane*, int, float) override;
  void RemoveEnvironment(const dynamicEnvironmentHandle_t&) override;
  void SetEnvironmentEffects(const dynamicEnvironmentHandle_t&, const idDeclEnv*) override;
  void SetEnvironmentBounds(const dynamicEnvironmentHandle_t&, const idBounds&) override;
  int FindEnvironment(const dynamicEnvironmentHandle_t&) const override;
  const idDeclEnv* FindEnvironmentDecl(const idVec3&) const override;
  idOceanSharedTileMemory* GetOceanSharedTileMemory() override {
    return m_pSharedTileMemory;
  }
  idRenderLight* AllocRenderLight(bool) override;
  idRenderModel* AllocBaseRenderModel() override;
  idRenderModel* AllocStaticModel(const char*, bool) override;
  bool AddRenderModel(idRenderModel*) override;
  idRenderModel* StampModel() override;
  idRenderModelFoliage* FoliageRenderModel() override;
  idRenderModelDetail* DetailRenderModel() override;
  idRenderModelTransparency* TransparencyRenderModel() override;
  idRenderModelDecal* DecalModel() override;
  void SetBinaryModelState(const char*, bool) override;
  void ApproximateLightingForBounds(const idBounds*, approximateLighting_t*) override;
  void ApproximateAmbientForBounds(const idBounds*, approximateLighting_t*) override;
  void SetNumRenderViews(int) override;
  idRenderView* RenderViewForIndex(int) const override;
  void UpdateInView(const idRenderView*, const idRenderView*) override;
  void Commit(bool) override;
  void UpdateDeferredPositions() override;
  void MarkLightSerialized(idRenderLight*, bool, bool) override;
  void MarkAllLightsAsSerialized() override;
  void WriteToSnapshot(idSnapShot&, const idSnapShot*) const override;
  void ReadFromSnapshot(idSnapShot&, const idSnapShot*) override;
  void ResetSnapshots() override;
  idRenderDebug* GetRenderDebug() override;
  const idParmBlock* GetCurrentBlendedParms() override;
  void GetStaticWorldBounds(idBounds*) override;
  idRenderWorldCommitted* GetCommittedWorld() const override;
  bool Trace(modelTrace_t*, const idVec3*, const idVec3*, const idTraceOpts*) override;
  void BoundsInAreas(const idBounds*, int*, int*, int);
  void ClearOcclusionQueryForModel(idRenderModel* model);
  void AssignNodeOcclusionIndices_r(int nodeNum, int depth,
                                    int& numOcclusionIndices);
  idBounds BuildNodeBounds_r(int nodeNum, idPlane* planes, int numPlanes);
  void ParseNodes(idLexer* source);
  void ParseEnvAreas(idLexer* source);
  void ParseInterAreaPortals(idLexer* source);
  void ParseAreas(idLexer* source);
  void ParseEnvArea(const char* worldName, idLexer* source, int version);
  bool LoadEnvAreas(const char* worldName, const char* fileName);
  void ClearWorld();
  bool ParseProc(const char* fileName);
  void Ocean_Init() override;
  void Ocean_Swap();
  void Ocean_Destroy();
  bool& CommitLight(int index) { return commitLights.ptr[index]; }

  idOceanSharedTileMemory* m_pSharedTileMemory{};
  idArray<bool, 8192> commitModels{};
  idArray<bool, 8192> commitLights{};
  idArray<bool, 8192> deletedModels{};
  idStr mapName;
  bool everInitialized{};
  bool specificationCompleteHasBeenCalled{};
  std::uint8_t recoveredHandlePadding[2]{};
  int currentDynamicEnvHandle{};
  idVertexBuffer worldVertexBuffer;
  idIndexBuffer worldIndexBuffer;
  idStaticModel* worldStaticModel{};
  idRenderModel* worldRenderModel{};
  idRenderModel* skyModel{};
  idRenderModel* occlusionBaseModel{};
  idRenderModel* stampModel{};
  idRenderModelFoliageEdit* foliageEditRenderModel{};
  idRenderModelFoliage* foliageRenderModel{};
  idRenderModelDetailEdit* detailEditRenderModel{};
  idRenderModelDetail* detailRenderModel{};
  idRenderModelTransparency* transparencyRenderModel{};
  idRenderModelDecal* decalRenderModel{};
  idStaticList<idRenderModel*, 8192> renderModels;
  idStaticList<idRenderLight*, 8192> renderLights;
  idStaticList<idRenderLight*, 8192> serializedLights;
  idStaticList<idRenderView*, 2> renderViews;
  idParallelJobList* renderModelJobList{};
  idRenderDebugLocal* debugData{};
  idList<idVirtualTexture*, 61> virtualTextures;
  idList<idMegaTexture*, 61> megaTextures;
  idRenderWorldCommitted* committedWorld{};
};

static_assert(sizeof(dynamicEnvironmentHandle_t) == 4, "dynamic handle ABI drift");
static_assert(sizeof(dynamicEnvironment_t) == 304, "dynamic environment ABI drift");
static_assert(sizeof(debugLine_t) == 48, "debug line ABI drift");
static_assert(sizeof(debugText_t) == 204, "debug text ABI drift");
static_assert(sizeof(debugPolygon_t) == 56, "debug polygon ABI drift");
static_assert(sizeof(debugBounds_t) == 96, "debug bounds ABI drift");
static_assert(sizeof(idRenderDebugLocal) == 104, "debug local ABI drift");
static_assert(sizeof(idRenderWorldCommitted) == 228712, "committed world ABI drift");
static_assert(offsetof(idRenderWorldCommitted, dynamicEnvironments) == 42152,
              "committed dynamic environments offset drift");
static_assert(offsetof(idRenderWorldCommitted, committedRenderModels) == 81112,
              "committed model list offset drift");
static_assert(offsetof(idRenderWorldCommitted, committedRenderLights) == 146672,
              "committed light list offset drift");
static_assert(offsetof(idRenderWorldCommitted, postCommitModels) == 212272,
              "committed model flags offset drift");
static_assert(offsetof(idRenderWorldCommitted, rendererJobList) == 228704,
              "committed renderer job list offset drift");
static_assert(sizeof(approximateLighting_t) == 224, "approximate lighting ABI drift");
static_assert(offsetof(approximateLighting_t, primeLightDir) == 16,
              "approximate light direction offset drift");
static_assert(offsetof(approximateLighting_t, lightColor) == 80,
              "approximate light color offset drift");
static_assert(offsetof(approximateLighting_t, channelLighting) == 128,
              "approximate channel lighting offset drift");
static_assert(sizeof(localTrace_t) == 44, "local trace ABI drift");
static_assert(offsetof(modelTrace_t, surf) == 48, "model trace surface offset drift");
static_assert(sizeof(modelTrace_t) == 64, "model trace ABI drift");
static_assert(sizeof(renderView_t) == 6528, "render view parameters ABI drift");
static_assert(offsetof(renderView_t, vieworg) == 96, "render view origin offset drift");
static_assert(offsetof(renderView_t, viewaxis) == 108, "render view axis offset drift");
static_assert(offsetof(idRenderWorldLocal, mapName) == 24592, "world map name offset drift");
static_assert(offsetof(idRenderWorldLocal, renderModels) == 24776, "world model list offset drift");
static_assert(offsetof(idRenderWorldLocal, renderViews) == 221456, "world view list offset drift");
static_assert(offsetof(idRenderWorldLocal, committedWorld) == 221560,
              "world committed pointer offset drift");
static_assert(sizeof(idRenderWorldLocal) == 221568, "render world local ABI drift");
static_assert(offsetof(idRenderView, viewIndex) == 6536, "render view index offset drift");
static_assert(offsetof(idRenderView, projectionMatrix) == 13248,
              "render view projection offset drift");
static_assert(offsetof(idRenderView, renderWidth) == 13832,
              "render view width offset drift");
static_assert(offsetof(idRenderView, occlusionState) == 13904,
              "render view occlusion offset drift");
static_assert(sizeof(idRenderView) == 13920, "idRenderView ABI drift");
