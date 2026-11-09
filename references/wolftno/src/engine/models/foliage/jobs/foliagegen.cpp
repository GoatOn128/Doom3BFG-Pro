#include "engine/models/foliage/jobs/foliagegen.h"

#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <emmintrin.h>

namespace {

float RandomSigned(std::int32_t x, std::int32_t y, std::int32_t z) {
  const std::uint32_t bits =
      (1664525u * static_cast<std::uint32_t>(z) *
           static_cast<std::uint32_t>(y) * static_cast<std::uint32_t>(x) -
       1117345u) &
          0x7fffffu |
      0x3f800000u;
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value * 2.0f - 3.0f;
}

float RandomUnit(std::uint32_t seed) {
  const std::uint32_t bits =
      (1664525u * seed - 1117345u) & 0x7fffffu | 0x3f800000u;
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value - 1.0f;
}

float SinPolynomial(float value) {
  const float square = value * value;
  return (((((0.0000027526f - square * 0.0000000239f) * square -
              0.000198409f) *
                 square +
             0.0083333319f) *
                square -
            0.16666667f) *
               square +
           1.0f) *
         value;
}

void SinCos16(float angle, float* sine, float* cosine) {
  float value = angle;
  if (value < 0.0f || value >= 6.2831855f)
    value -= std::floor(value * 0.15915494f) * 6.2831855f;
  float cosineSign = 1.0f;
  if (value < 3.1415927f) {
    if (value > 1.5707964f) {
      value = 3.1415927f - value;
      cosineSign = -1.0f;
    }
  } else if (value > 4.712389f) {
    value -= 6.2831855f;
  } else {
    value = 3.1415927f - value;
    cosineSign = -1.0f;
  }
  *sine = SinPolynomial(value);
  const float square = value * value;
  *cosine = cosineSign *
            (((((0.0000247609f - square * 0.0000002605f) * square -
                 0.0013888397f) *
                    square +
                0.041666642f) *
                   square -
               0.5f) *
                  square +
              1.0f);
}

idVec3 Normalize(const idVec3& value) {
  const float lengthSquared =
      value.x * value.x + value.y * value.y + value.z * value.z;
  const float clampedLengthSquared =
      lengthSquared > idMath::FLT_SMALLEST_NON_DENORMAL
          ? lengthSquared
          : idMath::FLT_SMALLEST_NON_DENORMAL;
  const float inverseLength =
      1.0f / std::sqrt(clampedLengthSquared);
  return {value.x * inverseLength, value.y * inverseLength,
          value.z * inverseLength};
}

idVec3 RotateNegative(const idVec3& value, const idVec3& axis,
                      float sine, float cosine) {
  const float projection =
      axis.x * value.x + axis.y * value.y + axis.z * value.z;
  const idVec3 parallel{axis.x * projection, axis.y * projection,
                        axis.z * projection};
  const idVec3 perpendicular{value.x - parallel.x,
                             value.y - parallel.y,
                             value.z - parallel.z};
  const idVec3 cross{axis.y * perpendicular.z -
                         axis.z * perpendicular.y,
                     axis.z * perpendicular.x -
                         axis.x * perpendicular.z,
                     axis.x * perpendicular.y -
                         axis.y * perpendicular.x};
  return {perpendicular.x * cosine - cross.x * sine + parallel.x,
          perpendicular.y * cosine - cross.y * sine + parallel.y,
          perpendicular.z * cosine - cross.z * sine + parallel.z};
}

std::int16_t PackTexCoord(float value) {
  int packed = static_cast<int>(
      std::floor((value - 0.5f) / ((1.0f - 0.5f) / 32767.0f) + 0.5f));
  packed = std::clamp(packed, -32768, 32767);
  return static_cast<std::int16_t>(packed);
}

void RejectQuad(idTransparencyVert* verts, float* quadDepth) {
  for (int index = 0; index != 4; ++index) {
    std::memset(&verts[index], 0, sizeof(verts[index]));
    std::memset(verts[index].color, 0xff, sizeof(verts[index].color));
  }
  if (quadDepth != nullptr)
    *quadDepth = 0.0f;
}

float CalculateDepth(const idVec3& origin,
                     const foliageRenderView_t* viewData) {
  return (origin.y - viewData->viewOrg.y) * viewData->viewFwd.y +
             (origin.x - viewData->viewOrg.x) * viewData->viewFwd.x +
         (origin.z - viewData->viewOrg.z) * viewData->viewFwd.z;
}

void StreamFoliageVertices(idTransparencyVert* destination,
                           const idTransparencyVert* source, int count) {
  for (int index = 0; index < count; ++index) {
    const auto* sourceBlocks =
        reinterpret_cast<const __m128i*>(&source[index]);
    auto* destinationBlocks = reinterpret_cast<__m128i*>(&destination[index]);
    _mm_stream_si128(&destinationBlocks[0], _mm_load_si128(&sourceBlocks[0]));
    _mm_stream_si128(&destinationBlocks[1], _mm_load_si128(&sourceBlocks[1]));
  }
}

void SetupDataPointers(fm_subTreeDataPtrs_t& pointers,
                       const fm_subTreeData_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(
      const_cast<fm_subTreeData_t*>(data));
  pointers.foliage =
      reinterpret_cast<fm_foliage_t*>(base + data->foliageOffset);
  pointers.attributes =
      reinterpret_cast<fm_attribute_t*>(base + data->attributeOffset);
}

} // namespace

