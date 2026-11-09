#pragma once

#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

enum beamOrientType_t : int {
  BEAM_ORIENT_VIEWER = 0,
  BEAM_ORIENT_AXIS = 1,
  BEAM_ORIENT_EXPLICIT = 2,
};

struct beamNodeParms_t {
  using idListAllocReturnsReferenceMarker = void;

  idVec3 startPos;
  idVec3 endPos;
  idVec3 startPos2;
  idVec3 endPos2;
  idVec3 orientVec;
  idVec2 sMinMax;
  idVec2 tMinMax;
#if defined(WOLFTNO_RETAIL_BEAMNODE_TRIVIAL_DEFAULT_ABI)
  float halfWidth;
  std::uint8_t color[2][4];
  std::uint8_t tangent[4][4];
#else
  float halfWidth = 0.0f;
  std::uint8_t color[2][4]{};
  std::uint8_t tangent[4][4]{};
#endif
};

struct beam_t {
  const idMaterial* material = nullptr;
  idList<beamNodeParms_t, 5> nodes;
  beamOrientType_t orientType = BEAM_ORIENT_VIEWER;
  std::uint32_t recoveredPadding = 0;
};

struct beamMaxInfo_t {
  int maxSurfaces = 0;
  int maxBeams = 0;
  int maxBeamNodes = 0;
  int maxBeamQuads = 0;
};

using beamFrameNumberCallback_t = int (*)();
void SetRenderModelBeamFrameNumberCallback(beamFrameNumberCallback_t callback);

class alignas(16) idRenderModelBeam final : public idRenderModel {
public:
  explicit idRenderModelBeam(bool large);
  ~idRenderModelBeam() override;

  static void Init();
  static void Shutdown();

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  bool CommitSubclass() override;

  void Update(int currTime);
  beam_t* DrawBeamAlloc();
  // Retail gamelib ABI: single-node callers pass the node by reference.
  void DrawBeam(beamNodeParms_t& node, const idMaterial* material,
                beamOrientType_t orientType);
  void DrawBeam(beamNodeParms_t* node, const idMaterial* material,
                beamOrientType_t orientType);
  // Retail ABI used by gamelib callers. Keep the unsigned declaration until
  // the render-model compiland itself is recovered.
  void DrawBeam(beamNodeParms_t* nodes, int numNodes,
                const idMaterial* material, beamOrientType_t orientType);
  void DrawBeam(beamNodeParms_t* nodes, unsigned numNodes,
                const idMaterial* material, beamOrientType_t orientType);

  static idVertexBuffer preAllocatedVertexBuffer[2];
  static idIndexBuffer preAllocatedIndexBuffer;
  static bool preAllocatedBufferInUse;

  idList<beam_t, 5> beamBuffer[2];
  beamMaxInfo_t maxInfo;
  idVertexBuffer vertexBuffer[2];
  idIndexBuffer indexBuffer;
  idTriangles* triangles = nullptr;
  int beamRenderBufferIndex = 0;
  bool usesPreAllocatedBuffer = false;
  std::uint8_t recoveredBeamPadding[11]{};
};

static_assert(sizeof(beamNodeParms_t) == 104,
              "beamNodeParms_t ABI drift");
static_assert(offsetof(beamNodeParms_t, endPos) == 12,
              "beam node end offset drift");
static_assert(offsetof(beamNodeParms_t, orientVec) == 48,
              "beam node orientation offset drift");
static_assert(offsetof(beamNodeParms_t, sMinMax) == 60,
              "beam node s range offset drift");
static_assert(offsetof(beamNodeParms_t, halfWidth) == 76,
              "beam node width offset drift");
static_assert(offsetof(beamNodeParms_t, color) == 80,
              "beam node color offset drift");
static_assert(offsetof(beamNodeParms_t, tangent) == 88,
              "beam node tangent offset drift");
static_assert(sizeof(beam_t) == 40, "beam_t ABI drift");
static_assert(sizeof(beamMaxInfo_t) == 16, "beam max info ABI drift");
static_assert(offsetof(idRenderModelBeam, beamBuffer) == 0x880,
              "beam buffer offset drift");
static_assert(offsetof(idRenderModelBeam, maxInfo) == 0x8B0,
              "beam max info offset drift");
static_assert(offsetof(idRenderModelBeam, vertexBuffer) == 0x8C0,
              "beam vertex buffer offset drift");
static_assert(offsetof(idRenderModelBeam, indexBuffer) == 0x8F0,
              "beam index buffer offset drift");
static_assert(offsetof(idRenderModelBeam, triangles) == 0x908,
              "beam triangles offset drift");
static_assert(offsetof(idRenderModelBeam, beamRenderBufferIndex) == 0x910,
              "beam render index offset drift");
static_assert(offsetof(idRenderModelBeam, usesPreAllocatedBuffer) == 0x914,
              "beam preallocation flag offset drift");
static_assert(sizeof(idRenderModelBeam) == 0x920,
              "idRenderModelBeam ABI drift");
