#include "engine/models/rendermodelbeam.h"

#include "engine/renderer/render.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

idCVar r_skipBeams("r_skipBeams", "0", CVAR_BOOL,
                   "skip render model beam system");

idVertexBuffer idRenderModelBeam::preAllocatedVertexBuffer[2];
idIndexBuffer idRenderModelBeam::preAllocatedIndexBuffer;
bool idRenderModelBeam::preAllocatedBufferInUse = false;

namespace {

constexpr int kPreAllocatedBeamQuads = 8128;
beamFrameNumberCallback_t frameNumberCallback = nullptr;

std::vector<std::uint16_t> BuildBeamIndexes(int quadCount) {
  std::vector<std::uint16_t> indexes(static_cast<std::size_t>(quadCount) * 6U);
  for (int quad = 0; quad < quadCount; ++quad) {
    const int vertex = quad * 4;
    const int index = quad * 6;
    indexes[index + 0] = static_cast<std::uint16_t>(vertex + 0);
    indexes[index + 1] = static_cast<std::uint16_t>(vertex + 2);
    indexes[index + 2] = static_cast<std::uint16_t>(vertex + 3);
    indexes[index + 3] = static_cast<std::uint16_t>(vertex + 0);
    indexes[index + 4] = static_cast<std::uint16_t>(vertex + 3);
    indexes[index + 5] = static_cast<std::uint16_t>(vertex + 1);
  }
  return indexes;
}

void AllocateBeamBuffers(idVertexBuffer (&vertexBuffers)[2],
                         idIndexBuffer& indexBuffer, int quadCount) {
  const int vertexBytes = quadCount * 4 * static_cast<int>(sizeof(idDrawVert));
  for (idVertexBuffer& buffer : vertexBuffers)
    buffer.AllocBufferObject(nullptr, vertexBytes, 31, BU_DYNAMIC);

  const std::vector<std::uint16_t> indexes = BuildBeamIndexes(quadCount);
  indexBuffer.AllocBufferObject(indexes.data(),
                                static_cast<int>(indexes.size() * sizeof(indexes[0])),
                                BU_STATIC);
}

void ConfigureStaticList(idList<beamNodeParms_t, 5>& list,
                         beamNodeParms_t* storage, int capacity) {
  list.list = storage;
  list.num = 0;
  list.size = capacity;
  list.granularity = 1;
  list.memTag = 5;
  list.listStatic = 1;
}

void ConfigureStaticList(idList<beam_t, 5>& list, beam_t* storage,
                         int capacity) {
  list.list = storage;
  list.num = 0;
  list.size = capacity;
  list.granularity = 1;
  list.memTag = 5;
  list.listStatic = 1;
}

idVec3 Subtract(const idVec3& left, const idVec3& right) {
  return {left.x - right.x, left.y - right.y, left.z - right.z};
}

idVec3 Cross(const idVec3& left, const idVec3& right) {
  return {left.y * right.z - left.z * right.y,
          left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

idVec3 Scale(const idVec3& value, float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

idVec3 Add(const idVec3& left, const idVec3& right) {
  return {left.x + right.x, left.y + right.y, left.z + right.z};
}

void WriteOrientedQuad(idDrawVert* destination, const beamNodeParms_t& node,
                       beamOrientType_t orientType, const idRenderView* view,
                       idVec3& oldLeft, bool firstNode) {
  idDrawVert local[4]{};
  for (idDrawVert& vertex : local)
    std::memset(vertex.color, 0xFF, sizeof(vertex.color));

  if (orientType == BEAM_ORIENT_EXPLICIT) {
    local[0].xyz = node.startPos;
    local[1].xyz = node.startPos2;
    local[2].xyz = node.endPos;
    local[3].xyz = node.endPos2;
  } else {
    const idVec3 direction = Subtract(node.endPos, node.startPos);
    const idVec3 orientation = orientType == BEAM_ORIENT_AXIS
                                   ? node.orientVec
                                   : Subtract(view->r.vieworg, node.startPos);
    const idVec3 cross = Cross(direction, orientation);
    const float lengthSquared = cross.x * cross.x + cross.y * cross.y +
                                cross.z * cross.z;
    const float inverseLength =
        1.0f / std::sqrt(std::max(lengthSquared, 1.17549435e-38f));
    const idVec3 left = Scale(cross, inverseLength * node.halfWidth);
    const idVec3 startLeft = firstNode ? left : oldLeft;
    local[0].xyz = Subtract(node.startPos, startLeft);
    local[1].xyz = Add(node.startPos, startLeft);
    local[2].xyz = Subtract(node.endPos, left);
    local[3].xyz = Add(node.endPos, left);
    oldLeft = left;
  }

  local[0].st = {node.sMinMax.x, node.tMinMax.x};
  local[1].st = {node.sMinMax.x, node.tMinMax.y};
  local[2].st = {node.sMinMax.y, node.tMinMax.x};
  local[3].st = {node.sMinMax.y, node.tMinMax.y};
  for (int vertex = 0; vertex < 4; ++vertex) {
    std::memcpy(local[vertex].color, node.color[vertex >> 1], 4);
    std::memcpy(local[vertex].tangent, node.tangent[vertex], 3);
  }
  std::memcpy(destination, local, sizeof(local));
}

} // namespace

void SetRenderModelBeamFrameNumberCallback(beamFrameNumberCallback_t callback) {
  frameNumberCallback = callback;
}

void idRenderModelBeam::Shutdown() {
  for (idVertexBuffer& buffer : preAllocatedVertexBuffer)
    buffer.FreeBufferObject();
  preAllocatedIndexBuffer.FreeBufferObject();
  preAllocatedBufferInUse = false;
}

void idRenderModelBeam::Init() {
  AllocateBeamBuffers(preAllocatedVertexBuffer, preAllocatedIndexBuffer,
                      kPreAllocatedBeamQuads);
  preAllocatedBufferInUse = false;
}

idRenderModelBeam::idRenderModelBeam(bool large) {
  renderModelType = RENDERMODEL_BEAM;
  if (large) {
    maxInfo.maxSurfaces = 32;
    maxInfo.maxBeams = 64;
    maxInfo.maxBeamNodes = 128;
  } else {
    maxInfo.maxSurfaces = 2;
    maxInfo.maxBeams = 4;
    maxInfo.maxBeamNodes = 4;
  }
  maxInfo.maxBeamQuads =
      maxInfo.maxBeams * (maxInfo.maxBeamNodes - 1);

  g.axis = mat3_identity;
  deferredAxis = mat3_identity;
  g.origin = vec3_origin;
  deferredOrigin = vec3_origin;
  FreeSurfaces();
  g.flags0 |= 0x80U;
  g.flags1 |= 0x20U;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;

  if (preAllocatedBufferInUse) {
    AllocateBeamBuffers(vertexBuffer, indexBuffer, maxInfo.maxBeamQuads);
    usesPreAllocatedBuffer = false;
  } else {
    for (int index = 0; index < 2; ++index)
      vertexBuffer[index].Reference(&preAllocatedVertexBuffer[index]);
    indexBuffer.Reference(&preAllocatedIndexBuffer);
    preAllocatedBufferInUse = true;
    usesPreAllocatedBuffer = true;
  }

  triangles = new idTriangles[maxInfo.maxSurfaces];
  for (int index = 0; index < maxInfo.maxSurfaces; ++index) {
    triangles[index].bounds.b[0] = {-99999.0f, -99999.0f, -99999.0f};
    triangles[index].bounds.b[1] = {99999.0f, 99999.0f, 99999.0f};
  }

  for (idList<beam_t, 5>& buffer : beamBuffer) {
    beam_t* beams = new beam_t[maxInfo.maxBeams];
    for (int beam = 0; beam < maxInfo.maxBeams; ++beam) {
      auto* nodes = new beamNodeParms_t[maxInfo.maxBeamNodes];
      ConfigureStaticList(beams[beam].nodes, nodes, maxInfo.maxBeamNodes);
    }
    ConfigureStaticList(buffer, beams, maxInfo.maxBeams);
  }
  beamRenderBufferIndex = 0;
}

idRenderModelBeam::~idRenderModelBeam() {
  if (usesPreAllocatedBuffer)
    preAllocatedBufferInUse = false;

  for (idVertexBuffer& buffer : vertexBuffer)
    buffer.FreeBufferObject();
  indexBuffer.FreeBufferObject();

  delete[] triangles;
  triangles = nullptr;

  for (idList<beam_t, 5>& buffer : beamBuffer) {
    beam_t* beams = buffer.list;
    if (beams != nullptr) {
      for (int beam = 0; beam < maxInfo.maxBeams; ++beam) {
        delete[] beams[beam].nodes.list;
        beams[beam].nodes.list = nullptr;
        beams[beam].nodes.num = 0;
        beams[beam].nodes.size = 0;
      }
      delete[] beams;
    }
    buffer.list = nullptr;
    buffer.num = 0;
    buffer.size = 0;
  }
}

bool idRenderModelBeam::UpdateInView(const idRenderView* currentView,
                                     const idRenderView*,
                                     idRenderModelUpdateTools*) {
  surfaces.SetNum(0);
  if (r_skipBeams.valueInteger != 0)
    return false;

  const int frameNumber = frameNumberCallback != nullptr
                              ? frameNumberCallback()
                              : 0;
  idVertexBuffer& activeVertexBuffer = vertexBuffer[frameNumber & 1];
  auto* output = static_cast<idDrawVert*>(activeVertexBuffer.MapBuffer(3));
  int outputVerts = 0;
  int outputIndexes = 0;
  const idList<beam_t, 5>& beams = beamBuffer[beamRenderBufferIndex];

  for (int sourceBeam = 0; sourceBeam < beams.Num(); ++sourceBeam) {
    const idMaterial* material = beams[sourceBeam].material;
    int surfaceIndex = 0;
    for (; surfaceIndex < surfaces.Num(); ++surfaceIndex) {
      if (surfaces[surfaceIndex].material == material)
        break;
    }
    if (surfaceIndex != surfaces.Num())
      continue;

    idRenderModelSurface surface{};
    idTriangles* geometry = &triangles[surfaces.Num()];
    surface.material = material;
    surface.geometry = geometry;
    surface.geometryIsReference = true;
    const int firstVert = outputVerts;
    const int firstIndex = outputIndexes;

    for (int beamIndex = sourceBeam; beamIndex < beams.Num(); ++beamIndex) {
      const beam_t& beam = beams[beamIndex];
      if (beam.material != material)
        continue;
      idVec3 oldLeft{};
      for (int nodeIndex = 0; nodeIndex < beam.nodes.Num(); ++nodeIndex) {
        const beamNodeParms_t& node = beam.nodes[nodeIndex];
        const idVec3 direction = Subtract(node.endPos, node.startPos);
        const float lengthSquared = direction.x * direction.x +
                                    direction.y * direction.y +
                                    direction.z * direction.z;
        if (lengthSquared < 0.0001f)
          continue;

        WriteOrientedQuad(output + outputVerts, node, beam.orientType,
                          currentView, oldLeft, nodeIndex == 0);
        outputVerts += 4;
        outputIndexes += 6;
        if (outputVerts > maxInfo.maxBeamQuads * 4 - 4)
          break;
      }
    }

    if (surfaces.Num() < maxInfo.maxSurfaces - 1) {
      geometry->numVerts = outputVerts - firstVert;
      geometry->numIndexes = outputIndexes - firstIndex;
      geometry->vertexBuffer.Reference(&activeVertexBuffer);
      geometry->indexBuffer.Reference(&indexBuffer, firstIndex * 2,
                                      geometry->numIndexes * 2);
      surfaces.Append(surface);
    }
  }

  activeVertexBuffer.UnmapBuffer();
  return true;
}

void idRenderModelBeam::Update(int) {
  beamBuffer[beamRenderBufferIndex ^ 1].num = 0;
  CommitThisFrame();
}

bool idRenderModelBeam::CommitSubclass() {
  beamBuffer[beamRenderBufferIndex].num = 0;
  beamRenderBufferIndex ^= 1;
  return false;
}

beam_t* idRenderModelBeam::DrawBeamAlloc() {
  if (r_skipBeams.valueInteger != 0)
    return nullptr;
  idList<beam_t, 5>& writeBuffer = beamBuffer[beamRenderBufferIndex ^ 1];
  if (writeBuffer.Num() >= maxInfo.maxBeams)
    return nullptr;
  beam_t* beam = writeBuffer.Alloc();
  beam->material = nullptr;
  beam->nodes.num = 0;
  return beam;
}

void idRenderModelBeam::DrawBeam(beamNodeParms_t* node,
                                 const idMaterial* material,
                                 beamOrientType_t orientType) {
  if (material == nullptr)
    return;
  beam_t* beam = DrawBeamAlloc();
  if (beam == nullptr)
    return;
  beam->material = material;
  beam->orientType = orientType;
  beam->nodes.Append(*node);
}

void idRenderModelBeam::DrawBeam(beamNodeParms_t* nodes, unsigned numNodes,
                                 const idMaterial* material,
                                 beamOrientType_t orientType) {
  if (nodes == nullptr || numNodes == 0 || material == nullptr)
    return;
  beam_t* beam = DrawBeamAlloc();
  if (beam == nullptr)
    return;
  beam->material = material;
  beam->orientType = orientType;
  beam->nodes.num = 0;
  for (unsigned node = 0; node < numNodes; ++node)
    beam->nodes.Append(nodes[node]);
}
