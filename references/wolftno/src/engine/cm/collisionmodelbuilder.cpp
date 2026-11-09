#include "engine/cm/collisionmodelbuilder.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <cmath>
#include <cstring>
#include <set>
#include <string>

namespace {
bool PointInsideBounds(const idVec3& point, const idBounds& bounds, float epsilon) {
  return point.x >= bounds[0].x - epsilon && point.x <= bounds[1].x + epsilon &&
         point.y >= bounds[0].y - epsilon && point.y <= bounds[1].y + epsilon &&
         point.z >= bounds[0].z - epsilon && point.z <= bounds[1].z + epsilon;
}
} // namespace

cm_buildModel_t::cm_buildModel_t()
    : model(std::make_shared<idCollisionModelLocal>()), root(std::make_unique<cmBuildNode_t>()) {}

idCollisionModelBuilder::idStaticModelGeometry::idStaticModelGeometry() = default;
idCollisionModelBuilder::idStaticModelGeometry::~idStaticModelGeometry() = default;
idCollisionModelBuilder::idStaticModelGeometry&
idCollisionModelBuilder::idStaticModelGeometry::operator=(const idStaticModelGeometry& other) {
  if (this != &other) {
    vertices = other.vertices;
    indices = other.indices;
    contents = other.contents;
  }
  return *this;
}
idCollisionModelBuilder::idCollisionModelGeometry::~idCollisionModelGeometry() = default;

template <> __declspec(noinline) bool idDeclInfoTemplate<idMaterial>::Load(const char* name) {
  loadedName = name ? name : "";
  return !loadedName.IsEmpty();
}

std::size_t CM_R_CountChildren(const cmBuildNode_t& node) {
  std::size_t count = 0;
  for (const auto& child : node.children) {
    if (child) {
      count += 1 + CM_R_CountChildren(*child);
    }
  }
  return count;
}

bool CM_R_InsideAllChildren(const idBounds& bounds, const cmBuildNode_t& node) {
  for (const auto& child : node.children) {
    if (child && (!CM_BoundsIntersect(bounds, child->bounds) ||
                  !CM_R_InsideAllChildren(bounds, *child))) {
      return false;
    }
  }
  return true;
}

bool CM_R_TestOptimisation(const cmBuildNode_t& node) {
  if (node.children[0] && node.children[1] && node.axis < 0) {
    return false;
  }
  for (const auto& child : node.children) {
    if (child && !CM_R_TestOptimisation(*child)) {
      return false;
    }
  }
  return true;
}
bool CM_TestOptimisation(const cmBuildNode_t& root) { return CM_R_TestOptimisation(root); }

std::unique_ptr<cmBuildNode_t> idCollisionModelBuilder::AllocNode() const {
  return std::make_unique<cmBuildNode_t>();
}
std::size_t idCollisionModelBuilder::AllocPolygonReference(cmBuildNode_t& node,
                                                           std::size_t polygon) const {
  node.polygons.push_back(polygon);
  return node.polygons.size() - 1;
}
std::size_t idCollisionModelBuilder::AllocPolytopeReference(cmBuildNode_t& node,
                                                            std::size_t polytope) const {
  node.polytopes.push_back(polytope);
  return node.polytopes.size() - 1;
}
cmPolygon_t& idCollisionModelBuilder::AllocPolygon(idCollisionModelLocal& model) const {
  return model.polygons.emplace_back();
}
cmPolytope_t& idCollisionModelBuilder::AllocPolytope(idCollisionModelLocal& model) const {
  return model.polytopes.emplace_back();
}
void idCollisionModelBuilder::AddPolygonToNode(cmBuildNode_t& node,
                                               std::size_t polygon) const {
  if (std::find(node.polygons.begin(), node.polygons.end(), polygon) == node.polygons.end()) {
    node.polygons.push_back(polygon);
  }
}
void idCollisionModelBuilder::AddPolytopeToNode(cmBuildNode_t& node,
                                                std::size_t polytope) const {
  if (std::find(node.polytopes.begin(), node.polytopes.end(), polytope) == node.polytopes.end()) {
    node.polytopes.push_back(polytope);
  }
}
std::uint32_t idCollisionModelBuilder::GetNodeContents(const cmBuildNode_t& node) const {
  std::uint32_t contents = node.contents;
  for (const auto& child : node.children) {
    if (child) {
      contents |= GetNodeContents(*child);
    }
  }
  return contents;
}

void idCollisionModelBuilder::SetupSubModelData(cm_buildModel_t& build) const {
  if (!build.model) {
    build.model = std::make_shared<idCollisionModelLocal>();
  }
  FinishModel(*build.model);
  build.stats = build.root ? GetStatsFromNode(*build.root) : cmBuildStats_t{};
}
void idCollisionModelBuilder::FindSubModels_r(cmBuildNode_t& node,
                                              std::vector<cmBuildNode_t*>& output) const {
  if (!node.children[0] && !node.children[1]) {
    output.push_back(&node);
    return;
  }
  for (auto& child : node.children) {
    if (child) {
      FindSubModels_r(*child, output);
    }
  }
}
void idCollisionModelBuilder::FreeModelMemory(cm_buildModel_t& build) const {
  build.root.reset();
  build.model.reset();
  ClearHash(build);
  build.stats = {};
}

