#include "engine/cm/collisionmodel.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>

namespace {
constexpr std::array<char, 8> kCollisionMagic{'W', 'T', 'N', 'O', 'C', 'M', '1', '\0'};

template <typename T> bool ReadValue(std::istream& stream, T& value) {
  return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
}

template <typename T> void WriteValue(std::ostream& stream, const T& value) {
  stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}

std::uint32_t Swap32(std::uint32_t value) {
  return (value >> 24) | ((value >> 8) & 0x0000FF00u) | ((value << 8) & 0x00FF0000u) |
         (value << 24);
}

float SwapFloat(float value) {
  std::uint32_t bits{};
  std::memcpy(&bits, &value, sizeof(bits));
  bits = Swap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}
} // namespace

idCollisionModel::~idCollisionModel() = default;

bool CM_BoundsIntersect(const idBounds& first, const idBounds& second) {
  for (int axis = 0; axis < 3; ++axis) {
    if (first[1][axis] < second[0][axis] || first[0][axis] > second[1][axis]) {
      return false;
    }
  }
  return true;
}

idBounds CM_TranslatedBounds(const idBounds& bounds, const idVec3& origin) {
  idBounds result = bounds;
  result[0] += origin;
  result[1] += origin;
  return result;
}

idCollisionModelLocal::idCollisionModelLocal() {
  MakeDefault();
  auto& resources = const_cast<std::vector<idCollisionModelLocal*>&>(GetResourceList());
  resources.push_back(this);
}

const idBounds& idCollisionModelLocal::GetBounds() const {
  return bounds;
}

std::size_t idCollisionModelLocal::GetLoadedMemory() const {
  return GetTotalMemory();
}

std::size_t idCollisionModelLocal::GetMaxResidentMemory() const {
  return GetTotalMemory();
}

idBounds idCollisionModelLocal::GetBox() const {
  return bounds;
}

int idCollisionModelLocal::GetJoint(std::size_t index) const {
  return index < polygons.size() ? polygons[index].material : -1;
}

const std::vector<idCollisionModelLocal*>& idCollisionModelLocal::GetResourceList() {
  static std::vector<idCollisionModelLocal*> resources;
  return resources;
}

bool idCollisionModelLocal::ValidateStreamFileHeader(const void* data, std::size_t size) {
  return data && size >= kCollisionMagic.size() &&
         std::memcmp(data, kCollisionMagic.data(), kCollisionMagic.size()) == 0;
}

bool idCollisionModelLocal::ReloadIfStale() {
  return !name.IsEmpty() && (type == CM_TYPE_SPHERE || !vertices.empty());
}

bool idCollisionModelLocal::LoadResource(const char* path) {
  return Load_Binary(path);
}

void idCollisionModelLocal::MakeDefault() {
  FreeData();
  name = "default";
  type = CM_TYPE_POLYGON;
  bounds[0] = {-8.0f, -8.0f, -8.0f};
  bounds[1] = {8.0f, 8.0f, 8.0f};
}

void idCollisionModelLocal::FreeData() {
  vertices.clear();
  edges.clear();
  polygons.clear();
  polytopes.clear();
  sphereRadius = 0.0f;
  bounds.Clear();
}

std::uint32_t idCollisionModelLocal::GetContents() const {
  std::uint32_t result = CONTENTS_NONE;
  for (const cmPolygon_t& polygon : polygons) {
    result |= polygon.contents;
  }
  for (const cmPolytope_t& polytope : polytopes) {
    result |= polytope.contents;
  }
  return result == CONTENTS_NONE ? CONTENTS_SOLID : result;
}

const idVec3* idCollisionModelLocal::GetVertex(std::size_t index) const {
  return index < vertices.size() ? &vertices[index] : nullptr;
}

const cmEdge_t* idCollisionModelLocal::GetEdge(std::size_t index) const {
  return index < edges.size() ? &edges[index] : nullptr;
}

const cmPolygon_t* idCollisionModelLocal::GetPolygon(std::size_t index) const {
  return index < polygons.size() ? &polygons[index] : nullptr;
}

const cmPolytope_t* idCollisionModelLocal::GetPolytope(std::size_t index) const {
  return index < polytopes.size() ? &polytopes[index] : nullptr;
}

