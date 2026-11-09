#pragma once

#include "engine/common/surfacetypes.h"
#include "engine/decls/decl.h"
#include "engine/models/cuttable/earcliptriangulate.h"
#include "engine/models/cuttable/triangleinterpolator.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

struct Contour;
class idMaterial;

struct cuttableGenSurface_t {
  const idMaterial* material = nullptr;
  int indexOffset = 0;
  int numIndices = 0;
  int numVertices = 0;
};

class idCuttableGenGeometry {
public:
  idList<cuttableGenSurface_t> surfaces;
  idList<std::uint16_t> indices;
  idList<idDrawVert> vertices;
  idList<idVec2> st;
};

class idRenderModelCuttableResource : public idResource {
public:
  idRenderModelCuttableResource();
  ~idRenderModelCuttableResource() override;

  bool ReloadIfStale() override;
  void BuildSurfaces(idRenderModel* model);
  idResourceList* GetResourceList() const override;
  bool GenerateSurfaces(idEarClipTriangulate& triangulator,
                        idCuttableGenGeometry& genGeometry,
                        const idVec3& centerOfMass);
  bool GenerateSurfaces(const Contour* outer, const Contour* inner,
                        idCuttableGenGeometry& genGeometry);
  void LoadResource() override;

  const idMaterial* material;
  const idMaterial* depthOnlyMaterial;
  const idMaterial* surfaceTypeMaterial;
  idStr coverMap;
  idTriangles* triangles;
  idVec2* stData;
  idVertexBuffer* stMap;
  idList<idVec3> contourBound;
  idTriangleInterpolator triangleInterpolator;
  int numInterpolatorTriangles;
  std::uint32_t timestamp;
  float thickness;
  surfTypes_t surfaceType;
  bool isOpaque;

  static idTypedResourceList<idRenderModelCuttableResource> resourceList;

private:
  void FreeCPUData();
  void FreeData();
  static const idMaterial* BuildDepthOnlyMaterial(const char* modelName,
                                                   const char* coverMap);
  bool LoadBinary(const char* fileName);
};

static_assert(sizeof(idRenderModelCuttableResource) == 320,
              "idRenderModelCuttableResource ABI drift");
static_assert(alignof(idRenderModelCuttableResource) == 16,
              "idRenderModelCuttableResource alignment drift");
