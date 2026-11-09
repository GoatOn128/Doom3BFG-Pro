#include "shared/idlib/geometry/tracemodel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <set>
#include <utility>
#include <vector>

namespace {

constexpr float kEpsilon = 1.0e-5f;
constexpr float kPi = 3.14159265358979323846f;

idVec3 ComponentMultiply(const idVec3& a, const idVec3& b) {
  return {a.x * b.x, a.y * b.y, a.z * b.z};
}

idVec3 BoundsCenter(const idBounds& bounds) {
  return (bounds[0] + bounds[1]) * 0.5f;
}

void ResetBounds(idTraceModel& model) {
  const float inf = std::numeric_limits<float>::infinity();
  model.bounds[0] = {inf, inf, inf};
  model.bounds[1] = {-inf, -inf, -inf};
  for (unsigned i = 0; i < model.numVerts; ++i) {
    const idVec3 v = model.GetVertex(i);
    for (int axis = 0; axis < 3; ++axis) {
      model.bounds[0][axis] = std::min(model.bounds[0][axis], v[axis]);
      model.bounds[1][axis] = std::max(model.bounds[1][axis], v[axis]);
    }
  }
  if (model.numVerts == 0) {
    model.bounds[0] = model.bounds[1] = {0.0f, 0.0f, 0.0f};
  }
  model.offset = BoundsCenter(model.bounds);
  model.radius = 0.0f;
  for (unsigned i = 0; i < model.numVerts; ++i) {
    model.radius = std::max(model.radius, (model.GetVertex(i) - model.offset).Length());
  }
}

unsigned EdgeIndex(std::uint8_t encoded) {
  return encoded & 0x7Fu;
}
bool EdgeReversed(std::uint8_t encoded) {
  return (encoded & 0x80u) != 0;
}

std::pair<unsigned, unsigned> OrientedEdge(const idTraceModel& model, std::uint8_t encoded) {
  const traceModelEdge_t& edge = model.edges[EdgeIndex(encoded)];
  if (EdgeReversed(encoded)) {
    return {edge.v[1], edge.v[0]};
  }
  return {edge.v[0], edge.v[1]};
}

void BuildFromFaces(idTraceModel& model, traceModel_t type, const std::vector<idVec3>& vertices,
                    std::vector<std::vector<unsigned>> faces) {
  model = idTraceModel{};
  model.type = type;
  model.numVerts =
      static_cast<std::uint32_t>(std::min<std::size_t>(vertices.size(), idTraceModel::MaxVerts));
  for (unsigned i = 0; i < model.numVerts; ++i) {
    model.SetVertex(i, vertices[i]);
  }

  idVec3 center{};
  for (unsigned i = 0; i < model.numVerts; ++i)
    center += model.GetVertex(i);
  if (model.numVerts != 0)
    center *= 1.0f / static_cast<float>(model.numVerts);

  model.numPolys =
      static_cast<std::uint32_t>(std::min<std::size_t>(faces.size(), idTraceModel::MaxPolys));
  for (unsigned p = 0; p < model.numPolys; ++p) {
    auto& face = faces[p];
    if (face.size() >= 3) {
      const idVec3 a = model.GetVertex(face[0]);
      const idVec3 b = model.GetVertex(face[1]);
      const idVec3 c = model.GetVertex(face[2]);
      const idVec3 normal = (b - a).Cross(c - a);
      if (normal * (a - center) < 0.0f)
        std::reverse(face.begin(), face.end());
    }
    const unsigned edgeCount =
        static_cast<unsigned>(std::min<std::size_t>(face.size(), idTraceModel::MaxPolyEdges));
    model.numPolyEdges[p] = edgeCount;
    model.maxPolyEdges = std::max(model.maxPolyEdges, edgeCount);
    for (unsigned e = 0; e < edgeCount; ++e) {
      const unsigned a = face[e];
      const unsigned b = face[(e + 1) % edgeCount];
      unsigned edgeIndex = 0;
      bool reverse = false;
      for (; edgeIndex < model.numEdges; ++edgeIndex) {
        if (model.edges[edgeIndex].v[0] == a && model.edges[edgeIndex].v[1] == b) {
          break;
        }
        if (model.edges[edgeIndex].v[0] == b && model.edges[edgeIndex].v[1] == a) {
          reverse = true;
          break;
        }
      }
      if (edgeIndex == model.numEdges) {
        if (model.numEdges >= idTraceModel::MaxEdges)
          continue;
        model.edges[edgeIndex].v[0] = static_cast<std::uint16_t>(a);
        model.edges[edgeIndex].v[1] = static_cast<std::uint16_t>(b);
        ++model.numEdges;
      }
      model.polyEdges[p][e] = static_cast<std::uint8_t>(edgeIndex | (reverse ? 0x80u : 0u));
    }
  }
  model.CalculatePolygonPlanes();
  model.GenerateEdgeNormals();
  ResetBounds(model);
  model.TestConvexity();
  model.ClearUnused();
}

std::vector<std::vector<unsigned>> ConvexFaces(const std::vector<idVec3>& vertices) {
  std::set<std::vector<unsigned>> uniqueFaces;
  const unsigned count = static_cast<unsigned>(vertices.size());
  for (unsigned i = 0; i < count; ++i) {
    for (unsigned j = i + 1; j < count; ++j) {
      for (unsigned k = j + 1; k < count; ++k) {
        idVec3 normal = (vertices[j] - vertices[i]).Cross(vertices[k] - vertices[i]);
        if (normal.Normalize() < kEpsilon)
          continue;
        float minDistance = 0.0f;
        float maxDistance = 0.0f;
        for (const idVec3& v : vertices) {
          const float d = normal * (v - vertices[i]);
          minDistance = std::min(minDistance, d);
          maxDistance = std::max(maxDistance, d);
        }
        if (minDistance < -kEpsilon && maxDistance > kEpsilon)
          continue;
        std::vector<unsigned> face;
        for (unsigned n = 0; n < count; ++n) {
          if (std::fabs(normal * (vertices[n] - vertices[i])) < 2.0e-4f) {
            face.push_back(n);
          }
        }
        if (face.size() < 3 || face.size() > idTraceModel::MaxPolyEdges)
          continue;
        std::vector<unsigned> key = face;
        std::sort(key.begin(), key.end());
        if (!uniqueFaces.insert(key).second)
          continue;

        idVec3 faceCenter{};
        for (unsigned n : face)
          faceCenter += vertices[n];
        faceCenter *= 1.0f / static_cast<float>(face.size());
        idVec3 u = vertices[face[0]] - faceCenter;
        u.Normalize();
        idVec3 outward = normal;
        if (outward * faceCenter < 0.0f)
          outward = -outward;
        idVec3 vAxis = outward.Cross(u);
        std::sort(face.begin(), face.end(), [&](unsigned lhs, unsigned rhs) {
          const idVec3 dl = vertices[lhs] - faceCenter;
          const idVec3 dr = vertices[rhs] - faceCenter;
          return std::atan2(dl * vAxis, dl * u) < std::atan2(dr * vAxis, dr * u);
        });
        const idVec3 orderedNormal =
            (vertices[face[1]] - vertices[face[0]]).Cross(vertices[face[2]] - vertices[face[1]]);
        if (orderedNormal * outward < 0.0f) {
          std::reverse(face.begin(), face.end());
        }
        uniqueFaces.erase(key);
        uniqueFaces.insert(face);
      }
    }
  }
  return {uniqueFaces.begin(), uniqueFaces.end()};
}

void AccumulateTriangle(const idVec3& a, const idVec3& b, const idVec3& c, double& volume,
                        std::array<double, 3>& first, std::array<double, 3>& second,
                        std::array<double, 3>& products) {
  const double v = static_cast<double>(a * b.Cross(c)) / 6.0;
  volume += v;
  const double p[3][3] = {{a.x, a.y, a.z}, {b.x, b.y, b.z}, {c.x, c.y, c.z}};
  for (int axis = 0; axis < 3; ++axis) {
    first[axis] += v * (p[0][axis] + p[1][axis] + p[2][axis]) / 4.0;
    second[axis] += v *
                    (p[0][axis] * p[0][axis] + p[1][axis] * p[1][axis] + p[2][axis] * p[2][axis] +
                     p[0][axis] * p[1][axis] + p[0][axis] * p[2][axis] + p[1][axis] * p[2][axis]) /
                    10.0;
  }
  constexpr int axes[3][2] = {{0, 1}, {1, 2}, {2, 0}};
  for (int product = 0; product < 3; ++product) {
    const int x = axes[product][0];
    const int y = axes[product][1];
    double sum = 2.0 * (p[0][x] * p[0][y] + p[1][x] * p[1][y] + p[2][x] * p[2][y]);
    for (int i = 0; i < 3; ++i) {
      for (int j = i + 1; j < 3; ++j) {
        sum += p[i][x] * p[j][y] + p[j][x] * p[i][y];
      }
    }
    products[product] += v * sum / 20.0;
  }
}

} // namespace

