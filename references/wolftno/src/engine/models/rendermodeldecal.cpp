#include "engine/models/rendermodeldecal.h"

#include "engine/renderer/declmaterial.h"
#include "engine/renderer/render.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>
#include <vector>

idCVar r_skipDecals("r_skipDecals", "0", CVAR_BOOL,
                    "skip rendermodel decals");
idCVar r_debugDecals(
    "r_debugDecals", "0", CVAR_INTEGER,
    "1 - To display the decal axis.  2 - To see the axis and obb.");
idCVar r_oldDecalGen(
    "r_oldDecalGen", "0", CVAR_BOOL,
    "1 - Fall back to old GenDecalVerts serially instead of 4 at a time");
idCVar g_decalMinCost(
    "g_decalMinCost", "1", CVAR_INTEGER,
    "Min cost for decals, multiplier with the cost set on decals, if 1 then cost * 1");
idCVar g_decalSphereRadiusSize("g_decalSphereRadiusSize", "0.20", CVAR_FLOAT,
                              "param size * mult = radius size");
idCVar g_decalSphereSizeMax("g_decalSphereSizeMax", "1000.0", CVAR_FLOAT,
                            "Max size of decal, 800 grenade, 16 bullet");
idCVar g_decalBarrierPop("g_decalBarrierPop", "300", CVAR_INTEGER,
                         "Set the barrier for popping on decals");
idCVar g_decalBarrierFade("g_decalBarrierFade", "256", CVAR_INTEGER,
                          "Set the barrier for fading on decals");
idCVar g_decalCollision(
    "g_decalCollision", "1", CVAR_INTEGER,
    "If 0 dont use collision when adding new decals, if 1 check for empty room");
idCVar g_decalRoundOffPosition("g_decalRoundOffPosition", "1", CVAR_INTEGER,
                               "round decal positions");
idCVar g_vehDecalBarrierPop(
    "g_vehDecalBarrierPop", "60", CVAR_INTEGER,
    "Set the barrier for vehicle popping on decals");
idCVar g_vehDecalBarrierFade(
    "g_vehDecalBarrierFade", "30", CVAR_INTEGER,
    "Set the barrier for vehicle fading on decals");

idVertexBuffer idRenderModelDecal::preAllocatedVertexBuffer[2];
idIndexBuffer idRenderModelDecal::preAllocatedIndexBufferBoxes;
idIndexBuffer idRenderModelDecal::preAllocatedIndexBufferQuads;
bool idRenderModelDecal::preAllocatedBufferInUse = false;

// The sphere tree itself belongs to models/glass/spheretree.cpp.  Keep its
// recovered object size at this boundary and use a sidecar for the operations
// exercised by this compiland until that later cpp is ported in sequence.
class idSphereTree {
public:
  std::uint8_t recoveredState[152]{};
};
static_assert(sizeof(idSphereTree) == 152, "idSphereTree ABI drift");

