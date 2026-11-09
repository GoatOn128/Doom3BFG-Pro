#include "engine/models/water/rendermodelocean.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/common.h"
#include "engine/models/renderworld.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <immintrin.h>
#include <vector>

extern idCVar r_skipWater;

idCVar r_ocean_jobmode("r_ocean_jobmode", "1", CVAR_INTEGER,
                       "0 = off, 1 = threaded");
idCVar r_ocean_usecvarparams("r_ocean_usecvarparams", "0", CVAR_INTEGER,
                             "Configure ocean by cvars (for debug/tuning)");
idCVar r_ocean_maxwaves("r_ocean_maxwaves", "36", CVAR_INTEGER,
                        "Ocean max wave count", 1.0f, 64.0f);
idCVar r_ocean_wavelodexp_v(
    "r_ocean_wavelodexp_v", "1.0", CVAR_FLOAT,
    "Ocean lod vertex wave count distribution exponent");
idCVar r_ocean_wavelodexp_n(
    "r_ocean_wavelodexp_n", "1.0", CVAR_FLOAT,
    "Ocean lod normalmap wave count distribution exponent");
idCVar r_ocean_ampmin("r_ocean_ampmin", "0.5", CVAR_FLOAT,
                      "Ocean wave gen min amp");
idCVar r_ocean_ampmax("r_ocean_ampmax", "12.0", CVAR_FLOAT,
                      "Ocean wave gen max amp");
idCVar r_ocean_ampdistrexp("r_ocean_ampdistrexp", "2.2", CVAR_FLOAT,
                           "Ocean wave gen amp distribution exponent");
idCVar r_ocean_minvertperwavelen("r_ocean_minvertperwavelen", "0.5",
                                 CVAR_FLOAT, "Ocean minvertperwavelen");
idCVar r_ocean_mintexelperwavelen("r_ocean_mintexelperwavelen", "0.5",
                                  CVAR_FLOAT, "Ocean mintexelperwavelen");
idCVar r_ocean_wavelodfaderange("r_ocean_wavelodfaderange", "128",
                                CVAR_FLOAT,
                                "Ocean r_ocean_wavelodfaderange");
idCVar r_ocean_gravk("r_ocean_gravk", "512", CVAR_FLOAT,
                     "Ocean wave gravity constant");
idCVar r_ocean_ampk("r_ocean_ampk", "0.06", CVAR_FLOAT,
                    "Ocean amp-displacement constant");
idCVar r_ocean_anglebase("r_ocean_anglebase", "0.0", CVAR_FLOAT,
                         "Ocean wave base direction in degrees");
idCVar r_ocean_anglespread("r_ocean_anglespread", "40.0", CVAR_FLOAT,
                           "Ocean wave direction spread in degrees");
idCVar r_ocean_lodk("r_ocean_lodk", "0.04", CVAR_FLOAT,
                    "Ocean lod scale constant. (lod = sqrt(tiledist)*lodk)");
idCVar r_ocean_debugflags("r_ocean_debugflags", "0", CVAR_INTEGER,
                          "Ocean debug flags.");
idCVar r_ocean_spamtimer("r_ocean_spamtimer", "0", CVAR_FLOAT,
                         "Ocean debug spam timer.");
idCVar r_ocean_printwaves("r_ocean_printwaves", "0", CVAR_INTEGER,
                          "Print ocean wave parameters.");
idCVar r_ocean_macrotilesubdivide("r_ocean_macrotilesubdivide", "5",
                                  CVAR_INTEGER,
                                  "Max macro tile subdivision.");
// The misspelling is present in the retail registration string.
idCVar r_ocean_syncdisable("r_ocean_synddisable", "0", CVAR_INTEGER,
                           "Ocean sync disable flags.");
idCVar oceanAtlasSize_x("oceanAtlasSize_x", "15", CVAR_INTEGER, "");
idCVar oceanAtlasSize_y("oceanAtlasSize_y", "12", CVAR_INTEGER, "");

idCodeResource<idDeclRenderParm> rp_OceanNormalMap("oceanNormalMap");
idCodeResource<idDeclRenderParm> rp_OceanMacroTileSTDim(
    "oceanMacroTileSTDim");

namespace {

constexpr memTag_t kWaterMemoryTag = TAG_RENDERMODEL;
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_set1_epi32(static_cast<int>(0x80000000u)));

idParallelJobRegistration register_OceanSyncJob(
    reinterpret_cast<jobRun_t>(OceanSyncJob), "OceanSyncJob");

void* AllocWater(std::size_t size, bool zero) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\water\\RenderModelOcean.cpp : "
      "TAG_WATER",
      size, kWaterMemoryTag, zero, ALIGN_16, HEAP_DEFAULTHEAP);
}

void FreeWater(void*& pointer) {
  if (pointer != nullptr)
    idMem::Singleton()->Free(pointer, ALIGN_16);
  pointer = nullptr;
}

std::uint32_t AverageTexel(std::uint32_t first, std::uint32_t second) {
  const std::uint32_t low =
      ((first & 0x00ff00ffu) + (second & 0x00ff00ffu)) >> 1;
  const std::uint32_t high =
      (((first >> 8) & 0x00ff00ffu) + ((second >> 8) & 0x00ff00ffu))
      >> 1;
  return (low & 0x00ff00ffu) | ((high & 0x00ff00ffu) << 8);
}

void AverageVertexPair(COceanVertex& first, COceanVertex& second) {
  const float x = (first.x + second.x) * 0.5f;
  const float y = (first.y + second.y) * 0.5f;
  const float z = (first.z + second.z) * 0.5f;
  first.x = second.x = x;
  first.y = second.y = y;
  first.z = second.z = z;
}

void InterpolateVertex(COceanVertex& destination, const COceanVertex& first,
                       const COceanVertex& second) {
  destination.x = (first.x + second.x) * 0.5f;
  destination.y = (first.y + second.y) * 0.5f;
  destination.z = (first.z + second.z) * 0.5f;
}

idRenderModelParms& ActiveParms(idRenderModel& model) {
  return model.useDeferredPosition != 0
             ? *reinterpret_cast<idRenderModelParms*>(&model.deferredOrigin)
             : model.g;
}

const idRenderModelParms& ActiveParms(const idRenderModel& model) {
  return model.useDeferredPosition != 0
             ? *reinterpret_cast<const idRenderModelParms*>(
                   &model.deferredOrigin)
             : model.g;
}

idVec3 WorldToLocal(const idRenderModel& model, const idVec3& position) {
  const idRenderModelParms& parms = ActiveParms(model);
  const idVec3 delta{position.x - parms.origin.x,
                     position.y - parms.origin.y,
                     position.z - parms.origin.z};
  return {(delta.x * parms.axis.mat[0].x +
           delta.y * parms.axis.mat[0].y +
           delta.z * parms.axis.mat[0].z) /
              model.g.scale.x,
          (delta.x * parms.axis.mat[1].x +
           delta.y * parms.axis.mat[1].y +
           delta.z * parms.axis.mat[1].z) /
              model.g.scale.y,
          (delta.x * parms.axis.mat[2].x +
           delta.y * parms.axis.mat[2].y +
           delta.z * parms.axis.mat[2].z) /
              model.g.scale.z};
}

idVec3 LocalToWorld(const idRenderModel& model, const idVec3& position) {
  const idRenderModelParms& parms = ActiveParms(model);
  const idVec3 scaled{position.x * model.g.scale.x,
                      position.y * model.g.scale.y,
                      position.z * model.g.scale.z};
  return {scaled.x * parms.axis.mat[0].x +
              scaled.y * parms.axis.mat[1].x +
              scaled.z * parms.axis.mat[2].x + parms.origin.x,
          scaled.x * parms.axis.mat[0].y +
              scaled.y * parms.axis.mat[1].y +
              scaled.z * parms.axis.mat[2].y + parms.origin.y,
          scaled.x * parms.axis.mat[0].z +
              scaled.y * parms.axis.mat[1].z +
              scaled.z * parms.axis.mat[2].z + parms.origin.z};
}

void DrawDebugRopes(idRenderModelOcean& ocean, unsigned int nodeCount,
                    unsigned int ropeCount, float gridStep) {
  idRenderDebug* const renderDebug = ocean.GetRenderDebug();
  if (renderDebug == nullptr || ocean.m_lQTNodes.list == nullptr ||
      ocean.m_lQTRopes.list == nullptr || ocean.m_pJobParams == nullptr)
    return;

  std::vector<std::uint8_t> ropeDone(ropeCount, 0);
  for (unsigned int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    CQTNode& node = ocean.m_lQTNodes.list[nodeIndex];
    const int ropeStartX[4] = {
        int(node.m_TileOfsX + node.m_TileTess), int(node.m_TileOfsX),
        int(node.m_TileOfsX), int(node.m_TileOfsX)};
    const int ropeStartY[4] = {
        int(node.m_TileOfsY), int(node.m_TileOfsY),
        int(node.m_TileOfsY + node.m_TileTess), int(node.m_TileOfsY)};

    for (unsigned int direction = 0; direction < 4; ++direction) {
      CQTRope* rope = node.m_lpRopes[direction];
      int startX = ropeStartX[direction];
      int startY = ropeStartY[direction];
      const int horizontal = direction >= 2 ? 1 : 0;
      const int vertical = direction < 2 ? 1 : 0;
      while (rope != nullptr) {
        const std::ptrdiff_t ropeIndex = rope - ocean.m_lQTRopes.list;
        if (ropeIndex < 0 ||
            static_cast<unsigned int>(ropeIndex) >= ropeCount ||
            ropeDone[static_cast<std::size_t>(ropeIndex)] != 0)
          break;
        ropeDone[static_cast<std::size_t>(ropeIndex)] = 1;

        const int endX = startX + int(rope->m_Len) * horizontal;
        const int endY = startY + int(rope->m_Len) * vertical;
        const idVec3 lineStart = LocalToWorld(
            ocean, {float(startX) * gridStep, float(startY) * gridStep, 0.0f});
        const idVec3 lineEnd = LocalToWorld(
            ocean, {float(endX) * gridStep, float(endY) * gridStep, 0.0f});
        const idVec3 lineMiddle0{
            (lineStart.x + lineEnd.x) * 0.5f,
            (lineStart.y + lineEnd.y) * 0.5f,
            (lineStart.z + lineEnd.z) * 0.5f};
        const idVec3 lineMiddle1{
            (lineStart.x + lineEnd.x * 3.0f) * 0.25f,
            (lineStart.y + lineEnd.y * 3.0f) * 0.25f,
            (lineStart.z + lineEnd.z * 3.0f) * 0.25f};
        renderDebug->DebugLine(idColor::colorWhite, lineStart, lineMiddle0,
                               0, false);
        renderDebug->DebugLine(idColor::colorLtGrey, lineMiddle0,
                               lineMiddle1, 0, false);
        renderDebug->DebugLine(idColor::colorDkGrey, lineMiddle1, lineEnd, 0,
                               false);

        for (unsigned int side = 0; side < 2; ++side) {
          CQTNode* const linkedNode = rope->m_lpNodes[side];
          if (linkedNode == nullptr || linkedNode->m_iTile == ~0u)
            continue;
          const idRenderModelOcean_TileJobParams& tile =
              ocean.m_pJobParams[linkedNode->m_iTile];
          int edgeX = tile.m_TileST1Ofs.x;
          int edgeY = tile.m_TileST1Ofs.y;
          if (side == 0) {
            edgeX += vertical * int(tile.m_TileSTDim) - vertical;
            edgeY += horizontal * int(tile.m_TileSTDim) - horizontal;
          } else {
            edgeX += vertical;
            edgeY += horizontal;
          }
          edgeX += horizontal * int(rope->m_lEdgeOfs[side]);
          edgeY += vertical * int(rope->m_lEdgeOfs[side]);
          const idVec3 edgePoint = LocalToWorld(
              ocean, {float(edgeX) * gridStep, float(edgeY) * gridStep, 0.0f});
          renderDebug->DebugLine(side == 0 ? idColor::colorYellow
                                           : idColor::colorPink,
                                 lineStart, edgePoint, 0, false);
        }

        startX = endX;
        startY = endY;
        rope = rope->m_pNext;
      }
    }
  }
}