void* idTraceModel::operator new(std::size_t size) {
  return ::operator new(size);
}
void idTraceModel::operator delete(void* memory) noexcept {
  ::operator delete(memory);
}

idVec3 idTraceModel::GetVertex(unsigned index) const {
  return {vertsX[index], vertsY[index], vertsZ[index]};
}

void idTraceModel::SetVertex(unsigned index, const idVec3& vertex) {
  vertsX[index] = vertex.x;
  vertsY[index] = vertex.y;
  vertsZ[index] = vertex.z;
}

void idTraceModel::ClearUnused() {
  std::fill(vertsX + numVerts, vertsX + MaxVerts, 0.0f);
  std::fill(vertsY + numVerts, vertsY + MaxVerts, 0.0f);
  std::fill(vertsZ + numVerts, vertsZ + MaxVerts, 0.0f);
  std::fill(edgeNormalX + numEdges, edgeNormalX + MaxEdges, 0.0f);
  std::fill(edgeNormalY + numEdges, edgeNormalY + MaxEdges, 0.0f);
  std::fill(edgeNormalZ + numEdges, edgeNormalZ + MaxEdges, 0.0f);
  std::fill(polyPlaneX + numPolys, polyPlaneX + MaxPolys, 0.0f);
  std::fill(polyPlaneY + numPolys, polyPlaneY + MaxPolys, 0.0f);
  std::fill(polyPlaneZ + numPolys, polyPlaneZ + MaxPolys, 0.0f);
  std::fill(polyPlaneW + numPolys, polyPlaneW + MaxPolys, 0.0f);
  std::memset(edges + numEdges, 0, sizeof(traceModelEdge_t) * (MaxEdges - numEdges));
  maxPolyEdges = 0;
  for (unsigned p = 0; p < MaxPolys; ++p) {
    if (p >= numPolys)
      numPolyEdges[p] = 0;
    const unsigned used = p < numPolys ? numPolyEdges[p] : 0;
    maxPolyEdges = std::max(maxPolyEdges, used);
    std::fill(polyEdges[p] + used, polyEdges[p] + MaxPolyEdges, static_cast<std::uint8_t>(0));
  }
}

