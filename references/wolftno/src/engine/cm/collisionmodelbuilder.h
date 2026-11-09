#pragma once

#include "engine/cm/collisionmodel.h"
#include "shared/idlib/geometry/winding.h"

#include <memory>
#include <unordered_map>

struct cmBuildNode_t {
  idBounds bounds{};
  int axis{-1};
  float distance{};
  std::uint32_t contents{};
  std::vector<std::size_t> polygons;
  std::vector<std::size_t> polytopes;
  std::unique_ptr<cmBuildNode_t> children[2];
};

struct cmBuildStats_t {
  std::size_t nodes{};
  std::size_t leafs{};
  std::size_t polygons{};
  std::size_t polytopes{};
};

struct cm_buildModel_t {
  cm_buildModel_t();
  std::shared_ptr<idCollisionModelLocal> model;
  std::unique_ptr<cmBuildNode_t> root;
  cmBuildStats_t stats;
  std::unordered_map<std::uint64_t, std::uint32_t> vertexHash;
  std::unordered_map<std::uint64_t, std::uint32_t> edgeHash;
};

struct idMaterial {
  idStr name;
  std::uint32_t contents{CONTENTS_SOLID};
};

template <typename T> class idDeclInfoTemplate {
public:
  bool Load(const char* name);
  idStr loadedName;
};

template <> bool idDeclInfoTemplate<idMaterial>::Load(const char* name);

class idCollisionModelBuilder {
public:
  struct idStaticModelGeometry {
    idStaticModelGeometry();
    ~idStaticModelGeometry();
    idStaticModelGeometry& operator=(const idStaticModelGeometry& other);
    std::vector<idVec3> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t contents{CONTENTS_SOLID};
  };

  struct idCollisionModelGeometry {
    ~idCollisionModelGeometry();
    std::vector<idStaticModelGeometry> surfaces;
  };

  std::shared_ptr<idCollisionModelLocal> BuildForTrm(
      const idTraceModel& traceModel, const char* name = "traceModel",
      std::uint32_t contents = CONTENTS_SOLID) const;
  std::shared_ptr<idCollisionModelLocal>
  BuildForBounds(const idBounds& bounds, const char* name = "bounds",
                 std::uint32_t contents = CONTENTS_SOLID) const;
  std::shared_ptr<idCollisionModelLocal> BuildSphere(
      float radius, const char* name = "sphere",
      std::uint32_t contents = CONTENTS_SOLID) const;
  std::shared_ptr<idCollisionModelLocal>
  BuildForDeclMD6(const char* name, const idBounds& animatedBounds,
                  std::uint32_t contents = CONTENTS_BODY) const;
  std::shared_ptr<idCollisionModelLocal> BuildForGrid(const idBounds& bounds,
                                                      const char* name = "grid") const;
  std::shared_ptr<idCollisionModelLocal> BuildForMapFile(
      const std::vector<idStaticModelGeometry>& surfaces, const char* name) const;
  std::shared_ptr<idCollisionModelLocal> BuildForStaticGeometry(
      const idStaticModelGeometry& geometry, const char* name) const;
  std::shared_ptr<idCollisionModelLocal> BuildForStaticModel(
      const idStaticModelGeometry& geometry, const char* name) const;
  std::shared_ptr<idCollisionModelLocal> BuildForRenderModelSurface(
      const std::vector<idVec3>& vertices, const std::vector<std::uint32_t>& indices,
      const char* name, std::uint32_t contents = CONTENTS_SOLID) const;
  std::shared_ptr<idCollisionModelLocal> BuildCollisionModelForGeometry(
      const idCollisionModelGeometry& geometry, const char* name) const;

  bool IsStaticRenderModel(const char* name) const;
  bool IsAnimatedRenderModel(const char* name) const;
  void FinishModel(idCollisionModelLocal& model) const;

