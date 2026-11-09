#include "engine/models/cuttable/rendermodelcuttablebeams.h"

#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <utility>

idCVar g_cuttableBeamIncisions(
    "g_cuttableBeamIncisions", "0", CVAR_INTEGER,
    "0 Render normaly, 1 Render only incisions, 2 Render only incision crossings, 3 Render only incision end-points");

namespace {

constexpr float kBeamWidth = 8.0f;
constexpr float kBeamRepoint = 0.6f;
constexpr float kBeamRepeat = 12.0f;

idRenderModelSurface* GetSurface(idRenderModelCuttableBeams* model, int index) {
  return model->surfaces.list != nullptr && index >= 0 && index < model->surfaces.num
             ? &model->surfaces.list[index]
             : nullptr;
}

void ClearGeometry(idRenderModelSurface* surface, bool clearMaterial) {
  if (surface == nullptr || surface->geometry == nullptr)
    return;
  if (clearMaterial)
    surface->material = nullptr;
  surface->geometry->numVerts = 0;
  surface->geometry->numIndexes = 0;
}

void InitializeVertex(idDrawVert& vertex, const idVec3& position,
                      const idVec2& st = idVec2{}) {
  std::memset(&vertex, 0, sizeof(vertex));
  vertex.xyz = position;
  vertex.st = st;
  vertex.tangent[0] = 128;
  vertex.tangent[1] = 128;
  vertex.tangent[2] = 128;
  std::memset(vertex.color, 0xff, sizeof(vertex.color));
}

void SetCornerEncoding(idDrawVert& vertex, bool firstAxis, bool secondAxis) {
  const std::uint32_t first = firstAxis ? 0x3f800000U : 0U;
  const std::uint32_t second = secondAxis ? 0x3f800000U : 0U;
  std::memcpy(vertex.normal, &first, sizeof(first));
  std::memcpy(vertex.tangent, &second, sizeof(second));
}

void CopyQuad(idDrawVert* destination, const idDrawVert (&vertices)[4]) {
  std::memcpy(destination, vertices, sizeof(vertices));
}

void ReferenceGeometry(idRenderModelSurface* surface, int firstVertex, int numVertices,
                       int firstIndex, int numIndices) {
  surface->geometry->numVerts = numVertices;
  surface->geometry->numIndexes = numIndices;
  surface->geometry->vertexBuffer.Reference(
      &idRenderModelCuttableBeams::vertexBuffer[idRenderModelCuttableBeams::currentBuffer]);
  surface->geometry->indexBuffer.Reference(&idRenderModelCuttableBeams::indexBuffer,
                                           firstIndex * 2, numIndices * 2);
  (void)firstVertex;
}

void BuildMarkerVertices(idDrawVert (&vertices)[4], const idVec3& point,
                         const idVec3& graphLeft, const idVec3& graphUp) {
  const idVec3 halfLeft = graphLeft * (kBeamWidth * 0.5f);
  const idVec3 halfUp = graphUp * (kBeamWidth * 0.5f);
  InitializeVertex(vertices[0], point - halfLeft - halfUp);
  InitializeVertex(vertices[1], point + halfLeft - halfUp);
  InitializeVertex(vertices[2], point - halfLeft + halfUp);
  InitializeVertex(vertices[3], point + halfLeft + halfUp);
  SetCornerEncoding(vertices[0], true, true);
  SetCornerEncoding(vertices[1], false, true);
  SetCornerEncoding(vertices[2], true, false);
  SetCornerEncoding(vertices[3], false, false);
}

template <typename PointAt>
void BuildMarkerSurface(idRenderModelCuttableBeams* model, idRenderModelSurface* surface,
                        const idMaterial* material, int pointCount, PointAt pointAt) {
  const int verticesPerPoint = model->useThickness ? 8 : 4;
  const int count = std::min(pointCount,
                             (idRenderModelCuttableBeams::MAX_BEAM_VERTICES -
                              idRenderModelCuttableBeams::vertexOffset) /
                                 verticesPerPoint);
  if (surface == nullptr)
    return;
  if (material == nullptr || count <= 0) {
    ClearGeometry(surface, true);
    return;
  }

  idVec3 graphNormal{};
  idVec3 graphLeft{};
  idVec3 graphUp{};
  model->graphManager->GetNormalVectors(model->graphHandle, graphNormal, graphLeft, graphUp);
  const int firstVertex = idRenderModelCuttableBeams::vertexOffset;
  const int firstIndex = 6 * (firstVertex >> 2);
  idDrawVert* output = idRenderModelCuttableBeams::deferredVertices + firstVertex;
  for (int index = 0; index < count; ++index) {
    idDrawVert vertices[4];
    BuildMarkerVertices(vertices, pointAt(index), graphLeft, graphUp);
    CopyQuad(output, vertices);
    output += 4;
    if (model->useThickness) {
      SwapValues(&vertices[0], &vertices[1]);
      SwapValues(&vertices[2], &vertices[3]);
      for (idDrawVert& vertex : vertices)
        vertex.xyz -= graphNormal;
      CopyQuad(output, vertices);
      output += 4;
    }
  }

  const int numVertices = count * verticesPerPoint;
  const int numIndices = 6 * (numVertices >> 2);
  idRenderModelCuttableBeams::vertexOffset += numVertices;
  surface->material = material;
  ReferenceGeometry(surface, firstVertex, numVertices, firstIndex, numIndices);
}

idVec3 NormalizeRetail(const idVec3& value) {
  const float lengthSquared = value.LengthSqr();
  const float inverseLength =
      1.0f / std::sqrt(std::max(lengthSquared, std::numeric_limits<float>::min()));
  return value * inverseLength;
}

} // namespace