void idCollisionModelBuilder::ClearHash(cm_buildModel_t& build) const {
  build.vertexHash.clear();
  build.edgeHash.clear();
}
void idCollisionModelBuilder::SetupHash(cm_buildModel_t& build) const {
  ClearHash(build);
  if (!build.model) {
    return;
  }
  for (std::uint32_t i = 0; i < build.model->vertices.size(); ++i) {
    const idVec3& v = build.model->vertices[i];
    const auto x = static_cast<std::int64_t>(std::llround(v.x * 1024.0f));
    const auto y = static_cast<std::int64_t>(std::llround(v.y * 1024.0f));
    const auto z = static_cast<std::int64_t>(std::llround(v.z * 1024.0f));
    const std::uint64_t key = (static_cast<std::uint64_t>(x) * 73856093ull) ^
                              (static_cast<std::uint64_t>(y) * 19349663ull) ^
                              (static_cast<std::uint64_t>(z) * 83492791ull);
    build.vertexHash[key] = i;
  }
  for (std::uint32_t i = 0; i < build.model->edges.size(); ++i) {
    const cmEdge_t& edge = build.model->edges[i];
    const std::uint32_t low = std::min(edge.vertex[0], edge.vertex[1]);
    const std::uint32_t high = std::max(edge.vertex[0], edge.vertex[1]);
    build.edgeHash[(static_cast<std::uint64_t>(low) << 32) | high] = i;
  }
}
void idCollisionModelBuilder::ShutdownHash(cm_buildModel_t& build) const { ClearHash(build); }

std::uint32_t idCollisionModelBuilder::GetVertex(cm_buildModel_t& build,
                                                 const idVec3& vertex) const {
  if (!build.model) {
    build.model = std::make_shared<idCollisionModelLocal>();
    build.model->FreeData();
  }
  const auto x = static_cast<std::int64_t>(std::llround(vertex.x * 1024.0f));
  const auto y = static_cast<std::int64_t>(std::llround(vertex.y * 1024.0f));
  const auto z = static_cast<std::int64_t>(std::llround(vertex.z * 1024.0f));
  const std::uint64_t key = (static_cast<std::uint64_t>(x) * 73856093ull) ^
                            (static_cast<std::uint64_t>(y) * 19349663ull) ^
                            (static_cast<std::uint64_t>(z) * 83492791ull);
  const auto found = build.vertexHash.find(key);
  if (found != build.vertexHash.end() &&
      (build.model->vertices[found->second] - vertex).LengthSqr() < 1.0e-8f) {
    return found->second;
  }
  const std::uint32_t index = static_cast<std::uint32_t>(build.model->vertices.size());
  build.model->vertices.push_back(vertex);
  build.vertexHash[key] = index;
  return index;
}

std::uint32_t idCollisionModelBuilder::GetEdge(cm_buildModel_t& build, std::uint32_t first,
                                               std::uint32_t second) const {
  const std::uint32_t low = std::min(first, second);
  const std::uint32_t high = std::max(first, second);
  const std::uint64_t key = (static_cast<std::uint64_t>(low) << 32) | high;
  const auto found = build.edgeHash.find(key);
  if (found != build.edgeHash.end()) {
    return found->second;
  }
  const std::uint32_t index = static_cast<std::uint32_t>(build.model->edges.size());
  build.model->edges.push_back({{first, second}, {}});
  build.edgeHash[key] = index;
  return index;
}

idBounds idCollisionModelBuilder::GetNodeBounds_r(const cmBuildNode_t& node) const {
  idBounds result = node.bounds;
  for (const auto& child : node.children) {
    if (child) {
      const idBounds childBounds = GetNodeBounds_r(*child);
      result.AddPoint(childBounds[0]);
      result.AddPoint(childBounds[1]);
    }
  }
  return result;
}
void idCollisionModelBuilder::CreateStatsForSubTree_r(const cmBuildNode_t& node,
                                                      cmBuildStats_t& stats) const {
  ++stats.nodes;
  stats.polygons += node.polygons.size();
  stats.polytopes += node.polytopes.size();
  if (!node.children[0] && !node.children[1]) {
    ++stats.leafs;
  }
  for (const auto& child : node.children) {
    if (child) {
      CreateStatsForSubTree_r(*child, stats);
    }
  }
}
cmBuildStats_t idCollisionModelBuilder::GetStatsFromNode(const cmBuildNode_t& node) const {
  cmBuildStats_t result;
  CreateStatsForSubTree_r(node, result);
  return result;
}
void idCollisionModelBuilder::CreateNodeStats_r(cmBuildNode_t& node,
                                                const idCollisionModelLocal& model) const {
  node.contents = CONTENTS_NONE;
  node.bounds.Clear();
  for (std::size_t index : node.polygons) {
    if (index < model.polygons.size()) {
      node.contents |= model.polygons[index].contents;
      node.bounds.AddPoint(model.polygons[index].bounds[0]);
      node.bounds.AddPoint(model.polygons[index].bounds[1]);
    }
  }
  for (std::size_t index : node.polytopes) {
    if (index < model.polytopes.size()) {
      node.contents |= model.polytopes[index].contents;
      node.bounds.AddPoint(model.polytopes[index].bounds[0]);
      node.bounds.AddPoint(model.polytopes[index].bounds[1]);
    }
  }
  for (auto& child : node.children) {
    if (child) {
      CreateNodeStats_r(*child, model);
      node.contents |= child->contents;
      node.bounds.AddPoint(child->bounds[0]);
      node.bounds.AddPoint(child->bounds[1]);
    }
  }
}