float idMath::Sin16(float angle) {
  float value = angle;
  if (value < 0.0f || value >= 6.2831855f)
    value -= std::floor(value * 0.15915494f) * 6.2831855f;
  if (value >= 3.1415927f) {
    if (value > 4.712389f)
      value -= 6.2831855f;
    else
      value = 3.1415927f - value;
  } else if (value > 1.5707964f) {
    value = 3.1415927f - value;
  }
  return SinPolynomial(value);
}

void CalculateFoliageBounds(const std::int16_t* const xyz,
                            float uniformSizeScale, float quadWidth,
                            float quadHeight, float widthVariance,
                            float heightVariance, const idVec3& upVec,
                            idBounds& bounds) {
  const float random = RandomSigned(xyz[0], xyz[1], xyz[2]);
  const float width = (random * widthVariance * quadWidth + quadWidth) *
                      uniformSizeScale;
  const float height = (random * heightVariance * quadHeight + quadHeight) *
                       uniformSizeScale;
  const float radius = std::fmax(width, height) * 0.5f;
  bounds.b[1].x = static_cast<float>(xyz[0]);
  bounds.b[1].z = static_cast<float>(xyz[2]);
  bounds.b[1].y = static_cast<float>(xyz[1]);
  bounds.b[0].x = static_cast<float>(xyz[0]);
  bounds.b[0].y = bounds.b[1].y;
  bounds.b[0].z = bounds.b[1].z;
  bounds.b[0].x -= radius;
  bounds.b[0].y -= radius;
  bounds.b[1].x += radius;
  bounds.b[1].y += radius;
  if (upVec.z >= 0.0f)
    bounds.b[1].z += height;
  else
    bounds.b[0].z -= height;
}

void CalculateFoliageBounds(const fm_foliage_t* foliage,
                            const fm_attribute_t* attribute,
                            idBounds& bounds) {
  const idVec3 up{
      static_cast<float>(foliage->upVec[0]) * 0.0078431377f - 1.0f,
      static_cast<float>(foliage->upVec[1]) * 0.0078431377f - 1.0f,
      static_cast<float>(foliage->upVec[2]) * 0.0078431377f - 1.0f};
  CalculateFoliageBounds(
      foliage->xyz, static_cast<float>(foliage->uniformSizeScale) * 0.01f,
      static_cast<float>(attribute->quadWidth),
      static_cast<float>(attribute->quadHeight),
      static_cast<float>(attribute->widthVariance) * 0.01f,
      static_cast<float>(attribute->heightVariance) * 0.01f, up, bounds);
}

void GetFoliageOffsetForSubTree(
    const foliageSubTreeStats_t* foliageStatsList, int subTreeIndex,
    int& totalFoliage, int& totalOpaque) {
  for (int index = 0; index < subTreeIndex; ++index) {
    totalFoliage += foliageStatsList[index].subTreeFoliageCount;
    totalOpaque += foliageStatsList[index].subTreeOpaqueCount;
  }
}

