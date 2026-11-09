#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/water/jobs/oceanjob.h"
#include "engine/renderer/bufferobject.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/geometry/rendermatrix.h"

#include <cstddef>
#include <cstdint>

class idDeclEnv;
class idImage;
class idMaterial;
struct RenderModelOceanValidationAccess;

struct idOceanParams_t {
  float m_AmpMin{0.5f};
  float m_AmpMax{12.0f};
  float m_AmpDistributionExp{2.2f};
  float m_AmpK{0.06f};
  float m_AngleBase{};
  float m_AngleSpread{40.0f};
  float m_GravityK{512.0f};
  int m_nMaxWaves{36};
  int m_nMacroTileSubdivide{5};
  idVec2i m_nMacroTiles{4, 4};
  int m_MacroTileDim{0x2000};
  float m_MinVertPerWaveLen{0.5f};
  float m_MinTexelPerWaveLen{0.5f};
  float m_LODFadeRange{128.0f};
  float m_LODK{0.04f};
};

struct CQTRope;

struct CQTNode {
  std::uint16_t m_iCh{};
  std::uint16_t m_TileTess{};
  std::uint16_t m_TileOfsX{};
  std::uint16_t m_TileOfsY{};
  std::uint32_t m_iTile{~0u};
  std::uint32_t recoveredPadding0C{};
  CQTRope* m_lpRopes[4]{};
};

struct CQTRope {
  CQTNode* m_lpNodes[2]{};
  CQTRope* m_pNext{};
  std::uint16_t m_Len{};
  std::uint16_t recoveredPadding1A{};
  std::uint16_t m_lEdgeOfs[2]{};
};

struct CSubdivideParams {
  void SplitRope(CQTNode* currentNode, unsigned int direction,
                 unsigned int ropeSide, CQTNode* destination0,
                 CQTNode* destination1, unsigned int splitLength);
  CQTRope* AssignNewRope(CQTNode* target0, CQTNode* target1,
                         unsigned int direction0, unsigned int direction1,
                         std::uint16_t length, CQTRope* next);

  CQTNode* m_pQTNodes{};
  unsigned int m_nQTNodes{};
  unsigned int m_nQTNodesMax{};
  CQTRope* m_pQTRopes{};
  unsigned int m_nQTRopes{};
  unsigned int m_nQTRopesMax{};
  idRenderModelOcean_TileJobParams* m_pTileJobs{};
  std::uint8_t* m_pTileLODs{};
  idVec3 m_CamLocal{};
  std::uint32_t recoveredPadding3C{};
  idRenderMatrix m_MVP{};
  float m_kLOD{};
  float m_LODofs{};
  unsigned int m_MaxLOD{};
  float m_Cullmargin{};
};

class idOceanSharedTileMemory {
public:
  enum { NUM_FRAMES = 3, NUM_LODS = 4 };

  struct CFrameState {
    CFrameState();
    ~CFrameState();

    bool Create(unsigned int vertexCount, idVec2i atlasSize, int frame);

    idVertexBuffer m_VB;
    unsigned int m_VertMax{};
    unsigned int m_VertAllocPos{};
    unsigned int m_AtlasAllocPos{};
    std::uint32_t recoveredPadding24{};
    void* m_pMappedVB{};
    idImage* m_pNormals{};
  };

  struct CLODIndices {
    unsigned int m_nIndices{};
    unsigned int m_nVertices{};
    unsigned int m_Ofs{};
  };

  idOceanSharedTileMemory();
  ~idOceanSharedTileMemory();

  void Swap();
  void Destroy();
  bool Alloc(unsigned int tileCount, unsigned int tileBaseSize,
             const std::uint8_t* lods, unsigned int* vertexOffsets,
             idVec2i* atlasTexelOffsets, void** workMemory);
  bool Create(unsigned int vertexCount, unsigned int tileBaseSize,
              idVec2i atlasSizeInTiles, unsigned int workMemorySize);