  std::unique_ptr<cmBuildNode_t> AllocNode() const;
  std::size_t AllocPolygonReference(cmBuildNode_t& node, std::size_t polygon) const;
  std::size_t AllocPolytopeReference(cmBuildNode_t& node, std::size_t polytope) const;
  cmPolygon_t& AllocPolygon(idCollisionModelLocal& model) const;
  cmPolytope_t& AllocPolytope(idCollisionModelLocal& model) const;
  void AddPolygonToNode(cmBuildNode_t& node, std::size_t polygon) const;
  void AddPolytopeToNode(cmBuildNode_t& node, std::size_t polytope) const;
  std::uint32_t GetNodeContents(const cmBuildNode_t& node) const;
  void SetupSubModelData(cm_buildModel_t& build) const;
  void FindSubModels_r(cmBuildNode_t& node, std::vector<cmBuildNode_t*>& output) const;
  void FreeModelMemory(cm_buildModel_t& build) const;
  void GenerateEdgeNormals_r(idCollisionModelLocal& model, cmBuildNode_t& node) const;
  void ReplacePolygons(idCollisionModelLocal& model, std::size_t first,
                       std::vector<cmPolygon_t> replacements) const;
  bool RemovePolygon(idCollisionModelLocal& model, std::size_t polygon) const;
  bool PointInsidePolygon(const idVec3& point, const cmPolygon_t& polygon,
                          const idCollisionModelLocal& model, float epsilon = 0.001f) const;
  std::vector<std::size_t> FindInternalEdgesOnPolygon(
      const cmPolygon_t& polygon, const idCollisionModelLocal& model) const;
  std::vector<std::size_t> FindInternalPolygonEdges(const idCollisionModelLocal& model) const;
  void FindInternalEdges(idCollisionModelLocal& model) const;
  void OffsetPolygonEdges(cmPolygon_t& polygon, idCollisionModelLocal& model,
                          float amount) const;
  void OffsetPolygonEdges_r(cmBuildNode_t& node, idCollisionModelLocal& model,
                            float amount) const;
  bool SplitterDividesPrimitives(const cmBuildNode_t& node, int axis,
                                 float distance) const;
  bool FindSplitter(const cmBuildNode_t& node, int& axis, float& distance) const;
  void FilterPolygonIntoTree_r(cmBuildNode_t& node, std::size_t polygon,
                               const idCollisionModelLocal& model) const;
  void FilterPolytopeIntoTree_r(cmBuildNode_t& node, std::size_t polytope,
                                const idCollisionModelLocal& model) const;
  void CreateAxialBSPTree_r(cmBuildNode_t& node, const idCollisionModelLocal& model,
                            int depth) const;
  std::unique_ptr<cmBuildNode_t> CreateAxialBSPTree(
      const idCollisionModelLocal& model) const;

  void ClearHash(cm_buildModel_t& build) const;
  void SetupHash(cm_buildModel_t& build) const;
  void ShutdownHash(cm_buildModel_t& build) const;
  std::uint32_t GetVertex(cm_buildModel_t& build, const idVec3& vertex) const;
  std::uint32_t GetEdge(cm_buildModel_t& build, std::uint32_t first,
                        std::uint32_t second) const;
  idBounds GetNodeBounds_r(const cmBuildNode_t& node) const;
  cmBuildStats_t GetStatsFromNode(const cmBuildNode_t& node) const;
  void CreateStatsForSubTree_r(const cmBuildNode_t& node, cmBuildStats_t& stats) const;
  void CreateNodeStats_r(cmBuildNode_t& node, const idCollisionModelLocal& model) const;
  void AddBuildNodePrimitivesToSubModelNode(const cmBuildNode_t& source,
                                            cmBuildNode_t& destination) const;
  std::shared_ptr<idCollisionModelLocal> CreateSingleSubModel_r(
      const cmBuildNode_t& node, const idCollisionModelLocal& source,
      const char* name) const;
  std::size_t CountModelTreeNodes_r(const cmBuildNode_t& node) const;
  void SetupBuildGroups(cm_buildModel_t& build) const;