bool idTraceModel::IsClosedSurface() const {
  std::array<unsigned, MaxEdges> users{};
  for (unsigned p = 0; p < numPolys; ++p) {
    for (unsigned e = 0; e < numPolyEdges[p]; ++e)
      ++users[EdgeIndex(polyEdges[p][e])];
  }
  return std::all_of(users.begin(), users.begin() + numEdges,
                     [](unsigned count) { return count == 2; });
}

void idTraceModel::CalculatePolygonPlanes() {
  for (unsigned p = 0; p < numPolys; ++p) {
    if (numPolyEdges[p] < 3)
      continue;
    const idVec3 a = GetVertex(OrientedEdge(*this, polyEdges[p][0]).first);
    const idVec3 b = GetVertex(OrientedEdge(*this, polyEdges[p][1]).first);
    const idVec3 c = GetVertex(OrientedEdge(*this, polyEdges[p][2]).first);
    idVec3 normal = (b - a).Cross(c - a);
    normal.Normalize();
    polyPlaneX[p] = normal.x;
    polyPlaneY[p] = normal.y;
    polyPlaneZ[p] = normal.z;
    polyPlaneW[p] = -(normal * a);
  }
}

int idTraceModel::GenerateEdgeNormals() {
  int valid = 0;
  for (unsigned edge = 0; edge < numEdges; ++edge) {
    idVec3 normal{};
    for (unsigned p = 0; p < numPolys; ++p) {
      for (unsigned e = 0; e < numPolyEdges[p]; ++e) {
        if (EdgeIndex(polyEdges[p][e]) == edge) {
          normal += idVec3{polyPlaneX[p], polyPlaneY[p], polyPlaneZ[p]};
        }
      }
    }
    if (normal.Normalize() > kEpsilon)
      ++valid;
    edgeNormalX[edge] = normal.x;
    edgeNormalY[edge] = normal.y;
    edgeNormalZ[edge] = normal.z;
  }
  return valid;
}