bool idCollisionModelBuilder::SplitterDividesPrimitives(const cmBuildNode_t& node, int axis,
                                                        float distance) const {
  return axis >= 0 && axis < 3 && node.bounds[0][axis] < distance &&
         node.bounds[1][axis] > distance &&
         (node.polygons.size() + node.polytopes.size() > 1);
}
bool idCollisionModelBuilder::FindSplitter(const cmBuildNode_t& node, int& axis,
                                           float& distance) const {
  const idVec3 size = node.bounds[1] - node.bounds[0];
  axis = size.x >= size.y && size.x >= size.z ? 0 : size.y >= size.z ? 1 : 2;
  distance = (node.bounds[0][axis] + node.bounds[1][axis]) * 0.5f;
  return SplitterDividesPrimitives(node, axis, distance);
}
void idCollisionModelBuilder::FilterPolygonIntoTree_r(
    cmBuildNode_t& node, std::size_t polygon, const idCollisionModelLocal& model) const {
  if (polygon >= model.polygons.size() || !node.children[0]) {
    AddPolygonToNode(node, polygon);
    return;
  }
  const cmPolygon_t& primitive = model.polygons[polygon];
  bool added = false;
  for (auto& child : node.children) {
    if (child && CM_BoundsIntersect(primitive.bounds, child->bounds)) {
      FilterPolygonIntoTree_r(*child, polygon, model);
      added = true;
    }
  }
  if (!added) {
    AddPolygonToNode(node, polygon);
  }
}
void idCollisionModelBuilder::FilterPolytopeIntoTree_r(
    cmBuildNode_t& node, std::size_t polytope, const idCollisionModelLocal& model) const {
  if (polytope >= model.polytopes.size() || !node.children[0]) {
    AddPolytopeToNode(node, polytope);
    return;
  }
  const cmPolytope_t& primitive = model.polytopes[polytope];
  bool added = false;
  for (auto& child : node.children) {
    if (child && CM_BoundsIntersect(primitive.bounds, child->bounds)) {
      FilterPolytopeIntoTree_r(*child, polytope, model);
      added = true;
    }
  }
  if (!added) {
    AddPolytopeToNode(node, polytope);
  }
}
void idCollisionModelBuilder::CreateAxialBSPTree_r(cmBuildNode_t& node,
                                                   const idCollisionModelLocal& model,
                                                   int depth) const {
  if (depth >= 16 || node.polygons.size() + node.polytopes.size() <= 4) {
    return;
  }
  int axis{};
  float distance{};
  if (!FindSplitter(node, axis, distance)) {
    return;
  }
  node.axis = axis;
  node.distance = distance;
  node.children[0] = AllocNode();
  node.children[1] = AllocNode();
  node.children[0]->bounds = node.bounds;
  node.children[1]->bounds = node.bounds;
  node.children[0]->bounds[1][axis] = distance;
  node.children[1]->bounds[0][axis] = distance;
  const auto polygons = std::move(node.polygons);
  const auto polytopes = std::move(node.polytopes);
  for (std::size_t polygon : polygons) {
    FilterPolygonIntoTree_r(node, polygon, model);
  }
  for (std::size_t polytope : polytopes) {
    FilterPolytopeIntoTree_r(node, polytope, model);
  }
  for (auto& child : node.children) {
    CreateAxialBSPTree_r(*child, model, depth + 1);
  }
}
std::unique_ptr<cmBuildNode_t>
idCollisionModelBuilder::CreateAxialBSPTree(const idCollisionModelLocal& model) const {
  auto root = AllocNode();
  root->bounds = model.bounds;
  for (std::size_t i = 0; i < model.polygons.size(); ++i) {
    root->polygons.push_back(i);
  }
  for (std::size_t i = 0; i < model.polytopes.size(); ++i) {
    root->polytopes.push_back(i);
  }
  CreateAxialBSPTree_r(*root, model, 0);
  CreateNodeStats_r(*root, model);
  return root;
}

void idCollisionModelBuilder::AddBuildNodePrimitivesToSubModelNode(
    const cmBuildNode_t& source, cmBuildNode_t& destination) const {
  for (std::size_t polygon : source.polygons) {
    AddPolygonToNode(destination, polygon);
  }
  for (std::size_t polytope : source.polytopes) {
    AddPolytopeToNode(destination, polytope);
  }
}
std::size_t idCollisionModelBuilder::CountModelTreeNodes_r(const cmBuildNode_t& node) const {
  return 1 + CM_R_CountChildren(node);
}
void idCollisionModelBuilder::SetupBuildGroups(cm_buildModel_t& build) const {
  if (!build.model) {
    return;
  }
  build.root = CreateAxialBSPTree(*build.model);
  build.stats = GetStatsFromNode(*build.root);
}

std::shared_ptr<idCollisionModelLocal> idCollisionModelBuilder::CreateSingleSubModel_r(
    const cmBuildNode_t& node, const idCollisionModelLocal& source, const char* name) const {
  auto result = std::make_shared<idCollisionModelLocal>();
  result->FreeData();
  result->name = name ? name : "submodel";
  result->type = source.type;
  result->vertices = source.vertices;
  result->edges = source.edges;
  for (std::size_t polygon : node.polygons) {
    if (polygon < source.polygons.size()) {
      result->polygons.push_back(source.polygons[polygon]);
    }
  }
  for (std::size_t polytope : node.polytopes) {
    if (polytope < source.polytopes.size()) {
      result->polytopes.push_back(source.polytopes[polytope]);
    }
  }
  FinishModel(*result);
  return result;
}
void idCollisionModelBuilder::CreateSubModels_r(
    const cmBuildNode_t& node, const idCollisionModelLocal& source,
    std::vector<std::shared_ptr<idCollisionModelLocal>>& output) const {
  if (!node.children[0] && !node.children[1]) {
    output.push_back(CreateSingleSubModel_r(node, source, "submodel"));
    return;
  }
  for (const auto& child : node.children) {
    if (child) {
      CreateSubModels_r(*child, source, output);
    }
  }
}
void idCollisionModelBuilder::AddSubModelsToCollisionModel(
    idCollisionModelLocal& destination,
    const std::vector<std::shared_ptr<idCollisionModelLocal>>& subModels) const {
  for (const auto& subModel : subModels) {
    if (subModel) {
      AddCollisionModelGeometry(destination, *subModel);
    }
  }
  FinishModel(destination);
}