int idRenderModelCuttableBeams::currentBuffer = 0;
int idRenderModelCuttableBeams::vertexOffset = 0;
idDrawVert* idRenderModelCuttableBeams::deferredVertices = nullptr;
idVertexBuffer idRenderModelCuttableBeams::vertexBuffer[NUM_VERTEX_BUFFERS]{};
idIndexBuffer idRenderModelCuttableBeams::indexBuffer{};

graphTree_t::graphTree_t() {
  positions.granularity = 1;
  intersections.granularity = 1;
  endpoints.granularity = 1;
  contours.granularity = 1;
  branches.granularity = 1;
}

graphTree_t::~graphTree_t() = default;

template <typename T> void SwapValues(T* left, T* right) {
  std::swap(*left, *right);
}

template void SwapValues<idDrawVert>(idDrawVert* left, idDrawVert* right);

void idRenderModelCuttableBeams::Create(const idMaterial* materialIncision_,
                                        const idMaterial* materialIncisionInside_,
                                        const idMaterial* materialIncisionCross_,
                                        bool useThickness_) {
  materialIncision = materialIncision_;
  materialIncisionInside = materialIncisionInside_;
  useThickness = useThickness_;
  materialIncisionCross = materialIncisionCross_;
}

void idRenderModelCuttableBeams::EndFrame() {
  vertexBuffer[currentBuffer].UnmapBuffer();
  deferredVertices = nullptr;
}

void idRenderModelCuttableBeams::Update(idCutterGraphManager* graphManager_,
                                        int graphHandle_) {
  graphManager = graphManager_;
  graphHandle = graphHandle_;
  CommitThisFrame();
}

idRenderModelCuttableBeams::idRenderModelCuttableBeams() {
  materialIncision = nullptr;
  materialIncisionInside = nullptr;
  materialIncisionCross = nullptr;
  graphManager = nullptr;
  graphHandle = -1;
  useThickness = false;
  renderModelType = RENDERMODEL_CUTTABLEBEAMS;
  recoveredParms[116] |= 0x80U;
  recoveredParms[117] |= 0x01U;
  recoveredParms[118] |= 0x20U;
  recoveredParms[120] |= 0x04U;
  constexpr int modelDepthSortAfterWorld = 1;
  std::memcpy(recoveredParms + 172, &modelDepthSortAfterWorld,
              sizeof(modelDepthSortAfterWorld));
  referenceBounds.b[0] = {-1.0f, -1.0f, -1.0f};
  referenceBounds.b[1] = {1.0f, 1.0f, 1.0f};

  for (int index = 0; index < SURFACE_MAX; ++index) {
    idRenderModelSurface surface{};
    surface.geometry = new idTriangles;
    surface.geometry->bounds = referenceBounds;
    surface.geometry->vertexBuffer.Reference(vertexBuffer);
    surface.geometry->indexBuffer.Reference(&indexBuffer);
    surface.geometry->numVerts = 0;
    surface.geometry->numIndexes = 0;
    AddSurface(surface);
  }
  FinishSurfaces();
}