void GetFoliageTotalStats(const foliageSubTreeStats_t* foliageStatsList,
                          int& totalFoliage, int& totalOpaque) {
  for (int index = 0;
       foliageStatsList[index].subTreeFoliageCount != -1; ++index) {
    totalFoliage += foliageStatsList[index].subTreeFoliageCount;
    totalOpaque += foliageStatsList[index].subTreeOpaqueCount;
  }
}

void GetFoliageOffsetAndCountForJob(int numJobs, int jobIndex,
                                    int totalCount, int& jobOffset,
                                    int& jobCount) {
  const int offset = totalCount * jobIndex / numJobs;
  jobCount = totalCount * (jobIndex + 1) / numJobs - offset;
  jobOffset = offset;
}

void GetOpaqueOffsetAndCountForJob(
    int jobOffset, int jobCount, const fm_subTreeDataPtrs_t& subTreePtrs,
    const foliageIndex_t* foliageIndices, int& opaqueOffset,
    int& opaqueCount) {
  opaqueOffset = 0;
  opaqueCount = 0;
  for (int index = 0; index < jobOffset; ++index) {
    const fm_foliage_t& foliage =
        subTreePtrs.foliage[foliageIndices[index].foliageIndex];
    if (subTreePtrs.attributes[foliage.attribIdx].opaque != 0)
      ++opaqueOffset;
  }
  for (int index = jobOffset; index < jobOffset + jobCount; ++index) {
    const fm_foliage_t& foliage =
        subTreePtrs.foliage[foliageIndices[index].foliageIndex];
    if (subTreePtrs.attributes[foliage.attribIdx].opaque != 0)
      ++opaqueCount;
  }
}

