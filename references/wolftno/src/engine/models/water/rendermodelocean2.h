#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/water/jobs/oceanjob.h"
#include "engine/models/water/jobs/oceanjob2.h"
#include "engine/renderer/bufferobject.h"

#include <cstddef>
#include <cstdint>

class idImage;
class idMaterial;
struct RenderModelOcean2ValidationAccess;

class idRenderModelOcean2 : public idRenderModel {
public:
  enum { NUM_FRAMES = 3, NUMTRIFRAMES = 1 };

  struct COcean2FrameState {
    COcean2FrameState();
    ~COcean2FrameState();

    bool m_bValid{};
    std::uint8_t recoveredPadding01[7]{};
    idImage* m_pNormals{};
    idVertexBuffer m_VB;
    void* m_pMappedVB{};
  };

  explicit idRenderModelOcean2(const char* modelName);
  ~idRenderModelOcean2() override;

  bool CheckValid();
  void CreateWaveParams();
  bool GetSurfacePosition(const idVec3& position, idVec3& surfacePosition,
                          float& height, idVec3& normal);
  void Create(const idMaterial* material, const idBounds& bounds,
              float tileDimension, const char* modelName);
  void Destroy();

private:
  friend struct RenderModelOcean2ValidationAccess;

  void FreeData();
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  bool CommitSubclass() override { return true; }

public:
  idVec2i m_nTiles{};
  idVec2i m_TileDim{};
  idVec2i m_VBDim{};
  COcean2FrameState m_lFrames[NUM_FRAMES];
  int m_iCurrentFrame{};
  std::uint8_t recoveredPadding92C[4]{};
  idTriangles* m_pTri{};
  idIndexBuffer m_IB;
  float m_SpamTimer{};
  std::uint8_t recoveredPadding954[4]{};
  const idMaterial* m_pMaterial{};
  idRenderModelOcean2_TileJobParams* m_pJobParams{};
  std::uint8_t recoveredPadding968[8]{};
  idRenderModelOcean_WaveParams m_WaveParams{};
};

static_assert(sizeof(idRenderModelOcean2::COcean2FrameState) == 0x30,
              "ocean2 frame-state ABI drift");
static_assert(offsetof(idRenderModelOcean2::COcean2FrameState, m_pNormals) ==
                  0x8,
              "ocean2 frame normal-map offset drift");
static_assert(offsetof(idRenderModelOcean2::COcean2FrameState, m_VB) == 0x10,
              "ocean2 frame vertex-buffer offset drift");
static_assert(offsetof(idRenderModelOcean2::COcean2FrameState, m_pMappedVB) ==
                  0x28,
              "ocean2 frame mapped-buffer offset drift");
static_assert(sizeof(idRenderModelOcean2) == 0x1EC0,
              "idRenderModelOcean2 ABI drift");
static_assert(offsetof(idRenderModelOcean2, m_nTiles) == 0x880,
              "ocean2 tile-count offset drift");
static_assert(offsetof(idRenderModelOcean2, m_lFrames) == 0x898,
              "ocean2 frame-array offset drift");
static_assert(offsetof(idRenderModelOcean2, m_IB) == 0x938,
              "ocean2 index-buffer offset drift");
static_assert(offsetof(idRenderModelOcean2, m_WaveParams) == 0x970,
              "ocean2 wave-parameter offset drift");
