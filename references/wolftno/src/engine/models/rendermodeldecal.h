#pragma once

#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/triangles.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idDeclRenderParm;
class idSphereTree;

struct decalParams_t {
  const idMaterial* decalMat = nullptr;
  float depth = 0.0f;
  idVec2 size{};
  float angle = 0.0f;
  int lifeTime = 0;
  int fadeOutTime = 0;
  int fadeInTime = 0;
  bool quad = false;
  bool overlap = false;
  std::uint8_t recoveredPadding[2]{};
  float overlapRadius = 0.0f;
  int cost = 0;
};

struct tireDecalParams_t {
  const idMaterial* decalMat = nullptr;
  float depth = 0.0f;
  int lifeTime = 0;
  int fadeTime = 0;
  idVec3 corners[4]{};
  idVec3 left{};
  float halfWidth = 0.0f;
  float halfHeight = 0.0f;
  bool markStart = false;
  bool markEnd = false;
  std::uint8_t recoveredPadding[2]{};
  int cost = 0;
};

struct idRenderModelDecalRuntimeCallbacks {
  int (*frameNumber)() = nullptr;
  const idDeclRenderParm* (*findRenderParm)(const char*) = nullptr;
  void (*getAtlasInfo)(int* width, int* height, int* widthBits,
                       int* heightBits) = nullptr;
  void (*debugTree)(const void* tree) = nullptr;
  void (*debugAxis)(const idVec3& origin, const idMat3& axis) = nullptr;
  void (*debugBox)(const idDrawVert* vertices, int count) = nullptr;
  void (*debugPoint)(const idVec3& point, int number) = nullptr;
};

void SetRenderModelDecalRuntimeCallbacks(
    const idRenderModelDecalRuntimeCallbacks& callbacks);

int VertexSizeFromMask(unsigned vertexMask);
void PackFloat(std::uint8_t* value, float number);
void BuildIndexBufferBoxes(idIndexBuffer* indexBuffer, int maxBoxes);
void BuildIndexBufferQuads(idIndexBuffer* indexBuffer, int maxQuads);

class alignas(16) idRenderModelDecal final : public idRenderModel {
public:
  struct maxInfo_t {
    int maxSurfaces = 0;
    int maxTireDecals = 0;
    int maxImpactDecals = 0;
    int maxPersistentDecals = 0;
    int maxDecals = 0;
    int maxBoxes = 0;
    int maxQuads = 0;
    int persistentStartRange = 0;
    int persistentEndRange = 0;
    int maxTransientDecals = 0;
  };

  struct decalInfo_t {
    const idMaterial* decalMtr = nullptr;
    int startTime = 0;
    int fadeInStart = 0;
    int lifeTime = 0;
    std::uint16_t fadeOutTime = 0;
    std::uint16_t fadeInTime = 0;
    float scaleBiasX = 0.0f;
    float scaleBiasY = 0.0f;
    float depth = 0.0f;
    idVec3 pos{};
    idMat3 axis{};
    idVec2 size{};
    float sizeScale = 0.0f;
    decalHandle_t handle{-1, -1};
    int cost = 0;
    int index = -2;
    union {
      struct {
        std::uint8_t quad : 1;
        std::uint8_t fadeState : 1;
      };
      std::uint8_t flags = 0;
    };
    std::uint8_t recoveredPadding[7]{};
  };

  struct tireDecalInfo_t {
    int startTime = 0;
    int lifeTime = 0;
    const idMaterial* decalMtr = nullptr;
    int fadeTime = 0;
    float scaleBiasX = 0.0f;
    float scaleBiasY = 0.0f;
    idVec3 pos{};
    idVec3 dir{};
    float depth = 0.0f;
    idVec3 corners[4]{};
    idVec3 left{};
    float halfWidth = 0.0f;
    float halfHeight = 0.0f;
    bool markStart = false;
    bool markEnd = false;
    std::uint8_t recoveredPadding0[2]{};
    int cost = 0;
    int fadeState = 0;
    int index = -2;
    std::uint8_t recoveredPadding1[4]{};
  };

  template <typename T> struct DecalBuffer {
    int pop = 0;
    int fade = 0;
    int cost = 0;
    int maxItems = 0;
    int range[2]{};
    int latchedRange[2]{};
    T* decals = nullptr;
    idSphereTree* tree = nullptr;

    void Init(int maxAlloc, int popBarrier, int fadeBarrier,
              int freeSpawnExtraSlots);
    void Update(int gameTime, int max);
  };

  explicit idRenderModelDecal(bool large);
  ~idRenderModelDecal() override;

  static void Init();
  static void Shutdown();

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  bool CommitSubclass() override;

  int GetDecalGenRadius(float radius);
  void GenQuadDecalVerts(const decalInfo_t* decal, idDrawVert* verts);
  void GenBoxDecalVerts(const decalInfo_t* decal, idDrawVert* verts);
  void GenSphericalDecalVerts(const decalInfo_t* decal,
                              idDrawVert* verts);
  decalInfo_t* GetDecalFromHandle(decalHandle_t decalHandle);
  const decalInfo_t* GetDecalFromHandle(decalHandle_t decalHandle) const;
  decalHandle_t AddDecalFromPoint(const decalParams_t* parms, int startTime,
                                  const idVec3& pos, const idVec3& dir);
  void AddDecalFromTireMark(const tireDecalParams_t* parms, int startTime,
                            const idVec3& pos, const idVec3& dir);
  void Update(int currTime);
  bool SetDecalPosition(decalHandle_t decalHandle,
                        const idVec3& decalWorldPosition,
                        const idMat3& decalWorldAxis);
  bool SetDecalAttributes(decalHandle_t decalHandle,
                          const idVec3& decalWorldPosition,
                          const idMat3& decalWorldAxis, float sizeScale);
  bool GetDecalAxis(decalHandle_t decalHandle, idMat3& decalAxis);
  bool RemoveDecal(decalHandle_t decalHandle);
  bool IsDecalInvalid(decalHandle_t decalHandle);

