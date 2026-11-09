#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/math/plane.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
class alignas(16) idTriangleInterpolator {
public:
  struct triangle_t {
    idVec2 xy;
    idVec2 v0;
    idVec2 v1;
    float d00;
    float d01;
    float d11;
    float invDenom;
    float invArea;
    idVec2 st[2];
    idVec2 st0[2];
    idVec2 st1[2];
    std::uint32_t c0;
    std::uint32_t c1;
    std::uint32_t c2;
    std::uint8_t tangent[3][4];
    std::uint8_t normal[3][4];
  };
  static_assert(sizeof(triangle_t) == 128, "triangle interpolator ABI drift");

  void Clear();
  bool Interpolate(const idVec2& point, idDrawVert* verts,
                   idVec2* st) const;
  void AddTriangles(const idDrawVert* verts, const idVec2* st,
                    const std::uint16_t* indices, int numFrontIndices);

  idList<triangle_t> front;
  idList<triangle_t> back;
  idDrawVert refVert{};
  idPlane refPlane{};
  idVec3 refLeft{};
  idVec3 refDown{};

private:
#if defined(WOLF_TRIANGLEINTERPOLATOR_TEST_ACCESS)
  friend struct idTriangleInterpolatorTestAccess;
#endif
  int FindTriangle(const idVec2& point) const;
  static idVec3 InterpolateVector(const idVec3& v0, const idVec3& v1,
                                  const idVec3& v2, float u, float v);
  static std::uint32_t InterpolateColor(const triangle_t& triangle, float u,
                                        float v);
  triangle_t CreateTriangle(const idVec3& left, const idVec3& down,
                            const idDrawVert* verts, const idVec2* uv,
                            const std::uint16_t* indices, int triIndex);
  static void InterpolateTBN(const triangle_t& triangle, float u, float v,
                             idVec3& tangent, idVec3& bitangent,
                             idVec3& normal);
  void Interpolate(const idVec2& point, const triangle_t& triangle,
                   idDrawVert& vert, idVec2& st) const;
};
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

static_assert(sizeof(idTriangleInterpolator) == 128,
              "triangle interpolator ABI drift");