void idCollisionModelBuilder::GenerateEdgeNormals_r(idCollisionModelLocal& model,
                                                    cmBuildNode_t& node) const {
  for (std::size_t polygonIndex : node.polygons) {
    if (polygonIndex >= model.polygons.size()) {
      continue;
    }
    const cmPolygon_t& polygon = model.polygons[polygonIndex];
    const idVec3 normal{polygon.plane.a, polygon.plane.b, polygon.plane.c};
    for (std::size_t edge = 0; edge < polygon.vertices.size(); ++edge) {
      const std::uint32_t first = polygon.vertices[edge];
      const std::uint32_t second = polygon.vertices[(edge + 1) % polygon.vertices.size()];
      for (cmEdge_t& candidate : model.edges) {
        if ((candidate.vertex[0] == first && candidate.vertex[1] == second) ||
            (candidate.vertex[0] == second && candidate.vertex[1] == first)) {
          candidate.normal += normal;
          candidate.normal.Normalize();
        }
      }
    }
  }
  for (auto& child : node.children) {
    if (child) {
      GenerateEdgeNormals_r(model, *child);
    }
  }
}

void idCollisionModelBuilder::ReplacePolygons(idCollisionModelLocal& model, std::size_t first,
                                              std::vector<cmPolygon_t> replacements) const {
  if (first >= model.polygons.size()) {
    model.polygons.insert(model.polygons.end(), std::make_move_iterator(replacements.begin()),
                          std::make_move_iterator(replacements.end()));
    return;
  }
  model.polygons.erase(model.polygons.begin() + first);
  model.polygons.insert(model.polygons.begin() + first,
                        std::make_move_iterator(replacements.begin()),
                        std::make_move_iterator(replacements.end()));
}
bool idCollisionModelBuilder::RemovePolygon(idCollisionModelLocal& model,
                                            std::size_t polygon) const {
  if (polygon >= model.polygons.size()) {
    return false;
  }
  model.polygons.erase(model.polygons.begin() + polygon);
  return true;
}
bool idCollisionModelBuilder::PointInsidePolygon(const idVec3& point,
                                                 const cmPolygon_t& polygon,
                                                 const idCollisionModelLocal& model,
                                                 float epsilon) const {
  if (!PointInsideBounds(point, polygon.bounds, epsilon) || polygon.vertices.size() < 3) {
    return false;
  }
  const idVec3 normal{polygon.plane.a, polygon.plane.b, polygon.plane.c};
  for (std::size_t i = 0; i < polygon.vertices.size(); ++i) {
    const idVec3& first = model.vertices[polygon.vertices[i]];
    const idVec3& second = model.vertices[polygon.vertices[(i + 1) % polygon.vertices.size()]];
    if (((second - first).Cross(point - first)) * normal < -epsilon) {
      return false;
    }
  }
  return true;
}

std::vector<std::size_t> idCollisionModelBuilder::FindInternalEdgesOnPolygon(
    const cmPolygon_t& polygon, const idCollisionModelLocal& model) const {
  std::vector<std::size_t> result;
  for (std::size_t edgeIndex = 0; edgeIndex < model.edges.size(); ++edgeIndex) {
    const cmEdge_t& edge = model.edges[edgeIndex];
    const bool first = std::find(polygon.vertices.begin(), polygon.vertices.end(), edge.vertex[0]) !=
                       polygon.vertices.end();
    const bool second = std::find(polygon.vertices.begin(), polygon.vertices.end(), edge.vertex[1]) !=
                        polygon.vertices.end();
    if (first && second) {
      bool boundary = false;
      for (std::size_t i = 0; i < polygon.vertices.size(); ++i) {
        const auto a = polygon.vertices[i];
        const auto b = polygon.vertices[(i + 1) % polygon.vertices.size()];
        boundary |= (a == edge.vertex[0] && b == edge.vertex[1]) ||
                    (a == edge.vertex[1] && b == edge.vertex[0]);
      }
      if (!boundary) {
        result.push_back(edgeIndex);
      }
    }
  }
  return result;
}
std::vector<std::size_t>
idCollisionModelBuilder::FindInternalPolygonEdges(const idCollisionModelLocal& model) const {
  std::set<std::size_t> unique;
  for (const cmPolygon_t& polygon : model.polygons) {
    const auto edges = FindInternalEdgesOnPolygon(polygon, model);
    unique.insert(edges.begin(), edges.end());
  }
  return {unique.begin(), unique.end()};
}
void idCollisionModelBuilder::FindInternalEdges(idCollisionModelLocal& model) const {
  for (std::size_t edge : FindInternalPolygonEdges(model)) {
    if (edge < model.edges.size()) {
      model.edges[edge].normal = {};
    }
  }
}

void idCollisionModelBuilder::OffsetPolygonEdges(cmPolygon_t& polygon,
                                                 idCollisionModelLocal& model,
                                                 float amount) const {
  const idVec3 normal{polygon.plane.a, polygon.plane.b, polygon.plane.c};
  for (std::uint32_t vertex : polygon.vertices) {
    if (vertex < model.vertices.size()) {
      model.vertices[vertex] += normal * amount;
    }
  }
  polygon.plane.d -= amount;
  polygon.bounds.Clear();
  for (std::uint32_t vertex : polygon.vertices) {
    polygon.bounds.AddPoint(model.vertices[vertex]);
  }
}
void idCollisionModelBuilder::OffsetPolygonEdges_r(cmBuildNode_t& node,
                                                   idCollisionModelLocal& model,
                                                   float amount) const {
  for (std::size_t polygon : node.polygons) {
    if (polygon < model.polygons.size()) {
      OffsetPolygonEdges(model.polygons[polygon], model, amount);
    }
  }
  for (auto& child : node.children) {
    if (child) {
      OffsetPolygonEdges_r(*child, model, amount);
    }
  }
}

