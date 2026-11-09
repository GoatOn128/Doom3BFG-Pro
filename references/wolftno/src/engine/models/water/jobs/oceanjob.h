#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

struct COceanVertex {
  float x;
  float y;
  float z;
  std::uint16_t s0;
  std::uint16_t t0;
};

struct idRenderModelOcean_WaveParams {
  struct CWave {
    CWave();

    idVec2 m_WaveVec;
    float m_WaveVecMag;
    float m_Amp;
    float m_Freq;
    float m_LodFadeNearOfs_v;
    float m_LodFadeFarOfs_v;
    float m_LodFadeNearOfs_n;
    float m_LodFadeFarOfs_n;
    std::uint32_t m_Padding[3];
  };

  struct CWaveSOAStore {
    void Create(CWave* waves, int waveCount);

    float m_lWaveVec_x[64];
    float m_lWaveVec_y[64];
    float m_lWaveVecMag[64];
    float m_lAmp[64];
    float m_lFreq[64];
    float m_lLodFadeNearOfs_v[64];
    float m_lLodFadeFarOfs_v[64];
    float m_lLodFadeNearOfs_n[64];
    float m_lLodFadeFarOfs_n[64];
    std::uint32_t m_nWaves;
    std::uint32_t m_Padding[3];
  };

  struct CWaveSOA {
    std::uint32_t m_nWaves;
    const float* m_pWaveVec_x;
    const float* m_pWaveVec_y;
    const float* m_pWaveVecMag;
    const float* m_pAmp;
    const float* m_pFreq;
    const float* m_pLodFadeNearOfs_v;
    const float* m_pLodFadeFarOfs_v;
    const float* m_pLodFadeNearOfs_n;
    const float* m_pLodFadeFarOfs_n;
  };

  idRenderModelOcean_WaveParams();
  ~idRenderModelOcean_WaveParams();

  idVec3 m_TilesOrigin;
  idVec2 m_NormalMapSizeInv;
  float m_Time;
  float m_WaveLodFadeRangeRcp;
  std::uint32_t m_nWavesMax_v;
  std::uint32_t m_nLODs;
  std::uint32_t m_MaxMacroTileSubdivide;
  std::uint32_t m_DebugFlags;
  idVec4 m_LODCamPos;
  std::uint32_t m_Padding[1];
  CWave m_lWaves[64];
  CWaveSOAStore m_WavesSOA;
};

struct idRenderModelOcean_Timers {
  std::int64_t m_TicksWave_v;
  std::int64_t m_TicksWave_n;
  std::int64_t m_TicksUpSample;
  std::int64_t m_TicksVertices;
  std::int64_t m_TicksNormals;
  std::int64_t m_TicksVertSync;
  std::int64_t m_TicksTexSync;
  std::int64_t m_TicksTexUpload;
};

struct CQTNode;

struct idRenderModelOcean_TileJobParams {
  COceanVertex* m_pVB;
  void* m_pWorkMem;
  void* m_pNormalMap;
  CQTNode* m_pQTNode;
  idRenderModelOcean_WaveParams* m_pParams;
  std::uint32_t m_Padding1[2];
  std::uint32_t m_TileTess;
  idBounds m_TileBounds;
  idVec2 m_TileOfs;
  idVec2i m_TileNormalMapOfs;
  idVec2i m_TileST1Ofs;
  std::uint32_t m_TileSTDim;
  std::uint32_t m_TileSTScale;
  float m_TileDim;
  std::uint32_t m_iLOD;
  std::uint32_t m_NormalMapPitch;
  float m_MinDist;
  float m_MaxDist;
  idRenderModelOcean_Timers m_Timers;
};

void UpSampleVertices_BiLinear(unsigned int sourceSide,
                               unsigned int destinationSide,
                               const idVec4* source, idVec4* destination,
                               unsigned int debugFlags, unsigned int x0,
                               unsigned int y0, unsigned int x1,
                               unsigned int y1);
void UpSampleVertices_BiCubic(unsigned int sourceSide,
                              unsigned int destinationSide,
                              const idVec4* source, idVec4* destination,
                              unsigned int debugFlags);
void CreateNormals(unsigned int sourceSide, const idVec4* source,
                   unsigned char* texels, unsigned int pitch,
                   unsigned int debugFlags);

template <int AddToExisting>
void ApplyWavesSOA(
    const idVec3& offset, float time, float vertexStep,
    unsigned int vertexSide,
    const idRenderModelOcean_WaveParams::CWaveSOA& waves,
    unsigned int waveCount, idVec4* destination, unsigned int debugFlags,
    const idVec4& lodCameraPosition, float lodFadeReciprocal);

extern template void ApplyWavesSOA<0>(
    const idVec3&, float, float, unsigned int,
    const idRenderModelOcean_WaveParams::CWaveSOA&, unsigned int, idVec4*,
    unsigned int, const idVec4&, float);
extern template void ApplyWavesSOA<1>(
    const idVec3&, float, float, unsigned int,
    const idRenderModelOcean_WaveParams::CWaveSOA&, unsigned int, idVec4*,
    unsigned int, const idVec4&, float);

void OceanGenJob(idRenderModelOcean_TileJobParams* params);

static_assert(sizeof(COceanVertex) == 16);
static_assert(offsetof(COceanVertex, s0) == 12);
static_assert(sizeof(idRenderModelOcean_WaveParams::CWave) == 48);
static_assert(sizeof(idRenderModelOcean_WaveParams::CWaveSOAStore) == 2320);
static_assert(offsetof(idRenderModelOcean_WaveParams::CWaveSOAStore, m_nWaves) ==
              2304);
static_assert(sizeof(idRenderModelOcean_WaveParams::CWaveSOA) == 80);
static_assert(offsetof(idRenderModelOcean_WaveParams::CWaveSOA,
                       m_pWaveVec_x) == 8);
static_assert(sizeof(idRenderModelOcean_WaveParams) == 5456);
static_assert(offsetof(idRenderModelOcean_WaveParams, m_LODCamPos) == 44);
static_assert(offsetof(idRenderModelOcean_WaveParams, m_lWaves) == 64);
static_assert(offsetof(idRenderModelOcean_WaveParams, m_WavesSOA) == 3136);
static_assert(sizeof(idRenderModelOcean_Timers) == 64);
static_assert(sizeof(idRenderModelOcean_TileJobParams) == 192);
static_assert(offsetof(idRenderModelOcean_TileJobParams, m_TileTess) == 48);
static_assert(offsetof(idRenderModelOcean_TileJobParams, m_Timers) == 128);