int BuildFoliageQuad(idTransparencyVert* verts, float* quadDepth,
                     unsigned int foliageIndex, const fm_foliage_t* foliage,
                     const fm_attribute_t* attribute,
                     const foliageRenderView_t* viewData,
                     const influenceSphere_t* influenceSpheres,
                     const std::int16_t* influenceSphereIndices,
                     float foliageStartFadeDist,
                     float foliageSmallFadeDistMax,
                     float foliageBigFadeDistMax) {
  const idVec3 origin{static_cast<float>(foliage->xyz[0]),
                      static_cast<float>(foliage->xyz[1]),
                      static_cast<float>(foliage->xyz[2])};
  const float random =
      RandomSigned(foliage->xyz[0], foliage->xyz[1], foliage->xyz[2]);
  const float scale = static_cast<float>(foliage->uniformSizeScale) * 0.01f;
  const float width =
      (static_cast<float>(attribute->widthVariance) * 0.01f * random *
           static_cast<float>(attribute->quadWidth) +
       static_cast<float>(attribute->quadWidth)) *
      scale;
  const float height =
      (static_cast<float>(attribute->heightVariance) * 0.01f * random *
           static_cast<float>(attribute->quadHeight) +
       static_cast<float>(attribute->quadHeight)) *
      scale;

  float fadeMaximum;
  float fadeMinimum;
  if (std::fmax(width, height) * 0.5f >= 100.0f) {
    fadeMinimum = foliageSmallFadeDistMax;
    fadeMaximum = foliageBigFadeDistMax;
  } else {
    const float fadeRange = foliageSmallFadeDistMax - foliageStartFadeDist;
    const float value = RandomUnit(foliageIndex);
    fadeMaximum = value * value * value * fadeRange + foliageStartFadeDist;
    fadeMinimum = fadeMaximum - fadeRange * 0.25f;
  }
  const float fadeSpan = fadeMaximum - fadeMinimum;
  int alpha = 255;
  if (std::fabs(fadeSpan) >= 1.0f) {
    const float depth = CalculateDepth(origin, viewData);
    alpha = static_cast<int>((1.0f - (depth - fadeMinimum) / fadeSpan) *
                             255.0f);
  }
  if (alpha < 0 || alpha == 0) {
    RejectQuad(verts, quadDepth);
    return 4;
  }
  if (alpha > 255)
    alpha = 255;

  float shapeOffset = 0.0f;
  switch (foliage->shapeType) {
    case 1: shapeOffset = 90.0f; break;
    case 2: shapeOffset = 120.0f; break;
    case 3: shapeOffset = 240.0f; break;
    default: break;
  }
  const idVec3 up{
      static_cast<float>(foliage->upVec[0]) * 0.0078431377f - 1.0f,
      static_cast<float>(foliage->upVec[1]) * 0.0078431377f - 1.0f,
      static_cast<float>(foliage->upVec[2]) * 0.0078431377f - 1.0f};
  idVec3 facing = viewData->viewLeft;
  if (attribute->shape != 0) {
    SinCos16((shapeOffset + std::fabs(random * 360.0f)) * 0.017453292f,
             &facing.y, &facing.x);
    facing.z = 0.0f;
  }
  const float halfWidth = width * 0.5f;
  const idVec3 right{facing.x * halfWidth, facing.y * halfWidth,
                     facing.z * halfWidth};
  const idVec3 top{up.x * height, up.y * height, up.z * height};
  idVec3 topLeft{top.x - right.x, top.y - right.y, top.z - right.z};
  idVec3 topRight{top.x + right.x, top.y + right.y, top.z + right.z};

  if (attribute->swayMagnitude != 0) {
    const float swayScale =
        static_cast<float>(attribute->swayMagnitude) * 0.00050000002f *
        height;
    float windSin;
    float windCos;
    SinCos16(viewData->time * random * 6.2831855f, &windSin, &windCos);
    topLeft.x += windSin * swayScale;
    topLeft.y += windCos * swayScale;
    topRight.x += windSin * swayScale;
    topRight.y += windCos * swayScale;

    const idVec3 swayAxis = Normalize(right.Cross(top));
    const float strongScale = swayScale * 5.0f;
    const float phase = 6.2831855f * random;
    const float leftSway = idMath::Sin16(phase + viewData->time) * strongScale;
    const float rightSway =
        idMath::Sin16(viewData->time * 5.0f + phase) * strongScale;
    topLeft.x += swayAxis.x * leftSway;
    topLeft.y += swayAxis.y * leftSway;
    topLeft.z += swayAxis.z * leftSway;
    topRight.x += swayAxis.x * rightSway;
    topRight.y += swayAxis.y * rightSway;
    topRight.z += swayAxis.z * rightSway;

    idVec3 testPosition = origin;
    if (up.z < 0.0f)
      testPosition.z -= height;
    for (int listIndex = 0; influenceSphereIndices[listIndex] >= 0;
         ++listIndex) {
      const influenceSphere_t& sphere =
          influenceSpheres[influenceSphereIndices[listIndex]];
      const idVec3 delta{testPosition.x - sphere.center.x,
                         testPosition.y - sphere.center.y,
                         testPosition.z - sphere.center.z};
      const float distanceSquared = delta.x * delta.x + delta.y * delta.y +
                                    delta.z * delta.z;
      const float outerSquared = sphere.outerRadius * sphere.outerRadius;
      if (outerSquared <= distanceSquared)
        continue;
      float angle = sphere.angle;
      const float innerSquared = sphere.innerRadius * sphere.innerRadius;
      if (distanceSquared > innerSquared &&
          sphere.outerRadius > sphere.innerRadius) {
        angle *= (outerSquared - distanceSquared) /
                 (outerSquared - innerSquared);
      }
      const idVec3 axis = Normalize(delta.Cross(up));
      float sine;
      float cosine;
      SinCos16(angle * 0.017453292f, &sine, &cosine);
      topLeft = RotateNegative(topLeft, axis, sine, cosine);
      topRight = RotateNegative(topRight, axis, sine, cosine);
    }
  }

  verts[0].xyz = {origin.x + topLeft.x, origin.y + topLeft.y,
                  origin.z + topLeft.z};
  verts[1].xyz = {origin.x + topRight.x, origin.y + topRight.y,
                  origin.z + topRight.z};
  verts[2].xyz = {origin.x - right.x, origin.y - right.y,
                  origin.z - right.z};
  verts[3].xyz = {origin.x + right.x, origin.y + right.y,
                  origin.z + right.z};

  const float inverseAtlasX =
      8.0f / (static_cast<float>(viewData->atlasWidth) *
              attribute->sortScaleBias[0]);
  const float inverseAtlasY =
      8.0f / (static_cast<float>(viewData->atlasHeight) *
              attribute->sortScaleBias[1]);
  float leftU;
  float rightU;
  if (attribute->flipHoriz != 0 && random < 0.0f) {
    leftU = attribute->sortScaleBias[0] * (1.0f - inverseAtlasX) +
            attribute->sortScaleBias[2];
    rightU = attribute->sortScaleBias[0] * inverseAtlasX +
             attribute->sortScaleBias[2];
  } else {
    leftU = attribute->sortScaleBias[0] * inverseAtlasX +
            attribute->sortScaleBias[2];
    rightU = attribute->sortScaleBias[0] * (1.0f - inverseAtlasX) +
             attribute->sortScaleBias[2];
  }
  const float topV = attribute->sortScaleBias[1] * inverseAtlasY +
                     attribute->sortScaleBias[3];
  const float bottomV = attribute->sortScaleBias[1] *
                            (1.0f - inverseAtlasY) +
                        attribute->sortScaleBias[3];
  const float u[4] = {leftU, rightU, leftU, rightU};
  const float v[4] = {topV, topV, bottomV, bottomV};
  for (int index = 0; index != 4; ++index) {
    verts[index].st[0] = verts[index].st1[0] = PackTexCoord(u[index]);
    verts[index].st[1] = verts[index].st1[1] = PackTexCoord(v[index]);
    std::memset(verts[index].normal, 0, sizeof(verts[index].normal));
    verts[index].tangent[0] = 0xff;
    verts[index].tangent[1] = 0x00;
    verts[index].tangent[2] = 0x00;
    verts[index].tangent[3] = 0xff;
    verts[index].color[0] = foliage->color[0];
    verts[index].color[1] = foliage->color[1];
    verts[index].color[2] = foliage->color[2];
    verts[index].color[3] = static_cast<std::uint8_t>(alpha);
  }
  if (quadDepth != nullptr)
    *quadDepth = CalculateDepth(origin, viewData);
  return 4;
}