void idTraceModel::CalculateInsetSphereRadius() {
  radius = std::numeric_limits<float>::infinity();
  for (unsigned p = 0; p < numPolys; ++p) {
    const idVec3 n{polyPlaneX[p], polyPlaneY[p], polyPlaneZ[p]};
    radius = std::min(radius, -(n * offset + polyPlaneW[p]));
  }
  if (!std::isfinite(radius) || radius < 0.0f)
    radius = 0.0f;
}

void idTraceModel::TestConvexity() {
  isConvex = true;
  for (unsigned p = 0; p < numPolys && isConvex; ++p) {
    const idVec3 n{polyPlaneX[p], polyPlaneY[p], polyPlaneZ[p]};
    for (unsigned v = 0; v < numVerts; ++v) {
      if (n * GetVertex(v) + polyPlaneW[p] > 2.0e-4f) {
        isConvex = false;
        break;
      }
    }
  }
}

void idTraceModel::Translate(const idVec3& translation) {
  for (unsigned i = 0; i < numVerts; ++i)
    SetVertex(i, GetVertex(i) + translation);
  offset += translation;
  bounds[0] += translation;
  bounds[1] += translation;
  CalculatePolygonPlanes();
}

void idTraceModel::Rotate(const idMat3& rotation) {
  for (unsigned i = 0; i < numVerts; ++i)
    SetVertex(i, rotation * GetVertex(i));
  CalculatePolygonPlanes();
  GenerateEdgeNormals();
  ResetBounds(*this);
}

void idTraceModel::Scale(const idVec3& scale) {
  for (unsigned i = 0; i < numVerts; ++i)
    SetVertex(i, ComponentMultiply(GetVertex(i), scale));
  CalculatePolygonPlanes();
  GenerateEdgeNormals();
  ResetBounds(*this);
  TestConvexity();
}

bool idTraceModel::Compare(const idTraceModel& other) const {
  if (type != other.type || numVerts != other.numVerts || numEdges != other.numEdges ||
      numPolys != other.numPolys)
    return false;
  for (unsigned i = 0; i < numVerts; ++i) {
    if ((GetVertex(i) - other.GetVertex(i)).LengthSqr() > 1.0e-8f)
      return false;
  }
  return true;
}

bool idTraceModel::ContainsPoint(const idVec3& point) const {
  if (!isConvex || numPolys == 0)
    return false;
  for (unsigned p = 0; p < numPolys; ++p) {
    const idVec3 n{polyPlaneX[p], polyPlaneY[p], polyPlaneZ[p]};
    if (n * point + polyPlaneW[p] > 1.0e-4f)
      return false;
  }
  return true;
}

void idTraceModel::InitBox() {
  SetupBox(idBounds{{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}});
}

void idTraceModel::SetupBox(const idBounds& b) {
  const std::vector<idVec3> v = {{b[0].x, b[0].y, b[0].z}, {b[1].x, b[0].y, b[0].z},
                                 {b[1].x, b[1].y, b[0].z}, {b[0].x, b[1].y, b[0].z},
                                 {b[0].x, b[0].y, b[1].z}, {b[1].x, b[0].y, b[1].z},
                                 {b[1].x, b[1].y, b[1].z}, {b[0].x, b[1].y, b[1].z}};
  BuildFromFaces(
      *this, TRM_BOX, v,
      {{0, 3, 2, 1}, {4, 5, 6, 7}, {0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}});
}

void idTraceModel::SetupBox(float size) {
  const float h = size * 0.5f;
  SetupBox(idBounds{{{-h, -h, -h}, {h, h, h}}});
}

void idTraceModel::InitOctahedron() {
  SetupOctahedron(idBounds{{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}}});
}