  CFrameState m_lFrameMem[NUM_FRAMES];
  bool m_bInit{};
  std::uint8_t recoveredPaddingA9[3]{};
  unsigned int m_TileBaseSize{};
  idVec2i m_AtlasSizeInTiles{};
  idVec2i m_BaseAtlasTileOfs{};
  idVec2i m_AtlasSize{};
  unsigned int m_nAtlasBaseTiles{};
  int m_iCurrentFrame{};
  void* m_pWorkMem{};
  unsigned int m_WorkMemSize{};
  unsigned int m_WorkMemAllocPos{};
  unsigned int m_Starve{};
  std::uint8_t recoveredPaddingE4[12]{};
  idList<CLODIndices, 5> m_lLODindices;
  idIndexBuffer m_IB;
};

class idRenderModelOcean : public idRenderModel {
public:
  enum { NUM_FRAMES = 3, NUM_TRI_FRAMES = 1 };

  struct COceanFrameState {
    COceanFrameState();
    ~COceanFrameState();
    void Create(unsigned int tileCount);
    void Destroy();

    std::uint8_t* m_pTileLOD{};
    unsigned int* m_pTileVBOffset{};
    idVec2i* m_pTileAtlasTexelOfs{};
    void** m_ppTileWorkMem{};
    bool m_bValid{};
    std::uint8_t recoveredPadding21[3]{};
    unsigned int m_nTilesUsed{};
  };

  explicit idRenderModelOcean(const char* modelName);
  ~idRenderModelOcean() override;

  void CreateWaveParams(const idOceanParams_t* params);
  bool GetSurfacePosition(const idVec3& position, float quality,
                          float normalQuality, int time,
                          idVec3& surfacePosition, idVec3& normal,
                          float& waveHeight);
  bool CheckValid();
  void Create(const idOceanParams_t* params, const idMaterial* material,
              const idDeclEnv* environmentAir,
              const idDeclEnv* environmentWater);

private:
  friend struct RenderModelOceanValidationAccess;

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  bool CommitSubclass() override { return true; }

public:
  idOceanParams_t m_Params{};
  int m_UseCVarParams{};
  std::uint32_t recoveredPadding8C4{};
  idList<idRenderModel*, 5> m_lpMacroTileOccTest;
  idOceanSharedTileMemory* m_pTileSharedMem{};
  float m_MacroTileDim{};
  idVec2i m_nMacroTiles{};
  std::uint32_t recoveredPadding8F4{};
  COceanFrameState m_lFrames[NUM_FRAMES];
  int m_iCurrentFrame{};
  int m_iCurrentTriFrame{};
  idList<CQTNode, 5> m_lQTNodes;
  idList<CQTRope, 5> m_lQTRopes;
  unsigned int m_nTilesMax{};
  unsigned int m_nTilesPrevUsed{};
  unsigned int m_TileTess{};
  std::uint32_t recoveredPadding9B4{};
  idTriangles* m_lpTri[NUM_TRI_FRAMES]{};
  const idMaterial* m_pMaterial{};
  const idDeclEnv* m_pEnvAir{};
  const idDeclEnv* m_pEnvWater{};
  std::uint8_t recoveredPadding9D8[8]{};
  idRenderModelOcean_WaveParams m_WaveParams{};
  idRenderModelOcean_TileJobParams* m_pJobParams{};
  float m_SpamTimer{};
  std::uint32_t recoveredPadding1F3C{};
};

void Ocean_SetCVars(const idOceanParams_t* params);
void TraceQT(CQTNode* nodes, unsigned int nodeCount, CQTRope* ropes,
             unsigned int ropeCount);
void ValidateQT(CQTNode* nodes, unsigned int nodeCount, CQTRope* ropes,
                unsigned int ropeCount,
                idRenderModelOcean_TileJobParams* jobParams);
CQTRope* FindNeighbourRope(CQTRope* rope, unsigned int side,
                           unsigned int direction, unsigned int distance);
void FillWithJunk(idImage* image, int index);
void SyncVert_1x_1x(COceanVertex* vertices1, COceanVertex* vertices2,
                    unsigned int vertexCount);
void SyncVert_1x_1x(COceanVertex* vertices1, unsigned int stride1,
                    COceanVertex* vertices2, unsigned int stride2,
                    unsigned int vertexCount);