idRenderModelOcean_WaveParams::CWaveSOA MakeWaveView(
    const idRenderModelOcean_WaveParams::CWaveSOAStore& store,
    unsigned int count) {
  return {count,
          store.m_lWaveVec_x,
          store.m_lWaveVec_y,
          store.m_lWaveVecMag,
          store.m_lAmp,
          store.m_lFreq,
          store.m_lLodFadeNearOfs_v,
          store.m_lLodFadeFarOfs_v,
          store.m_lLodFadeNearOfs_n,
          store.m_lLodFadeFarOfs_n};
}

} // namespace

idRenderModelOcean_WaveParams::CWave::CWave() = default;
idRenderModelOcean_WaveParams::idRenderModelOcean_WaveParams() = default;
idRenderModelOcean_WaveParams::~idRenderModelOcean_WaveParams() = default;

void idRenderModelOcean_WaveParams::CWaveSOAStore::Create(CWave* waves,
                                                          int waveCount) {
  m_nWaves = static_cast<std::uint32_t>(waveCount);
  for (int index = 0; index < waveCount; ++index) {
    m_lWaveVec_x[index] = waves[index].m_WaveVec.x;
    m_lWaveVec_y[index] = waves[index].m_WaveVec.y;
    m_lWaveVecMag[index] = waves[index].m_WaveVecMag;
    m_lAmp[index] = waves[index].m_Amp;
    m_lFreq[index] = waves[index].m_Freq;
    m_lLodFadeNearOfs_v[index] = waves[index].m_LodFadeNearOfs_v;
    m_lLodFadeFarOfs_v[index] = waves[index].m_LodFadeFarOfs_v;
    m_lLodFadeNearOfs_n[index] = waves[index].m_LodFadeNearOfs_n;
    m_lLodFadeFarOfs_n[index] = waves[index].m_LodFadeFarOfs_n;
  }
  std::memset(m_Padding, 0, sizeof(m_Padding));
}

void TraceQT(CQTNode* nodes, unsigned int nodeCount, CQTRope* ropes,
             unsigned int ropeCount) {
  for (unsigned int index = 0; index < nodeCount; ++index) {
    CQTNode& node = nodes[index];
    idLib::Printf(
        "N %d, iT %d, iChN %d, Ofs %.4x, %.4x, T %.4x, Ropes "
        "%d(%p), %d(%p), %d(%p), %d(%p)\n",
        index, node.m_iTile, node.m_iCh, node.m_TileOfsX, node.m_TileOfsY,
        node.m_TileTess,
        node.m_lpRopes[0] != nullptr ? int(node.m_lpRopes[0] - ropes) : -1,
        node.m_lpRopes[0],
        node.m_lpRopes[1] != nullptr ? int(node.m_lpRopes[1] - ropes) : -1,
        node.m_lpRopes[1],
        node.m_lpRopes[2] != nullptr ? int(node.m_lpRopes[2] - ropes) : -1,
        node.m_lpRopes[2],
        node.m_lpRopes[3] != nullptr ? int(node.m_lpRopes[3] - ropes) : -1,
        node.m_lpRopes[3]);
  }
  for (unsigned int index = 0; index < ropeCount; ++index) {
    CQTRope& rope = ropes[index];
    idLib::Printf(
        "R %d, Len %.4x, Ofs %.4x, %.4x, N %d(%p), %d(%p), RNext "
        "%d(%p)\n",
        index, rope.m_Len, rope.m_lEdgeOfs[0], rope.m_lEdgeOfs[1],
        rope.m_lpNodes[0] != nullptr ? int(rope.m_lpNodes[0] - nodes) : -1,
        rope.m_lpNodes[0],
        rope.m_lpNodes[1] != nullptr ? int(rope.m_lpNodes[1] - nodes) : -1,
        rope.m_lpNodes[1],
        rope.m_pNext != nullptr ? int(rope.m_pNext - ropes) : -1,
        rope.m_pNext);
  }
}

void ValidateQT(CQTNode* nodes, unsigned int nodeCount, CQTRope* ropes,
                unsigned int ropeCount,
                idRenderModelOcean_TileJobParams* jobParams) {
  bool broken = false;
  std::vector<std::uint8_t> linked(ropeCount, 0);
  for (unsigned int index = 0; index < ropeCount; ++index) {
    if (ropes[index].m_pNext == nullptr)
      continue;
    const std::ptrdiff_t next = ropes[index].m_pNext - ropes;
    if (next < 0 || static_cast<unsigned int>(next) >= ropeCount ||
        ++linked[static_cast<std::size_t>(next)] > 1) {
      idLib::Printf("Rope %d was linked to %d times.\n", int(next),
                    next >= 0 && static_cast<unsigned int>(next) < ropeCount
                        ? linked[static_cast<std::size_t>(next)]
                        : 0);
      broken = true;
    }
  }

  for (unsigned int nodeIndex = 0; nodeIndex < nodeCount; ++nodeIndex) {
    CQTNode& node = nodes[nodeIndex];
    for (unsigned int direction = 0; direction < 4; ++direction) {
      CQTRope* rope = node.m_lpRopes[direction];
      unsigned int followed = 0;
      while (rope != nullptr && followed < node.m_TileTess) {
        const unsigned int side = direction & 1u;
        if (rope->m_lpNodes[side] != &node) {
          idLib::Printf(
              "Node %d(%p), dir %d, pointed to rope %d(%p), but it "
              "pointed back at %d(%p)\n",
              nodeIndex, &node, direction, int(rope - ropes), rope,
              rope->m_lpNodes[side] != nullptr
                  ? int(rope->m_lpNodes[side] - nodes)
                  : -1,
              rope->m_lpNodes[side]);
          broken = true;
          break;
        }
        if (rope->m_lEdgeOfs[side] != followed) {
          idLib::Printf(
              "Node %d(%p), dir %d, pointed to rope %d(%p), edge ofs "
              "is %d, should be %d\n",
              nodeIndex, &node, direction, int(rope - ropes), rope,
              rope->m_lEdgeOfs[side], followed);
          broken = true;
        }
        followed += rope->m_Len;
        rope = rope->m_pNext;
      }
      if (node.m_lpRopes[direction] != nullptr &&
          followed != node.m_TileTess) {
        idLib::Printf(
            "Node %d(%p), dir %d, Was only able to follow rope %d "
            "units out of %d\n",
            nodeIndex, &node, direction, followed, node.m_TileTess);
        broken = true;
      }
    }
    if (node.m_iTile != ~0u && jobParams[node.m_iTile].m_pQTNode != &node) {
      idLib::Printf("Incorrect tile %d node ptr %p != %p\n", node.m_iTile,
                    jobParams[node.m_iTile].m_pQTNode, &node);
      broken = true;
    }
  }
  if (broken)
    TraceQT(nodes, nodeCount, ropes, ropeCount);
}

void CSubdivideParams::SplitRope(CQTNode* currentNode,
                                 unsigned int direction,
                                 unsigned int ropeSide,
                                 CQTNode* destination0,
                                 CQTNode* destination1,
                                 unsigned int splitLength) {
  if (splitLength == 0) {
    TraceQT(m_pQTNodes, m_nQTNodes, m_pQTRopes, m_nQTRopes);
    idLib::FatalError("Broken ropes!");
    return;
  }
  CQTRope* rope = currentNode->m_lpRopes[direction];
  if (rope == nullptr) {
    destination0->m_lpRopes[direction] = nullptr;
    destination1->m_lpRopes[direction] = nullptr;
    return;
  }

  destination0->m_lpRopes[direction] = rope;
  unsigned int position = 0;
  while (position < splitLength) {
    if (rope == nullptr || rope->m_lpNodes[ropeSide] != currentNode) {
      TraceQT(m_pQTNodes, m_nQTNodes, m_pQTRopes, m_nQTRopes);
      idLib::FatalError("Broken ropes!");
      return;
    }
    if (position + rope->m_Len > splitLength) {
      if (m_nQTRopes >= m_nQTRopesMax)
        return;
      CQTRope* tail = &m_pQTRopes[m_nQTRopes++];
      *tail = *rope;
      tail->m_Len = static_cast<std::uint16_t>(position + rope->m_Len -
                                               splitLength);
      rope->m_Len = static_cast<std::uint16_t>(rope->m_Len - tail->m_Len);
      tail->m_lEdgeOfs[0] = static_cast<std::uint16_t>(
          tail->m_lEdgeOfs[0] + rope->m_Len);
      tail->m_lEdgeOfs[1] = static_cast<std::uint16_t>(
          tail->m_lEdgeOfs[1] + rope->m_Len);
      rope->m_pNext = tail;
    }
    rope->m_lpNodes[ropeSide] = destination0;
    position += rope->m_Len;
    rope = rope->m_pNext;
  }

  destination1->m_lpRopes[direction] = rope;
  while (position < splitLength * 2) {
    if (rope == nullptr || rope->m_lpNodes[ropeSide] != currentNode) {
      TraceQT(m_pQTNodes, m_nQTNodes, m_pQTRopes, m_nQTRopes);
      idLib::FatalError("Broken ropes!");
      return;
    }
    rope->m_lEdgeOfs[ropeSide] =
        static_cast<std::uint16_t>(position - splitLength);
    rope->m_lpNodes[ropeSide] = destination1;
    position += rope->m_Len;
    rope = rope->m_pNext;
  }
  currentNode->m_lpRopes[direction] = nullptr;
}

CQTRope* CSubdivideParams::AssignNewRope(
    CQTNode* target0, CQTNode* target1, unsigned int direction0,
    unsigned int direction1, std::uint16_t length, CQTRope* next) {
  if (m_nQTRopes >= m_nQTRopesMax) {
    idLib::FatalError("Out of ropes!");
    return nullptr;
  }
  CQTRope* rope = &m_pQTRopes[m_nQTRopes++];
  rope->m_lpNodes[0] = target0;
  rope->m_lpNodes[1] = target1;
  rope->m_pNext = next;
  rope->m_Len = length;
  rope->recoveredPadding1A = 0;
  rope->m_lEdgeOfs[0] = 0;
  rope->m_lEdgeOfs[1] = 0;
  target0->m_lpRopes[direction0] = rope;
  target1->m_lpRopes[direction1] = rope;
  return rope;
}

CQTRope* FindNeighbourRope(CQTRope* rope, unsigned int side,
                           unsigned int direction, unsigned int distance) {
  if (rope == nullptr) {
    idLib::FatalError("Rope should've been longer.");
    return nullptr;
  }
  while (distance != 0) {
    if (rope->m_Len > distance)
      return nullptr;
    distance -= rope->m_Len;
    rope = rope->m_pNext;
    if (rope == nullptr) {
      idLib::FatalError("Rope should've been longer.");
      return nullptr;
    }
  }
  CQTNode* node = rope->m_lpNodes[side];
  CQTRope* result = node->m_lpRopes[direction];
  unsigned int remaining = node->m_TileTess;
  while (result != nullptr && result->m_Len < remaining) {
    remaining -= result->m_Len;
    result = result->m_pNext;
  }
  return result;
}