void idTraceModel::SetupOctahedron(const idBounds& b) {
  const idVec3 c = BoundsCenter(b);
  const idVec3 h = (b[1] - b[0]) * 0.5f;
  std::vector<idVec3> v = {{c.x + h.x, c.y, c.z}, {c.x - h.x, c.y, c.z}, {c.x, c.y + h.y, c.z},
                           {c.x, c.y - h.y, c.z}, {c.x, c.y, c.z + h.z}, {c.x, c.y, c.z - h.z}};
  BuildFromFaces(
      *this, TRM_OCTAHEDRON, v,
      {{4, 0, 2}, {4, 2, 1}, {4, 1, 3}, {4, 3, 0}, {5, 2, 0}, {5, 1, 2}, {5, 3, 1}, {5, 0, 3}});
}

void idTraceModel::InitDodecahedron() {
  SetupDodecahedron(1.0f);
}

void idTraceModel::SetupDodecahedron(float size) {
  const float h = size * 0.5f;
  SetupDodecahedron(idBounds{{{-h, -h, -h}, {h, h, h}}});
}

void idTraceModel::SetupDodecahedron(const idBounds& b) {
  constexpr float phi = 1.6180339887498948482f;
  constexpr float inv = 1.0f / phi;
  std::vector<idVec3> v;
  for (int x : {-1, 1})
    for (int y : {-1, 1})
      for (int z : {-1, 1})
        v.push_back({static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)});
  for (int a : {-1, 1})
    for (int d : {-1, 1}) {
      v.push_back({0.0f, a * inv, d * phi});
      v.push_back({a * inv, d * phi, 0.0f});
      v.push_back({d * phi, 0.0f, a * inv});
    }
  const idVec3 c = BoundsCenter(b);
  const idVec3 h = (b[1] - b[0]) * 0.5f;
  for (idVec3& p : v)
    p = c + ComponentMultiply(p * inv, h);
  BuildFromFaces(*this, TRM_DODECAHEDRON, v, ConvexFaces(v));
}

void idTraceModel::SetupCylinder(float height, float width, int sides) {
  const float hz = height * 0.5f, r = width * 0.5f;
  SetupCylinder(idBounds{{{-r, -r, -hz}, {r, r, hz}}}, sides);
}

void idTraceModel::SetupCylinder(const idBounds& b, int sides) {
  sides = std::clamp(sides, 3, 14);
  const idVec3 c = BoundsCenter(b);
  const float rx = (b[1].x - b[0].x) * 0.5f, ry = (b[1].y - b[0].y) * 0.5f;
  std::vector<idVec3> v;
  for (int layer = 0; layer < 2; ++layer)
    for (int i = 0; i < sides; ++i) {
      const float a = 2.0f * kPi * i / sides;
      v.push_back({c.x + rx * std::cos(a), c.y + ry * std::sin(a), layer ? b[1].z : b[0].z});
    }
  std::vector<std::vector<unsigned>> faces;
  std::vector<unsigned> bottom, top;
  for (int i = sides - 1; i >= 0; --i)
    bottom.push_back(static_cast<unsigned>(i));
  for (int i = 0; i < sides; ++i)
    top.push_back(static_cast<unsigned>(sides + i));
  faces.push_back(bottom);
  faces.push_back(top);
  for (int i = 0; i < sides; ++i) {
    unsigned n = (i + 1) % sides;
    faces.push_back({static_cast<unsigned>(i), n, static_cast<unsigned>(sides + n),
                     static_cast<unsigned>(sides + i)});
  }
  BuildFromFaces(*this, TRM_CYLINDER, v, faces);
}

void idTraceModel::SetupCone(const idBounds& b, int sides) {
  sides = std::clamp(sides, 3, 15);
  const idVec3 c = BoundsCenter(b);
  const float rx = (b[1].x - b[0].x) * 0.5f, ry = (b[1].y - b[0].y) * 0.5f;
  std::vector<idVec3> v;
  for (int i = 0; i < sides; ++i) {
    const float a = 2 * kPi * i / sides;
    v.push_back({c.x + rx * std::cos(a), c.y + ry * std::sin(a), b[0].z});
  }
  v.push_back({c.x, c.y, b[1].z});
  std::vector<std::vector<unsigned>> faces;
  std::vector<unsigned> base;
  for (int i = sides - 1; i >= 0; --i)
    base.push_back(static_cast<unsigned>(i));
  faces.push_back(base);
  for (int i = 0; i < sides; ++i)
    faces.push_back({static_cast<unsigned>(i), static_cast<unsigned>((i + 1) % sides),
                     static_cast<unsigned>(sides)});
  BuildFromFaces(*this, TRM_CONE, v, faces);
}

