#include "engine/models/cable/rendermodelcable.h"

#include "engine/models/transparency/rendermodeltransparency.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>

idCVar r_useCableGenJobs("r_useCableGenJobs", "0", CVAR_INTEGER,
                         "0 = off, 1 = threaded", 0.0f, 1.0f);

namespace {

constexpr memTag_t kCurveTag = static_cast<memTag_t>(116);
constexpr memTag_t kRenderModelTag = static_cast<memTag_t>(85);

template <typename T> T* AllocCableObject(const char* location) {
  return static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T), kCurveTag, false, ALIGN_16, HEAP_DEFAULTHEAP));
}

float InverseLength(const idVec3& vector) {
  return 1.0f /
         std::sqrt(std::max(vector.LengthSqr(),
                            std::numeric_limits<float>::min()));
}

} // namespace

template <> idCurve<idVec3>::idCurve()
    : times(), values(), currentIndex(-1), changed(false) {}

template <> idCurve<idVec3>::~idCurve() = default;

template <>
idCurve_NonUniformBSpline<idVec3>::~idCurve_NonUniformBSpline() = default;

template <>
idCurve<idVec3>* idCurve_NonUniformBSpline<idVec3>::CreateNewCurve() {
  auto* memory = AllocCableObject<idCurve_NonUniformBSpline<idVec3>>(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag");
  return memory != nullptr
             ? ::new (static_cast<void*>(memory))
                   idCurve_NonUniformBSpline<idVec3>()
             : nullptr;
}

template <> idCurve_NURBS<idVec3>::idCurve_NURBS() = default;

template <> idCurve_NURBS<idVec3>::~idCurve_NURBS() = default;

template <>
idCurve_NURBS<idVec3>* idCurve_NURBS<idVec3>::CreateNewCurve() {
  auto* memory = AllocCableObject<idCurve_NURBS<idVec3>>(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag");
  return memory != nullptr
             ? ::new (static_cast<void*>(memory)) idCurve_NURBS<idVec3>()
             : nullptr;
}

idRenderModelCable::idRenderModelCable()
    : material(nullptr), lengthMaxSubdivisions(512), diameterSubdivisions(16),
      diameter(1.0f), dirty(true), jobQueued(false), subdivPoints(nullptr),
      parms(nullptr), currentPointsBuffer(0), currentGeometryBuffer(0),
      pointsBuffer{}, vertexBuffer{}, indexBuffer{}, numVertices(0),
      numIndices(0) {
  renderModelType = RENDERMODEL_CABLE;
  for (auto& points : pointsBuffer)
    points.ResizeForPort(32);
}

idRenderModelCable::~idRenderModelCable() {
  if ((static_cast<unsigned>(vertexBuffer[currentGeometryBuffer].size) &
       0x80000000u) != 0u) {
    vertexBuffer[currentGeometryBuffer].UnmapBuffer();
  }
  if ((static_cast<unsigned>(indexBuffer[currentGeometryBuffer].size) &
       0x80000000u) != 0u) {
    indexBuffer[currentGeometryBuffer].UnmapBuffer();
  }
  for (auto& points : pointsBuffer)
    points.ClearFree();
  idMem::Singleton()->Free(subdivPoints, ALIGN_16);
  subdivPoints = nullptr;
  idMem::Singleton()->Free(parms, ALIGN_16);
  parms = nullptr;
  for (int index = 0; index < 3; ++index) {
    vertexBuffer[index].FreeBufferObject();
    indexBuffer[index].FreeBufferObject();
  }
}

void idRenderModelCable::FinializeJob() {
  if (!jobQueued || jobDone == 0)
    return;

  idVertexBuffer& completedVertices = vertexBuffer[currentGeometryBuffer];
  if ((static_cast<unsigned>(completedVertices.size) & 0x80000000u) != 0u) {
    completedVertices.UnmapBuffer();
    indexBuffer[currentGeometryBuffer].UnmapBuffer();
  }
  idTriangles* geometry = surfaces[0].geometry;
  geometry->vertexBuffer.Reference(&completedVertices);
  geometry->indexBuffer.Reference(&indexBuffer[currentGeometryBuffer]);
  geometry->numVerts = numVertices;
  geometry->numIndexes = numIndices;
  jobDone = 0;
  jobQueued = false;
  currentGeometryBuffer = (currentGeometryBuffer + 1) % 3;
}

bool idRenderModelCable::UpdateInView(const idRenderView*, const idRenderView*,
                                      idRenderModelUpdateTools* updateTools) {
  if (surfaces.Num() == 0 || (!dirty && !jobQueued))
    return false;

  FinializeJob();
  if (!jobQueued && dirty) {
    jobDone = 0;
    dirty = false;
    jobQueued = true;

    auto* vertices = static_cast<idDrawVert*>(
        vertexBuffer[currentGeometryBuffer].MapBuffer(3));
    auto* indices = static_cast<std::uint16_t*>(
        indexBuffer[currentGeometryBuffer].MapBuffer(3));
    parms->subdivPoints = subdivPoints;
    parms->points = pointsBuffer[currentPointsBuffer].list;
    parms->numPoints = pointsBuffer[currentPointsBuffer].num;
    parms->diameterSubdivision = diameterSubdivisions;
    parms->lengthMaxSubdivision = lengthMaxSubdivisions;
    parms->diameter = diameter;
    parms->maxVertices = diameterSubdivisions * lengthMaxSubdivisions;
    parms->maxIndices =
        6 * diameterSubdivisions * (lengthMaxSubdivisions - 1);
    parms->vertices = vertices;
    parms->indices = indices;
    parms->numVertices = &numVertices;
    parms->numIndices = &numIndices;
    parms->done = &jobDone;
    currentPointsBuffer = (currentPointsBuffer + 1) % 2;

    if (r_useCableGenJobs.valueInteger == 1) {
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(GenCable), parms);
    } else {
      GenCable(parms);
    }
  }
  FinializeJob();
  return true;
}

void idRenderModelCable::SetPoints(const idVec3* points, const float* tension,
                                   int num) {
  idVec3 minimum{1.0e30f, 1.0e30f, 1.0e30f};
  idVec3 maximum{-1.0e30f, -1.0e30f, -1.0e30f};
  for (int index = 0; index < num; ++index) {
    minimum.x = std::fmin(minimum.x, points[index].x);
    minimum.y = std::fmin(minimum.y, points[index].y);
    minimum.z = std::fmin(minimum.z, points[index].z);
    maximum.x = std::fmax(maximum.x, points[index].x);
    maximum.y = std::fmax(maximum.y, points[index].y);
    maximum.z = std::fmax(maximum.z, points[index].z);
  }

  idBounds bounds{};
  bounds.b[0] = {minimum.x - diameter, minimum.y - diameter,
                 minimum.z - diameter};
  bounds.b[1] = {maximum.x + diameter, maximum.y + diameter,
                 maximum.z + diameter};
  idList<idVec4, 5>& destination = pointsBuffer[currentPointsBuffer];
  destination.SetNum(num);
  for (int index = 0; index < num; ++index) {
    destination[index] =
        {points[index].x, points[index].y, points[index].z, tension[index]};
  }

  surfaces[0].geometry->bounds = bounds;
  referenceBounds = bounds;
  dirty = true;
}

void idRenderModelCable::BuildModel(const idMaterial* material_,
                                    int lengthMaxSubdivisions_, float diameter_,
                                    int diameterSubdivisions_) {
  material = material_;
  lengthMaxSubdivisions = std::clamp(lengthMaxSubdivisions_, 2, 512);
  diameterSubdivisions = std::clamp(diameterSubdivisions_, 3, 16);
  diameter = diameter_;

  parms = static_cast<cableGenParms_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\models\\cable\\RenderModelCable.cpp(106) : TAG_RENDERMODEL",
          sizeof(cableGenParms_t), kRenderModelTag, true, ALIGN_16,
          HEAP_DEFAULTHEAP));
  subdivPoints = static_cast<subdivPoint_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\models\\cable\\RenderModelCable.cpp(109) : TAG_RENDERMODEL",
          sizeof(subdivPoint_t) *
              static_cast<std::size_t>(diameterSubdivisions),
          kRenderModelTag, true, ALIGN_16, HEAP_DEFAULTHEAP));

  idCurve_NURBS<idVec3> circle;
  const float radius = diameter * 0.5f;
  circle.AddValue(0.0f, {radius, radius, 0.0f}, 1.0f);
  circle.AddValue(100.0f, {radius, -radius, 0.0f}, 1.0f);
  circle.AddValue(200.0f, {-radius, -radius, 0.0f}, 1.0f);
  circle.AddValue(300.0f, {-radius, radius, 0.0f}, 1.0f);
  circle.SetBoundaryType(idCurve_Spline<idVec3>::CLOSED);
  circle.SetCloseTime(100.0f);

  const float fullCircleTime =
      circle.GetTime(circle.GetNumValues() - 1) - circle.GetTime(0) +
      circle.GetCloseTime();
  for (int index = 0; index < diameterSubdivisions; ++index) {
    const float time = static_cast<float>(index) * fullCircleTime /
                       static_cast<float>(diameterSubdivisions);
    subdivPoints[index].position = circle.GetCurrentValue(time);
    subdivPoints[index].tangent = circle.GetCurrentFirstDerivative(time);
    subdivPoints[index].texcoord =
        static_cast<float>(index) /
        static_cast<float>(diameterSubdivisions);
    subdivPoints[index].tangent *= InverseLength(subdivPoints[index].tangent);
  }

  const int vertexBufferSize =
      static_cast<int>(sizeof(idDrawVert)) * lengthMaxSubdivisions *
      diameterSubdivisions;
  const int indexBufferSize =
      static_cast<int>(sizeof(std::uint16_t)) * 6 * diameterSubdivisions *
      (lengthMaxSubdivisions - 1);
  for (int index = 0; index < 3; ++index) {
    vertexBuffer[index].AllocBufferObject(nullptr, vertexBufferSize, 31,
                                          BU_DYNAMIC);
    indexBuffer[index].AllocBufferObject(nullptr, indexBufferSize, BU_DYNAMIC);
  }

  FreeSurfaces();
  if ((useDeferredPosition & (deferredPositionInitialized >> 1U) & 1U) == 0U)
    g.origin = vec3_origin;
  deferredOrigin = vec3_origin;
  auto* geometry = new (std::nothrow) idTriangles{};
  geometry->bounds = {};
  geometry->numVerts = 0;
  geometry->numIndexes = 0;
  geometry->vertexBuffer.Reference(&vertexBuffer[0]);
  geometry->indexBuffer.Reference(&indexBuffer[0]);

  idRenderModelSurface surface{};
  surface.material = material;
  surface.geometry = geometry;
  AddSurface(surface);
  FinishSurfaces();
  dirty = true;
}