  bool ChoppedAwayByProcBSP_r(const idBounds& bounds, const cmBuildNode_t& node) const;
  bool ChoppedAwayByProcBSP(const idBounds& bounds, const cmBuildNode_t& root) const;
  std::vector<idFixedWinding> ChopWindingListWithPolytope(
      const std::vector<idFixedWinding>& windings, const cmPolytope_t& polytope) const;
  std::vector<idFixedWinding> ChopWindingListWithTreePolytopes_r(
      const std::vector<idFixedWinding>& windings, const cmBuildNode_t& node,
      const idCollisionModelLocal& model) const;
  bool TryMergePolygons(const cmPolygon_t& first, const cmPolygon_t& second,
                        cmPolygon_t& merged, const idCollisionModelLocal& model) const;
  bool MergePolygonWithTreePolygons(std::size_t polygon, idCollisionModelLocal& model,
                                    cmBuildNode_t& node) const;
  void MergeTreePolygons(idCollisionModelLocal& model, cmBuildNode_t& node) const;
  void MergeTreePolygonsWithCompatibleMaterials(idCollisionModelLocal& model,
                                                cmBuildNode_t& node) const;
  void AddMapModelEstimates(cm_buildModel_t& build,
                            const idStaticModelGeometry& geometry) const;
  bool WindingOutsidePolytopes(const idFixedWinding& winding,
                               const std::vector<cmPolytope_t>& polytopes) const;
  bool SplitPolygon(const cmPolygon_t& polygon, const idPlane& plane,
                    idCollisionModelLocal& model, cmPolygon_t& front,
                    cmPolygon_t& back) const;
  std::size_t CalculateSubModelDataSize(const idCollisionModelLocal& model) const;
  std::vector<std::uint8_t> AllocSubModelData(const idCollisionModelLocal& model) const;
  void CreateSubModels_r(const cmBuildNode_t& node, const idCollisionModelLocal& source,
                         std::vector<std::shared_ptr<idCollisionModelLocal>>& output) const;
  void AddSubModelsToCollisionModel(
      idCollisionModelLocal& destination,
      const std::vector<std::shared_ptr<idCollisionModelLocal>>& subModels) const;
  std::unique_ptr<cm_buildModel_t> AllocBuildModel() const;
  void CollectSurfaces(const idCollisionModelGeometry& geometry,
                       std::vector<idStaticModelGeometry>& output) const;
  bool CreatePolygon(idCollisionModelLocal& model, const idVec3& first,
                     const idVec3& second, const idVec3& third,
                     std::uint32_t contents = CONTENTS_SOLID) const;
  bool PolygonFromWinding(idCollisionModelLocal& model, const idWinding& winding,
                          std::uint32_t contents = CONTENTS_SOLID) const;
  void ConvertBrushSides(const idBounds& brush, idCollisionModelLocal& model,
                         std::uint32_t contents) const;
  void ConvertBrush(const idBounds& brush, idCollisionModelLocal& model,
                    std::uint32_t contents) const;
  void ConvertMapModelPrimitives(const std::vector<idBounds>& brushes,
                                 idCollisionModelLocal& model) const;
  void ConvertMapModelPolytopes(const std::vector<cmPolytope_t>& polytopes,
                               idCollisionModelLocal& model) const;
  void ConvertRenderModelSurfaces(const std::vector<idStaticModelGeometry>& surfaces,
                                  idCollisionModelLocal& model) const;
  void AddCollisionModelGeometry(idCollisionModelLocal& destination,
                                 const idCollisionModelLocal& source) const;
  void MergeModelTrees(cmBuildNode_t& destination, const cmBuildNode_t& source) const;
};

std::size_t CM_R_CountChildren(const cmBuildNode_t& node);
bool CM_R_InsideAllChildren(const idBounds& bounds, const cmBuildNode_t& node);
bool CM_R_TestOptimisation(const cmBuildNode_t& node);
bool CM_TestOptimisation(const cmBuildNode_t& root);