void idTraceModel::InitBone() {
  SetupBone(1.0f, 0.25f);
}

void idTraceModel::SetupBone(float length, float width) {
  const float h = length * 0.5f, r = width * 0.5f;
  std::vector<idVec3> v = {{0, 0, -h}, {0, 0, h}, {r, r, 0}, {-r, r, 0}, {-r, -r, 0}, {r, -r, 0}};
  BuildFromFaces(
      *this, TRM_BONE, v,
      {{0, 2, 3}, {0, 3, 4}, {0, 4, 5}, {0, 5, 2}, {1, 3, 2}, {1, 4, 3}, {1, 5, 4}, {1, 2, 5}});
}

void idTraceModel::SetupPolygon(const idVec3* vertices, unsigned count) {
  count = std::clamp(count, 3u, 16u);
  std::vector<idVec3> v(vertices, vertices + count);
  std::vector<unsigned> face;
  for (unsigned i = 0; i < count; ++i)
    face.push_back(i);
  BuildFromFaces(*this, TRM_POLYGON, v, {face});
  isConvex = true;
}

void idTraceModel::ExtendPolygonToVolume(const idVec3& direction) {
  if (type != TRM_POLYGON || numVerts < 3)
    return;
  std::vector<idVec3> v;
  const unsigned count = numVerts;
  for (unsigned i = 0; i < count; ++i)
    v.push_back(GetVertex(i));
  for (unsigned i = 0; i < count; ++i)
    v.push_back(GetVertex(i) + direction);
  std::vector<std::vector<unsigned>> faces;
  std::vector<unsigned> bottom, top;
  for (int i = static_cast<int>(count) - 1; i >= 0; --i)
    bottom.push_back(static_cast<unsigned>(i));
  for (unsigned i = 0; i < count; ++i)
    top.push_back(count + i);
  faces.push_back(bottom);
  faces.push_back(top);
  for (unsigned i = 0; i < count; ++i) {
    unsigned n = (i + 1) % count;
    faces.push_back({i, n, count + n, count + i});
  }
  BuildFromFaces(*this, TRM_POLYGONVOLUME, v, faces);
}

void idTraceModel::SetupPolygonVolume(const idVec3* vertices, unsigned count,
                                      const idVec3& direction) {
  count = std::min(count, 14u);
  SetupPolygon(vertices, count);
  ExtendPolygonToVolume(direction);
}

void idTraceModel::Shrink(float amount) {
  if (amount <= 0.0f || numVerts == 0)
    return;
  const idVec3 center = offset;
  for (unsigned i = 0; i < numVerts; ++i) {
    idVec3 d = GetVertex(i) - center;
    const float l = d.Length();
    if (l > kEpsilon)
      SetVertex(i, center + d * (std::max(0.0f, l - amount) / l));
  }
  CalculatePolygonPlanes();
  GenerateEdgeNormals();
  ResetBounds(*this);
  TestConvexity();
}