int GenFoliageForSubTree(const deferredFoliageGenParms_t* parms,
                         const foliageIndex_t* foliageIndices, int num,
                         int opaqueVertexOffset, int transVertexOffset,
                         int numOpaque, int numTrans) {
  fm_subTreeDataPtrs_t subTreePointers;
  SetupDataPointers(subTreePointers, parms->subTreeData);
  for (int index = 0; index < num; ++index) {
    const foliageIndex_t& foliageIndex = foliageIndices[index];
    const fm_foliage_t* foliage =
        &subTreePointers.foliage[foliageIndex.foliageIndex];
    const fm_attribute_t* attribute =
        &subTreePointers.attributes[foliage->attribIdx];
    if ((attribute->opaque != 0 && numOpaque <= 0) ||
        (attribute->opaque == 0 && numTrans <= 0))
      continue;
    alignas(16) idTransparencyVert localVertices[4];
    float quadDepth;
    const int vertexCount = BuildFoliageQuad(
        localVertices, &quadDepth, foliageIndex.foliageIndex, foliage,
        attribute, parms->viewData, parms->visibleInfluenceSpheres->spheres,
        &parms->influenceSphereIndices[foliageIndex.influenceSphereOffset],
        parms->foliageStartFadeDist, parms->foliageSmallFadeDistMax,
        parms->foliageBigFadeDistMax);
    if (attribute->opaque != 0) {
      --numOpaque;
      StreamFoliageVertices(&parms->quadsVertices[opaqueVertexOffset],
                            localVertices, vertexCount);
      opaqueVertexOffset += vertexCount;
    } else {
      --numTrans;
      parms->quadsDepths[transVertexOffset >> 2] = quadDepth;
      StreamFoliageVertices(&parms->quadsVertices[transVertexOffset],
                            localVertices, vertexCount);
      transVertexOffset += vertexCount;
    }
  }
  return num;
}