void Ocean_SetCVars(const idOceanParams_t* params) {
  r_ocean_maxwaves.SetInteger(params->m_nMaxWaves, true);
  r_ocean_ampdistrexp.SetFloat(params->m_AmpDistributionExp, true);
  r_ocean_ampmin.SetFloat(params->m_AmpMin, true);
  r_ocean_ampmax.SetFloat(params->m_AmpMax, true);
  r_ocean_anglebase.SetFloat(params->m_AngleBase, true);
  r_ocean_anglespread.SetFloat(params->m_AngleSpread, true);
  r_ocean_gravk.SetFloat(params->m_GravityK, true);
  r_ocean_ampk.SetFloat(params->m_AmpK, true);
  r_ocean_macrotilesubdivide.SetInteger(params->m_nMacroTileSubdivide, true);
  r_ocean_wavelodfaderange.SetFloat(params->m_LODFadeRange, true);
  r_ocean_minvertperwavelen.SetFloat(params->m_MinVertPerWaveLen, true);
  r_ocean_mintexelperwavelen.SetFloat(params->m_MinTexelPerWaveLen, true);
  r_ocean_lodk.SetFloat(params->m_LODK, true);
}

void FillWithJunk(idImage* image, int index) {
  const int width = image->opts.width;
  const int height = image->opts.height;
  auto* pixels = static_cast<std::uint8_t*>(
      AllocWater(static_cast<std::size_t>(width) * height * 4, false));
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int first = static_cast<int>(
          std::sin((float(y + 2 * x) + float(index) * 4.0f) * 0.1f) *
              127.0f +
          127.0f);
      const int second = static_cast<int>(
          std::sin(float(2 * y + x) * 0.07f) * 127.0f + 127.0f);
      std::uint8_t* pixel = pixels + (y * width + x) * 4;
      pixel[0] = static_cast<std::uint8_t>(first);
      pixel[1] = static_cast<std::uint8_t>(second);
      pixel[2] = static_cast<std::uint8_t>(first);
      pixel[3] = static_cast<std::uint8_t>(second);
    }
  }
  image->SubImageUpload(0, 0, 0, 0, width, height, pixels, 0);
  void* allocation = pixels;
  FreeWater(allocation);
}

void SyncVert_1x_1x(COceanVertex* vertices1, COceanVertex* vertices2,
                    unsigned int vertexCount) {
  for (unsigned int index = 0; index < vertexCount; ++index)
    AverageVertexPair(vertices1[index], vertices2[index]);
}

void SyncVert_1x_1x(COceanVertex* vertices1, unsigned int stride1,
                    COceanVertex* vertices2, unsigned int stride2,
                    unsigned int vertexCount) {
  for (unsigned int index = 0; index < vertexCount; ++index)
    AverageVertexPair(vertices1[index * stride1], vertices2[index * stride2]);
}

void SyncVert_2x_1x(COceanVertex* vertices2x, COceanVertex* vertices1x,
                    unsigned int vertexCount) {
  const unsigned int lowCount = (vertexCount + 1) >> 1;
  for (unsigned int index = 0; index < lowCount; ++index)
    AverageVertexPair(vertices2x[index * 2], vertices1x[index]);
  for (unsigned int index = 1; index < vertexCount; index += 2)
    InterpolateVertex(vertices2x[index], vertices1x[index >> 1],
                      vertices1x[(index >> 1) + 1]);
}

void SyncVert_2x_1x(COceanVertex* vertices2x, unsigned int stride2x,
                    COceanVertex* vertices1x, unsigned int stride1x,
                    unsigned int vertexCount) {
  const unsigned int lowCount = (vertexCount + 1) >> 1;
  for (unsigned int index = 0; index < lowCount; ++index)
    AverageVertexPair(vertices2x[index * 2 * stride2x],
                      vertices1x[index * stride1x]);
  for (unsigned int index = 1; index < vertexCount; index += 2)
    InterpolateVertex(vertices2x[index * stride2x],
                      vertices1x[(index >> 1) * stride1x],
                      vertices1x[((index >> 1) + 1) * stride1x]);
}

void SyncTexel_1x_1x(std::uint32_t* texels1, std::uint32_t* texels2,
                     unsigned int texelCount) {
  for (unsigned int index = 0; index < texelCount; ++index) {
    const std::uint32_t average = AverageTexel(texels1[index], texels2[index]);
    texels1[index] = average;
    texels2[index] = average;
  }
}

void SyncTexel_1x_1x(std::uint32_t* texels1, int stride1,
                     std::uint32_t* texels2, int stride2,
                     unsigned int texelCount) {
  for (unsigned int index = 0; index < texelCount; ++index) {
    const std::uint32_t average =
        AverageTexel(texels1[index * stride1], texels2[index * stride2]);
    texels1[index * stride1] = average;
    texels2[index * stride2] = average;
  }
}

void SyncTexel_2x_1x(std::uint32_t* texels2x, unsigned int stride2x,
                     std::uint32_t* texels1x, unsigned int stride1x,
                     unsigned int texelCount) {
  for (unsigned int index = 0; index < texelCount; index += 2) {
    const unsigned int low = index >> 1;
    const std::uint32_t average =
        AverageTexel(texels2x[index * stride2x], texels1x[low * stride1x]);
    texels2x[index * stride2x] = average;
    texels1x[low * stride1x] = average;
  }
  for (unsigned int index = 1; index < texelCount; index += 2) {
    const unsigned int low = index >> 1;
    texels2x[index * stride2x] =
        AverageTexel(texels1x[low * stride1x],
                     texels1x[(low + 1) * stride1x]);
  }
}

idOceanSharedTileMemory::CFrameState::CFrameState()
    : m_VB(), m_VertMax(0), m_VertAllocPos(0), m_AtlasAllocPos(0),
      m_pMappedVB(nullptr), m_pNormals(nullptr) {}

idOceanSharedTileMemory::CFrameState::~CFrameState() {
  if (m_pMappedVB != nullptr) {
    m_VB.UnmapBuffer();
    m_pMappedVB = nullptr;
  }
  m_VB.FreeBufferObject();
  if (m_pNormals != nullptr)
    m_pNormals->PurgeImage();
  m_pNormals = nullptr;
}

bool idOceanSharedTileMemory::CFrameState::Create(unsigned int vertexCount,
                                                  idVec2i atlasSize,
                                                  int frame) {
  if (m_pMappedVB != nullptr) {
    m_VB.UnmapBuffer();
    m_pMappedVB = nullptr;
  }
  m_VB.FreeBufferObject();
  if (m_pNormals != nullptr)
    m_pNormals->PurgeImage();
  m_pNormals = nullptr;
  m_VertMax = vertexCount;
  m_VertAllocPos = 0;
  m_AtlasAllocPos = 0;
  if (!m_VB.AllocBufferObject(nullptr, static_cast<int>(16 * vertexCount),
                              579, BU_DYNAMIC))
    return false;

  idImageOpts options{};
  options.textureType = TT_2D;
  options.format = FMT_RGBA8;
  options.width = atlasSize.x;
  options.height = atlasSize.y;
  options.numLevels = 1;
  options.packedTail = true;
  options.readback = true;
  options.linear = true;
  options.sampler.filter = TF_DEFAULT;
  options.sampler.repeat = TR_REPEAT;
  options.sampler.lodMaxClamp = 13.0f;

  char name[64];
  std::snprintf(name, sizeof(name), "_OceanFrameAtlas%d", frame);
  m_pNormals = globalImages->ScratchImage(name, options);
  if (m_pNormals == nullptr) {
    idLib::FatalError("Ocean normal map atlas allocation failed.");
    return false;
  }
  FillWithJunk(m_pNormals, frame);
  return true;
}

idOceanSharedTileMemory::idOceanSharedTileMemory()
    : m_lFrameMem{}, m_lLODindices(), m_IB() {
  m_bInit = false;
  m_iCurrentFrame = 0;
  m_pWorkMem = nullptr;
  m_WorkMemSize = 0;
  m_WorkMemAllocPos = 0;
  m_Starve = 0;
}

idOceanSharedTileMemory::~idOceanSharedTileMemory() { Destroy(); }

void idOceanSharedTileMemory::Swap() {
  if (!m_bInit)
    return;
  CFrameState& retiring = m_lFrameMem[(m_iCurrentFrame + 2) % NUM_FRAMES];
  if (retiring.m_pMappedVB != nullptr) {
    retiring.m_VB.UnmapBuffer();
    retiring.m_pMappedVB = nullptr;
  }
  m_iCurrentFrame = (m_iCurrentFrame + 1) % NUM_FRAMES;
  m_lFrameMem[m_iCurrentFrame].m_VertAllocPos = 0;
  m_lFrameMem[m_iCurrentFrame].m_AtlasAllocPos = 0;
  m_WorkMemAllocPos = 0;
  m_Starve = 0;
}

void idOceanSharedTileMemory::Destroy() {
  for (CFrameState& frame : m_lFrameMem) {
    if (frame.m_pMappedVB != nullptr) {
      frame.m_VB.UnmapBuffer();
      frame.m_pMappedVB = nullptr;
    }
    frame.m_VB.FreeBufferObject();
    if (frame.m_pNormals != nullptr)
      frame.m_pNormals->PurgeImage();
    frame.m_pNormals = nullptr;
  }
  FreeWater(m_pWorkMem);
  m_WorkMemSize = 0;
  m_WorkMemAllocPos = 0;
  m_IB.FreeBufferObject();
  m_bInit = false;
}

bool idOceanSharedTileMemory::Alloc(unsigned int tileCount,
                                    unsigned int tileBaseSize,
                                    const std::uint8_t* lods,
                                    unsigned int* vertexOffsets,
                                    idVec2i* atlasTexelOffsets,
                                    void** workMemory) {
  if (!m_bInit) {
    idLib::Debugf("(idOceanSharedTileMemory::Alloc) Not initialized.\n");
    return false;
  }
  if (m_TileBaseSize != tileBaseSize) {
    idLib::Debugf(
        "(idOceanSharedTileMemory::Alloc) TileBaseSize mismatch.\n");
    return false;
  }

  CFrameState& frame = m_lFrameMem[m_iCurrentFrame];
  if (tileCount + frame.m_AtlasAllocPos > m_nAtlasBaseTiles) {
    m_Starve |= 1u;
    return false;
  }

  unsigned int vertexPosition = frame.m_VertAllocPos;
  unsigned int workPosition = m_WorkMemAllocPos;
  for (unsigned int index = 0; index < tileCount; ++index) {
    unsigned int tessellation = tileBaseSize >> lods[index];
    if (tessellation == 0)
      tessellation = 1;
    vertexOffsets[index] = 16 * vertexPosition;
    workMemory[index] = static_cast<std::uint8_t*>(m_pWorkMem) + workPosition;
    vertexPosition += (tessellation + 1) * (tessellation + 1);
    const unsigned int workBytes =
        4 * (4 * tessellation + 1) * (4 * tessellation + 1);
    workPosition += (workBytes + 31) & ~15u;
  }

  if (vertexPosition > frame.m_VertMax || workPosition > m_WorkMemSize) {
    m_Starve |= 2u;
    return false;
  }

  for (unsigned int index = 0; index < tileCount; ++index) {
    const unsigned int atlasIndex = frame.m_AtlasAllocPos + index;
    atlasTexelOffsets[index].x =
        int(atlasIndex % unsigned(m_AtlasSizeInTiles.x)) *
        m_BaseAtlasTileOfs.x;
    atlasTexelOffsets[index].y =
        int(atlasIndex / unsigned(m_AtlasSizeInTiles.x)) *
        m_BaseAtlasTileOfs.y;
  }
  frame.m_AtlasAllocPos += tileCount;
  frame.m_VertAllocPos = vertexPosition;
  m_WorkMemAllocPos = workPosition;
  return true;
}

