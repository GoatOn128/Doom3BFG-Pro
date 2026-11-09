#pragma once

#include "engine/models/cable/jobs/cablegen.h"
#include "engine/models/rendermodel.h"
#include "shared/idlib/math/curve.h"

#include <cstddef>

class idCVar;
class idMaterial;

class idRenderModelCable : public idRenderModel {
public:
  idRenderModelCable();
  ~idRenderModelCable() override;

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
  void SetPoints(const idVec3* points, const float* tension, int num);
  void BuildModel(const idMaterial* material, int lengthMaxSubdivisions,
                  float diameter, int diameterSubdivisions);

  // Retained spelling from the retail symbol.
  void FinializeJob();

  const idMaterial* material;
  int lengthMaxSubdivisions;
  int diameterSubdivisions;
  float diameter;
  bool dirty;
  bool jobQueued;
  unsigned char recoveredCableFlagsPadding[2];
  subdivPoint_t* subdivPoints;
  cableGenParms_t* parms;
  int currentPointsBuffer;
  int currentGeometryBuffer;
  idList<idVec4, 5> pointsBuffer[2];
  idVertexBuffer vertexBuffer[3];
  idIndexBuffer indexBuffer[3];
  int numVertices;
  int numIndices;
  int jobDone;
  unsigned int recoveredCableTail;
};

extern idCVar r_useCableGenJobs;

static_assert(offsetof(idRenderModelCable, material) == 0x880,
              "idRenderModelCable material offset drift");
static_assert(offsetof(idRenderModelCable, dirty) == 0x894,
              "idRenderModelCable flags offset drift");
static_assert(offsetof(idRenderModelCable, subdivPoints) == 0x898,
              "idRenderModelCable subdivision offset drift");
static_assert(offsetof(idRenderModelCable, pointsBuffer) == 0x8B0,
              "idRenderModelCable points offset drift");
static_assert(offsetof(idRenderModelCable, vertexBuffer) == 0x8E0,
              "idRenderModelCable vertex buffer offset drift");
static_assert(offsetof(idRenderModelCable, indexBuffer) == 0x928,
              "idRenderModelCable index buffer offset drift");
static_assert(offsetof(idRenderModelCable, numVertices) == 0x970,
              "idRenderModelCable count offset drift");
static_assert(offsetof(idRenderModelCable, jobDone) == 0x978,
              "idRenderModelCable completion offset drift");
static_assert(sizeof(idRenderModelCable) == 0x980,
              "idRenderModelCable ABI drift");