namespace {

constexpr int kPreAllocatedBoxes = 2688;
constexpr int kPreAllocatedQuads = 5376;
constexpr int kPreAllocatedVertexBytes = 688128;
constexpr float kTwoPi = 6.28318530717958647692f;

constexpr idVec3 kSimpleCube[8] = {
    {-1.0f, -1.0f, -1.0f}, {1.0f, -1.0f, -1.0f},
    {1.0f, 1.0f, -1.0f},   {-1.0f, 1.0f, -1.0f},
    {-1.0f, -1.0f, 1.0f},  {1.0f, -1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},    {-1.0f, 1.0f, 1.0f}};
constexpr idVec3 kSimpleQuad[4] = {
    {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f},
    {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}};
constexpr idVec2 kSimpleQuadST[4] = {
    {0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

struct SphereRecord {
  idVec3 center{};
  float radius = 0.0f;
  int userId = 0;
  bool active = false;
};

struct SphereTreeSidecar {
  int maxSpheres = 0;
  std::vector<SphereRecord> spheres;
  std::size_t collisionCursor = 0;
};

std::unordered_map<const idSphereTree*, SphereTreeSidecar> sphereTrees;
idRenderModelDecalRuntimeCallbacks decalCallbacks{};

float DistanceSquared(const idVec3& left, const idVec3& right) {
  const idVec3 delta = left - right;
  return delta.LengthSqr();
}

bool SpheresOverlap(const SphereRecord& sphere, const idVec3& center,
                    float radius) {
  const float combined = sphere.radius + radius;
  return sphere.active &&
         DistanceSquared(sphere.center, center) <= combined * combined;
}

void SphereTreeAlloc(idSphereTree* tree, int maxSpheres) {
  SphereTreeSidecar& state = sphereTrees[tree];
  state.maxSpheres = maxSpheres;
  state.spheres.clear();
  state.spheres.reserve(static_cast<std::size_t>(std::max(0, maxSpheres)));
  state.collisionCursor = 0;
}

int SphereTreeAdd(idSphereTree* tree, const idVec3& center, float radius,
                  int userId) {
  SphereTreeSidecar& state = sphereTrees[tree];
  for (std::size_t index = 0; index < state.spheres.size(); ++index) {
    SphereRecord& sphere = state.spheres[index];
    if (!sphere.active) {
      sphere = {center, radius, userId, true};
      return static_cast<int>(index);
    }
  }
  if (static_cast<int>(state.spheres.size()) >= state.maxSpheres)
    return -1;
  state.spheres.push_back({center, radius, userId, true});
  return static_cast<int>(state.spheres.size() - 1);
}

void SphereTreeRemove(idSphereTree* tree, int index) {
  const auto found = sphereTrees.find(tree);
  if (found == sphereTrees.end() || index < 0 ||
      index >= static_cast<int>(found->second.spheres.size()))
    return;
  found->second.spheres[static_cast<std::size_t>(index)].active = false;
}

bool SphereTreeFirstCollision(idSphereTree* tree, const idVec3& center,
                              float radius, int* userId) {
  SphereTreeSidecar& state = sphereTrees[tree];
  state.collisionCursor = 0;
  while (state.collisionCursor < state.spheres.size()) {
    const SphereRecord& sphere = state.spheres[state.collisionCursor++];
    if (SpheresOverlap(sphere, center, radius)) {
      if (userId != nullptr)
        *userId = sphere.userId;
      return true;
    }
  }
  return false;
}

bool SphereTreeNextCollision(idSphereTree* tree, const idVec3& center,
                             float radius, int* userId) {
  SphereTreeSidecar& state = sphereTrees[tree];
  while (state.collisionCursor < state.spheres.size()) {
    const SphereRecord& sphere = state.spheres[state.collisionCursor++];
    if (SpheresOverlap(sphere, center, radius)) {
      if (userId != nullptr)
        *userId = sphere.userId;
      return true;
    }
  }
  return false;
}

bool SphereTreeLeafCollision(idSphereTree* tree, const idVec3& center,
                             float radius) {
  int ignored = -1;
  return SphereTreeFirstCollision(tree, center, radius, &ignored);
}

std::uint8_t ClampByte(float value) {
  const int converted = static_cast<int>(value);
  return static_cast<std::uint8_t>(
      converted < 0 ? 0 : converted > 255 ? 255 : converted);
}

std::uint8_t EncodeUnitFloat(float value) {
  return ClampByte((value + 1.0f) * 127.5f + 0.5f);
}

idVec3 TransformOffset(const idMat3& axis, const idVec3& offset) {
  return axis.mat[0] * offset.x + axis.mat[1] * offset.y +
         axis.mat[2] * offset.z;
}

void EncodeFrame(idDrawVert& vertex, const idMat3& axis) {
  vertex.normal[0] = EncodeUnitFloat(axis.mat[2].x);
  vertex.normal[1] = EncodeUnitFloat(axis.mat[2].y);
  vertex.normal[2] = EncodeUnitFloat(axis.mat[2].z);
  vertex.tangent[0] = EncodeUnitFloat(axis.mat[0].x);
  vertex.tangent[1] = EncodeUnitFloat(axis.mat[0].y);
  vertex.tangent[2] = EncodeUnitFloat(axis.mat[0].z);

  const idVec3 normal{
      vertex.normal[0] * (2.0f / 255.0f) - 1.0f,
      vertex.normal[1] * (2.0f / 255.0f) - 1.0f,
      vertex.normal[2] * (2.0f / 255.0f) - 1.0f};
  const idVec3 tangent{
      vertex.tangent[0] * (2.0f / 255.0f) - 1.0f,
      vertex.tangent[1] * (2.0f / 255.0f) - 1.0f,
      vertex.tangent[2] * (2.0f / 255.0f) - 1.0f};
  const float handedness = normal.Cross(tangent) * axis.mat[1];
  vertex.tangent[3] = static_cast<std::uint8_t>(
      (vertex.tangent[3] & 0x7FU) | (handedness >= 0.0f ? 0x80U : 0U));
}

float DecalAlpha(const idRenderModelDecal& model,
                 const idRenderModelDecal::decalInfo_t& decal) {
  const int fadeOutStart =
      decal.startTime + decal.lifeTime - static_cast<int>(decal.fadeOutTime);
  const float fadeIn = decal.fadeInTime != 0
                           ? static_cast<float>(model.latchedTime -
                                                decal.fadeInStart) /
                                 static_cast<float>(decal.fadeInTime)
                           : 1.0f;
  const float fadeOut = decal.fadeOutTime != 0
                            ? static_cast<float>(model.latchedTime -
                                                 fadeOutStart) /
                                  static_cast<float>(decal.fadeOutTime)
                            : 0.0f;
  return std::min(255.0f,
                  (1.0f - std::clamp(fadeOut, 0.0f, 1.0f)) *
                      std::clamp(fadeIn, 0.0f, 1.0f) * 255.5f);
}

void GetAtlasInfo(int& width, int& height, int& widthBits, int& heightBits) {
  width = 4096;
  height = 4096;
  widthBits = 12;
  heightBits = 12;
  if (decalCallbacks.getAtlasInfo != nullptr)
    decalCallbacks.getAtlasInfo(&width, &height, &widthBits, &heightBits);
}

void InitializeTriangle(idTriangles& triangle) {
  triangle.generateNormals = false;
  triangle.numIndexes = 0;
  triangle.numVerts = 0;
  triangle.detailOffset = 0;
  triangle.allowGpuHosting = true;
  triangle.vsb.xyzScale = {1.0f, 1.0f, 1.0f};
  triangle.vsb.xyzBias = {};
  triangle.vsb.stScale = {1.0f, 1.0f};
  triangle.vsb.stBias = {};
  triangle.cpuVertexMask = 31;
  triangle.verts = nullptr;
  triangle.indexes = nullptr;
  triangle.vertexMask = 31;
  triangle.bounds.b[0] = {-99999.0f, -99999.0f, -99999.0f};
  triangle.bounds.b[1] = {99999.0f, 99999.0f, 99999.0f};
}

} // namespace

void SetRenderModelDecalRuntimeCallbacks(
    const idRenderModelDecalRuntimeCallbacks& callbacks) {
  decalCallbacks = callbacks;
}

int VertexSizeFromMask(unsigned vertexMask) {
  return static_cast<int>(
      (vertexMask & 4U) + ((vertexMask >> 1U) & 4U) +
      ((vertexMask >> 2U) & 4U) + ((vertexMask >> 9U) & 4U) +
      ((vertexMask & 1U) != 0U ? 12U : 0U) +
      ((vertexMask & 0x20U) != 0U ? 0xFFFFFFFCU : 0U) +
      ((vertexMask & 0x40U) != 0U ? 0xFFFFFFFCU : 0U) +
      ((vertexMask & 0x1000U) != 0U ? 12U : 0U) +
      4U * (vertexMask & 2U));
}

void PackFloat(std::uint8_t* value, float number) {
  const float scaled = number / 1000.0f;
  constexpr float multipliers[4] = {1.0f, 256.0f, 65536.0f,
                                    16777216.0f};
  for (int index = 0; index < 4; ++index) {
    const float component = scaled * multipliers[index];
    value[index] = ClampByte((component - std::floor(component)) * 255.0f);
  }
}

void BuildIndexBufferBoxes(idIndexBuffer* indexBuffer, int maxBoxes) {
  static constexpr std::uint16_t pattern[36] = {
      2, 3, 0, 1, 2, 0, 5, 1, 0, 4, 5, 0,
      7, 4, 0, 3, 7, 0, 1, 5, 6, 2, 1, 6,
      3, 2, 6, 7, 3, 6, 4, 7, 6, 5, 4, 6};
  std::vector<std::uint16_t> indices(
      static_cast<std::size_t>(std::max(0, maxBoxes)) * 36U);
  for (int box = 0; box < maxBoxes; ++box) {
    for (int index = 0; index < 36; ++index) {
      indices[static_cast<std::size_t>(box) * 36U +
              static_cast<std::size_t>(index)] =
          static_cast<std::uint16_t>(pattern[index] + box * 8);
    }
  }
  indexBuffer->AllocBufferObject(
      indices.data(), static_cast<int>(indices.size() * sizeof(indices[0])),
      BU_STATIC);
}

void BuildIndexBufferQuads(idIndexBuffer* indexBuffer, int maxQuads) {
  static constexpr std::uint16_t pattern[6] = {2, 3, 0, 1, 2, 0};
  std::vector<std::uint16_t> indices(
      static_cast<std::size_t>(std::max(0, maxQuads)) * 6U);
  for (int quad = 0; quad < maxQuads; ++quad) {
    for (int index = 0; index < 6; ++index) {
      indices[static_cast<std::size_t>(quad) * 6U +
              static_cast<std::size_t>(index)] =
          static_cast<std::uint16_t>(pattern[index] + quad * 4);
    }
  }
  indexBuffer->AllocBufferObject(
      indices.data(), static_cast<int>(indices.size() * sizeof(indices[0])),
      BU_STATIC);
}

void idRenderModelDecal::Shutdown() {
  for (idVertexBuffer& buffer : preAllocatedVertexBuffer)
    buffer.FreeBufferObject();
  preAllocatedIndexBufferBoxes.FreeBufferObject();
  preAllocatedIndexBufferQuads.FreeBufferObject();
  preAllocatedBufferInUse = false;
}

void idRenderModelDecal::Init() {
  for (idVertexBuffer& buffer : preAllocatedVertexBuffer)
    buffer.AllocBufferObject(nullptr, kPreAllocatedVertexBytes, 31, BU_DYNAMIC);
  BuildIndexBufferBoxes(&preAllocatedIndexBufferBoxes, kPreAllocatedBoxes);
  BuildIndexBufferQuads(&preAllocatedIndexBufferQuads, kPreAllocatedQuads);
  preAllocatedBufferInUse = false;
}

template <typename T>
void idRenderModelDecal::DecalBuffer<T>::Init(int maxAlloc, int popBarrier,
                                               int fadeBarrier,
                                               int freeSpawnExtraSlots) {
  decals = new T[static_cast<std::size_t>(maxAlloc)]{};
  pop = popBarrier;
  fade = fadeBarrier;
  maxItems = popBarrier + freeSpawnExtraSlots;
  tree = new idSphereTree{};
  SphereTreeAlloc(tree, maxItems);
}

template <typename T>
void idRenderModelDecal::DecalBuffer<T>::Update(int currentGameTime,
                                                 int max) {
  int head = range[0];
  if (head == range[1])
    return;

  if (cost > pop) {
    do {
      T& decal = decals[(max - 1) & head];
      cost -= g_decalMinCost.valueInteger * decal.cost;
      range[0] = ++head;
      if (g_decalCollision.valueInteger > 0) {
        SphereTreeRemove(tree, decal.index);
        decal.index = -2;
      }
    } while (head != range[1] && cost > pop);
  }

  if (cost <= fade)
    return;

  int contiguousHead = range[0];
  int examined = contiguousHead;
  const int end =
      examined + std::min(cost - fade, range[1] - contiguousHead);
  while (examined < end) {
    T& decal = decals[(max - 1) & examined];
    if (decal.index == -2) {
      if (contiguousHead == examined) {
        cost -= g_decalMinCost.valueInteger * decal.cost;
        ++contiguousHead;
      }
    } else {
      decal.fadeState = 1;
      if (currentGameTime - decal.startTime >= decal.lifeTime) {
        if (g_decalCollision.valueInteger > 0)
          SphereTreeRemove(tree, decal.index);
        if (contiguousHead == examined) {
          cost -= g_decalMinCost.valueInteger * decal.cost;
          ++contiguousHead;
        }
        decal.index = -2;
      }
    }
    ++examined;
  }
  range[0] = contiguousHead;
}

template struct idRenderModelDecal::DecalBuffer<
    idRenderModelDecal::decalInfo_t>;
template struct idRenderModelDecal::DecalBuffer<
    idRenderModelDecal::tireDecalInfo_t>;

idRenderModelDecal::idRenderModelDecal(bool large) {
  renderModelType = RENDERMODEL_DECAL;
  if (large) {
    maxInfo.maxSurfaces = 64;
    maxInfo.maxTireDecals = 512;
    maxInfo.maxImpactDecals = 2048;
    maxInfo.maxPersistentDecals = 128;
  } else {
    maxInfo.maxSurfaces = 2;
    maxInfo.maxTireDecals = 1;
    maxInfo.maxImpactDecals = 4;
    maxInfo.maxPersistentDecals = 4;
  }

  decalSpawnId = 0;
  maxInfo.maxDecals =
      maxInfo.maxImpactDecals + maxInfo.maxPersistentDecals;
  maxInfo.maxBoxes = maxInfo.maxDecals + maxInfo.maxTireDecals;
  maxInfo.maxQuads = (8 * maxInfo.maxBoxes) / 4;
  maxInfo.persistentStartRange =
      maxInfo.maxDecals - maxInfo.maxPersistentDecals;
  maxInfo.persistentEndRange = maxInfo.maxDecals;
  persistentCurIndex = maxInfo.persistentStartRange;
  maxInfo.maxTransientDecals =
      maxInfo.maxDecals - maxInfo.maxPersistentDecals;

  g.axis = mat3_identity;
  deferredAxis = mat3_identity;
  FreeSurfaces();
  g.flags0 |= 0x80U;
  g.flags1 |= 0x20U;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;

  static constexpr const char* renderParmNames[4] = {
      "decalPosToLocalBoxSpaceX", "decalPosToLocalBoxSpaceY",
      "decalPosToLocalBoxSpaceZ", "decalPosToLocalBoxSpaceW"};
  if (decalCallbacks.findRenderParm != nullptr) {
    for (int index = 0; index < 4; ++index) {
      parmPositionToLocalBoxSpace[index] =
          decalCallbacks.findRenderParm(renderParmNames[index]);
    }
  }

  gameTime = 0;
  latchedTime = 0;
  const int vertexBytes = 32 * (8 * maxInfo.maxBoxes);
  if (preAllocatedBufferInUse) {
    BuildIndexBufferBoxes(&indexBufferBoxes, maxInfo.maxBoxes);
    BuildIndexBufferQuads(&indexBufferQuads, maxInfo.maxQuads);
    for (idVertexBuffer& buffer : vertexBuffer)
      buffer.AllocBufferObject(nullptr, vertexBytes, 31, BU_DYNAMIC);
    usesPreAllocatedBuffer = false;
  } else {
    for (int index = 0; index < 2; ++index)
      vertexBuffer[index].Reference(&preAllocatedVertexBuffer[index]);
    indexBufferBoxes.Reference(&preAllocatedIndexBufferBoxes);
    indexBufferQuads.Reference(&preAllocatedIndexBufferQuads);
    preAllocatedBufferInUse = true;
    usesPreAllocatedBuffer = true;
  }

  triangles = new idTriangles[static_cast<std::size_t>(maxInfo.maxSurfaces)];
  for (int index = 0; index < maxInfo.maxSurfaces; ++index)
    InitializeTriangle(triangles[index]);

  vehDecBuffer.Init(maxInfo.maxTireDecals,
                    g_vehDecalBarrierPop.valueInteger,
                    g_vehDecalBarrierFade.valueInteger, 0);
  decalBuffer.Init(maxInfo.maxDecals, g_decalBarrierPop.valueInteger,
                   g_decalBarrierFade.valueInteger, 10);
}

idRenderModelDecal::~idRenderModelDecal() {
  delete[] decalBuffer.decals;
  sphereTrees.erase(decalBuffer.tree);
  delete decalBuffer.tree;
  delete[] vehDecBuffer.decals;
  sphereTrees.erase(vehDecBuffer.tree);
  delete vehDecBuffer.tree;

  if (usesPreAllocatedBuffer)
    preAllocatedBufferInUse = false;
  for (idVertexBuffer& buffer : vertexBuffer)
    buffer.FreeBufferObject();
  indexBufferBoxes.FreeBufferObject();
  indexBufferQuads.FreeBufferObject();
  delete[] triangles;
}

bool idRenderModelDecal::CommitSubclass() {
  decalBuffer.latchedRange[0] = decalBuffer.range[0];
  decalBuffer.latchedRange[1] = decalBuffer.range[1];
  vehDecBuffer.latchedRange[0] = vehDecBuffer.range[0];
  vehDecBuffer.latchedRange[1] = vehDecBuffer.range[1];
  latchedTime = gameTime;
  if (r_debugDecals.valueInteger > 0 && decalCallbacks.debugTree != nullptr)
    decalCallbacks.debugTree(decalBuffer.tree);
  return false;
}

int idRenderModelDecal::GetDecalGenRadius(float radius) {
  const float maximum = g_decalSphereSizeMax.valueFloat;
  const float clamped = std::min(radius, maximum - 1.0f);
  return static_cast<int>(((maximum - clamped) /
                           std::max(0.00000011920929f, maximum)) *
                          (g_decalSphereRadiusSize.valueFloat * clamped));
}

void idRenderModelDecal::GenQuadDecalVerts(const decalInfo_t* decal,
                                            idDrawVert* verts) {
  const float alpha = DecalAlpha(*this, *decal);
  idVec3 center = decal->pos;
  if (g_decalRoundOffPosition.valueInteger == 1) {
    center.x = std::floor(center.x + 0.5f);
    center.y = std::floor(center.y + 0.5f);
    center.z = std::floor(center.z + 0.5f);
  } else if (g_decalRoundOffPosition.valueInteger == 2) {
    center.x = std::floor(center.x * 10.0f + 0.5f) * 0.1f;
    center.y = std::floor(center.y * 10.0f + 0.5f) * 0.1f;
    center.z = std::floor(center.z * 10.0f + 0.5f) * 0.1f;
  }

  for (int index = 0; index < 4; ++index) {
    const idVec3 local{kSimpleQuad[index].x * decal->sizeScale *
                           decal->size.x,
                       kSimpleQuad[index].y * decal->sizeScale *
                           decal->size.y,
                       decal->depth};
    verts[index].xyz = center + TransformOffset(decal->axis, local);
    verts[index].st = {decal->scaleBiasX, decal->scaleBiasY};
    verts[index].color[0] = ClampByte(kSimpleQuadST[index].x * 255.0f);
    verts[index].color[1] = ClampByte(kSimpleQuadST[index].y * 255.0f);
    verts[index].color[2] = 0;
    verts[index].color[3] = ClampByte(alpha);
  }
}

void idRenderModelDecal::GenBoxDecalVerts(const decalInfo_t* decal,
                                           idDrawVert* verts) {
  const float alpha = DecalAlpha(*this, *decal);
  for (int index = 0; index < 8; ++index) {
    idDrawVert& vertex = verts[index];
    const idVec3 local{kSimpleCube[index].x * decal->sizeScale *
                           decal->size.x,
                       kSimpleCube[index].y * decal->sizeScale *
                           decal->size.y,
                       kSimpleCube[index].z * decal->depth};
    const idVec3 offset = TransformOffset(decal->axis, local);
    vertex.xyz = decal->pos + offset;
    vertex.st = {decal->scaleBiasX, decal->scaleBiasY};
    EncodeFrame(vertex, decal->axis);
    vertex.color[0] = ClampByte(std::fabs(offset.x));
    vertex.color[1] = ClampByte(std::fabs(offset.y));
    vertex.color[2] = ClampByte(std::fabs(offset.z));
    vertex.color[3] = ClampByte(alpha);
    vertex.normal[3] = static_cast<std::uint8_t>(
        (offset.x > 0.0f ? 1U : 0U) | (offset.y > 0.0f ? 2U : 0U) |
        (offset.z > 0.0f ? 4U : 0U));
  }
}

void idRenderModelDecal::GenSphericalDecalVerts(
    const decalInfo_t* decal, idDrawVert* verts) {
  for (int index = 0; index < 8; ++index) {
    idDrawVert& vertex = verts[index];
    const idVec3 local = kSimpleCube[index] * decal->size.x;
    vertex.xyz = decal->pos + TransformOffset(decal->axis, local);
    vertex.st = {decal->scaleBiasX, decal->scaleBiasY};
    EncodeFrame(vertex, decal->axis);
    PackFloat(vertex.color, local.Length());
    vertex.normal[3] = static_cast<std::uint8_t>(
        (local.x > 0.0f ? 1U : 0U) | (local.y > 0.0f ? 2U : 0U) |
        (local.z > 0.0f ? 4U : 0U));
  }
}

idRenderModelDecal::decalInfo_t* idRenderModelDecal::GetDecalFromHandle(
    decalHandle_t decalHandle) {
  return const_cast<decalInfo_t*>(
      std::as_const(*this).GetDecalFromHandle(decalHandle));
}

const idRenderModelDecal::decalInfo_t*
idRenderModelDecal::GetDecalFromHandle(decalHandle_t decalHandle) const {
  if (decalHandle.decalIndex == -1)
    return nullptr;
  if (decalHandle.decalIndex >= maxInfo.persistentStartRange)
    return &decalBuffer.decals[decalHandle.decalIndex];

  const int mask = maxInfo.maxTransientDecals - 1;
  const int first = mask & decalBuffer.range[0];
  const int last = mask & (decalBuffer.range[1] - 1);
  const bool inRange =
      last >= first ? decalHandle.decalIndex >= first &&
                          decalHandle.decalIndex <= last
                    : decalHandle.decalIndex >= first ||
                          decalHandle.decalIndex <= last;
  if (!inRange)
    return nullptr;
  const decalInfo_t* decal = &decalBuffer.decals[decalHandle.decalIndex];
  return gameTime - decal->startTime > decal->lifeTime ? nullptr : decal;
}

decalHandle_t idRenderModelDecal::AddDecalFromPoint(
    const decalParams_t* parms, int startTime, const idVec3& pos,
    const idVec3& dir) {
  const decalHandle_t invalid{-1, -1};
  if (r_skipDecals.valueInteger != 0 || parms == nullptr ||
      parms->decalMat == nullptr)
    return invalid;

  const int maxTransientDecals = maxInfo.maxTransientDecals;
  int decalIndex = -1;
  int sphereIndex = -1;
  if (parms->lifeTime < 0) {
    int slot = persistentCurIndex;
    while (slot < maxInfo.persistentEndRange &&
           decalBuffer.decals[slot].lifeTime != 0) {
      ++slot;
    }
    if (slot == maxInfo.maxDecals)
      return invalid;
    persistentCurIndex = slot;
    decalIndex = slot;
  } else {
    const int tail = decalBuffer.range[1];
    if (tail - decalBuffer.latchedRange[0] >= maxTransientDecals)
      return invalid;
    decalBuffer.range[1] = tail + 1;
    decalIndex = tail & (maxTransientDecals - 1);

    if (!parms->overlap && g_decalCollision.valueInteger > 0) {
      int otherId = -1;
      const float radius =
          static_cast<float>(GetDecalGenRadius(parms->overlapRadius));
      bool collision = SphereTreeFirstCollision(decalBuffer.tree, pos, radius,
                                                &otherId);
      while (collision &&
             decalBuffer.decals[otherId - 1].decalMtr != parms->decalMat) {
        collision = SphereTreeNextCollision(decalBuffer.tree, pos, radius,
                                            &otherId);
      }
      if (collision) {
        --decalBuffer.range[1];
        return invalid;
      }
      sphereIndex = SphereTreeAdd(decalBuffer.tree, pos, radius,
                                  decalIndex + 1);
      if (sphereIndex == -1) {
        --decalBuffer.range[1];
        return invalid;
      }
    }
  }

  decalInfo_t& decal = decalBuffer.decals[decalIndex];
  decal.handle = {decalIndex, decalSpawnId++};
  decal.startTime = startTime;
  decal.decalMtr = parms->decalMat;
  decal.fadeInStart = startTime;
  decal.fadeOutTime = static_cast<std::uint16_t>(parms->fadeOutTime);
  decal.fadeInTime = static_cast<std::uint16_t>(parms->fadeInTime);
  decal.pos = pos;
  decal.size = {parms->size.x * 0.5f, parms->size.y * 0.5f};
  decal.depth = parms->depth * 0.5f;
  decal.quad = parms->quad ? 1U : 0U;
  decal.fadeState = 0;
  decal.lifeTime = parms->lifeTime;
  decal.index = sphereIndex;
  decal.cost = g_decalMinCost.valueInteger * parms->cost;
  decalBuffer.cost += decal.cost;

  float angle = parms->angle;
  if (angle == 0.0f) {
    randomSeed = 1664525U * randomSeed + 1013904223U;
    angle = static_cast<float>((randomSeed >> 10U) & 0x7FFFU) *
            0.000030518509f * kTwoPi;
  }
  decal.axis = dir.ToZAxis();
  decal.axis.RotateSelf(decal.axis.mat[2], -angle);
  decal.size.x = std::min(decal.size.x, 255.0f);
  decal.size.y = std::min(decal.size.y, 255.0f);
  decal.sizeScale = 1.0f;
  decal.depth = std::min(decal.depth, 255.0f);

  int atlasWidth = 0;
  int atlasHeight = 0;
  int atlasWidthBits = 0;
  int atlasHeightBits = 0;
  GetAtlasInfo(atlasWidth, atlasHeight, atlasWidthBits, atlasHeightBits);
  const idVec4& atlas = parms->decalMat->atlasScaleBias;
  decal.scaleBiasX = static_cast<float>(
      static_cast<int>(atlasWidth * atlas.x) |
      (static_cast<int>(atlasWidth * atlas.z) << atlasWidthBits));
  decal.scaleBiasY = static_cast<float>(
      static_cast<int>(atlasHeight * atlas.y) |
      (static_cast<int>(atlasHeight * atlas.w) << atlasHeightBits));
  return decal.handle;
}

void idRenderModelDecal::AddDecalFromTireMark(
    const tireDecalParams_t* parms, int startTime, const idVec3& pos,
    const idVec3& dir) {
  if (r_skipDecals.valueInteger != 0 || parms == nullptr ||
      parms->decalMat == nullptr)
    return;
  const int maximum = maxInfo.maxTireDecals;
  if (vehDecBuffer.range[1] - vehDecBuffer.latchedRange[0] >= maximum)
    return;

  const int slot = vehDecBuffer.range[1] & (maximum - 1);
  int sphereIndex = -1;
  if (g_decalCollision.valueInteger > 0) {
    const float radius = static_cast<float>(
        GetDecalGenRadius(parms->halfHeight + parms->halfWidth));
    if (SphereTreeLeafCollision(decalBuffer.tree, pos, radius))
      return;
    sphereIndex = SphereTreeAdd(vehDecBuffer.tree, pos, radius, slot + 1);
    if (sphereIndex == -1)
      return;
  }

  tireDecalInfo_t& decal = vehDecBuffer.decals[slot];
  ++vehDecBuffer.range[1];
  decal.decalMtr = parms->decalMat;
  decal.startTime = startTime;
  decal.lifeTime = parms->lifeTime;
  decal.fadeTime = parms->fadeTime;
  decal.pos = pos;
  decal.dir = dir;
  decal.depth = parms->depth;
  std::copy(std::begin(parms->corners), std::end(parms->corners),
            std::begin(decal.corners));
  decal.left = parms->left;
  decal.halfWidth = parms->halfWidth;
  decal.halfHeight = parms->halfHeight;
  decal.markStart = parms->markStart;
  decal.markEnd = parms->markEnd;
  decal.index = sphereIndex;

  int atlasWidth = 0;
  int atlasHeight = 0;
  int atlasWidthBits = 0;
  int atlasHeightBits = 0;
  GetAtlasInfo(atlasWidth, atlasHeight, atlasWidthBits, atlasHeightBits);
  const idVec4& atlas = parms->decalMat->atlasScaleBias;
  decal.scaleBiasX = static_cast<float>(
      static_cast<int>(atlasWidth * atlas.x) |
      (static_cast<int>(atlasWidth * atlas.z) << 12));
  decal.scaleBiasY = static_cast<float>(
      static_cast<int>(atlasHeight * atlas.y) |
      (static_cast<int>(atlasHeight * atlas.w) << 12));
}

void idRenderModelDecal::Update(int currTime) {
  gameTime = currTime;
  decalBuffer.Update(gameTime, maxInfo.maxTransientDecals);
  vehDecBuffer.Update(gameTime, maxInfo.maxTireDecals);
  CommitThisFrame();
}

bool idRenderModelDecal::SetDecalPosition(
    decalHandle_t decalHandle, const idVec3& decalWorldPosition,
    const idMat3& decalWorldAxis) {
  decalInfo_t* decal = GetDecalFromHandle(decalHandle);
  if (decal == nullptr)
    return false;
  decal->axis = decalWorldAxis;
  decal->pos = decalWorldPosition;
  return true;
}

bool idRenderModelDecal::SetDecalAttributes(
    decalHandle_t decalHandle, const idVec3& decalWorldPosition,
    const idMat3& decalWorldAxis, float sizeScale) {
  decalInfo_t* decal = GetDecalFromHandle(decalHandle);
  if (decal == nullptr)
    return false;
  decal->axis = decalWorldAxis;
  decal->pos = decalWorldPosition;
  decal->sizeScale = sizeScale;
  return true;
}

bool idRenderModelDecal::GetDecalAxis(decalHandle_t decalHandle,
                                      idMat3& decalAxis) {
  const decalInfo_t* decal = GetDecalFromHandle(decalHandle);
  if (decal == nullptr) {
    decalAxis = mat3_identity;
    return false;
  }
  decalAxis = decal->axis;
  return true;
}

bool idRenderModelDecal::RemoveDecal(decalHandle_t decalHandle) {
  decalInfo_t* decal = GetDecalFromHandle(decalHandle);
  if (decal == nullptr || decalHandle.spawnIndex != decal->handle.spawnIndex)
    return false;
  SphereTreeRemove(decalBuffer.tree, decal->index);
  decal->index = -2;
  decal->lifeTime = 0;
  return true;
}

bool idRenderModelDecal::IsDecalInvalid(decalHandle_t decalHandle) {
  const decalInfo_t* decal = GetDecalFromHandle(decalHandle);
  return decal == nullptr ||
         decalHandle.spawnIndex != decal->handle.spawnIndex;
}

bool idRenderModelDecal::UpdateInView(
    const idRenderView* currentView, const idRenderView*,
    idRenderModelUpdateTools*) {
  if (r_skipDecals.valueInteger != 0) {
    if (surfaces.list != nullptr && surfaces.list[0].geometry != nullptr) {
      surfaces.list[0].geometry->numVerts = 0;
      surfaces.list[0].geometry->numIndexes = 0;
      surfaces.list[0].material = nullptr;
    }
    return false;
  }

  if (currentView != nullptr) {
    idRenderMatrix windowToLocal{};
    idRenderMatrix::Multiply(currentView->worldSpaceInverseMVPMatrix,
                             renderMatrix_windowSpaceToClipSpace,
                             windowToLocal);
    for (int row = 0; row < 4; ++row) {
      if (parmPositionToLocalBoxSpace[row] != nullptr) {
        SetParm(parmPositionToLocalBoxSpace[row],
                idVec4{windowToLocal.m[row * 4 + 0],
                       windowToLocal.m[row * 4 + 1],
                       windowToLocal.m[row * 4 + 2],
                       windowToLocal.m[row * 4 + 3]});
      }
    }
  }

  surfaces.Clear();
  const int frame =
      decalCallbacks.frameNumber != nullptr ? decalCallbacks.frameNumber() : 0;
  idVertexBuffer& activeVertexBuffer = vertexBuffer[frame & 1];
  auto* mapped = static_cast<idDrawVert*>(activeVertexBuffer.MapBuffer(3));
  if (mapped == nullptr)
    return false;

  const int vertexSize = VertexSizeFromMask(triangles[0].vertexMask);
  const int maximumVertices = 8 * maxInfo.maxBoxes;
  const int maximumQuadVertices = 4 * maxInfo.maxQuads;
  int vertexCount = 0;
  int indexCount = 0;

  auto findSurface = [&](const idMaterial* material, int firstSurface) {
    int index = firstSurface;
    while (index < surfaces.Num() && surfaces[index].material != material)
      ++index;
    return index;
  };

  auto finishSurface = [&](const idMaterial* material, int startVertex,
                           int startIndex, idIndexBuffer& sourceIndexes) {
    if (surfaces.Num() >= maxInfo.maxSurfaces)
      return false;
    idTriangles& geometry = triangles[surfaces.Num()];
    geometry.numVerts = vertexCount - startVertex;
    geometry.numIndexes = indexCount - startIndex;
    geometry.vertexBuffer.Reference(&activeVertexBuffer,
                                    vertexSize * startVertex,
                                    vertexSize * geometry.numVerts);
    geometry.indexBuffer.Reference(&sourceIndexes, 0,
                                   2 * geometry.numIndexes);
    idRenderModelSurface surface{};
    surface.material = material;
    surface.geometry = &geometry;
    surface.geometryIsReference = true;
    surfaces.Append(surface);
    return true;
  };

  const int transientMask = maxInfo.maxTransientDecals - 1;
  for (int outer = decalBuffer.latchedRange[0];
       outer < decalBuffer.latchedRange[1]; ++outer) {
    decalInfo_t& outerDecal = decalBuffer.decals[transientMask & outer];
    if (!outerDecal.fadeState)
      outerDecal.startTime = latchedTime;
    if (outerDecal.lifeTime >= 0 &&
        latchedTime - outerDecal.startTime >= outerDecal.lifeTime)
      continue;
    const idMaterial* material = outerDecal.decalMtr;
    if (material == nullptr || findSurface(material, 0) != surfaces.Num())
      continue;
    if (surfaces.Num() >= maxInfo.maxSurfaces)
      break;

    const bool quad = outerDecal.quad != 0;
    const bool spherical = material->materialDecalProjType == 1;
    const int startVertex = vertexCount;
    const int startIndex = indexCount;
    for (int scan = outer; scan < decalBuffer.latchedRange[1]; ++scan) {
      decalInfo_t& decal = decalBuffer.decals[transientMask & scan];
      if (decal.decalMtr == nullptr || decal.decalMtr != material ||
          (decal.quad != 0) != quad)
        continue;
      if (decal.lifeTime >= 0 &&
          latchedTime - decal.startTime > decal.lifeTime)
        continue;

      idDrawVert local[8]{};
      if (quad) {
        GenQuadDecalVerts(&decal, local);
        std::memcpy(mapped + vertexCount, local, 4 * sizeof(idDrawVert));
        vertexCount += 4;
        indexCount += 6;
        if (vertexCount > maximumQuadVertices - 4)
          break;
      } else {
        if (spherical)
          GenSphericalDecalVerts(&decal, local);
        else
          GenBoxDecalVerts(&decal, local);
        if (r_debugDecals.valueInteger > 0 &&
            decalCallbacks.debugAxis != nullptr)
          decalCallbacks.debugAxis(decal.pos, decal.axis);
        if (r_debugDecals.valueInteger > 1 &&
            decalCallbacks.debugBox != nullptr)
          decalCallbacks.debugBox(local, 8);
        std::memcpy(mapped + vertexCount, local, 8 * sizeof(idDrawVert));
        vertexCount += 8;
        indexCount += 36;
        if (vertexCount > maximumVertices - 8)
          break;
      }
    }
    finishSurface(material, startVertex, startIndex,
                  quad ? indexBufferQuads : indexBufferBoxes);
  }

  const int persistentSurfaceStart = surfaces.Num();
  for (int outer = maxInfo.persistentStartRange;
       outer < persistentCurIndex; ++outer) {
    decalInfo_t& outerDecal = decalBuffer.decals[outer];
    const idMaterial* material = outerDecal.decalMtr;
    if (material == nullptr ||
        findSurface(material, persistentSurfaceStart) != surfaces.Num())
      continue;
    if (surfaces.Num() >= maxInfo.maxSurfaces)
      break;

    const bool quad = outerDecal.quad != 0;
    const bool spherical = material->materialDecalProjType == 1;
    const int startVertex = vertexCount;
    const int startIndex = indexCount;
    for (int scan = outer; scan < maxInfo.persistentEndRange; ++scan) {
      decalInfo_t& decal = decalBuffer.decals[scan];
      if (decal.decalMtr == nullptr || decal.decalMtr != material ||
          (decal.quad != 0) != quad)
        continue;
      idDrawVert local[8]{};
      if (quad) {
        GenQuadDecalVerts(&decal, local);
        std::memcpy(mapped + vertexCount, local, 4 * sizeof(idDrawVert));
        vertexCount += 4;
        indexCount += 6;
        if (vertexCount > maximumQuadVertices - 4)
          break;
      } else {
        if (spherical)
          GenSphericalDecalVerts(&decal, local);
        else
          GenBoxDecalVerts(&decal, local);
        if (r_debugDecals.valueInteger > 0 &&
            decalCallbacks.debugAxis != nullptr)
          decalCallbacks.debugAxis(decal.pos, decal.axis);
        if (r_debugDecals.valueInteger > 1 &&
            decalCallbacks.debugBox != nullptr)
          decalCallbacks.debugBox(local, 8);
        std::memcpy(mapped + vertexCount, local, 8 * sizeof(idDrawVert));
        vertexCount += 8;
        indexCount += 36;
        if (vertexCount > maximumVertices - 8)
          break;
      }
    }
    // Retail references the box index buffer for this persistent pass even
    // when the selected generator emitted a four-vertex quad.
    finishSurface(material, startVertex, startIndex, indexBufferBoxes);
  }

  const int tireSurfaceStart = surfaces.Num();
  const int tireMask = maxInfo.maxTireDecals - 1;
  for (int outer = vehDecBuffer.latchedRange[0];
       outer < vehDecBuffer.latchedRange[1]; ++outer) {
    tireDecalInfo_t& outerDecal = vehDecBuffer.decals[tireMask & outer];
    if (outerDecal.fadeState == 0)
      outerDecal.startTime = latchedTime;
    if (outerDecal.lifeTime >= 0 &&
        latchedTime - outerDecal.startTime > outerDecal.lifeTime)
      continue;
    const idMaterial* material = outerDecal.decalMtr;
    if (findSurface(material, tireSurfaceStart) != surfaces.Num())
      continue;
    if (surfaces.Num() >= maxInfo.maxSurfaces)
      break;

    const int startVertex = vertexCount;
    const int startIndex = indexCount;
    for (int scan = outer; scan < vehDecBuffer.latchedRange[1]; ++scan) {
      tireDecalInfo_t& decal = vehDecBuffer.decals[tireMask & scan];
      if (decal.decalMtr != material)
        continue;

      idDrawVert local[8]{};
      const idMat3 axis = mat3_identity;
      if (r_debugDecals.valueInteger > 0 &&
          decalCallbacks.debugAxis != nullptr)
        decalCallbacks.debugAxis(decal.pos, axis);
      for (int corner = 0; corner < 8; ++corner) {
        idVec3 offset = decal.corners[corner & 3];
        if (corner < 4)
          offset.z -= decal.depth * 2.0f;
        idDrawVert& vertex = local[corner];
        vertex.xyz = decal.pos + TransformOffset(axis, offset);
        if (r_debugDecals.valueInteger > 0 &&
            decalCallbacks.debugPoint != nullptr)
          decalCallbacks.debugPoint(vertex.xyz, corner);
        vertex.st = {decal.scaleBiasX, decal.scaleBiasY};
        EncodeFrame(vertex, axis);
        vertex.color[0] = ClampByte(255.0f / std::fabs(offset.x));
        vertex.color[1] = ClampByte(255.0f / std::fabs(offset.y));
        vertex.color[2] = ClampByte(255.0f / std::fabs(offset.z));
        vertex.color[3] = 255;
        vertex.normal[3] = static_cast<std::uint8_t>(
            (offset.x > 0.0f ? 1U : 0U) |
            (offset.y > 0.0f ? 2U : 0U) |
            (offset.z > 0.0f ? 4U : 0U));
      }
      std::memcpy(mapped + vertexCount, local, sizeof(local));
      vertexCount += 8;
      indexCount += 36;
      if (vertexCount > maximumVertices - 8)
        break;
    }
    finishSurface(material, startVertex, startIndex, indexBufferBoxes);
  }

  activeVertexBuffer.UnmapBuffer();
  return false;
}
