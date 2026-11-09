#include "engine/models/discreteanimation/rendermodelmanyboxes.h"

#include "engine/renderer/renderersystem.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>

namespace {

constexpr int kMaximumBoxes = 1024;
constexpr int kVerticesPerBox = 24;
constexpr int kIndexesPerBox = 36;
constexpr int kMaximumVertices = kMaximumBoxes * kVerticesPerBox;
constexpr int kMaximumIndexes = kMaximumBoxes * kIndexesPerBox;
constexpr int kVertexBytes = kMaximumVertices * sizeof(idDrawVert);
constexpr int kIndexBytes = kMaximumIndexes * sizeof(std::uint16_t);

std::uint8_t PackNormalComponent(float component) {
  const int packed = static_cast<int>((component + 1.0f) * 127.5f + 0.5f);
  return static_cast<std::uint8_t>(std::clamp(packed, 0, 255));
}

void SetPackedNormal(idDrawVert* verts, int firstVertex, const idVec3& axis,
                     bool negative) {
  std::uint8_t packed[4] = {
      PackNormalComponent(axis.x), PackNormalComponent(axis.y),
      PackNormalComponent(axis.z), 0};
  if (negative) {
    packed[0] = static_cast<std::uint8_t>(0U - packed[0]);
    packed[1] = static_cast<std::uint8_t>(0U - packed[1]);
    packed[2] = static_cast<std::uint8_t>(0U - packed[2]);
  }
  for (int index = 0; index < 4; ++index)
    std::memcpy(verts[firstVertex + index].normal, packed, sizeof(packed));
}

idVec3 Scale(const idVec3& value, float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

idVec3 Corner(const idVec3& center, const idVec3& axis0, float sign0,
              const idVec3& axis1, float sign1, const idVec3& axis2,
              float sign2) {
  return {center.x + sign0 * axis0.x + sign1 * axis1.x + sign2 * axis2.x,
          center.y + sign0 * axis0.y + sign1 * axis1.y + sign2 * axis2.y,
          center.z + sign0 * axis0.z + sign1 * axis1.z + sign2 * axis2.z};
}

void WriteBox(idDrawVert* verts, const idRenderModelManyBoxes::box_t& box) {
  const idVec3 axis0 = Scale(box.orientation[0], box.radius);
  const idVec3 axis1 = Scale(box.orientation[1], box.radius);
  const idVec3 axis2 = Scale(box.orientation[2], box.radius);

  SetPackedNormal(verts, 0, box.orientation[0], false);
  SetPackedNormal(verts, 4, box.orientation[0], true);
  SetPackedNormal(verts, 8, box.orientation[1], false);
  SetPackedNormal(verts, 12, box.orientation[1], true);
  SetPackedNormal(verts, 16, box.orientation[2], false);
  SetPackedNormal(verts, 20, box.orientation[2], true);

  verts[0].xyz = Corner(box.position, axis0, 1.0f, axis1, -1.0f,
                        axis2, 1.0f);
  verts[1].xyz = Corner(box.position, axis0, 1.0f, axis1, 1.0f,
                        axis2, 1.0f);
  verts[2].xyz = Corner(box.position, axis0, 1.0f, axis1, 1.0f,
                        axis2, -1.0f);
  verts[3].xyz = Corner(box.position, axis0, 1.0f, axis1, -1.0f,
                        axis2, -1.0f);

  verts[4].xyz = Corner(box.position, axis0, -1.0f, axis1, -1.0f,
                        axis2, 1.0f);
  verts[5].xyz = Corner(box.position, axis0, -1.0f, axis1, -1.0f,
                        axis2, -1.0f);
  verts[6].xyz = Corner(box.position, axis0, -1.0f, axis1, 1.0f,
                        axis2, -1.0f);
  verts[7].xyz = Corner(box.position, axis0, -1.0f, axis1, 1.0f,
                        axis2, 1.0f);

  verts[8].xyz = Corner(box.position, axis0, 1.0f, axis1, 1.0f,
                        axis2, -1.0f);
  verts[9].xyz = Corner(box.position, axis0, 1.0f, axis1, 1.0f,
                        axis2, 1.0f);
  verts[10].xyz = Corner(box.position, axis0, -1.0f, axis1, 1.0f,
                         axis2, 1.0f);
  verts[11].xyz = Corner(box.position, axis0, -1.0f, axis1, 1.0f,
                         axis2, -1.0f);

  verts[12].xyz = Corner(box.position, axis0, 1.0f, axis1, -1.0f,
                         axis2, -1.0f);
  verts[13].xyz = Corner(box.position, axis0, -1.0f, axis1, -1.0f,
                         axis2, -1.0f);
  verts[14].xyz = Corner(box.position, axis0, -1.0f, axis1, -1.0f,
                         axis2, 1.0f);
  verts[15].xyz = Corner(box.position, axis0, 1.0f, axis1, -1.0f,
                         axis2, 1.0f);

  verts[16].xyz = Corner(box.position, axis0, -1.0f, axis1, 1.0f,
                         axis2, 1.0f);
  verts[17].xyz = Corner(box.position, axis0, 1.0f, axis1, 1.0f,
                         axis2, 1.0f);
  verts[18].xyz = Corner(box.position, axis0, 1.0f, axis1, -1.0f,
                         axis2, 1.0f);
  verts[19].xyz = Corner(box.position, axis0, -1.0f, axis1, -1.0f,
                         axis2, 1.0f);

  verts[20].xyz = Corner(box.position, axis0, -1.0f, axis1, 1.0f,
                         axis2, -1.0f);
  verts[21].xyz = Corner(box.position, axis0, -1.0f, axis1, -1.0f,
                         axis2, -1.0f);
  verts[22].xyz = Corner(box.position, axis0, 1.0f, axis1, -1.0f,
                         axis2, -1.0f);
  verts[23].xyz = Corner(box.position, axis0, 1.0f, axis1, 1.0f,
                         axis2, -1.0f);
}

} // namespace

idRenderModelManyBoxes::idRenderModelManyBoxes()
    : gameTime(0), material(nullptr), boxes{}, vertexBuffers{}, triangles{},
      indexBuffer{} {
  renderModelType = RENDERMODEL_MANYBOXES;

  auto indices = std::make_unique<std::uint16_t[]>(kMaximumIndexes);
  for (int vertex = 0, index = 0; vertex < kMaximumVertices;
       vertex += 4, index += 6) {
    indices[index + 0] = static_cast<std::uint16_t>(vertex + 0);
    indices[index + 1] = static_cast<std::uint16_t>(vertex + 1);
    indices[index + 2] = static_cast<std::uint16_t>(vertex + 2);
    indices[index + 3] = static_cast<std::uint16_t>(vertex + 0);
    indices[index + 4] = static_cast<std::uint16_t>(vertex + 2);
    indices[index + 5] = static_cast<std::uint16_t>(vertex + 3);
  }
  indexBuffer.AllocBufferObject(indices.get(), kIndexBytes, BU_DEFAULT);

  for (idVertexBuffer& vertexBuffer : vertexBuffers) {
    vertexBuffer.AllocBufferObject(nullptr, kVertexBytes, 31, BU_DYNAMIC);
    auto* verts = static_cast<idDrawVert*>(vertexBuffer.MapBuffer(BM_WRITE));
    if (verts != nullptr) {
      std::memset(verts, 0, static_cast<std::size_t>(kVertexBytes));
      for (int vertex = 0; vertex < kMaximumVertices; vertex += 4) {
        verts[vertex + 0].st = {0.0f, 0.0f};
        verts[vertex + 1].st = {1.0f, 0.0f};
        verts[vertex + 2].st = {1.0f, 1.0f};
        verts[vertex + 3].st = {0.0f, 1.0f};
        for (int corner = 0; corner < 4; ++corner)
          std::memset(verts[vertex + corner].color, 0xff, 4);
      }
    }
    vertexBuffer.UnmapBuffer();
  }

  triangles.indexBuffer.Reference(indexBuffer);
  triangles.bounds.b[0] = {-99999.0f, -99999.0f, -99999.0f};
  triangles.bounds.b[1] = {99999.0f, 99999.0f, 99999.0f};

  if ((useDeferredPosition & (deferredPositionInitialized >> 1) & 1U) == 0)
    g.origin = vec3_origin;
  deferredOrigin = vec3_origin;
  if ((useDeferredPosition & (deferredPositionInitialized >> 1) & 1U) == 0)
    g.axis = mat3_identity;
  deferredAxis = mat3_identity;

  FreeSurfaces();
  g.flags0 |= 0x80U;
  g.flags1 |= 0x20U;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;
}

idRenderModelManyBoxes::~idRenderModelManyBoxes() = default;

bool idRenderModelManyBoxes::CommitSubclass() {
  surfaces.Clear();
  idVertexBuffer& vertexBuffer =
      vertexBuffers[renderSystem->FrameNumber() & 1];
  auto* verts =
      static_cast<idDrawVert*>(vertexBuffer.MapBuffer(BM_WRITE_NOSYNC));

  int numVerts = 0;
  int numIndexes = 0;
  if (verts != nullptr) {
    for (int boxIndex = 0; boxIndex < boxes.Num(); ++boxIndex) {
      WriteBox(verts + numVerts, boxes[boxIndex]);
      numVerts += kVerticesPerBox;
      numIndexes += kIndexesPerBox;
    }
  }

  triangles.numVerts = numVerts;
  triangles.numIndexes = numIndexes;
  triangles.vertexBuffer.Reference(vertexBuffer);

  idRenderModelSurface surface{};
  surface.material = material;
  surface.geometry = &triangles;
  surface.geometryIsReference = true;
  surfaces.Append(surface);

  vertexBuffer.UnmapBuffer();
  return false;
}

template int idList<idRenderModelSurface, 85>::Append(
    const idRenderModelSurface& value);