bool idCollisionModelBuilder::TryMergePolygons(const cmPolygon_t& first,
                                               const cmPolygon_t& second,
                                               cmPolygon_t& merged,
                                               const idCollisionModelLocal& model) const {
  if (first.contents != second.contents || first.material != second.material ||
      std::fabs(first.plane.a - second.plane.a) > 0.001f ||
      std::fabs(first.plane.b - second.plane.b) > 0.001f ||
      std::fabs(first.plane.c - second.plane.c) > 0.001f ||
      std::fabs(first.plane.d - second.plane.d) > 0.01f) {
    return false;
  }
  if (first.vertices.size() < 3 || second.vertices.size() < 3) {
    return false;
  }

  std::size_t firstEdge = first.vertices.size();
  std::size_t secondEdge = second.vertices.size();
  for (std::size_t i = 0; i < first.vertices.size() && firstEdge == first.vertices.size(); ++i) {
    const std::uint32_t a = first.vertices[i];
    const std::uint32_t b = first.vertices[(i + 1) % first.vertices.size()];
    for (std::size_t j = 0; j < second.vertices.size(); ++j) {
      if (second.vertices[j] == b &&
          second.vertices[(j + 1) % second.vertices.size()] == a) {
        firstEdge = i;
        secondEdge = j;
        break;
      }
    }
  }
  if (firstEdge == first.vertices.size()) {
    return false;
  }

  merged = first;
  merged.vertices.clear();
  for (std::size_t i = 0; i < first.vertices.size(); ++i) {
    merged.vertices.push_back(first.vertices[(firstEdge + 1 + i) % first.vertices.size()]);
  }
  for (std::size_t i = 1; i + 1 < second.vertices.size(); ++i) {
    merged.vertices.push_back(second.vertices[(secondEdge + 1 + i) % second.vertices.size()]);
  }

  std::set<std::uint32_t> unique(merged.vertices.begin(), merged.vertices.end());
  if (unique.size() != merged.vertices.size()) {
    return false;
  }
  const idVec3 normal{merged.plane.a, merged.plane.b, merged.plane.c};
  for (std::size_t i = 0; i < merged.vertices.size(); ++i) {
    const std::uint32_t previous = merged.vertices[(i + merged.vertices.size() - 1) %
                                                    merged.vertices.size()];
    const std::uint32_t current = merged.vertices[i];
    const std::uint32_t next = merged.vertices[(i + 1) % merged.vertices.size()];
    if (previous >= model.vertices.size() || current >= model.vertices.size() ||
        next >= model.vertices.size() ||
        ((model.vertices[current] - model.vertices[previous])
             .Cross(model.vertices[next] - model.vertices[current]) *
         normal) < -0.001f) {
      return false;
    }
  }

  merged.bounds.Clear();
  for (std::uint32_t vertex : merged.vertices) {
    merged.bounds.AddPoint(model.vertices[vertex]);
  }
  return merged.vertices.size() >= 3;
}
bool idCollisionModelBuilder::MergePolygonWithTreePolygons(std::size_t polygon,
                                                           idCollisionModelLocal& model,
                                                           cmBuildNode_t& node) const {
  if (polygon >= model.polygons.size()) {
    return false;
  }
  for (std::size_t candidate : node.polygons) {
    if (candidate == polygon || candidate >= model.polygons.size()) {
      continue;
    }
    cmPolygon_t merged;
    if (TryMergePolygons(model.polygons[polygon], model.polygons[candidate], merged, model)) {
      model.polygons[polygon] = std::move(merged);
      RemovePolygon(model, candidate);
      return true;
    }
  }
  for (auto& child : node.children) {
    if (child && MergePolygonWithTreePolygons(polygon, model, *child)) {
      return true;
    }
  }
  return false;
}
void idCollisionModelBuilder::MergeTreePolygons(idCollisionModelLocal& model,
                                                cmBuildNode_t& node) const {
  for (std::size_t index = 0; index < model.polygons.size();) {
    if (!MergePolygonWithTreePolygons(index, model, node)) {
      ++index;
    }
  }
}
void idCollisionModelBuilder::MergeTreePolygonsWithCompatibleMaterials(
    idCollisionModelLocal& model, cmBuildNode_t& node) const {
  MergeTreePolygons(model, node);
}

bool idCollisionModelBuilder::ChoppedAwayByProcBSP_r(const idBounds& bounds,
                                                     const cmBuildNode_t& node) const {
  if (!CM_BoundsIntersect(bounds, node.bounds)) {
    return true;
  }
  if (!node.children[0] && !node.children[1]) {
    return false;
  }
  bool chopped = true;
  for (const auto& child : node.children) {
    if (child) {
      chopped &= ChoppedAwayByProcBSP_r(bounds, *child);
    }
  }
  return chopped;
}
bool idCollisionModelBuilder::ChoppedAwayByProcBSP(const idBounds& bounds,
                                                   const cmBuildNode_t& root) const {
  return ChoppedAwayByProcBSP_r(bounds, root);
}