void idTraceModel::ProjectionIntegrals(int polyNum, int a, int b, projectionIntegrals_t& o) const {
  std::memset(&o, 0, sizeof(o));
  if (polyNum < 0 || static_cast<unsigned>(polyNum) >= numPolys)
    return;
  for (unsigned e = 0; e < numPolyEdges[polyNum]; ++e) {
    const auto edge = OrientedEdge(*this, polyEdges[polyNum][e]);
    const idVec3 v0 = GetVertex(edge.first), v1 = GetVertex(edge.second);
    const double a0 = v0[a], b0 = v0[b], a1 = v1[a], b1 = v1[b];
    const double da = a1 - a0, db = b1 - b0;
    const double C1 = a1 + a0, Ca = a1 * C1 + a0 * a0, Caa = a1 * Ca + a0 * a0 * a0;
    const double Cb = b1 * (b1 + b0) + b0 * b0, Cbb = b1 * Cb + b0 * b0 * b0;
    const double Cab = 3 * a1 * a1 + 2 * a1 * a0 + a0 * a0;
    const double Kab = a1 * a1 + 2 * a1 * a0 + 3 * a0 * a0;
    const double Caab = a0 * Cab + 4 * a1 * a1 * a1;
    const double Kaab = a1 * Kab + 4 * a0 * a0 * a0;
    const double Cabb = 4 * b1 * b1 * b1 + b0 * (3 * b1 * b1 + 2 * b1 * b0 + b0 * b0);
    const double Kabb = b1 * (b1 * b1 + 2 * b1 * b0 + 3 * b0 * b0) + 4 * b0 * b0 * b0;
    o.P1 += static_cast<float>(db * C1);
    o.Pa += static_cast<float>(db * Ca);
    o.Paa += static_cast<float>(db * Caa);
    o.Paaa += static_cast<float>(db * (a1 * Caa + a0 * a0 * a0 * a0));
    o.Pb += static_cast<float>(da * Cb);
    o.Pbb += static_cast<float>(da * Cbb);
    o.Pbbb += static_cast<float>(da * (b1 * Cbb + b0 * b0 * b0 * b0));
    o.Pab += static_cast<float>(db * (b1 * Cab + b0 * Kab));
    o.Paab += static_cast<float>(db * (b1 * Caab + b0 * Kaab));
    o.Pabb += static_cast<float>(da * (a1 * Cabb + a0 * Kabb));
  }
  o.P1 *= 0.5f;
  o.Pa /= 6;
  o.Paa /= 12;
  o.Paaa /= 20;
  o.Pb /= -6;
  o.Pbb /= -12;
  o.Pbbb /= -20;
  o.Pab /= 24;
  o.Paab /= 60;
  o.Pabb /= -60;
}

void idTraceModel::PolygonIntegrals(int polyNum, int a, int b, int c, polygonIntegrals_t& o) const {
  std::memset(&o, 0, sizeof(o));
  projectionIntegrals_t p{};
  ProjectionIntegrals(polyNum, a, b, p);
  const float n[3] = {polyPlaneX[polyNum], polyPlaneY[polyNum], polyPlaneZ[polyNum]};
  if (std::fabs(n[c]) < kEpsilon)
    return;
  const float k1 = 1.0f / n[c], k2 = k1 * k1, k3 = k2 * k1, k4 = k3 * k1, w = polyPlaneW[polyNum];
  o.Fa = k1 * p.Pa;
  o.Fb = k1 * p.Pb;
  o.Fc = -k2 * (n[a] * p.Pa + n[b] * p.Pb + w * p.P1);
  o.Faa = k1 * p.Paa;
  o.Fbb = k1 * p.Pbb;
  o.Fcc = k3 * (n[a] * n[a] * p.Paa + 2 * n[a] * n[b] * p.Pab + n[b] * n[b] * p.Pbb +
                w * (2 * (n[a] * p.Pa + n[b] * p.Pb) + w * p.P1));
  o.Faaa = k1 * p.Paaa;
  o.Fbbb = k1 * p.Pbbb;
  o.Fccc = -k4 * (n[a] * n[a] * n[a] * p.Paaa + 3 * n[a] * n[a] * n[b] * p.Paab +
                  3 * n[a] * n[b] * n[b] * p.Pabb + n[b] * n[b] * n[b] * p.Pbbb);
  o.Faab = k1 * p.Paab;
  o.Fbbc = -k2 * (n[a] * p.Pabb + n[b] * p.Pbbb + w * p.Pbb);
  o.Fcca = k3 * (n[a] * n[a] * p.Paaa + 2 * n[a] * n[b] * p.Paab + n[b] * n[b] * p.Pabb);
}