std::size_t idCollisionModelLocal::GetPolytopes(const idBounds& query, const cmPolytope_t** output,
                                                std::size_t capacity) const {
  std::size_t count = 0;
  for (const cmPolytope_t& polytope : polytopes) {
    if (CM_BoundsIntersect(query, polytope.bounds)) {
      if (output && count < capacity) {
        output[count] = &polytope;
      }
      ++count;
    }
  }
  return count;
}

cmModelStats_t idCollisionModelLocal::GetPolytopeStats() const {
  cmModelStats_t result;
  result.vertices = vertices.size();
  result.edges = edges.size();
  result.polygons = polygons.size();
  result.polytopes = polytopes.size();
  result.bytes = GetTotalMemory();
  return result;
}

std::size_t idCollisionModelLocal::GetTotalMemory() const {
  std::size_t result =
      sizeof(*this) + vertices.capacity() * sizeof(idVec3) + edges.capacity() * sizeof(cmEdge_t) +
      polygons.capacity() * sizeof(cmPolygon_t) + polytopes.capacity() * sizeof(cmPolytope_t);
  for (const cmPolygon_t& polygon : polygons) {
    result += polygon.vertices.capacity() * sizeof(std::uint32_t);
  }
  for (const cmPolytope_t& polytope : polytopes) {
    result += polytope.planes.capacity() * sizeof(idPlane);
  }
  return result;
}

bool idCollisionModelLocal::Write_Binary(const char* path) const {
  if (!path) {
    return false;
  }
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    return false;
  }
  stream.write(kCollisionMagic.data(), kCollisionMagic.size());
  const std::uint32_t nameLength = static_cast<std::uint32_t>(idStr::Length(name.data));
  const std::uint32_t vertexCount = static_cast<std::uint32_t>(vertices.size());
  const std::uint32_t edgeCount = static_cast<std::uint32_t>(edges.size());
  const std::uint32_t polygonCount = static_cast<std::uint32_t>(polygons.size());
  WriteValue(stream, nameLength);
  stream.write(name.data, nameLength);
  WriteValue(stream, type);
  WriteValue(stream, bounds);
  WriteValue(stream, sphereRadius);
  WriteValue(stream, vertexCount);
  WriteValue(stream, edgeCount);
  WriteValue(stream, polygonCount);
  stream.write(reinterpret_cast<const char*>(vertices.data()), vertices.size() * sizeof(idVec3));
  stream.write(reinterpret_cast<const char*>(edges.data()), edges.size() * sizeof(cmEdge_t));
  for (const cmPolygon_t& polygon : polygons) {
    WriteValue(stream, polygon.plane);
    WriteValue(stream, polygon.bounds);
    WriteValue(stream, polygon.contents);
    WriteValue(stream, polygon.material);
    const std::uint32_t count = static_cast<std::uint32_t>(polygon.vertices.size());
    WriteValue(stream, count);
    stream.write(reinterpret_cast<const char*>(polygon.vertices.data()),
                 polygon.vertices.size() * sizeof(std::uint32_t));
  }
  return static_cast<bool>(stream);
}

bool idCollisionModelLocal::Load_Binary(const char* path) {
  if (!path) {
    return false;
  }
  std::ifstream stream(path, std::ios::binary);
  std::array<char, 8> magic{};
  if (!stream.read(magic.data(), magic.size()) || magic != kCollisionMagic) {
    return false;
  }
  std::uint32_t nameLength{}, vertexCount{}, edgeCount{}, polygonCount{};
  if (!ReadValue(stream, nameLength) || nameLength > 4096) {
    return false;
  }
  std::vector<char> modelName(nameLength + 1);
  if (!stream.read(modelName.data(), nameLength) || !ReadValue(stream, type) ||
      !ReadValue(stream, bounds) || !ReadValue(stream, sphereRadius) ||
      !ReadValue(stream, vertexCount) || !ReadValue(stream, edgeCount) ||
      !ReadValue(stream, polygonCount) || vertexCount > (1u << 24) || edgeCount > (1u << 24) ||
      polygonCount > (1u << 22)) {
    return false;
  }
  name = modelName.data();
  vertices.resize(vertexCount);
  edges.resize(edgeCount);
  polygons.resize(polygonCount);
  if (!stream.read(reinterpret_cast<char*>(vertices.data()), vertices.size() * sizeof(idVec3)) ||
      !stream.read(reinterpret_cast<char*>(edges.data()), edges.size() * sizeof(cmEdge_t))) {
    FreeData();
    return false;
  }
  for (cmPolygon_t& polygon : polygons) {
    std::uint32_t count{};
    if (!ReadValue(stream, polygon.plane) || !ReadValue(stream, polygon.bounds) ||
        !ReadValue(stream, polygon.contents) || !ReadValue(stream, polygon.material) ||
        !ReadValue(stream, count) || count > (1u << 20)) {
      FreeData();
      return false;
    }
    polygon.vertices.resize(count);
    if (!stream.read(reinterpret_cast<char*>(polygon.vertices.data()),
                     polygon.vertices.size() * sizeof(std::uint32_t))) {
      FreeData();
      return false;
    }
  }
  return true;
}