std::vector<idFixedWinding> idCollisionModelBuilder::ChopWindingListWithPolytope(
    const std::vector<idFixedWinding>& windings, const cmPolytope_t& polytope) const {
  std::vector<idFixedWinding> result;
  for (const idFixedWinding& source : windings) {
    idFixedWinding winding = source;
    bool kept = true;
    for (const idPlane& plane : polytope.planes) {
      if (!winding.ClipInPlace(plane, 0.001f, true)) {
        kept = false;
        break;
      }
    }
    if (kept && winding.GetNumPoints() >= 3) {
      result.push_back(winding);
    }
  }
  return result;
}
std::vector<idFixedWinding> idCollisionModelBuilder::ChopWindingListWithTreePolytopes_r(
    const std::vector<idFixedWinding>& windings, const cmBuildNode_t& node,
    const idCollisionModelLocal& model) const {
  std::vector<idFixedWinding> result = windings;
  for (std::size_t polytope : node.polytopes) {
    if (polytope < model.polytopes.size()) {
      result = ChopWindingListWithPolytope(result, model.polytopes[polytope]);
    }
  }
  for (const auto& child : node.children) {
    if (child) {
      result = ChopWindingListWithTreePolytopes_r(result, *child, model);
    }
  }
  return result;
}
bool idCollisionModelBuilder::WindingOutsidePolytopes(
    const idFixedWinding& winding, const std::vector<cmPolytope_t>& polytopes) const {
  for (const cmPolytope_t& polytope : polytopes) {
    for (int point = 0; point < winding.GetNumPoints(); ++point) {
      if (PointInsideBounds(winding[point].ToVec3(), polytope.bounds, 0.001f)) {
        return false;
      }
    }
  }
  return true;
}

bool idCollisionModelBuilder::SplitPolygon(const cmPolygon_t& polygon, const idPlane& plane,
                                           idCollisionModelLocal& model,
                                           cmPolygon_t& front, cmPolygon_t& back) const {
  constexpr float epsilon = 0.001f;
  front = polygon;
  back = polygon;
  front.vertices.clear();
  back.vertices.clear();
  const auto distance = [&plane](const idVec3& point) {
    return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
  };
  for (std::size_t i = 0; i < polygon.vertices.size(); ++i) {
    const std::uint32_t firstIndex = polygon.vertices[i];
    const std::uint32_t secondIndex = polygon.vertices[(i + 1) % polygon.vertices.size()];
    const idVec3& first = model.vertices[firstIndex];
    const idVec3& second = model.vertices[secondIndex];
    const float firstDistance = distance(first);
    const float secondDistance = distance(second);
    if (firstDistance >= -epsilon) {
      front.vertices.push_back(firstIndex);
    }
    if (firstDistance <= epsilon) {
      back.vertices.push_back(firstIndex);
    }
    if ((firstDistance > epsilon && secondDistance < -epsilon) ||
        (firstDistance < -epsilon && secondDistance > epsilon)) {
      const float fraction = firstDistance / (firstDistance - secondDistance);
      const idVec3 intersection = first + (second - first) * fraction;
      auto found = std::find_if(model.vertices.begin(), model.vertices.end(),
                                [&intersection](const idVec3& vertex) {
                                  return (vertex - intersection).LengthSqr() < 1.0e-8f;
                                });
      std::uint32_t intersectionIndex;
      if (found == model.vertices.end()) {
        intersectionIndex = static_cast<std::uint32_t>(model.vertices.size());
        model.vertices.push_back(intersection);
      } else {
        intersectionIndex = static_cast<std::uint32_t>(found - model.vertices.begin());
      }
      front.vertices.push_back(intersectionIndex);
      back.vertices.push_back(intersectionIndex);
    }
  }

  front.bounds.Clear();
  for (std::uint32_t vertex : front.vertices) {
    front.bounds.AddPoint(model.vertices[vertex]);
  }
  back.bounds.Clear();
  for (std::uint32_t vertex : back.vertices) {
    back.bounds.AddPoint(model.vertices[vertex]);
  }
  return front.vertices.size() >= 3 && back.vertices.size() >= 3;
}

std::size_t
idCollisionModelBuilder::CalculateSubModelDataSize(const idCollisionModelLocal& model) const {
  return model.GetTotalMemory();
}
std::vector<std::uint8_t>
idCollisionModelBuilder::AllocSubModelData(const idCollisionModelLocal& model) const {
  const cmModelStats_t stats = model.GetPolytopeStats();
  std::vector<std::uint8_t> result(sizeof(stats));
  std::memcpy(result.data(), &stats, sizeof(stats));
  return result;
}
std::unique_ptr<cm_buildModel_t> idCollisionModelBuilder::AllocBuildModel() const {
  return std::make_unique<cm_buildModel_t>();
}

void idCollisionModelBuilder::AddMapModelEstimates(
    cm_buildModel_t& build, const idStaticModelGeometry& geometry) const {
  build.stats.polygons += geometry.indices.size() / 3;
  build.stats.polytopes += geometry.vertices.empty() ? 0 : 1;
}
void idCollisionModelBuilder::CollectSurfaces(
    const idCollisionModelGeometry& geometry,
    std::vector<idStaticModelGeometry>& output) const {
  output.insert(output.end(), geometry.surfaces.begin(), geometry.surfaces.end());
}

