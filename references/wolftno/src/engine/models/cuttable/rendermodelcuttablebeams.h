#pragma once

#include "engine/models/cuttable/cuttergraph.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"

#include <cstdint>

class idMaterial;
class idRenderModelUpdateTools;
class idRenderView;
class CuttableBeamsValidationAccess;

template <typename T> void SwapValues(T* left, T* right);

class alignas(16) idRenderModelCuttableBeams final : public idRenderModel {
public:
  enum : int {
    MAX_BEAM_QUADS = 4096,
    MAX_BEAM_VERTICES = 16384,
    MAX_BEAM_INDICES = 24576,
    NUM_VERTEX_BUFFERS = 3,
    SURFACE_INCISIONS = 0,
    SURFACE_INCISIONINSIDES = 1,
    SURFACE_INCISIONREPOINT = 2,
    SURFACE_INCISIONCROSSINGS = 3,
    SURFACE_INCISIONENDPOINTS = 4,
    SURFACE_MAX = 5,
  };

  idRenderModelCuttableBeams();
  ~idRenderModelCuttableBeams() override = default;

  void Create(const idMaterial* materialIncision_, const idMaterial* materialIncisionInside_,
              const idMaterial* materialIncisionCross_, bool useThickness_);
  static void EndFrame();
  void Update(idCutterGraphManager* graphManager_, int graphHandle_);
  static void Init();
  static void Shutdown();
  static void StartFrame();
  void SetBounds(const idBounds& bounds);
  void Save(idFile* file) const override;
  bool Load(idFile* file) override;

private:
  void BuildIncisionInsides(const graphTree_t& graphTree);
  void BuildIncisionRepoints(const idList<int>& repoints, const graphTree_t& graphTree);
  void BuildIncisionCrossings(const graphTree_t& graphTree);
  void BuildIncisionEndpoints(const graphTree_t& graphTree);
  void BuildIncisions(const graphTree_t& graphTree);
  bool UpdateInView(const idRenderView* currentView, const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

public:
  const idMaterial* materialIncision = nullptr;
  const idMaterial* materialIncisionInside = nullptr;
  const idMaterial* materialIncisionCross = nullptr;
  idCutterGraphManager* graphManager = nullptr;
  int graphHandle = -1;
  bool useThickness = false;

  static int currentBuffer;
  static int vertexOffset;
  static idDrawVert* deferredVertices;
  static idVertexBuffer vertexBuffer[NUM_VERTEX_BUFFERS];
  static idIndexBuffer indexBuffer;

  friend class CuttableBeamsValidationAccess;
};

static_assert(sizeof(idRenderModelCuttableBeams) == 2224,
              "idRenderModelCuttableBeams ABI drift");