bool idOceanSharedTileMemory::Create(unsigned int vertexCount,
                                     unsigned int tileBaseSize,
                                     idVec2i atlasSizeInTiles,
                                     unsigned int workMemorySize) {
  Destroy();
  m_TileBaseSize = tileBaseSize;
  m_iCurrentFrame = 0;
  m_AtlasSizeInTiles = atlasSizeInTiles;
  m_BaseAtlasTileOfs.y = int(4 * tileBaseSize + 1);
  m_BaseAtlasTileOfs.x = (m_BaseAtlasTileOfs.y + 3) & ~3;
  m_AtlasSize.y = atlasSizeInTiles.y * m_BaseAtlasTileOfs.y;
  m_AtlasSize.x =
      (atlasSizeInTiles.x * m_BaseAtlasTileOfs.x + 15) & ~15;
  for (int frame = 0; frame < NUM_FRAMES; ++frame) {
    if (!m_lFrameMem[frame].Create(vertexCount, m_AtlasSize, frame)) {
      Destroy();
      return false;
    }
  }
  m_nAtlasBaseTiles =
      unsigned(atlasSizeInTiles.x * atlasSizeInTiles.y);
  m_pWorkMem = AllocWater(workMemorySize, false);
  if (m_pWorkMem == nullptr) {
    idLib::FatalError("Out of memory.");
    return false;
  }
  m_WorkMemSize = workMemorySize;

  m_lLODindices.SetNum(NUM_LODS);
  unsigned int totalIndices = 0;
  for (unsigned int lod = 0; lod < NUM_LODS; ++lod) {
    const unsigned int tessellation = tileBaseSize >> lod;
    const unsigned int side = tessellation + 1;
    CLODIndices& info = m_lLODindices[lod];
    info.m_nIndices = tessellation * (2 * side + 1);
    info.m_nVertices = side * side;
    info.m_Ofs = 2 * totalIndices;
    totalIndices += info.m_nIndices;
  }

  auto* indices = static_cast<std::uint16_t*>(
      AllocWater(2 * totalIndices, false));
  unsigned int output = 0;
  for (unsigned int lod = 0; lod < NUM_LODS; ++lod) {
    const unsigned int tessellation = tileBaseSize >> lod;
    const unsigned int side = tessellation + 1;
    for (unsigned int y = 0; y < tessellation; ++y) {
      for (unsigned int x = 0; x < side; ++x) {
        indices[output++] = static_cast<std::uint16_t>(y * side + x);
        indices[output++] =
            static_cast<std::uint16_t>((y + 1) * side + x);
      }
      indices[output++] = 0xffffu;
    }
  }
  m_IB.AllocBufferObject(indices, static_cast<int>(2 * totalIndices),
                         BU_DEFAULT);
  void* allocation = indices;
  FreeWater(allocation);
  m_bInit = true;
  return true;
}

idRenderModelOcean::COceanFrameState::COceanFrameState() = default;

idRenderModelOcean::COceanFrameState::~COceanFrameState() { Destroy(); }

void idRenderModelOcean::COceanFrameState::Destroy() {
  void* pointer = m_pTileLOD;
  FreeWater(pointer);
  m_pTileLOD = nullptr;
  pointer = m_pTileVBOffset;
  FreeWater(pointer);
  m_pTileVBOffset = nullptr;
  pointer = m_pTileAtlasTexelOfs;
  FreeWater(pointer);
  m_pTileAtlasTexelOfs = nullptr;
  pointer = m_ppTileWorkMem;
  FreeWater(pointer);
  m_ppTileWorkMem = nullptr;
  m_bValid = false;
  m_nTilesUsed = 0;
}

void idRenderModelOcean::COceanFrameState::Create(unsigned int tileCount) {
  Destroy();
  m_pTileLOD =
      static_cast<std::uint8_t*>(AllocWater(tileCount, true));
  m_pTileVBOffset = static_cast<unsigned int*>(
      AllocWater(sizeof(unsigned int) * tileCount, true));
  m_pTileAtlasTexelOfs = static_cast<idVec2i*>(
      AllocWater(sizeof(idVec2i) * tileCount, true));
  m_ppTileWorkMem =
      static_cast<void**>(AllocWater(sizeof(void*) * tileCount, true));
  m_bValid = false;
  m_nTilesUsed = 0;
}

idRenderModelOcean::idRenderModelOcean(const char* modelName)
    : idRenderModel(), m_Params(), m_lpMacroTileOccTest(), m_lFrames{},
      m_lQTNodes(), m_lQTRopes(), m_WaveParams{} {
  renderModelType = RENDERMODEL_OCEAN;
  g.flags4 |= 0x40u;
  m_pTileSharedMem = nullptr;
  m_MacroTileDim = 0.0f;
  m_nMacroTiles = {};
  m_nTilesMax = 0;
  m_nTilesPrevUsed = 0;
  m_TileTess = 0;
  m_lpTri[0] = nullptr;
  m_pMaterial = nullptr;
  m_pEnvAir = nullptr;
  m_pEnvWater = nullptr;
  m_iCurrentFrame = 0;
  m_iCurrentTriFrame = 0;
  m_pJobParams = nullptr;
  m_SpamTimer = 0.0f;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x01u;
  SetName(modelName);
  g.modelDepthSort = MODEL_DEPTH_SORT_AFTER_WORLD;
}

idRenderModelOcean::~idRenderModelOcean() {
  void* jobs = m_pJobParams;
  FreeWater(jobs);
  m_pJobParams = nullptr;
  if (m_lpTri[0] != nullptr) {
    auto* header = reinterpret_cast<std::uint32_t*>(m_lpTri[0]) - 2;
    const unsigned int count = header[0];
    for (unsigned int index = count; index > 0; --index)
      m_lpTri[0][index - 1].~idTriangles();
    void* allocation = header;
    FreeWater(allocation);
    m_lpTri[0] = nullptr;
  }
}

bool idRenderModelOcean::CheckValid() {
  if (idLib::production != PROD_PRODUCTION)
    return m_nTilesMax != 0 &&
           static_cast<unsigned int>(surfaces.num) == m_nTilesMax;
  return true;
}

void idRenderWorldLocal::Ocean_Swap() {
  if (m_pSharedTileMemory != nullptr)
    m_pSharedTileMemory->Swap();
}

void idRenderWorldLocal::Ocean_Destroy() {
  if (m_pSharedTileMemory == nullptr)
    return;
  m_pSharedTileMemory->~idOceanSharedTileMemory();
  void* memory = m_pSharedTileMemory;
  FreeWater(memory);
  m_pSharedTileMemory = nullptr;
}

void idRenderWorldLocal::Ocean_Init() {
  if (m_pSharedTileMemory != nullptr)
    return;
  void* memory = AllocWater(sizeof(idOceanSharedTileMemory), false);
  m_pSharedTileMemory =
      memory != nullptr ? ::new (memory) idOceanSharedTileMemory : nullptr;
  if (m_pSharedTileMemory != nullptr) {
    m_pSharedTileMemory->Create(
        0x10000u, 0x10u,
        {oceanAtlasSize_x.GetInteger(), oceanAtlasSize_y.GetInteger()},
        0x300000u);
  }
}

void idRenderModelOcean::CreateWaveParams(const idOceanParams_t* params) {
  std::memcpy(&m_Params, params, sizeof(m_Params));
  std::memset(&m_WaveParams, 0, sizeof(m_WaveParams));
  m_WaveParams.m_DebugFlags =
      static_cast<std::uint32_t>(r_ocean_debugflags.GetInteger());

  unsigned int waveCount =
      static_cast<unsigned int>(std::max(1, r_ocean_maxwaves.GetInteger()));
  m_WaveParams.m_nWavesMax_v = waveCount;
  waveCount = std::min(waveCount, 64u);
  m_WaveParams.m_MaxMacroTileSubdivide = static_cast<unsigned int>(
      std::clamp(params->m_nMacroTileSubdivide, 0, 10));
  m_WaveParams.m_WaveLodFadeRangeRcp = 1.0f / params->m_LODFadeRange;

  float lodDistance[16];
  float lodMinimumMagnitude[16];
  float lodMinimumTexelMagnitude[16];
  const float baseVertexStep =
      m_MacroTileDim /
      float(m_TileTess * (1u << m_WaveParams.m_MaxMacroTileSubdivide));
  int scale = 1;
  for (unsigned int lod = 0; lod < 16; ++lod) {
    const float distance = float(int(lod + 1)) / params->m_LODK;
    lodDistance[lod] = distance * distance;
    lodMinimumMagnitude[lod] =
        float(scale) * params->m_MinVertPerWaveLen * baseVertexStep;
    lodMinimumTexelMagnitude[lod] =
        float(scale) * params->m_MinTexelPerWaveLen * baseVertexStep * 0.25f;
    if (r_ocean_printwaves.GetInteger() != 0) {
      idLib::Printf("%d : Dist %f, minmag %f\n", lod, lodDistance[lod],
                    lodMinimumMagnitude[lod]);
    }
    scale <<= 1;
  }

  std::srand(0);
  std::uint32_t randomState = 0;
  const float angleSpread =
      (idMath::TWO_PI / 360.0f) * params->m_AngleSpread;
  const float angleBase =
      (idMath::TWO_PI / 360.0f) * params->m_AngleBase;
  const float amplitudeRange = params->m_AmpMax - params->m_AmpMin;
  for (unsigned int index = 0; index < waveCount; ++index) {
    randomState = 1103515245u * randomState + 12345u;
    const float random = float((randomState >> 16) & 0x7fffu) *
                             0.000030518509f * 2.0f -
                         1.0f;
    const float angle = random * angleSpread * 0.5f - angleBase;
    auto& wave = m_WaveParams.m_lWaves[index];
    wave.m_WaveVec = {std::cos(angle), std::sin(angle)};
    const float distribution =
        std::pow(float(int(waveCount - index - 1)) /
                     float(int(waveCount - 1)),
                 params->m_AmpDistributionExp);
    wave.m_Amp = distribution * amplitudeRange + params->m_AmpMin;
    wave.m_WaveVecMag = params->m_AmpK / wave.m_Amp;
    wave.m_Freq =
        std::sqrt(std::max(wave.m_WaveVecMag * params->m_GravityK,
                           idMath::FLT_SMALLEST_NON_DENORMAL));
    wave.m_LodFadeNearOfs_v = -10.0f;

    const float wavelength = 1.0f / wave.m_WaveVecMag;
    unsigned int lod = 0;
    while (lod < 15 && lodMinimumMagnitude[lod + 1] <= wavelength)
      ++lod;
    wave.m_LodFadeFarOfs_v =
        m_WaveParams.m_WaveLodFadeRangeRcp * lodDistance[lod];
    wave.m_LodFadeNearOfs_n = wave.m_LodFadeFarOfs_v - 1.0f;
    while (lod < 15 && lodMinimumTexelMagnitude[lod + 1] <= wavelength)
      ++lod;
    wave.m_LodFadeFarOfs_n =
        m_WaveParams.m_WaveLodFadeRangeRcp * lodDistance[lod];

    if (r_ocean_printwaves.GetInteger() != 0) {
      idLib::Printf(
          "Wave %d, Amp %.2f, Vec (%.2f, %.2f), Mag %.2f, Freq %f, "
          "VLodFar %.2f, TLodNEar %.2f, TLodFar %.2f\n",
          index, wave.m_Amp, wave.m_WaveVec.x, wave.m_WaveVec.y,
          wavelength, wave.m_Freq, wave.m_LodFadeFarOfs_v,
          wave.m_LodFadeNearOfs_n, wave.m_LodFadeFarOfs_n);
    }
  }
  m_WaveParams.m_WavesSOA.Create(m_WaveParams.m_lWaves,
                                  static_cast<int>(waveCount));
  if (r_ocean_printwaves.GetInteger() != 0)
    r_ocean_printwaves.SetInteger(0, true);
}