bool idCollisionModelBuilder::CreatePolygon(idCollisionModelLocal& model, const idVec3& first,
                                            const idVec3& second, const idVec3& third,
                                            std::uint32_t contents) const {
  idVec3 normal = (second - first).Cross(third - first);
  if (normal.Normalize() == 0.0f) {
    return false;
  }
  cmPolygon_t polygon;
  polygon.contents = contents;
  polygon.plane = {normal.x, normal.y, normal.z, -(normal * first)};
  polygon.bounds.Clear();
  const idVec3 points[3] = {first, second, third};
  for (const idVec3& point : points) {
    auto found = std::find_if(model.vertices.begin(), model.vertices.end(),
                              [&point](const idVec3& vertex) {
                                return (vertex - point).LengthSqr() < 1.0e-8f;
                              });
    const std::uint32_t index = found == model.vertices.end()
                                    ? static_cast<std::uint32_t>(model.vertices.size())
                                    : static_cast<std::uint32_t>(found - model.vertices.begin());
    if (found == model.vertices.end()) {
      model.vertices.push_back(point);
    }
    polygon.vertices.push_back(index);
    polygon.bounds.AddPoint(point);
  }
  for (int edge = 0; edge < 3; ++edge) {
    const std::uint32_t a = polygon.vertices[edge];
    const std::uint32_t b = polygon.vertices[(edge + 1) % 3];
    const auto found = std::find_if(model.edges.begin(), model.edges.end(), [a, b](const cmEdge_t& e) {
      return (e.vertex[0] == a && e.vertex[1] == b) ||
             (e.vertex[0] == b && e.vertex[1] == a);
    });
    if (found == model.edges.end()) {
      model.edges.push_back({{a, b}, normal});
    }
  }
  model.polygons.push_back(std::move(polygon));
  return true;
}

bool idCollisionModelBuilder::PolygonFromWinding(idCollisionModelLocal& model,
                                                 const idWinding& winding,
                                                 std::uint32_t contents) const {
  if (winding.GetNumPoints() < 3) {
    return false;
  }
  bool created = false;
  const idVec3 first = winding[0].ToVec3();
  for (int point = 2; point < winding.GetNumPoints(); ++point) {
    created |= CreatePolygon(model, first, winding[point - 1].ToVec3(),
                             winding[point].ToVec3(), contents);
  }
  return created;
}

void idCollisionModelBuilder::ConvertBrushSides(const idBounds& brush,
                                                idCollisionModelLocal& model,
                                                std::uint32_t contents) const {
  const auto temporary = BuildForBounds(brush, "brush", contents);
  AddCollisionModelGeometry(model, *temporary);
}
void idCollisionModelBuilder::ConvertBrush(const idBounds& brush,
                                           idCollisionModelLocal& model,
                                           std::uint32_t contents) const {
  ConvertBrushSides(brush, model, contents);
}
void idCollisionModelBuilder::ConvertMapModelPrimitives(
    const std::vector<idBounds>& brushes, idCollisionModelLocal& model) const {
  for (const idBounds& brush : brushes) {
    ConvertBrush(brush, model, CONTENTS_SOLID);
  }
}
void idCollisionModelBuilder::ConvertMapModelPolytopes(
    const std::vector<cmPolytope_t>& polytopes, idCollisionModelLocal& model) const {
  model.polytopes.insert(model.polytopes.end(), polytopes.begin(), polytopes.end());
}
void idCollisionModelBuilder::ConvertRenderModelSurfaces(
    const std::vector<idStaticModelGeometry>& surfaces, idCollisionModelLocal& model) const {
  for (const idStaticModelGeometry& surface : surfaces) {
    for (std::size_t index = 0; index + 2 < surface.indices.size(); index += 3) {
      const auto a = surface.indices[index];
      const auto b = surface.indices[index + 1];
      const auto c = surface.indices[index + 2];
      if (a < surface.vertices.size() && b < surface.vertices.size() && c < surface.vertices.size()) {
        CreatePolygon(model, surface.vertices[a], surface.vertices[b], surface.vertices[c],
                      surface.contents);
      }
    }
  }
}

void idCollisionModelBuilder::AddCollisionModelGeometry(
    idCollisionModelLocal& destination, const idCollisionModelLocal& source) const {
  const std::uint32_t vertexBase = static_cast<std::uint32_t>(destination.vertices.size());
  destination.vertices.insert(destination.vertices.end(), source.vertices.begin(),
                              source.vertices.end());
  for (cmEdge_t edge : source.edges) {
    edge.vertex[0] += vertexBase;
    edge.vertex[1] += vertexBase;
    destination.edges.push_back(edge);
  }
  for (cmPolygon_t polygon : source.polygons) {
    for (std::uint32_t& vertex : polygon.vertices) {
      vertex += vertexBase;
    }
    destination.polygons.push_back(std::move(polygon));
  }
  destination.polytopes.insert(destination.polytopes.end(), source.polytopes.begin(),
                               source.polytopes.end());
  destination.bounds.AddPoint(source.bounds[0]);
  destination.bounds.AddPoint(source.bounds[1]);
}
void idCollisionModelBuilder::MergeModelTrees(cmBuildNode_t& destination,
                                              const cmBuildNode_t& source) const {
  AddBuildNodePrimitivesToSubModelNode(source, destination);
  destination.contents |= source.contents;
  destination.bounds.AddPoint(source.bounds[0]);
  destination.bounds.AddPoint(source.bounds[1]);
  for (int side = 0; side < 2; ++side) {
    if (source.children[side]) {
      if (!destination.children[side]) {
        destination.children[side] = AllocNode();
      }
      MergeModelTrees(*destination.children[side], *source.children[side]);
    }
  }
}

std::shared_ptr<idCollisionModelLocal> idCollisionModelBuilder::BuildForMapFile(
    const std::vector<idStaticModelGeometry>& surfaces, const char* name) const {
  idCollisionModelGeometry geometry;
  geometry.surfaces = surfaces;
  return BuildCollisionModelForGeometry(geometry, name ? name : "mapFile");
}

std::shared_ptr<idCollisionModelLocal> idCollisionModelBuilder::BuildForStaticGeometry(
    const idStaticModelGeometry& geometry, const char* name) const {
  idCollisionModelGeometry modelGeometry;
  modelGeometry.surfaces.push_back(geometry);
  return BuildCollisionModelForGeometry(modelGeometry, name ? name : "staticGeometry");
}

std::shared_ptr<idCollisionModelLocal> idCollisionModelBuilder::BuildForStaticModel(
    const idStaticModelGeometry& geometry, const char* name) const {
  return BuildForStaticGeometry(geometry, name ? name : "staticModel");
}

