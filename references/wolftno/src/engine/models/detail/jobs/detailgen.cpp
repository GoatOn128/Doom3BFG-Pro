#include "engine/models/detail/jobs/detailgen.h"
#include "shared/idlib/math/mathlib.h"

#include <cmath>
#include <cstring>

namespace {

idQuat DetailQuaternion(const dm_detail_t& detail) {
  const float remainder =
      1.0f - (detail.rotation.x * detail.rotation.x +
              detail.rotation.y * detail.rotation.y +
              detail.rotation.z * detail.rotation.z);
  const float absoluteRemainder = std::fabs(remainder);
  return {detail.rotation.x, detail.rotation.y, detail.rotation.z,
          std::sqrt(absoluteRemainder > idMath::FLT_SMALLEST_NON_DENORMAL
                        ? absoluteRemainder
                        : idMath::FLT_SMALLEST_NON_DENORMAL)};
}

idVec3 RotateDetailPoint(const idQuat& quat, const idVec3& point) {
  const float xx = quat.x * quat.x;
  const float yy = quat.y * quat.y;
  const float zz = quat.z * quat.z;
  const float ww = quat.w * quat.w;
  const float zx2 = (quat.z * quat.x) * 2.0f;
  const float yx2 = (quat.y * quat.x) * 2.0f;
  const float yz2 = (quat.y * quat.z) * 2.0f;
  const float wx2 = (quat.w * quat.x) * 2.0f;
  const float yw2 = (quat.y * quat.w) * 2.0f;
  const float wz2 = (quat.w * quat.z) * 2.0f;
  return {
      (xx - yy - zz + ww) * point.x + (wz2 + yx2) * point.y +
          (zx2 - yw2) * point.z,
      (yy - xx - zz + ww) * point.y + (yx2 - wz2) * point.x +
          (yz2 + wx2) * point.z,
      (yw2 + zx2) * point.x + (yz2 - wx2) * point.y +
          (zz - yy - xx + ww) * point.z};
}

} // namespace

void CalculateDetailBounds(const dm_detail_t* detail,
                           const idDetailPaletteData* data,
                           idBounds& bounds) {
  bounds = data->bounds;
  idMat3 axis = DetailQuaternion(*detail).ToMat3();
  const float scale = static_cast<float>(detail->uniformSizeScale) * 0.01f;
  for (idVec3& row : axis.mat)
    row *= scale;
  bounds.FromTransformedBounds(bounds, vec3_origin, axis);
  const idVec3 translation{static_cast<float>(detail->xyz[0]),
                           static_cast<float>(detail->xyz[1]),
                           static_cast<float>(detail->xyz[2])};
  bounds.b[0] += translation;
  bounds.b[1] += translation;
}

void GetDetailTotalCount(const detailSubTreeStats_t* detailCountsList,
                         detailGenTotals_t* totalsCount) {
  totalsCount->totalVerts = 0;
  totalsCount->totalIndices = 0;
  for (int index = 0;
       detailCountsList[index].subTreeDetailCount != -1; ++index) {
    totalsCount->totalVerts += detailCountsList[index].vertCount;
    totalsCount->totalIndices += detailCountsList[index].indexCount;
  }
}

void GetDetailOffsetsForSubTree(
    const detailSubTreeStats_t* detailCountsList, int subTreeIndex,
    int& vertCount, int& indexCount) {
  vertCount = 0;
  indexCount = 0;
  for (int index = 0; index < subTreeIndex; ++index) {
    vertCount += detailCountsList[index].vertCount;
    indexCount += detailCountsList[index].indexCount;
  }
}

void BuildDetailModel(const detailPalette_t* palette, idDrawVert* verts,
                      std::uint16_t* indices, int vertOffset,
                      const dm_detail_t* detail,
                      const dm_attribute_t* attribute,
                      detailGenTotals_t& totals) {
  (void)attribute;
  const detailPaletteEntry_t& entry = *palette->entry;
  const std::uint16_t* sourceIndices =
      palette->indices + entry.indexOffset;
  const int roundedIndexCount = (entry.numIndices + 1) & ~1;
  for (int index = 0; index < roundedIndexCount; ++index) {
    indices[index] = static_cast<std::uint16_t>(
        vertOffset + sourceIndices[index]);
  }

  const idQuat rotation = DetailQuaternion(*detail);
  const float scale = static_cast<float>(detail->uniformSizeScale) * 0.01f;
  const idVec3 translation{static_cast<float>(detail->xyz[0]),
                           static_cast<float>(detail->xyz[1]),
                           static_cast<float>(detail->xyz[2])};
  const std::uint8_t color[4] = {detail->color[0], detail->color[1],
                                 detail->color[2], 255};
  const detailPaletteVert_t* sourceVerts = palette->verts + entry.vertOffset;
  idDrawVert localVerts[50];
  for (int index = 0; index < entry.numVerts; ++index) {
    const idVec3 compressed{
        static_cast<float>(sourceVerts[index].xyz[0]),
        static_cast<float>(sourceVerts[index].xyz[1]),
        static_cast<float>(sourceVerts[index].xyz[2])};
    localVerts[index].xyz =
        translation + RotateDetailPoint(rotation, compressed) * scale;
    localVerts[index].st = {
        static_cast<float>(sourceVerts[index].st[0]) / 65535.0f,
        static_cast<float>(sourceVerts[index].st[1]) / 65535.0f};
    std::memcpy(localVerts[index].color, color, sizeof(color));
  }
  std::memcpy(verts, localVerts,
              static_cast<std::size_t>(entry.numVerts) * sizeof(idDrawVert));
  totals.totalVerts += entry.numVerts;
  totals.totalIndices += entry.numIndices;
}

int GenDetailForSubTree(
    const deferredDetailGenParms_t* parms,
    const std::uint16_t* detailIndices, int numDetail,
    int vertexBufferOffset, int indexBufferOffset, int indexOffset,
    detailGenTotals_t& totals) {
  totals = {};
  const auto* dataBase =
      reinterpret_cast<const std::uint8_t*>(parms->subTreeData);
  const auto* details = reinterpret_cast<const dm_detail_t*>(
      dataBase + parms->subTreeData->detailOffset);
  const auto* attributes = reinterpret_cast<const dm_attribute_t*>(
      dataBase + parms->subTreeData->attributeOffset);
  for (int index = 0; index < numDetail; ++index) {
    const dm_detail_t& detail = details[detailIndices[index]];
    const dm_attribute_t& attribute = attributes[detail.attribIdx];
    const detailPalette_t palette{
        &parms->paletteEntries[attribute.paletteIdx], parms->paletteVerts,
        parms->paletteIndices};
    BuildDetailModel(
        &palette, &parms->verts[vertexBufferOffset + totals.totalVerts],
        &parms->indices[indexBufferOffset + totals.totalIndices],
        totals.totalVerts + indexOffset, &detail, &attribute, totals);
  }
  return numDetail;
}