void SyncVert_2x_1x(COceanVertex* vertices2x, COceanVertex* vertices1x,
                    unsigned int vertexCount);
void SyncVert_2x_1x(COceanVertex* vertices2x, unsigned int stride2x,
                    COceanVertex* vertices1x, unsigned int stride1x,
                    unsigned int vertexCount);
void SyncTexel_1x_1x(std::uint32_t* texels1, std::uint32_t* texels2,
                     unsigned int texelCount);
void SyncTexel_1x_1x(std::uint32_t* texels1, int stride1,
                     std::uint32_t* texels2, int stride2,
                     unsigned int texelCount);
void SyncTexel_2x_1x(std::uint32_t* texels2x, unsigned int stride2x,
                     std::uint32_t* texels1x, unsigned int stride1x,
                     unsigned int texelCount);
void SyncTileVertices_QT(CQTNode* nodes, unsigned int nodeCount,
                         CQTRope* ropes, unsigned int ropeCount,
                         idRenderModelOcean_TileJobParams* jobParams,
                         unsigned int tileCount, std::uint8_t syncDisable);
unsigned int MacroTileSubdivide_r(CSubdivideParams* params, CQTNode* node,
                                  unsigned int level,
                                  unsigned int maximumTiles,
                                  unsigned int tileBase,
                                  const idVec3& tileMinimum,
                                  float tileDimension, bool visible);
void OceanSyncJob(idRenderModelOcean* ocean);

static_assert(sizeof(idOceanParams_t) == 0x40, "ocean params ABI drift");
static_assert(sizeof(CQTNode) == 0x30, "ocean quadtree-node ABI drift");
static_assert(offsetof(CQTNode, m_iTile) == 0x8,
              "ocean quadtree tile offset drift");
static_assert(offsetof(CQTNode, m_lpRopes) == 0x10,
              "ocean quadtree rope offset drift");
static_assert(sizeof(CQTRope) == 0x20, "ocean rope ABI drift");
static_assert(offsetof(CQTRope, m_Len) == 0x18,
              "ocean rope length offset drift");
static_assert(sizeof(CSubdivideParams) == 0x90,
              "ocean subdivision parameters ABI drift");
static_assert(sizeof(idOceanSharedTileMemory::CFrameState) == 0x38,
              "shared ocean frame-state ABI drift");
static_assert(offsetof(idOceanSharedTileMemory::CFrameState, m_pMappedVB) ==
                  0x28,
              "shared ocean mapped-buffer offset drift");
static_assert(sizeof(idOceanSharedTileMemory) == 0x120,
              "shared ocean memory ABI drift");
static_assert(offsetof(idOceanSharedTileMemory, m_iCurrentFrame) == 0xCC,
              "shared ocean frame index offset drift");
static_assert(offsetof(idOceanSharedTileMemory, m_lLODindices) == 0xF0,
              "shared ocean LOD list offset drift");
static_assert(offsetof(idOceanSharedTileMemory, m_IB) == 0x108,
              "shared ocean index buffer offset drift");
static_assert(sizeof(idRenderModelOcean::COceanFrameState) == 0x28,
              "ocean frame-state ABI drift");
static_assert(sizeof(idRenderModelOcean) == 0x1F40,
              "idRenderModelOcean ABI drift");
static_assert(offsetof(idRenderModelOcean, m_Params) == 0x880,
              "ocean parameter offset drift");
static_assert(offsetof(idRenderModelOcean, m_lpMacroTileOccTest) == 0x8C8,
              "ocean macro-tile list offset drift");
static_assert(offsetof(idRenderModelOcean, m_lFrames) == 0x8F8,
              "ocean frame-array offset drift");
static_assert(offsetof(idRenderModelOcean, m_lQTNodes) == 0x978,
              "ocean quadtree list offset drift");
static_assert(offsetof(idRenderModelOcean, m_lQTRopes) == 0x990,
              "ocean rope-list offset drift");
static_assert(offsetof(idRenderModelOcean, m_WaveParams) == 0x9E0,
              "ocean wave-parameter offset drift");
static_assert(offsetof(idRenderModelOcean, m_pJobParams) == 0x1F30,
              "ocean job-parameter offset drift");
