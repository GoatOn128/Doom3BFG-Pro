#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

enum traceModel_t {
  TRM_INVALID = 0,
  TRM_BOX,
  TRM_OCTAHEDRON,
  TRM_DODECAHEDRON,
  TRM_CYLINDER,
  TRM_CONE,
  TRM_BONE,
  TRM_POLYGON,
  TRM_POLYGONVOLUME,
  TRM_CUSTOM
};

class idEffectPhysicsBoxFountain;
class idPhysics_AI;

struct traceModelEdge_t {
  std::uint16_t v[2];
};

struct projectionIntegrals_t {
  float P1, Pa, Pb, Paa, Pab, Pbb, Paaa, Paab, Pabb, Pbbb;
};

struct polygonIntegrals_t {
  float Fa, Fb, Fc, Faa, Fbb, Fcc;
  float Faaa, Fbbb, Fccc, Faab, Fbbc, Fcca;
};

struct volumeIntegrals_t {
  float T0;
  idVec3 T1;
  idVec3 T2;
  idVec3 TP;
};

class idTraceModel {
public:
  static constexpr unsigned MaxVerts = 32;
  static constexpr unsigned MaxEdges = 32;
  static constexpr unsigned MaxPolys = 16;
  static constexpr unsigned MaxPolyEdges = 16;

  float vertsX[MaxVerts]{};
  float vertsY[MaxVerts]{};
  float vertsZ[MaxVerts]{};
  float edgeNormalX[MaxEdges]{};
  float edgeNormalY[MaxEdges]{};
  float edgeNormalZ[MaxEdges]{};
  float polyPlaneX[MaxPolys]{};
  float polyPlaneY[MaxPolys]{};
  float polyPlaneZ[MaxPolys]{};
  float polyPlaneW[MaxPolys]{};
  std::uint8_t polyEdges[MaxPolys][MaxPolyEdges]{};
  std::uint32_t numPolyEdges[MaxPolys]{};
  traceModelEdge_t edges[MaxEdges]{};
  traceModel_t type{TRM_INVALID};
  std::uint32_t numVerts{};
  std::uint32_t numEdges{};
  std::uint32_t numPolys{};
  std::uint32_t maxPolyEdges{};
  idVec3 offset{};
  idBounds bounds{};
  float radius{};
  bool isConvex{};
  std::uint8_t pad[3]{};

  idTraceModel() = default;
  explicit idTraceModel(const idBounds& boxBounds) { SetupBox(boxBounds); }
  idTraceModel(const idBounds& cylinderBounds, int sides) { SetupCylinder(cylinderBounds, sides); }
  idTraceModel(float boneLength, float boneWidth) { SetupBone(boneLength, boneWidth); }

  static void* operator new(std::size_t size);
  static void operator delete(void* memory) noexcept;

  void ClearUnused();
  bool IsClosedSurface() const;
  void InitOctahedron();
  void InitDodecahedron();
  void InitBone();
  int GenerateEdgeNormals();
  void CalculatePolygonPlanes();
  void CalculateInsetSphereRadius();
  void TestConvexity();
  void Translate(const idVec3& translation);
  void Rotate(const idMat3& rotation);
  void Scale(const idVec3& scale);
  bool Compare(const idTraceModel& other) const;
  bool ContainsPoint(const idVec3& point) const;

  void ProjectionIntegrals(int polyNum, int a, int b, projectionIntegrals_t& integrals) const;
  void PolygonIntegrals(int polyNum, int a, int b, int c, polygonIntegrals_t& integrals) const;
  void VolumeIntegrals(volumeIntegrals_t& integrals) const;

  void SetupBox(const idBounds& boxBounds);
  void SetupBox(float size);
  void SetupOctahedron(const idBounds& octBounds);
  void SetupDodecahedron(const idBounds& dodBounds);
  void SetupDodecahedron(float size);
  void SetupCylinder(const idBounds& cylinderBounds, int sides);
  void SetupCylinder(float height, float width, int sides);
  void SetupCone(const idBounds& coneBounds, int sides);
  void SetupBone(float length, float width);
  void SetupPolygon(const idVec3* vertices, unsigned count);
  void ExtendPolygonToVolume(const idVec3& direction);
  void SetupPolygonVolume(const idVec3* vertices, unsigned count, const idVec3& direction);
  void Shrink(float amount);
  void GetMassProperties(float density, float& mass, idVec3& centerOfMass,
                         idMat3& inertiaTensor) const;

private:
  void InitBox();

  friend class idEffectPhysicsBoxFountain;
  friend class idPhysics_AI;
  void Swap();

  bool operator==(const idTraceModel& other) const { return Compare(other); }
  bool operator!=(const idTraceModel& other) const { return !Compare(other); }

  idVec3 GetVertex(unsigned index) const;
  void SetVertex(unsigned index, const idVec3& vertex);
};

static_assert(sizeof(traceModelEdge_t) == 4, "trace edge ABI drift");
static_assert(sizeof(idTraceModel) == 1536, "trace model ABI drift");