bool idRenderModelOcean::GetSurfacePosition(
    const idVec3& position, float quality, float normalQuality, int time,
    idVec3& surfacePosition, idVec3& normal, float& waveHeight) {
  if (m_pTileSharedMem == nullptr)
    return false;

  const idVec3 localPosition = WorldToLocal(*this, position);
  unsigned int waveCount = m_WaveParams.m_nWavesMax_v;
  const int requestedWaves = static_cast<int>(float(waveCount) * quality);
  if (requestedWaves < static_cast<int>(waveCount))
    waveCount = static_cast<unsigned int>(std::max(0, requestedWaves));
  auto waves = MakeWaveView(m_WaveParams.m_WavesSOA, waveCount);

  unsigned int iterations =
      std::min(4u, static_cast<unsigned int>(int(quality * 4.0f)));
  iterations = std::max(iterations, 1u);
  unsigned int normalIteration =
      static_cast<unsigned int>(int(normalQuality * 4.0f));
  normalIteration = std::max(1u, std::min(normalIteration, iterations));

  idVec3 estimate = localPosition;
  const float waveTime = float(time & 0x3fffff) * 0.001f;
  float sampleRadius = r_ocean_ampmax.GetFloat();
  idVec4 cameraPosition{position.x, position.y, position.z, 1.0f};
  idVec4 samples[4];
  for (unsigned int iteration = 0; iteration < iterations; ++iteration) {
    const idVec3 sampleOrigin{estimate.x - sampleRadius,
                              estimate.y - sampleRadius, 0.0f};
    ApplyWavesSOA<0>(sampleOrigin, waveTime, sampleRadius * 2.0f, 2,
                     waves, m_WaveParams.m_nWavesMax_v, samples, 0,
                     cameraPosition, 0.0f);

    const float xTop =
        (localPosition.x - samples[0].x) /
        (samples[1].x - samples[0].x);
    const float xBottom =
        (localPosition.x - samples[2].x) /
        (samples[3].x - samples[2].x);
    const float yLeft =
        (localPosition.y - samples[0].y) /
        (samples[2].y - samples[0].y);
    const float yRight =
        (localPosition.y - samples[1].y) /
        (samples[3].y - samples[1].y);
    estimate.x = sampleOrigin.x + (xTop + xBottom) * 0.5f *
                                           (sampleRadius * 2.0f);
    estimate.y = sampleOrigin.y + (yLeft + yRight) * 0.5f *
                                           (sampleRadius * 2.0f);
    estimate.z = 0.0f;

    if (iteration + 1 == normalIteration) {
      const idVec3 edge10{samples[1].x - samples[0].x,
                          samples[1].y - samples[0].y,
                          samples[1].z - samples[0].z};
      const idVec3 edge20{samples[2].x - samples[0].x,
                          samples[2].y - samples[0].y,
                          samples[2].z - samples[0].z};
      const idVec3 edge13{samples[1].x - samples[3].x,
                          samples[1].y - samples[3].y,
                          samples[1].z - samples[3].z};
      const idVec3 edge23{samples[2].x - samples[3].x,
                          samples[2].y - samples[3].y,
                          samples[2].z - samples[3].z};
      idVec3 combined{
          edge10.y * edge20.z - edge10.z * edge20.y +
              edge23.y * edge13.z - edge23.z * edge13.y,
          edge10.z * edge20.x - edge10.x * edge20.z +
              edge23.z * edge13.x - edge23.x * edge13.z,
          edge20.y * edge10.x - edge10.y * edge20.x +
              edge13.y * edge23.x - edge23.y * edge13.x};
      const float reciprocalLength =
          1.0f /
          std::sqrt(std::max(combined.x * combined.x +
                                 combined.y * combined.y +
                                 combined.z * combined.z,
                             idMath::FLT_SMALLEST_NON_DENORMAL));
      normal = {combined.x * reciprocalLength,
                combined.y * reciprocalLength,
                combined.z * reciprocalLength};
    }
    sampleRadius *= 0.5f;
  }

  ApplyWavesSOA<0>(estimate, waveTime, sampleRadius, 1, waves,
                   m_WaveParams.m_nWavesMax_v, samples, 0, cameraPosition,
                   0.0f);
  const idVec3 localSurface{samples[0].x, samples[0].y, samples[0].z};
  surfacePosition = LocalToWorld(*this, localSurface);
  waveHeight = samples[0].z;
  return true;
}

void SyncTileVertices_QT(CQTNode*, unsigned int, CQTRope* ropes,
                         unsigned int ropeCount,
                         idRenderModelOcean_TileJobParams* jobParams,
                         unsigned int tileCount, std::uint8_t syncDisable) {
  std::vector<std::uint8_t> visited(ropeCount, 0);

  auto processVerticalEdge = [&](CQTRope* rope) {
    while (rope != nullptr) {
      const std::ptrdiff_t ropeIndex = rope - ropes;
      if (ropeIndex < 0 ||
          static_cast<unsigned int>(ropeIndex) >= ropeCount ||
          visited[static_cast<std::size_t>(ropeIndex)] != 0)
        return;
      visited[static_cast<std::size_t>(ropeIndex)] = 1;
      const std::uint32_t tile0 = rope->m_lpNodes[0]->m_iTile;
      const std::uint32_t tile1 = rope->m_lpNodes[1]->m_iTile;
      if (tile0 == ~0u || tile1 == ~0u)
        return;
      auto& first = jobParams[tile0];
      auto& second = jobParams[tile1];
      const unsigned int scale0 = first.m_TileSTScale;
      const unsigned int scale1 = second.m_TileSTScale;
      const unsigned int offset0 = rope->m_lEdgeOfs[0] / scale0;
      const unsigned int offset1 = rope->m_lEdgeOfs[1] / scale1;
      if (offset0 >= first.m_TileTess || offset1 >= second.m_TileTess)
        return;

      COceanVertex* edge0 =
          &first.m_pVB[first.m_TileTess +
                       offset0 * (first.m_TileTess + 1)];
      COceanVertex* edge1 =
          &second.m_pVB[offset1 * (second.m_TileTess + 1)];
      if (scale0 == scale1) {
        SyncVert_1x_1x(edge0, first.m_TileTess + 1, edge1,
                       second.m_TileTess + 1, rope->m_Len / scale0 + 1);
      } else if (2 * scale0 == scale1) {
        SyncVert_2x_1x(edge0, first.m_TileTess + 1, edge1,
                       second.m_TileTess + 1, rope->m_Len / scale0 + 1);
      } else if (scale0 == 2 * scale1) {
        SyncVert_2x_1x(edge1, second.m_TileTess + 1, edge0,
                       first.m_TileTess + 1, rope->m_Len / scale1 + 1);
      }

      auto* texels0 = static_cast<std::uint32_t*>(first.m_pNormalMap);
      auto* texels1 = static_cast<std::uint32_t*>(second.m_pNormalMap);
      const unsigned int pitch0 = first.m_NormalMapPitch >> 2;
      const unsigned int pitch1 = second.m_NormalMapPitch >> 2;
      std::uint32_t* texelEdge0 =
          texels0 + 4 * first.m_TileTess + 4 * offset0 * pitch0;
      std::uint32_t* texelEdge1 = texels1 + 4 * offset1 * pitch1;
      if (scale0 == scale1 && (syncDisable & 0x01u) == 0) {
        SyncTexel_1x_1x(texelEdge0, int(pitch0), texelEdge1, int(pitch1),
                        4 * (rope->m_Len / scale0) + 1);
      } else if (2 * scale0 == scale1 && (syncDisable & 0x02u) == 0) {
        SyncTexel_2x_1x(texelEdge0, pitch0, texelEdge1, pitch1,
                        4 * (rope->m_Len / scale0) + 1);
      } else if (scale0 == 2 * scale1 && (syncDisable & 0x04u) == 0) {
        SyncTexel_2x_1x(texelEdge1, pitch1, texelEdge0, pitch0,
                        4 * (rope->m_Len / scale1) + 1);
      }
      rope = rope->m_pNext;
    }
  };

  for (unsigned int tile = 0; tile < tileCount; ++tile) {
    if (jobParams[tile].m_pQTNode != nullptr)
      processVerticalEdge(jobParams[tile].m_pQTNode->m_lpRopes[0]);
  }

  std::fill(visited.begin(), visited.end(), 0);
  auto processHorizontalEdge = [&](CQTRope* rope) {
    while (rope != nullptr) {
      const std::ptrdiff_t ropeIndex = rope - ropes;
      if (ropeIndex < 0 ||
          static_cast<unsigned int>(ropeIndex) >= ropeCount ||
          visited[static_cast<std::size_t>(ropeIndex)] != 0)
        return;
      visited[static_cast<std::size_t>(ropeIndex)] = 1;
      const std::uint32_t tile0 = rope->m_lpNodes[0]->m_iTile;
      const std::uint32_t tile1 = rope->m_lpNodes[1]->m_iTile;
      if (tile0 == ~0u || tile1 == ~0u)
        return;
      auto& first = jobParams[tile0];
      auto& second = jobParams[tile1];
      const unsigned int scale0 = first.m_TileSTScale;
      const unsigned int scale1 = second.m_TileSTScale;
      const unsigned int offset0 = rope->m_lEdgeOfs[0] / scale0;
      const unsigned int offset1 = rope->m_lEdgeOfs[1] / scale1;
      if (offset0 >= first.m_TileTess || offset1 >= second.m_TileTess)
        return;
      COceanVertex* edge0 =
          &first.m_pVB[offset0 + first.m_TileTess *
                                     (first.m_TileTess + 1)];
      COceanVertex* edge1 = &second.m_pVB[offset1];
      if (scale0 == scale1) {
        SyncVert_1x_1x(edge0, edge1, rope->m_Len / scale0 + 1);
      } else if (2 * scale0 == scale1) {
        SyncVert_2x_1x(edge0, edge1, rope->m_Len / scale0 + 1);
      } else if (scale0 == 2 * scale1) {
        SyncVert_2x_1x(edge1, edge0, rope->m_Len / scale1 + 1);
      }

      auto* texels0 = static_cast<std::uint32_t*>(first.m_pNormalMap);
      auto* texels1 = static_cast<std::uint32_t*>(second.m_pNormalMap);
      const unsigned int pitch0 = first.m_NormalMapPitch >> 2;
      std::uint32_t* texelEdge0 =
          texels0 + 4 * offset0 + 4 * first.m_TileTess * pitch0;
      std::uint32_t* texelEdge1 = texels1 + 4 * offset1;
      if (scale0 == scale1 && (syncDisable & 0x08u) == 0) {
        SyncTexel_1x_1x(texelEdge0, texelEdge1,
                        4 * (rope->m_Len / scale0) + 1);
      } else if (2 * scale0 == scale1 && (syncDisable & 0x10u) == 0) {
        SyncTexel_2x_1x(texelEdge0, 1, texelEdge1, 1,
                        4 * (rope->m_Len / scale0) + 1);
      } else if (scale0 == 2 * scale1 && (syncDisable & 0x20u) == 0) {
        SyncTexel_2x_1x(texelEdge1, 1, texelEdge0, 1,
                        4 * (rope->m_Len / scale1) + 1);
      }
      rope = rope->m_pNext;
    }
  };

  for (unsigned int tile = 0; tile < tileCount; ++tile) {
    if (jobParams[tile].m_pQTNode != nullptr)
      processHorizontalEdge(jobParams[tile].m_pQTNode->m_lpRopes[2]);
  }
}

