#pragma once

#include "engine/models/rendermodel.h"
#include "engine/models/static/jobs/statictransparencygen.h"
#include "engine/models/transparency/jobs/influencespherecull.h"
#include "engine/models/transparency/jobs/transparencysort.h"

#include <cstddef>
#include <cstdint>

class idParallelJobList;
class idRenderModelEffects;
class idRenderModelParticle;
struct idRenderModelTransparencyValidationAccess;

struct updateInViewStats_t {
  bool showStats{};
  std::uint8_t recoveredPadding[3]{};
  int c_numParticleVertices{};
};

class idRenderModelTransparency : public idRenderModel {
public:
  idRenderModelTransparency();

  static void Init();
  static void Shutdown();
  static void StartFrame();

  static bool AllocateQuads(int numQuads, int& quadsOffset,
                            idTransparencyVert*& verts,
                            float*& quadDepths);
  static bool AllocateSegment(bool hasEmissivePass, bool hasOpaquePass,
                              int*& quadsOffset, int*& quadsAllocated,
                              int*& quadsUsed);
  static bool AllocateQuadSegment(int numQuads,
                                  idTransparencyVert*& verts,
                                  float*& quadDepths, int*& quadsUsed,
                                  bool hasEmissivePass);
  static bool AllocateUnsortedQuadSegment(int numQuads,
                                          idTransparencyVert*& verts,
                                          int& indexOffset, int& vertCount);

  void ClearInfluenceSpheres();
  void GenerateVisibleInfluenceSphereList(
      const idRenderView* currentRenderView,
      const idRenderView* nextRenderView,
      idRenderModelUpdateTools* updateTools);
  void AddTransparencySortJob(idRenderModelUpdateTools* updateTools);
  void AddInfluenceSphere(const idVec3& center,
                          const idVec3& directionalForce,
                          float innerRadius, float outerRadius, float angle);
  void GenerateTransparencyRenderList(const idRenderView* currentView);

private:
  friend class idRenderModelEffects;
  friend class idRenderModelParticle;
  friend struct idRenderModelTransparencyValidationAccess;

  static idVertexBuffer vertexBuffer[3];
  static idIndexBuffer indexBuffer[3];
  static idIndexBuffer indexBuffer2[3];
  static int mappedBufferIndex;
  static idVertexBuffer vertexBufferUnsorted[3];
  static idIndexBuffer indexBufferUnsorted;
  static idTransparencyVert* mappedVerts;
  static std::uint16_t* mappedIndices;
  static std::uint16_t* mappedIndices2;
  static int numVertsAllocated;
  static float* currentQuadDepths;
  static std::uint8_t* currentEmissiveQuads;
  static int numSegmentAllocations;
  static transSortSegmentInfo_t* segmentInfo;
  static idTransparencyVert* mappedVertsUnsorted;
  static int numVertsAllocatedUnsorted;
  static idTransparencyVert* binVerts;
  static std::uint16_t* binIndices;

public:
  transSortParms_t* sortParms{};
  transSortResult_t* sortResult{};
  influenceSphere_t* influenceSpheres[2]{};
  int numInfluenceSpheres[2]{};
  int currentSphereIndex{};
  visibleInfluenceSpheres_t* visibleInfluenceSpheres{};
  influenceSphereCullParms_t* influenceSphereCullParms{};

protected:
  ~idRenderModelTransparency() override;
};

void TransparencySortJob(const transSortParms_t* parms);

class idRenderModelUpdateTools {
public:
  void ApproximateLightingForBounds(idRenderModel* model,
                                    const idBounds* bounds,
                                    approximateLighting_t* lighting);
  void GetCurrentResolutionScale(float* x, float* y);

  idParallelJobList* renderModelJobList{};
  idRenderModelTransparency* transparencyModel{};
  const void* globalShadowMap{};
  updateInViewStats_t pc{};
};

static_assert(sizeof(updateInViewStats_t) == 8,
              "update-in-view stats ABI drift");
static_assert(offsetof(idRenderModelTransparency, visibleInfluenceSpheres) ==
                  2224,
              "idRenderModelTransparency visible spheres offset drift");
static_assert(offsetof(idRenderModelTransparency, sortParms) == 2176,
              "idRenderModelTransparency sort parms offset drift");
static_assert(offsetof(idRenderModelTransparency, sortResult) == 2184,
              "idRenderModelTransparency sort result offset drift");
static_assert(offsetof(idRenderModelTransparency, influenceSpheres) == 2192,
              "idRenderModelTransparency influence spheres offset drift");
static_assert(offsetof(idRenderModelTransparency, numInfluenceSpheres) == 2208,
              "idRenderModelTransparency influence counts offset drift");
static_assert(offsetof(idRenderModelTransparency, currentSphereIndex) == 2216,
              "idRenderModelTransparency sphere index offset drift");
static_assert(offsetof(idRenderModelTransparency, influenceSphereCullParms) ==
                  2232,
              "idRenderModelTransparency cull parms offset drift");
static_assert(sizeof(idRenderModelTransparency) == 2240,
              "idRenderModelTransparency ABI drift");
static_assert(sizeof(idRenderModelUpdateTools) == 32,
              "idRenderModelUpdateTools ABI drift");
