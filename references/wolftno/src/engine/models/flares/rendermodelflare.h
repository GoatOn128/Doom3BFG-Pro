#pragma once

#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"

#include <cstddef>

class idCVar;
class idDeclFlare;
class idRenderView;
class idRenderModelUpdateTools;
class idSerializer;

class idRenderModelFlareOcclusionQuad;

struct flareLockViewState_t {
  idVec4 quadColor;
  idVec4 entityColor;
  float entityColorScale;
  float occlusionRatio;
  float fade;
  float viewAngle;
  float viewDist;
};

class alignas(16) idRenderModelFlare : public idRenderModel {
public:
  explicit idRenderModelFlare(const idDeclFlare* declFlare);

  static void Init();
  static void Shutdown();
  static void StartFrame();
  static void EndFrame();

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  void SerializeSnapshot(idSerializer* serializer, bool serializeParmBlock);
  void Save(idFile* file) const override;
  bool Load(idFile* file) override;

  const idDeclFlare* flareDecl;
  idRenderModelFlareOcclusionQuad* occlusionQuadModel;
  bool isSunFlare;
  std::uint8_t recoveredFlarePadding[3];
  flareLockViewState_t lockedViewState;

private:
  static idVertexBuffer vertexBuffer[2];
  static idIndexBuffer indexBuffer;
  static idDrawVert* mappedVertices;
  static int mappedBufferIndex;
  static int vertOffset;

  friend class RenderModelFlareValidationAccess;
};

class alignas(16) idRenderModelFlareOcclusionQuad : public idRenderModel {
public:
  idRenderModelFlareOcclusionQuad();

  static void Init();
  static void Shutdown();
  static void StartFrame();

  float CalcScreenSpaceAreaForView(const idRenderView* currentView,
                                   const idRenderView* nextView,
                                   idRenderModelUpdateTools* updateTools);
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  void SerializeSnapshot(idSerializer* serializer, bool serializeParmBlock);
  void Save(idFile* file) const override;
  bool Load(idFile* file) override;

  bool isSunFlare;
  std::uint8_t recoveredOcclusionPadding[3];
  float screenSpaceArea[2];
  int currentIdx;

private:
  static idVertexBuffer vertexBuffer;
  static idIndexBuffer indexBuffer;
  static int vertOffset;
  static idVec3 points[4];

  friend class RenderModelFlareValidationAccess;
};

extern idCVar r_skipFlares;
extern idCVar r_skipSunFlares;
extern idCVar r_singleFlareModel;
extern idCVar r_debugFlareOcclusionQuad;

static_assert(sizeof(flareLockViewState_t) == 52,
              "flare lock-view state ABI drift");
static_assert(offsetof(idRenderModelFlare, flareDecl) == 0x880,
              "flare declaration offset drift");
static_assert(offsetof(idRenderModelFlare, occlusionQuadModel) == 0x888,
              "flare occlusion-model offset drift");
static_assert(offsetof(idRenderModelFlare, isSunFlare) == 0x890,
              "flare sun flag offset drift");
static_assert(offsetof(idRenderModelFlare, lockedViewState) == 0x894,
              "flare lock-view state offset drift");
static_assert(sizeof(idRenderModelFlare) == 0x8d0,
              "idRenderModelFlare ABI drift");
static_assert(offsetof(idRenderModelFlareOcclusionQuad, isSunFlare) == 0x880,
              "flare-quad sun flag offset drift");
static_assert(offsetof(idRenderModelFlareOcclusionQuad, screenSpaceArea) ==
                  0x884,
              "flare-quad area offset drift");
static_assert(offsetof(idRenderModelFlareOcclusionQuad, currentIdx) == 0x88c,
              "flare-quad current-index offset drift");
static_assert(sizeof(idRenderModelFlareOcclusionQuad) == 0x890,
              "idRenderModelFlareOcclusionQuad ABI drift");