bool SetupStreamAreaPtrs(const void* data, std::size_t size, std::vector<cmStreamArea_t>& areas) {
  areas.clear();
  if (!idCollisionModelLocal::ValidateStreamFileHeader(data, size)) {
    return false;
  }
  areas.push_back({static_cast<const std::uint8_t*>(data), size});
  return true;
}

std::shared_ptr<std::vector<std::uint8_t>> AcquireSubModelData(const void* data, std::size_t size) {
  if (!data && size != 0) {
    return {};
  }
  auto result = std::make_shared<std::vector<std::uint8_t>>(size);
  if (size != 0) {
    std::memcpy(result->data(), data, size);
  }
  return result;
}

void ReleaseSubModelData(std::shared_ptr<std::vector<std::uint8_t>>& data) {
  data.reset();
}

void SwapModelTreeNodes(std::vector<std::uint32_t>& nodes) {
  for (std::uint32_t& node : nodes) {
    node = Swap32(node);
  }
}

void SwapSubModelHeader(cmModelStats_t& header) {
  header.vertices = Swap32(static_cast<std::uint32_t>(header.vertices));
  header.edges = Swap32(static_cast<std::uint32_t>(header.edges));
  header.polygons = Swap32(static_cast<std::uint32_t>(header.polygons));
  header.polytopes = Swap32(static_cast<std::uint32_t>(header.polytopes));
  header.bytes = Swap32(static_cast<std::uint32_t>(header.bytes));
}

void SwapSubModelData(idCollisionModelLocal& model) {
  for (idVec3& vertex : model.vertices) {
    vertex = {SwapFloat(vertex.x), SwapFloat(vertex.y), SwapFloat(vertex.z)};
  }
  for (cmEdge_t& edge : model.edges) {
    edge.vertex[0] = Swap32(edge.vertex[0]);
    edge.vertex[1] = Swap32(edge.vertex[1]);
    edge.normal = {SwapFloat(edge.normal.x), SwapFloat(edge.normal.y), SwapFloat(edge.normal.z)};
  }
  for (cmPolygon_t& polygon : model.polygons) {
    polygon.plane = {SwapFloat(polygon.plane.a), SwapFloat(polygon.plane.b),
                     SwapFloat(polygon.plane.c), SwapFloat(polygon.plane.d)};
    polygon.contents = Swap32(polygon.contents);
    for (std::uint32_t& vertex : polygon.vertices) {
      vertex = Swap32(vertex);
    }
  }
}

void SwapStreamAreas(std::vector<cmStreamArea_t>& areas) {
  for (cmStreamArea_t& area : areas) {
    const std::uint64_t value = static_cast<std::uint64_t>(area.size);
    area.size = static_cast<std::size_t>(
        (static_cast<std::uint64_t>(Swap32(static_cast<std::uint32_t>(value))) << 32) |
        Swap32(static_cast<std::uint32_t>(value >> 32)));
  }
}

void SwapBigSphereModelStruct(idCollisionModelLocal& model) {
  model.sphereRadius = SwapFloat(model.sphereRadius);
  model.bounds[0] = {SwapFloat(model.bounds[0].x), SwapFloat(model.bounds[0].y),
                     SwapFloat(model.bounds[0].z)};
  model.bounds[1] = {SwapFloat(model.bounds[1].x), SwapFloat(model.bounds[1].y),
                     SwapFloat(model.bounds[1].z)};
}

void SwapBigSphereModelData(std::vector<float>& sphereData) {
  for (float& value : sphereData) {
    value = SwapFloat(value);
  }
}