std::shared_ptr<idCollisionModelLocal> idCollisionModelBuilder::BuildForRenderModelSurface(
    const std::vector<idVec3>& vertices, const std::vector<std::uint32_t>& indices,
    const char* name, std::uint32_t contents) const {
  idStaticModelGeometry geometry;
  geometry.vertices = vertices;
  geometry.indices = indices;
  geometry.contents = contents;
  return BuildForStaticGeometry(geometry, name ? name : "renderModelSurface");
}

std::shared_ptr<idCollisionModelLocal>
idCollisionModelBuilder::BuildCollisionModelForGeometry(
    const idCollisionModelGeometry& geometry, const char* name) const {
  std::vector<idStaticModelGeometry> surfaces;
  CollectSurfaces(geometry, surfaces);

  auto build = AllocBuildModel();
  build->model->FreeData();
  build->model->name = name ? name : "geometry";
  build->model->type = CM_TYPE_POLYGON;

  for (const idStaticModelGeometry& surface : surfaces) {
    AddMapModelEstimates(*build, surface);
  }
  ConvertRenderModelSurfaces(surfaces, *build->model);
  FinishModel(*build->model);

  if (!build->model->vertices.empty()) {
    SetupHash(*build);
    SetupBuildGroups(*build);
    FindInternalEdges(*build->model);
    if (build->root) {
      GenerateEdgeNormals_r(*build->model, *build->root);
    }
    SetupSubModelData(*build);
    ShutdownHash(*build);
  }

  return build->model;
}

std::shared_ptr<idCollisionModelLocal>
idCollisionModelBuilder::BuildForTrm(const idTraceModel& traceModel, const char* modelName,
                                     std::uint32_t contents) const {
  auto model = std::make_shared<idCollisionModelLocal>();
  model->FreeData();
  model->name = modelName ? modelName : "traceModel";
  model->type = CM_TYPE_POLYGON;
  model->bounds = traceModel.bounds;
  model->vertices.reserve(traceModel.numVerts);
  for (std::uint32_t i = 0; i < traceModel.numVerts; ++i) {
    model->vertices.push_back(traceModel.GetVertex(i));
  }
  model->edges.reserve(traceModel.numEdges);
  for (std::uint32_t i = 0; i < traceModel.numEdges; ++i) {
    cmEdge_t edge;
    edge.vertex[0] = traceModel.edges[i].v[0];
    edge.vertex[1] = traceModel.edges[i].v[1];
    edge.normal = {traceModel.edgeNormalX[i], traceModel.edgeNormalY[i], traceModel.edgeNormalZ[i]};
    model->edges.push_back(edge);
  }
  for (std::uint32_t polygonIndex = 0; polygonIndex < traceModel.numPolys; ++polygonIndex) {
    cmPolygon_t polygon;
    polygon.plane = {traceModel.polyPlaneX[polygonIndex], traceModel.polyPlaneY[polygonIndex],
                     traceModel.polyPlaneZ[polygonIndex], traceModel.polyPlaneW[polygonIndex]};
    polygon.contents = contents;
    polygon.bounds.Clear();
    for (std::uint32_t edgeIndex = 0; edgeIndex < traceModel.numPolyEdges[polygonIndex];
         ++edgeIndex) {
      const std::uint8_t encoded = traceModel.polyEdges[polygonIndex][edgeIndex];
      const std::uint32_t index = encoded & 0x7Fu;
      if (index >= traceModel.numEdges) {
        continue;
      }
      const std::uint32_t vertex = traceModel.edges[index].v[(encoded & 0x80u) ? 1 : 0];
      if (vertex < model->vertices.size()) {
        polygon.vertices.push_back(vertex);
        polygon.bounds.AddPoint(model->vertices[vertex]);
      }
    }
    if (polygon.vertices.size() >= 3) {
      model->polygons.push_back(std::move(polygon));
    }
  }
  cmPolytope_t polytope;
  polytope.bounds = traceModel.bounds;
  polytope.contents = contents;
  for (std::uint32_t i = 0; i < traceModel.numPolys; ++i) {
    polytope.planes.push_back({traceModel.polyPlaneX[i], traceModel.polyPlaneY[i],
                               traceModel.polyPlaneZ[i], traceModel.polyPlaneW[i]});
  }
  model->polytopes.push_back(std::move(polytope));
  FinishModel(*model);
  return model;
}

std::shared_ptr<idCollisionModelLocal>
idCollisionModelBuilder::BuildForBounds(const idBounds& bounds, const char* name,
                                        std::uint32_t contents) const {
  idTraceModel traceModel(bounds);
  return BuildForTrm(traceModel, name, contents);
}

std::shared_ptr<idCollisionModelLocal>
idCollisionModelBuilder::BuildForGrid(const idBounds& bounds, const char* name) const {
  auto model = BuildForBounds(bounds, name, CONTENTS_SOLID);
  model->type = CM_TYPE_GRID;
  return model;
}

bool idCollisionModelBuilder::IsStaticRenderModel(const char* name) const {
  if (!name) {
    return false;
  }
  std::string lower(name);
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  return lower.find(".lwo") != std::string::npos || lower.find(".ase") != std::string::npos ||
         lower.find(".obj") != std::string::npos;
}

void idCollisionModelBuilder::FinishModel(idCollisionModelLocal& model) const {
  if (model.vertices.empty() && model.type != CM_TYPE_SPHERE) {
    return;
  }
  if (model.type != CM_TYPE_SPHERE) {
    model.bounds.Clear();
    for (const idVec3& vertex : model.vertices) {
      model.bounds.AddPoint(vertex);
    }
  }
}