void idTraceModel::VolumeIntegrals(volumeIntegrals_t& o) const {
  double volume = 0;
  std::array<double, 3> first{}, second{}, products{};
  for (unsigned p = 0; p < numPolys; ++p) {
    if (numPolyEdges[p] < 3)
      continue;
    const idVec3 a = GetVertex(OrientedEdge(*this, polyEdges[p][0]).first);
    for (unsigned e = 1; e + 1 < numPolyEdges[p]; ++e) {
      const idVec3 b = GetVertex(OrientedEdge(*this, polyEdges[p][e]).first);
      const idVec3 c = GetVertex(OrientedEdge(*this, polyEdges[p][e + 1]).first);
      AccumulateTriangle(a, b, c, volume, first, second, products);
    }
  }
  o.T0 = static_cast<float>(volume);
  o.T1 = {static_cast<float>(first[0]), static_cast<float>(first[1]), static_cast<float>(first[2])};
  o.T2 = {static_cast<float>(second[0]), static_cast<float>(second[1]),
          static_cast<float>(second[2])};
  o.TP = {static_cast<float>(products[0]), static_cast<float>(products[1]),
          static_cast<float>(products[2])};
}

void idTraceModel::GetMassProperties(float density, float& mass, idVec3& center,
                                     idMat3& inertia) const {
  volumeIntegrals_t v{};
  VolumeIntegrals(v);
  float sign = v.T0 < 0 ? -1.0f : 1.0f;
  const float volume = std::fabs(v.T0);
  mass = density * volume;
  if (volume < kEpsilon) {
    center = {};
    inertia = idMat3{};
    for (int i = 0; i < 3; ++i)
      inertia[i] = {};
    return;
  }
  center = v.T1 * (sign / volume);
  const idVec3 s = v.T2 * sign;
  const idVec3 p = v.TP * sign;
  inertia[0] = {density * (s.y + s.z), -density * p.x, -density * p.z};
  inertia[1] = {-density * p.x, density * (s.x + s.z), -density * p.y};
  inertia[2] = {-density * p.z, -density * p.y, density * (s.x + s.y)};
  inertia[0].x -= mass * (center.y * center.y + center.z * center.z);
  inertia[1].y -= mass * (center.x * center.x + center.z * center.z);
  inertia[2].z -= mass * (center.x * center.x + center.y * center.y);
  inertia[0].y += mass * center.x * center.y;
  inertia[1].x = inertia[0].y;
  inertia[1].z += mass * center.y * center.z;
  inertia[2].y = inertia[1].z;
  inertia[0].z += mass * center.x * center.z;
  inertia[2].x = inertia[0].z;
}

void idTraceModel::Swap() {
  const auto swap16 = [](std::uint16_t value) {
    return static_cast<std::uint16_t>((value << 8) | (value >> 8));
  };
  const auto swap32 = [](std::uint32_t value) {
    return ((value & 0x000000ffu) << 24) |
           ((value & 0x0000ff00u) << 8) |
           ((value & 0x00ff0000u) >> 8) |
           ((value & 0xff000000u) >> 24);
  };
  const auto swapFloat = [&](float& value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    bits = swap32(bits);
    std::memcpy(&value, &bits, sizeof(value));
  };

  for (float& value : vertsX) swapFloat(value);
  for (float& value : vertsY) swapFloat(value);
  for (float& value : vertsZ) swapFloat(value);
  for (float& value : edgeNormalX) swapFloat(value);
  for (float& value : edgeNormalY) swapFloat(value);
  for (float& value : edgeNormalZ) swapFloat(value);
  for (float& value : polyPlaneX) swapFloat(value);
  for (float& value : polyPlaneY) swapFloat(value);
  for (float& value : polyPlaneZ) swapFloat(value);
  for (float& value : polyPlaneW) swapFloat(value);
  for (std::uint32_t& value : numPolyEdges) value = swap32(value);
  for (traceModelEdge_t& edge : edges) {
    edge.v[0] = swap16(edge.v[0]);
    edge.v[1] = swap16(edge.v[1]);
  }
  auto swapEnumOrCount = [&](auto& value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(value) == sizeof(bits));
    std::memcpy(&bits, &value, sizeof(bits));
    bits = swap32(bits);
    std::memcpy(&value, &bits, sizeof(value));
  };
  swapEnumOrCount(type);
  swapEnumOrCount(numVerts);
  swapEnumOrCount(numEdges);
  swapEnumOrCount(numPolys);
  swapEnumOrCount(maxPolyEdges);
  swapFloat(offset.x);
  swapFloat(offset.y);
  swapFloat(offset.z);
  for (idVec3& bound : bounds.b) {
    swapFloat(bound.x);
    swapFloat(bound.y);
    swapFloat(bound.z);
  }
  swapFloat(radius);
}