unsigned int MacroTileSubdivide_r(CSubdivideParams* params, CQTNode* node,
                                  unsigned int level,
                                  unsigned int maximumTiles,
                                  unsigned int tileBase,
                                  const idVec3& tileMinimum,
                                  float tileDimension, bool visible) {
  if (maximumTiles == 0)
    return 0;

  idBounds bounds;
  bounds.b[0] = {tileMinimum.x - params->m_Cullmargin,
                 tileMinimum.y - params->m_Cullmargin,
                 tileMinimum.z - params->m_Cullmargin};
  bounds.b[1] = {tileMinimum.x + tileDimension + params->m_Cullmargin,
                 tileMinimum.y + tileDimension + params->m_Cullmargin,
                 tileMinimum.z + params->m_Cullmargin};
  auto& job = params->m_pTileJobs[tileBase];

  unsigned char cullBits = 0;
  if (!visible ||
      idRenderMatrix::CullBoundsToMVPbits(params->m_MVP, bounds, cullBits,
                                          false)) {
    node->m_iTile = tileBase;
    params->m_pTileLODs[tileBase] = 1;
    job.m_MinDist = 20000.0f;
    job.m_MaxDist = 30000.0f;
    job.m_TileBounds = bounds;
    job.m_TileDim = tileDimension;
    job.m_TileOfs = {tileMinimum.x, tileMinimum.y};
    job.m_TileST1Ofs = {node->m_TileOfsX, node->m_TileOfsY};
    job.m_pQTNode = node;
    job.m_TileSTDim = node->m_TileTess;
    return 1;
  }

  const float nearestX =
      std::clamp(params->m_CamLocal.x, tileMinimum.x,
                 tileMinimum.x + tileDimension);
  const float nearestY =
      std::clamp(params->m_CamLocal.y, tileMinimum.y,
                 tileMinimum.y + tileDimension);
  const float nearestZ = tileMinimum.z;
  const float dx = params->m_CamLocal.x - nearestX;
  const float dy = params->m_CamLocal.y - nearestY;
  const float dz = params->m_CamLocal.z - nearestZ;
  const float minimumDistance =
      std::sqrt(std::max(dx * dx + dy * dy + dz * dz,
                         idMath::FLT_SMALLEST_NON_DENORMAL));
  const float farX = params->m_CamLocal.x <= tileMinimum.x + tileDimension * 0.5f
                         ? tileMinimum.x + tileDimension
                         : tileMinimum.x;
  const float farY = params->m_CamLocal.y <= tileMinimum.y + tileDimension * 0.5f
                         ? tileMinimum.y + tileDimension
                         : tileMinimum.y;
  const float farZ = tileMinimum.z;
  const float fdx = params->m_CamLocal.x - farX;
  const float fdy = params->m_CamLocal.y - farY;
  const float fdz = params->m_CamLocal.z - farZ;
  const float maximumDistance =
      std::sqrt(std::max(fdx * fdx + fdy * fdy + fdz * fdz,
                         idMath::FLT_SMALLEST_NON_DENORMAL));
  const unsigned int desiredLevel = static_cast<unsigned int>(std::max(
      0.0f, std::sqrt(minimumDistance) * params->m_kLOD + params->m_LODofs));

  if (level == 0 || desiredLevel >= level) {
    node->m_iTile = tileBase;
    params->m_pTileLODs[tileBase] = static_cast<std::uint8_t>(
        std::min(params->m_MaxLOD, desiredLevel - level));
    job.m_MinDist = minimumDistance;
    job.m_MaxDist = maximumDistance;
    job.m_TileBounds = bounds;
    job.m_TileDim = tileDimension;
    job.m_TileOfs = {tileMinimum.x, tileMinimum.y};
    job.m_TileST1Ofs = {node->m_TileOfsX, node->m_TileOfsY};
    job.m_pQTNode = node;
    job.m_TileSTDim = node->m_TileTess;
    return 1;
  }

  if (params->m_nQTNodes + 4 >= params->m_nQTNodesMax)
    return 0;
  CQTNode* children = &params->m_pQTNodes[params->m_nQTNodes];
  node->m_iCh = static_cast<std::uint16_t>(params->m_nQTNodes);
  params->m_nQTNodes += 4;
  const std::uint16_t childTess =
      static_cast<std::uint16_t>(node->m_TileTess >> 1);
  for (unsigned int child = 0; child < 4; ++child) {
    children[child] = {};
    children[child].m_iTile = ~0u;
    children[child].m_TileTess = childTess;
    children[child].m_TileOfsX = static_cast<std::uint16_t>(
        node->m_TileOfsX + ((child & 1u) != 0 ? childTess : 0));
    children[child].m_TileOfsY = static_cast<std::uint16_t>(
        node->m_TileOfsY + ((child & 2u) != 0 ? childTess : 0));
  }

  CQTRope* rightNeighbour =
      node->m_lpRopes[3] != nullptr
          ? FindNeighbourRope(node->m_lpRopes[3], 0, 1, childTess)
          : nullptr;
  CQTRope* leftNeighbour =
      node->m_lpRopes[2] != nullptr
          ? FindNeighbourRope(node->m_lpRopes[2], 1, 1, childTess)
          : nullptr;
  CQTRope* lower = params->AssignNewRope(
      &children[2], &children[3], 0, 1, childTess, leftNeighbour);
  CQTRope* upper = params->AssignNewRope(&children[0], &children[1], 0, 1,
                                         childTess, lower);
  if (rightNeighbour != nullptr) {
    if (rightNeighbour->m_pNext != nullptr)
      idLib::FatalError("Unexpected link");
    rightNeighbour->m_pNext = upper;
  }

  CQTRope* bottomNeighbour =
      node->m_lpRopes[1] != nullptr
          ? FindNeighbourRope(node->m_lpRopes[1], 0, 3, childTess)
          : nullptr;
  CQTRope* topNeighbour =
      node->m_lpRopes[0] != nullptr
          ? FindNeighbourRope(node->m_lpRopes[0], 1, 3, childTess)
          : nullptr;
  CQTRope* right = params->AssignNewRope(
      &children[1], &children[3], 2, 3, childTess, topNeighbour);
  CQTRope* left = params->AssignNewRope(&children[0], &children[2], 2, 3,
                                        childTess, right);
  if (bottomNeighbour != nullptr) {
    if (bottomNeighbour->m_pNext != nullptr)
      idLib::FatalError("Unexpected link");
    bottomNeighbour->m_pNext = left;
  }

  params->SplitRope(node, 0, 0, &children[1], &children[3], childTess);
  params->SplitRope(node, 1, 1, &children[0], &children[2], childTess);
  params->SplitRope(node, 2, 0, &children[2], &children[3], childTess);
  params->SplitRope(node, 3, 1, &children[0], &children[1], childTess);

  const float half = tileDimension * 0.5f;
  unsigned int used = MacroTileSubdivide_r(
      params, &children[0], level - 1, maximumTiles, tileBase, tileMinimum,
      half, true);
  idVec3 childMinimum{tileMinimum.x + half, tileMinimum.y, tileMinimum.z};
  used += MacroTileSubdivide_r(params, &children[1], level - 1,
                               maximumTiles - used, tileBase + used,
                               childMinimum, half, true);
  childMinimum = {tileMinimum.x, tileMinimum.y + half, tileMinimum.z};
  used += MacroTileSubdivide_r(params, &children[2], level - 1,
                               maximumTiles - used, tileBase + used,
                               childMinimum, half, true);
  childMinimum = {tileMinimum.x + half, tileMinimum.y + half,
                  tileMinimum.z};
  used += MacroTileSubdivide_r(params, &children[3], level - 1,
                               maximumTiles - used, tileBase + used,
                               childMinimum, half, true);
  return used;
}

void OceanSyncJob(idRenderModelOcean* ocean) {
  SyncTileVertices_QT(
      ocean->m_lQTNodes.list,
      static_cast<unsigned int>(ocean->m_lQTNodes.num),
      ocean->m_lQTRopes.list,
      static_cast<unsigned int>(ocean->m_lQTRopes.num), ocean->m_pJobParams,
      ocean->m_lFrames[ocean->m_iCurrentFrame].m_nTilesUsed,
      static_cast<std::uint8_t>(r_ocean_syncdisable.GetInteger()));
}

void idRenderModelOcean::Create(const idOceanParams_t* params,
                                const idMaterial* material,
                                const idDeclEnv* environmentAir,
                                const idDeclEnv* environmentWater) {
  if (m_pJobParams != nullptr) {
    void* jobs = m_pJobParams;
    FreeWater(jobs);
    m_pJobParams = nullptr;
  }
  m_pEnvAir = environmentAir;
  m_pEnvWater = environmentWater;

  idRenderWorld* renderWorld = common != nullptr ? common->RW() : nullptr;
  m_pTileSharedMem =
      renderWorld != nullptr ? renderWorld->GetOceanSharedTileMemory()
                             : nullptr;
  if (m_pTileSharedMem == nullptr) {
    idLib::Printf(
        "ERROR: No shared tile memory created. Ocean model not created.\n");
    return;
  }

  m_pMaterial = material;
  m_WaveParams.m_TilesOrigin = {};
  m_WaveParams.m_Time = 0.0f;
  m_MacroTileDim = static_cast<float>(params->m_MacroTileDim);
  m_nMacroTiles.x = std::max(1, params->m_nMacroTiles.x);
  m_nMacroTiles.y = std::max(1, params->m_nMacroTiles.y);

  const int macroTileCount = m_nMacroTiles.x * m_nMacroTiles.y;
  m_lpMacroTileOccTest.SetNum(macroTileCount);
  const float boundsMargin = m_Params.m_AmpMax * 6.5f;
  for (int y = 0; y < m_nMacroTiles.y; ++y) {
    for (int x = 0; x < m_nMacroTiles.x; ++x) {
      idRenderModelSurface surface{};
      surface.material = material;
      surface.geometry = new (TAG_TRIANGLES) idTriangles();
      surface.geometry->bounds.b[0] =
          {m_WaveParams.m_TilesOrigin.x + x * m_MacroTileDim - boundsMargin,
           m_WaveParams.m_TilesOrigin.y + y * m_MacroTileDim - boundsMargin,
           m_WaveParams.m_TilesOrigin.z - boundsMargin};
      surface.geometry->bounds.b[1] =
          {m_WaveParams.m_TilesOrigin.x + (x + 1) * m_MacroTileDim +
               boundsMargin,
           m_WaveParams.m_TilesOrigin.y + (y + 1) * m_MacroTileDim +
               boundsMargin,
           m_WaveParams.m_TilesOrigin.z + boundsMargin};

      idRenderModel* occlusionModel =
          renderWorld != nullptr ? renderWorld->AllocBaseRenderModel()
                                 : nullptr;
      if (occlusionModel != nullptr) {
        occlusionModel->AddSurface(surface);
        occlusionModel->FinishSurfaces();
        const idRenderModelParms& active = ActiveParms(*this);
        occlusionModel->g.origin = active.origin;
        occlusionModel->g.axis = active.axis;
        occlusionModel->deferredOrigin = active.origin;
        occlusionModel->deferredAxis = active.axis;
        occlusionModel->referenceBounds = surface.geometry->bounds;
        occlusionModel->g.flags2 |= 0x60u;
        occlusionModel->g.flags0 |= 0x80u;
        occlusionModel->g.flags1 |= 0x01u;
        occlusionModel->CommitThisFrame();
      }
      m_lpMacroTileOccTest[x + y * m_nMacroTiles.x] = occlusionModel;
    }
  }

  g.flags2 |= 0x18u;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x21u;
  m_nTilesMax = static_cast<unsigned int>(4 * macroTileCount + 160);
  m_nTilesPrevUsed = 0;
  m_lQTNodes.SetNum(static_cast<int>(m_nTilesMax + m_nTilesMax / 2));
  m_lQTRopes.SetNum(2 * m_lQTNodes.num);
  m_TileTess = m_pTileSharedMem->m_TileBaseSize;
  for (COceanFrameState& frame : m_lFrames)
    frame.Create(m_nTilesMax);

  CreateWaveParams(params);
  m_WaveParams.m_nLODs = 4;
  m_WaveParams.m_NormalMapSizeInv =
      {1.0f / float(m_pTileSharedMem->m_AtlasSize.x),
       1.0f / float(m_pTileSharedMem->m_AtlasSize.y)};
  m_pJobParams = static_cast<idRenderModelOcean_TileJobParams*>(
      AllocWater(sizeof(idRenderModelOcean_TileJobParams) * m_nTilesMax,
                 true));

  const std::size_t triangleBytes =
      sizeof(std::uint32_t) * 2 + sizeof(idTriangles) * m_nTilesMax;
  auto* triangleHeader = static_cast<std::uint32_t*>(
      AllocWater(triangleBytes, false));
  triangleHeader[0] = m_nTilesMax;
  triangleHeader[1] = 0;
  m_lpTri[0] = reinterpret_cast<idTriangles*>(triangleHeader + 2);
  for (unsigned int index = 0; index < m_nTilesMax; ++index)
    ::new (static_cast<void*>(&m_lpTri[0][index])) idTriangles();

  for (unsigned int index = 0; index < m_nTilesMax; ++index) {
    idTriangles& triangles = m_lpTri[0][index];
    triangles.bounds.b[0] = {-100000.0f, -100000.0f, -100000.0f};
    triangles.bounds.b[1] = {100000.0f, 100000.0f, 100000.0f};
    triangles.numIndexes = 0;
    triangles.numVerts = 0;
    triangles.vertexBuffer.Reference(
        m_pTileSharedMem
            ->m_lFrameMem[m_pTileSharedMem->m_iCurrentFrame]
            .m_VB,
        0, 1);
    triangles.indexBuffer.Reference(m_pTileSharedMem->m_IB);

    idRenderModelSurface surface{};
    surface.geometry = &triangles;
    surface.geometryIsReference = true;
    surface.referenceMask = 1;
    AddSurface(surface);
  }
  FinishSurfaces();
  CommitThisFrame();
}