void idRenderModelCuttableBeams::Init() {
  for (idVertexBuffer& buffer : vertexBuffer)
    buffer.AllocBufferObject(nullptr, 0x80000, 31, BU_DYNAMIC);
  indexBuffer.AllocBufferObject(nullptr, 49152, BU_DYNAMIC);
  auto* indices = static_cast<std::uint16_t*>(indexBuffer.MapBuffer(BM_WRITE));
  for (int quad = 0; quad < MAX_BEAM_QUADS; ++quad) {
    const std::uint16_t base = static_cast<std::uint16_t>(quad * 4);
    const int offset = quad * 6;
    indices[offset + 0] = base;
    indices[offset + 1] = static_cast<std::uint16_t>(base + 2);
    indices[offset + 2] = static_cast<std::uint16_t>(base + 3);
    indices[offset + 3] = base;
    indices[offset + 4] = static_cast<std::uint16_t>(base + 3);
    indices[offset + 5] = static_cast<std::uint16_t>(base + 1);
  }
  indexBuffer.UnmapBuffer();
}

void idRenderModelCuttableBeams::Shutdown() {
  if (idLib::headless)
    return;
  if (deferredVertices != nullptr)
    vertexBuffer[currentBuffer].UnmapBuffer();
  for (idVertexBuffer& buffer : vertexBuffer)
    buffer.FreeBufferObject();
  indexBuffer.FreeBufferObject();
}

void idRenderModelCuttableBeams::StartFrame() {
  currentBuffer = (currentBuffer + 1) % NUM_VERTEX_BUFFERS;
  vertexOffset = 0;
  deferredVertices = static_cast<idDrawVert*>(
      vertexBuffer[currentBuffer].MapBuffer(BM_WRITE_NOSYNC));
}

void idRenderModelCuttableBeams::SetBounds(const idBounds& bounds) {
  referenceBounds.b[0] = bounds.b[0] - idVec3{1.0f, 1.0f, 1.0f};
  referenceBounds.b[1] = bounds.b[1] + idVec3{1.0f, 1.0f, 1.0f};
  for (int index = 0; index < surfaces.num; ++index) {
    if (surfaces.list[index].geometry != nullptr)
      surfaces.list[index].geometry->bounds = referenceBounds;
  }
}

void idRenderModelCuttableBeams::Save(idFile* file) const {
  idRenderModel::Save(file);
  std::uint8_t hasMaterial = materialIncision != nullptr ? 1U : 0U;
  file->Write(&hasMaterial, sizeof(hasMaterial));
  if (hasMaterial != 0)
    file->WriteString(materialIncision->GetName());
  hasMaterial = materialIncisionCross != nullptr ? 1U : 0U;
  file->Write(&hasMaterial, sizeof(hasMaterial));
  if (hasMaterial != 0)
    file->WriteString(materialIncisionCross->GetName());
}

bool idRenderModelCuttableBeams::Load(idFile* file) {
  if (!idRenderModel::Load(file))
    return false;
  std::uint8_t hasMaterial = 0;
  if (file->Read(&hasMaterial, sizeof(hasMaterial)) == 0)
    return false;
  if (hasMaterial != 0) {
    idStr materialName;
    if (file->ReadString(materialName) == 0)
      return false;
    materialIncision = static_cast<const idMaterial*>(
        idMaterial::resourceList.FindWithInheritance(materialName.data, true, true));
  }
  hasMaterial = 0;
  if (file->Read(&hasMaterial, sizeof(hasMaterial)) == 0)
    return false;
  if (hasMaterial == 0)
    return true;
  idStr materialName;
  if (file->ReadString(materialName) == 0)
    return false;
  materialIncisionCross = static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(materialName.data, true, true));
  return true;
}