  static idVertexBuffer preAllocatedVertexBuffer[2];
  static idIndexBuffer preAllocatedIndexBufferBoxes;
  static idIndexBuffer preAllocatedIndexBufferQuads;
  static bool preAllocatedBufferInUse;

  maxInfo_t maxInfo;
  DecalBuffer<decalInfo_t> decalBuffer;
  DecalBuffer<tireDecalInfo_t> vehDecBuffer;
  int gameTime = 0;
  std::uint32_t randomSeed = 0;
  int persistentCurIndex = 0;
  int decalSpawnId = 0;
  int latchedTime = 0;
  std::uint8_t recoveredStatePadding[4]{};
  idVertexBuffer vertexBuffer[2];
  idIndexBuffer indexBufferBoxes;
  idIndexBuffer indexBufferQuads;
  bool usesPreAllocatedBuffer = false;
  std::uint8_t recoveredBufferPadding[7]{};
  idTriangles* triangles = nullptr;
  const idDeclRenderParm* parmPositionToLocalBoxSpace[4]{};
};

static_assert(sizeof(decalParams_t) == 48, "decalParams_t ABI drift");
static_assert(offsetof(decalParams_t, depth) == 8,
              "decal parameter depth offset drift");
static_assert(offsetof(decalParams_t, quad) == 36,
              "decal parameter flags offset drift");
static_assert(offsetof(decalParams_t, cost) == 44,
              "decal parameter cost offset drift");
static_assert(sizeof(tireDecalParams_t) == 96,
              "tireDecalParams_t ABI drift");
static_assert(offsetof(tireDecalParams_t, corners) == 20,
              "tire decal corners offset drift");
static_assert(offsetof(tireDecalParams_t, cost) == 92,
              "tire decal cost offset drift");
static_assert(sizeof(idRenderModelDecal::maxInfo_t) == 40,
              "decal max-info ABI drift");
static_assert(sizeof(idRenderModelDecal::decalInfo_t) == 120,
              "decal info ABI drift");
static_assert(offsetof(idRenderModelDecal::decalInfo_t, fadeOutTime) == 20,
              "decal fade offset drift");
static_assert(offsetof(idRenderModelDecal::decalInfo_t, pos) == 36,
              "decal position offset drift");
static_assert(offsetof(idRenderModelDecal::decalInfo_t, axis) == 48,
              "decal axis offset drift");
static_assert(offsetof(idRenderModelDecal::decalInfo_t, handle) == 96,
              "decal handle offset drift");
static_assert(offsetof(idRenderModelDecal::decalInfo_t, flags) == 112,
              "decal flags offset drift");
static_assert(sizeof(idRenderModelDecal::tireDecalInfo_t) == 144,
              "tire decal info ABI drift");
static_assert(offsetof(idRenderModelDecal::tireDecalInfo_t, corners) == 56,
              "tire decal info corners offset drift");
static_assert(offsetof(idRenderModelDecal::tireDecalInfo_t, cost) == 128,
              "tire decal info cost offset drift");
static_assert(sizeof(idRenderModelDecal::DecalBuffer<
                  idRenderModelDecal::decalInfo_t>) == 48,
              "decal buffer ABI drift");
static_assert(offsetof(idRenderModelDecal::DecalBuffer<
                           idRenderModelDecal::decalInfo_t>,
                       range) == 16,
              "decal buffer range offset drift");
static_assert(offsetof(idRenderModelDecal::DecalBuffer<
                           idRenderModelDecal::decalInfo_t>,
                       decals) == 32,
              "decal buffer storage offset drift");
static_assert(offsetof(idRenderModelDecal, maxInfo) == 0x880,
              "decal max-info offset drift");
static_assert(offsetof(idRenderModelDecal, decalBuffer) == 0x8A8,
              "decal buffer offset drift");
static_assert(offsetof(idRenderModelDecal, vehDecBuffer) == 0x8D8,
              "vehicle decal buffer offset drift");
static_assert(offsetof(idRenderModelDecal, gameTime) == 0x908,
              "decal game-time offset drift");
static_assert(offsetof(idRenderModelDecal, vertexBuffer) == 0x920,
              "decal vertex-buffer offset drift");
static_assert(offsetof(idRenderModelDecal, indexBufferBoxes) == 0x950,
              "decal box-index offset drift");
static_assert(offsetof(idRenderModelDecal, indexBufferQuads) == 0x968,
              "decal quad-index offset drift");
static_assert(offsetof(idRenderModelDecal, usesPreAllocatedBuffer) == 0x980,
              "decal preallocation flag offset drift");
static_assert(offsetof(idRenderModelDecal, triangles) == 0x988,
              "decal triangles offset drift");
static_assert(offsetof(idRenderModelDecal, parmPositionToLocalBoxSpace) ==
                  0x990,
              "decal render-parm offset drift");
static_assert(sizeof(idRenderModelDecal) == 0x9B0,
              "idRenderModelDecal ABI drift");