bool idRenderModelOcean::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipWater.GetInteger() != 0 || surfaces.num == 0 ||
      m_pTileSharedMem == nullptr) {
    for (unsigned int index = 0; index < m_nTilesMax; ++index) {
      if (surfaces.list[index].geometry != nullptr) {
        surfaces.list[index].geometry->numIndexes = 0;
        surfaces.list[index].geometry->numVerts = 0;
      }
    }
    return false;
  }

  const std::int64_t updateStart = Sys_GetClockTicks();
  const int retiredModelFrame = m_iCurrentFrame;
  COceanFrameState& retiredFrame = m_lFrames[retiredModelFrame];
  idOceanSharedTileMemory::CFrameState& retiredSharedFrame =
      m_pTileSharedMem
          ->m_lFrameMem[(m_pTileSharedMem->m_iCurrentFrame + 2) % 3];

  std::int64_t syncTicks = 0;
  if (retiredFrame.m_bValid && (m_WaveParams.m_DebugFlags & 0x100u) == 0) {
    if (retiredSharedFrame.m_VB.IsMapped()) {
      const std::int64_t start = Sys_GetClockTicks();
      SyncTileVertices_QT(
          m_lQTNodes.list, static_cast<unsigned int>(m_lQTNodes.num),
          m_lQTRopes.list, static_cast<unsigned int>(m_lQTRopes.num),
          m_pJobParams, retiredFrame.m_nTilesUsed,
          static_cast<std::uint8_t>(r_ocean_syncdisable.GetInteger()));
      syncTicks = Sys_GetClockTicks() - start;
    } else {
      idLib::Warning(
          "(idRenderModelOcean) VB not mapped. Should never happen.\n");
    }
  }

  std::int64_t uploadTicks = 0;
  idImage* retiredNormalMap = retiredSharedFrame.m_pNormals;
  if (retiredFrame.m_bValid && retiredNormalMap != nullptr) {
    const std::int64_t start = Sys_GetClockTicks();
    const unsigned int width =
        static_cast<unsigned int>(retiredNormalMap->opts.width);
    const unsigned int height =
        static_cast<unsigned int>(retiredNormalMap->opts.height);
    for (unsigned int tile = 0; tile < retiredFrame.m_nTilesUsed; ++tile) {
      const idVec2i position = retiredFrame.m_pTileAtlasTexelOfs[tile];
      const unsigned int tessellation =
          m_TileTess >> retiredFrame.m_pTileLOD[tile];
      const unsigned int side = 4 * tessellation + 1;
      if (unsigned(position.x) + side > width ||
          unsigned(position.y) + side > height) {
        idLib::Debugf("Messed up tile location: %d, %d, Size %d",
                      position.x, position.y, side);
      } else {
        retiredNormalMap->SubImageUpload(
            0, position.x, position.y, 0, static_cast<int>(side),
            static_cast<int>(side), m_pJobParams[tile].m_pNormalMap,
            static_cast<int>(side));
      }
    }
    uploadTicks = Sys_GetClockTicks() - start;
  }
  if (renderThreadParmState != nullptr && rp_OceanNormalMap.Ptr() != nullptr)
    renderThreadParmState->SetParmSampler(rp_OceanNormalMap.Ptr(),
        reinterpret_cast<const idTextureSampler*>(retiredNormalMap));

  idTriangles* triangles = m_lpTri[m_iCurrentTriFrame];
  for (unsigned int tile = 0; tile < retiredFrame.m_nTilesUsed; ++tile) {
    idRenderModelSurface& surface = surfaces.list[tile];
    idTriangles& geometry = triangles[tile];
    surface.geometry = &geometry;
    surface.material = m_pMaterial;
    const unsigned int offset = retiredFrame.m_pTileVBOffset[tile];
    geometry.vertexBuffer.Reference(retiredSharedFrame.m_VB,
                                    static_cast<int>(offset),
                                    retiredSharedFrame.m_VB.GetSize() -
                                        static_cast<int>(offset));
    const unsigned int lod = retiredFrame.m_pTileLOD[tile];
    const idOceanSharedTileMemory::CLODIndices& indexInfo =
        m_pTileSharedMem->m_lLODindices[lod];
    geometry.indexBuffer.Reference(m_pTileSharedMem->m_IB,
                                   static_cast<int>(indexInfo.m_Ofs),
                                   static_cast<int>(2 * indexInfo.m_nIndices));
    geometry.numIndexes = static_cast<int>(indexInfo.m_nIndices);
    geometry.numVerts = static_cast<int>(indexInfo.m_nVertices);
    geometry.vertexMask = 579;

    const float scale = 4.0f / float(m_pJobParams[tile].m_TileSTScale);
    surface.skinOffsets.x = m_WaveParams.m_NormalMapSizeInv.x * scale;
    surface.skinOffsets.y = m_WaveParams.m_NormalMapSizeInv.y * scale;
    surface.skinOffsets.z =
        (float(retiredFrame.m_pTileAtlasTexelOfs[tile].x) + 0.5f) *
            m_WaveParams.m_NormalMapSizeInv.x -
        float(m_pJobParams[tile].m_TileST1Ofs.x) * surface.skinOffsets.x;
    surface.skinOffsets.w =
        (float(retiredFrame.m_pTileAtlasTexelOfs[tile].y) + 0.5f) *
            m_WaveParams.m_NormalMapSizeInv.y -
        float(m_pJobParams[tile].m_TileST1Ofs.y) * surface.skinOffsets.y;
  }

  for (unsigned int tile = retiredFrame.m_nTilesUsed;
       tile < m_nTilesPrevUsed; ++tile) {
    idRenderModelSurface& surface = surfaces.list[tile];
    surface.material = nullptr;
    surface.skinOffsets = {};
    if (surface.geometry != nullptr) {
      surface.geometry->numIndexes = 0;
      surface.geometry->numVerts = 0;
    }
  }
  m_nTilesPrevUsed = retiredFrame.m_nTilesUsed;

  std::int64_t ticksWaveVertices = 0;
  std::int64_t ticksWaveNormals = 0;
  std::int64_t ticksUpsample = 0;
  std::int64_t ticksVertices = 0;
  std::int64_t ticksNormals = 0;
  for (unsigned int tile = 0; tile < retiredFrame.m_nTilesUsed; ++tile) {
    const auto& timers = m_pJobParams[tile].m_Timers;
    ticksWaveVertices += timers.m_TicksWave_v;
    ticksWaveNormals += timers.m_TicksWave_n;
    ticksUpsample += timers.m_TicksUpSample;
    ticksVertices += timers.m_TicksVertices;
    ticksNormals += timers.m_TicksNormals;
  }

  m_iCurrentTriFrame = 0;
  m_iCurrentFrame = (m_iCurrentFrame + 1) % NUM_FRAMES;
  COceanFrameState& frame = m_lFrames[m_iCurrentFrame];
  idOceanSharedTileMemory::CFrameState& sharedFrame =
      m_pTileSharedMem->m_lFrameMem[m_pTileSharedMem->m_iCurrentFrame];
  if (sharedFrame.m_pMappedVB == nullptr)
    sharedFrame.m_pMappedVB = sharedFrame.m_VB.MapBuffer(BM_WRITE_NOSYNC);
  auto* mappedVertices = static_cast<std::uint8_t*>(sharedFrame.m_pMappedVB);

  const int useCVarParams = r_ocean_usecvarparams.GetInteger();
  if (useCVarParams != m_UseCVarParams) {
    if (m_UseCVarParams == 0)
      Ocean_SetCVars(&m_Params);
    m_UseCVarParams = useCVarParams;
  }
  if (useCVarParams != 0) {
    idOceanParams_t updated = m_Params;
    updated.m_nMaxWaves = r_ocean_maxwaves.GetInteger();
    updated.m_nMacroTileSubdivide =
        r_ocean_macrotilesubdivide.GetInteger();
    updated.m_AmpDistributionExp = r_ocean_ampdistrexp.GetFloat();
    updated.m_AmpMin = r_ocean_ampmin.GetFloat();
    updated.m_AmpMax = r_ocean_ampmax.GetFloat();
    updated.m_AngleSpread = r_ocean_anglespread.GetFloat();
    updated.m_AngleBase = r_ocean_anglebase.GetFloat();
    updated.m_AmpK = r_ocean_ampk.GetFloat();
    updated.m_GravityK = r_ocean_gravk.GetFloat();
    updated.m_LODFadeRange = r_ocean_wavelodfaderange.GetFloat();
    updated.m_MinVertPerWaveLen = r_ocean_minvertperwavelen.GetFloat();
    updated.m_MinTexelPerWaveLen = r_ocean_mintexelperwavelen.GetFloat();
    updated.m_LODK = r_ocean_lodk.GetFloat();
    if (std::memcmp(&updated, &m_Params, sizeof(updated)) != 0)
      CreateWaveParams(&updated);
  }
  frame.m_bValid = false;
  frame.m_nTilesUsed = 0;

  const int macroTileCount = m_nMacroTiles.x * m_nMacroTiles.y;
  std::vector<std::uint8_t> macroVisible(
      static_cast<std::size_t>(macroTileCount), 0);
  bool anyMacroVisible = false;
  for (int index = 0; index < macroTileCount; ++index) {
    idRenderModel* occlusionModel = m_lpMacroTileOccTest[index];
    const int modelIndex = occlusionModel->GetIndex();
    const occlusionState_t* state = currentView->occlusionState;
    const int visibleCount =
        state->modelVisibleFrameNumber[modelIndex] < state->renderFrameNumber
            ? 0
            : state->modelVisibleFrameCount[modelIndex];
    const bool visible =
        visibleCount >= occlusionModel->g.firstVisibleFrameCount;
    macroVisible[static_cast<std::size_t>(index)] = visible ? 1u : 0u;
    anyMacroVisible |= visible;
  }

  const idVec3 cameraWorld{nextView->inverseViewMatrix.m[3],
                           nextView->inverseViewMatrix.m[7],
                           nextView->inverseViewMatrix.m[11]};
  const idVec3 cameraLocal = WorldToLocal(*this, cameraWorld);
  m_WaveParams.m_LODCamPos =
      {cameraLocal.x, cameraLocal.y, cameraLocal.z, 1.0f};

  CSubdivideParams subdivision{};
  subdivision.m_pQTNodes = m_lQTNodes.list;
  subdivision.m_pQTRopes = m_lQTRopes.list;
  subdivision.m_nQTNodesMax =
      static_cast<unsigned int>(m_lQTNodes.num);
  subdivision.m_nQTRopesMax =
      static_cast<unsigned int>(m_lQTRopes.num);
  subdivision.m_pTileJobs = m_pJobParams;
  subdivision.m_pTileLODs = frame.m_pTileLOD;
  subdivision.m_CamLocal = cameraLocal;
  subdivision.m_kLOD = m_Params.m_LODK;
  subdivision.m_MaxLOD =
      static_cast<unsigned int>(m_pTileSharedMem->m_lLODindices.num - 1);
  subdivision.m_Cullmargin = m_Params.m_AmpMax * 6.5f;
  SetupMVP(&subdivision.m_MVP, currentView, 1.0f, 20000.0f, 0.0f);

  unsigned int tilesUsed = 0;
  std::int64_t quadtreeTicks = 0;
  bool allocationSucceeded = false;
  for (float lodOffset = 0.0f;
       anyMacroVisible && lodOffset <= 5.0f && !allocationSucceeded;
       lodOffset += 0.5f) {
    subdivision.m_LODofs = lodOffset;
    const unsigned int rootTessellation =
        m_TileTess << m_WaveParams.m_MaxMacroTileSubdivide;
    const unsigned int rootCount =
        static_cast<unsigned int>(macroTileCount);
    for (unsigned int y = 0; y < unsigned(m_nMacroTiles.y); ++y) {
      for (unsigned int x = 0; x < unsigned(m_nMacroTiles.x); ++x) {
        CQTNode& root =
            m_lQTNodes.list[x + y * unsigned(m_nMacroTiles.x)];
        root = {};
        root.m_iTile = ~0u;
        root.m_TileTess =
            static_cast<std::uint16_t>(rootTessellation);
        root.m_TileOfsX =
            static_cast<std::uint16_t>(rootTessellation * x);
        root.m_TileOfsY =
            static_cast<std::uint16_t>(rootTessellation * y);
      }
    }

    unsigned int ropeCount = 0;
    for (unsigned int x = 0; x + 1 < unsigned(m_nMacroTiles.x); ++x) {
      CQTRope* previous = nullptr;
      for (unsigned int y = 0; y < unsigned(m_nMacroTiles.y); ++y) {
        CQTRope& rope = m_lQTRopes.list[ropeCount++];
        CQTNode& left =
            m_lQTNodes.list[x + y * unsigned(m_nMacroTiles.x)];
        CQTNode& right =
            m_lQTNodes.list[x + 1 + y * unsigned(m_nMacroTiles.x)];
        rope = {};
        rope.m_lpNodes[0] = &left;
        rope.m_lpNodes[1] = &right;
        rope.m_Len = static_cast<std::uint16_t>(rootTessellation);
        rope.recoveredPadding1A = 1;
        left.m_lpRopes[0] = &rope;
        right.m_lpRopes[1] = &rope;
        if (previous != nullptr)
          previous->m_pNext = &rope;
        previous = &rope;
      }
    }
    for (unsigned int y = 0; y + 1 < unsigned(m_nMacroTiles.y); ++y) {
      CQTRope* previous = nullptr;
      for (unsigned int x = 0; x < unsigned(m_nMacroTiles.x); ++x) {
        CQTRope& rope = m_lQTRopes.list[ropeCount++];
        CQTNode& top =
            m_lQTNodes.list[x + y * unsigned(m_nMacroTiles.x)];
        CQTNode& bottom =
            m_lQTNodes.list[x + (y + 1) * unsigned(m_nMacroTiles.x)];
        rope = {};
        rope.m_lpNodes[0] = &top;
        rope.m_lpNodes[1] = &bottom;
        rope.m_Len = static_cast<std::uint16_t>(rootTessellation);
        top.m_lpRopes[2] = &rope;
        bottom.m_lpRopes[3] = &rope;
        if (previous != nullptr)
          previous->m_pNext = &rope;
        previous = &rope;
      }
    }

    subdivision.m_nQTNodes = rootCount;
    subdivision.m_nQTRopes = ropeCount;
    tilesUsed = 0;
    const std::int64_t quadtreeStart = Sys_GetClockTicks();
    bool subdivisionFailed = false;
    for (int y = 0; y < m_nMacroTiles.y && !subdivisionFailed; ++y) {
      for (int x = 0; x < m_nMacroTiles.x; ++x) {
        const unsigned int rootIndex =
            static_cast<unsigned int>(x + y * m_nMacroTiles.x);
        const idVec3 minimum{
            m_WaveParams.m_TilesOrigin.x + x * m_MacroTileDim,
            m_WaveParams.m_TilesOrigin.y + y * m_MacroTileDim,
            m_WaveParams.m_TilesOrigin.z};
        const unsigned int generated = MacroTileSubdivide_r(
            &subdivision, &m_lQTNodes.list[rootIndex],
            m_WaveParams.m_MaxMacroTileSubdivide,
            m_nTilesMax - tilesUsed, tilesUsed, minimum, m_MacroTileDim,
            macroVisible[rootIndex] != 0);
        if (generated == 0) {
          subdivisionFailed = true;
          break;
        }
        tilesUsed += generated;
      }
    }
    quadtreeTicks = Sys_GetClockTicks() - quadtreeStart;
    if (!subdivisionFailed) {
      allocationSucceeded = m_pTileSharedMem->Alloc(
          tilesUsed, m_TileTess, frame.m_pTileLOD,
          frame.m_pTileVBOffset, frame.m_pTileAtlasTexelOfs,
          frame.m_ppTileWorkMem);
    }
  }

  if (!allocationSucceeded) {
    for (unsigned int index = 0; index < m_nTilesMax; ++index) {
      if (surfaces.list[index].geometry != nullptr) {
        surfaces.list[index].geometry->numIndexes = 0;
        surfaces.list[index].geometry->numVerts = 0;
      }
    }
    return false;
  }

  if ((m_WaveParams.m_DebugFlags & 0x400u) != 0) {
    const unsigned int rootTessellation =
        m_TileTess << m_WaveParams.m_MaxMacroTileSubdivide;
    DrawDebugRopes(*this, subdivision.m_nQTNodes,
                   subdivision.m_nQTRopes,
                   m_MacroTileDim / float(rootTessellation));
  }

  m_WaveParams.m_Time = float(nextView->r.renderTime & 0x3fffff) * 0.001f;
  if (std::fabs(cameraLocal.z) < m_Params.m_AmpMax * 8.0f &&
      m_pEnvAir != nullptr && m_pEnvWater != nullptr) {
    idVec3 cameraSurface{};
    idVec3 cameraNormal{};
    float cameraHeight = 0.0f;
    if (GetSurfacePosition(cameraWorld, 1.0f, 0.0f,
                           nextView->r.renderTime, cameraSurface,
                           cameraNormal, cameraHeight)) {
      idRenderView* mutableNextView = const_cast<idRenderView*>(nextView);
      mutableNextView->g.applyDynEnvOverride = true;
      mutableNextView->g.dynEnvOverrideDuration = 1.0f;
      mutableNextView->m_pModelEnvOverride =
          cameraWorld.z <= cameraSurface.z ? m_pEnvWater : m_pEnvAir;
    }
  }

  frame.m_nTilesUsed = tilesUsed;
  for (unsigned int tile = 0; tile < tilesUsed; ++tile) {
    idRenderModelOcean_TileJobParams& job = m_pJobParams[tile];
    const unsigned int lod = frame.m_pTileLOD[tile];
    unsigned int tessellation = m_TileTess >> lod;
    if (tessellation == 0)
      tessellation = 1;
    job.m_pParams = &m_WaveParams;
    job.m_TileTess = tessellation;
    job.m_TileNormalMapOfs = frame.m_pTileAtlasTexelOfs[tile];
    job.m_iLOD = lod;
    job.m_pWorkMem = nullptr;
    job.m_TileSTScale = job.m_TileSTDim / tessellation;
    job.m_NormalMapPitch = 16 * tessellation + 4;
    job.m_pNormalMap = frame.m_ppTileWorkMem[tile];
    job.m_pVB = reinterpret_cast<COceanVertex*>(
        mappedVertices + frame.m_pTileVBOffset[tile]);
    std::memset(&job.m_Timers, 0, sizeof(job.m_Timers));
  }

  if (rp_OceanMacroTileSTDim.Ptr() != nullptr) {
    const float reciprocalDimension =
        1.0f / float(16 << m_Params.m_nMacroTileSubdivide);
    gameParmBlock.SetParm(
        rp_OceanMacroTileSTDim.Ptr(),
        idVec4{reciprocalDimension, reciprocalDimension,
               reciprocalDimension, reciprocalDimension});
  }

  if (r_ocean_jobmode.GetInteger() == 1) {
    for (unsigned int tile = 0; tile < tilesUsed; ++tile) {
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(OceanGenJob), &m_pJobParams[tile]);
    }
  } else {
    for (unsigned int tile = 0; tile < tilesUsed; ++tile)
      OceanGenJob(&m_pJobParams[tile]);
  }

  const std::int64_t updateTicks = Sys_GetClockTicks() - updateStart;
  if (r_ocean_spamtimer.GetFloat() != 0.0f) {
    m_SpamTimer += 1.0f / 60.0f;
    if (m_SpamTimer >= r_ocean_spamtimer.GetFloat()) {
      m_SpamTimer = 0.0f;
      const float millisecondsPerTick =
          1000.0f / float(Sys_ClockTicksPerSecond());
      idLib::Printf(
          "Cam (%f, %f, %f), CamLocal (%f, %f, %f)\n",
          cameraWorld.x, cameraWorld.y, cameraWorld.z, cameraLocal.x,
          cameraLocal.y, cameraLocal.z);
      idLib::Printf(
          "Ocean Mem: nV %d/%d, WkB %d/%d, AT %d/%d St %d, T "
          "%d/%d, JobTime(ms): WV %.2f, WN %.2f, Nrm %.2f, Vtx "
          "%.2f, UpSmp %.2f, Tot %.2f, Sy %.2f, QT %.2f, TUp "
          "%.2f, UIV %.2f\n",
          sharedFrame.m_VertAllocPos, sharedFrame.m_VertMax,
          m_pTileSharedMem->m_WorkMemAllocPos >> 10,
          m_pTileSharedMem->m_WorkMemSize >> 10,
          sharedFrame.m_AtlasAllocPos,
          m_pTileSharedMem->m_nAtlasBaseTiles,
          m_pTileSharedMem->m_Starve, frame.m_nTilesUsed, m_nTilesMax,
          float(ticksWaveVertices) * millisecondsPerTick,
          float(ticksWaveNormals) * millisecondsPerTick,
          float(ticksNormals) * millisecondsPerTick,
          float(ticksVertices) * millisecondsPerTick,
          float(ticksUpsample) * millisecondsPerTick,
          float(ticksWaveVertices + ticksWaveNormals + ticksNormals +
                ticksVertices + ticksUpsample) *
              millisecondsPerTick,
          float(syncTicks) * millisecondsPerTick,
          float(quadtreeTicks) * millisecondsPerTick,
          float(uploadTicks) * millisecondsPerTick,
          float(updateTicks) * millisecondsPerTick);
    }
  }

  CommitThisFrame();
  frame.m_bValid = true;
  return true;
}