void idRenderModelCuttableBeams::BuildIncisionInsides(const graphTree_t& graphTree) {
  idRenderModelSurface* surface = GetSurface(this, SURFACE_INCISIONINSIDES);
  if (surface == nullptr)
    return;
  if (materialIncisionInside == nullptr) {
    ClearGeometry(surface, true);
    return;
  }

  idVec3 graphNormal{};
  idVec3 graphLeft{};
  idVec3 graphUp{};
  graphManager->GetNormalVectors(graphHandle, graphNormal, graphLeft, graphUp);
  const int firstVertex = vertexOffset;
  const int firstIndex = 6 * (firstVertex >> 2);
  int numVertices = 0;
  int numIndices = 0;
  for (int contourIndex = 0; contourIndex < graphTree.contours.num; ++contourIndex) {
    const graphBranch_t contour = graphTree.contours.list[contourIndex];
    if (contour.num <= 0 || vertexOffset + 4 * contour.num > MAX_BEAM_VERTICES)
      continue;
    idDrawVert* output = deferredVertices + vertexOffset;
    const int end = contour.index + contour.num - 1;
    for (int point = contour.index; point < end; ++point) {
      idDrawVert vertices[4];
      InitializeVertex(vertices[0], graphTree.positions.list[point], {0.0f, 0.0f});
      InitializeVertex(vertices[1], graphTree.positions.list[point + 1], {1.0f, 0.0f});
      InitializeVertex(vertices[2], graphTree.positions.list[point] - graphNormal,
                       {0.0f, 1.0f});
      InitializeVertex(vertices[3], graphTree.positions.list[point + 1] - graphNormal,
                       {1.0f, 1.0f});
      CopyQuad(output, vertices);
      output += 4;
    }
    idDrawVert vertices[4];
    InitializeVertex(vertices[0], graphTree.positions.list[end], {0.0f, 0.0f});
    InitializeVertex(vertices[1], graphTree.positions.list[contour.index], {1.0f, 0.0f});
    InitializeVertex(vertices[2], graphTree.positions.list[end] - graphNormal, {0.0f, 1.0f});
    InitializeVertex(vertices[3], graphTree.positions.list[contour.index] - graphNormal,
                     {1.0f, 1.0f});
    CopyQuad(output, vertices);
    vertexOffset += 4 * contour.num;
    numVertices += 4 * contour.num;
    numIndices += 6 * contour.num;
  }
  surface->material = materialIncisionInside;
  ReferenceGeometry(surface, firstVertex, numVertices, firstIndex, numIndices);
  (void)graphLeft;
  (void)graphUp;
}

void idRenderModelCuttableBeams::BuildIncisionRepoints(const idList<int>& repoints,
                                                       const graphTree_t& graphTree) {
  BuildMarkerSurface(this, GetSurface(this, SURFACE_INCISIONREPOINT), materialIncisionCross,
                     repoints.num,
                     [&](int index) { return graphTree.positions.list[repoints.list[index]]; });
}

void idRenderModelCuttableBeams::BuildIncisionCrossings(const graphTree_t& graphTree) {
  BuildMarkerSurface(this, GetSurface(this, SURFACE_INCISIONCROSSINGS), materialIncisionCross,
                     graphTree.intersections.num,
                     [&](int index) { return graphTree.intersections.list[index]; });
}

void idRenderModelCuttableBeams::BuildIncisionEndpoints(const graphTree_t& graphTree) {
  BuildMarkerSurface(this, GetSurface(this, SURFACE_INCISIONENDPOINTS), materialIncisionCross,
                     graphTree.endpoints.num,
                     [&](int index) { return graphTree.endpoints.list[index]; });
}

void idRenderModelCuttableBeams::BuildIncisions(const graphTree_t& graphTree) {
  idRenderModelSurface* surface = GetSurface(this, SURFACE_INCISIONS);
  if (surface == nullptr)
    return;
  if (materialIncision == nullptr) {
    ClearGeometry(surface, true);
    return;
  }

  idVec3 graphNormal{};
  idVec3 graphLeft{};
  idVec3 graphUp{};
  graphManager->GetNormalVectors(graphHandle, graphNormal, graphLeft, graphUp);
  const float halfWidth = kBeamWidth * 0.5f;
  const int firstVertex = vertexOffset;
  const int firstIndex = 6 * (firstVertex >> 2);
  int numVertices = 0;
  int numIndices = 0;
  idStaticList<int, 4096> repoints;
  repoints.granularity = 1;

  for (int branchIndex = 0; branchIndex < graphTree.branches.num; ++branchIndex) {
    const graphBranch_t branch = graphTree.branches.list[branchIndex];
    if (branch.num <= 0)
      continue;
    const int segmentCount = branch.num - 1;
    const int copies = useThickness ? 2 : 1;
    const int branchVertices = 4 * segmentCount * copies;
    if (vertexOffset + branchVertices > MAX_BEAM_VERTICES)
      continue;

    idDrawVert* output = deferredVertices + vertexOffset;
    idVec3 oldForward{};
    idVec3 oldLeft{};
    float textureOffset = 0.0f;
    for (int segment = 0; segment < segmentCount; ++segment) {
      const int pointIndex = branch.index + segment;
      const idVec3 start = graphTree.positions.list[pointIndex];
      const idVec3 end = graphTree.positions.list[pointIndex + 1];
      const idVec3 delta = end - start;
      const idVec3 left = NormalizeRetail(delta.Cross(graphNormal)) * halfWidth;
      const idVec3 forward = NormalizeRetail(delta);
      const float oldTextureOffset = textureOffset;
      textureOffset += std::sqrt(std::max(delta.LengthSqr(),
                                          std::numeric_limits<float>::min())) /
                       kBeamRepeat;

      idVec3 startLeft = oldLeft;
      if (segment == 0) {
        startLeft = left;
      } else if (oldForward * forward < kBeamRepoint) {
        startLeft = left;
        repoints.Append(pointIndex);
      }

      idDrawVert vertices[4];
      InitializeVertex(vertices[0], start - startLeft, {oldTextureOffset, 1.0f});
      InitializeVertex(vertices[1], start + startLeft, {oldTextureOffset, 0.0f});
      InitializeVertex(vertices[2], end - left, {textureOffset, 1.0f});
      InitializeVertex(vertices[3], end + left, {textureOffset, 0.0f});
      CopyQuad(output, vertices);
      output += 4;
      if (useThickness) {
        SwapValues(&vertices[0], &vertices[1]);
        SwapValues(&vertices[2], &vertices[3]);
        for (idDrawVert& vertex : vertices)
          vertex.xyz -= graphNormal;
        CopyQuad(output, vertices);
        output += 4;
      }
      oldForward = forward;
      oldLeft = left;
    }
    vertexOffset += branchVertices;
    numVertices += branchVertices;
    numIndices += 6 * segmentCount * copies;
  }

  surface->material = materialIncision;
  ReferenceGeometry(surface, firstVertex, numVertices, firstIndex, numIndices);
  if (useThickness)
    BuildIncisionInsides(graphTree);
  BuildIncisionRepoints(repoints, graphTree);
  (void)graphLeft;
  (void)graphUp;
}

bool idRenderModelCuttableBeams::UpdateInView(const idRenderView*, const idRenderView*,
                                              idRenderModelUpdateTools*) {
  if (graphManager == nullptr || graphHandle < 0 || surfaces.num <= 0 ||
      !graphManager->IsGraphValid(graphHandle))
    return false;
  graphTree_t graphTree;
  graphManager->BuildGraphTree(graphHandle, &graphTree);
  for (int index = 0; index < SURFACE_MAX; ++index)
    ClearGeometry(GetSurface(this, index), false);
  const int mode = g_cuttableBeamIncisions.GetInteger();
  if (static_cast<unsigned>(mode) < 2U)
    BuildIncisions(graphTree);
  if (mode == 0 || mode == 2)
    BuildIncisionCrossings(graphTree);
  if (mode == 0 || mode == 3)
    BuildIncisionEndpoints(graphTree);
  CommitThisFrame();
  return true;
}
