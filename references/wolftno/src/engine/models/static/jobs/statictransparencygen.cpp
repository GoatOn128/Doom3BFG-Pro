#include "engine/models/static/jobs/statictransparencygen.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

constexpr float kByteToSignedFloat = 0.0078431377f;

std::uint8_t ToColorByte(float value) {
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(value * 255.0f), 0, 255));
}

std::int16_t QuantizeTexCoord(float value) {
  const float zero = (1.0f + 0.0f) * 0.5f;
  const float scale = (1.0f - zero) / 32767.0f;
  const int quantized = static_cast<int>(
      std::floor((value - zero) / scale + 0.5f));
  return static_cast<std::int16_t>(std::clamp(quantized, -32768, 32767));
}

idVec3 DecodeNormal(const idDrawVert& vertex) {
  idVec3 normal{
      static_cast<float>(vertex.normal[0]) * kByteToSignedFloat - 1.0f,
      static_cast<float>(vertex.normal[1]) * kByteToSignedFloat - 1.0f,
      static_cast<float>(vertex.normal[2]) * kByteToSignedFloat - 1.0f};
  const float inverseLength = 1.0f / std::sqrt(std::max(
      normal.LengthSqr(), std::numeric_limits<float>::min()));
  return normal * inverseLength;
}

} // namespace

void StaticTransparencyGenJob(const staticTransparencyGenParms_t* parms) {
  const int quadCount = parms->numInputVerts / 4;
  const float inverseFadeDistance =
      1.0f / (parms->fadeMaxDist - parms->fadeStartDist);

  const idVec3 channel01{
      parms->lighting.channelLighting[0].x +
          parms->lighting.channelLighting[1].x,
      parms->lighting.channelLighting[0].y +
          parms->lighting.channelLighting[1].y,
      parms->lighting.channelLighting[0].z +
          parms->lighting.channelLighting[1].z};
  const idVec3 channel23{
      parms->lighting.channelLighting[2].x +
          parms->lighting.channelLighting[3].x,
      parms->lighting.channelLighting[2].y +
          parms->lighting.channelLighting[3].y,
      parms->lighting.channelLighting[2].z +
          parms->lighting.channelLighting[3].z};
  const idVec3 channel45{
      parms->lighting.channelLighting[4].x +
          parms->lighting.channelLighting[5].x,
      parms->lighting.channelLighting[4].y +
          parms->lighting.channelLighting[5].y,
      parms->lighting.channelLighting[4].z +
          parms->lighting.channelLighting[5].z};
  const idVec3 primeColor{parms->lighting.lightColor[0].x,
                          parms->lighting.lightColor[0].y,
                          parms->lighting.lightColor[0].z};
  const idVec3 primeDirection{parms->lighting.primeLightDir.x,
                              parms->lighting.primeLightDir.y,
                              parms->lighting.primeLightDir.z};

  for (int quad = 0; quad < quadCount; ++quad) {
    for (int corner = 0; corner < 4; ++corner) {
      const idDrawVert& input = parms->inputVerts[quad * 4 + corner];
      idTransparencyVert& output = parms->verts[quad * 4 + corner];
      const idVec3 localNormal = DecodeNormal(input);
      const idVec3 worldNormal = parms->axis * localNormal;
      output.xyz = parms->axis * input.xyz + parms->origin;

      const float mappedS = parms->transSortScaleBias.x * input.st.x +
                            parms->transSortScaleBias.z;
      const float mappedT = parms->transSortScaleBias.y * input.st.y +
                            parms->transSortScaleBias.w;
      output.st[0] = output.st1[0] = QuantizeTexCoord(mappedS);
      output.st[1] = output.st1[1] = QuantizeTexCoord(mappedT);
      output.normal[0] = 0;
      output.normal[1] = 0;
      output.normal[2] = 0;
      output.normal[3] = 0;
      output.tangent[0] = 255;
      output.tangent[1] = 0;
      output.tangent[2] = 0;
      output.tangent[3] = 255;

      const float weightX = std::fabs(worldNormal.x) * 0.5f;
      const float weightY = std::fabs(worldNormal.y) * 0.5f;
      const float weightZ = std::fabs(worldNormal.z) * 0.5f;
      const float primeWeight = std::fabs(primeDirection * worldNormal) * 0.5f;
      const idVec3 color{
          ((primeColor.x * primeWeight + channel01.x * weightX) +
           channel23.x * weightY) +
              channel45.x * weightZ,
          ((primeColor.y * primeWeight + channel01.y * weightX) +
           channel23.y * weightY) +
              channel45.y * weightZ,
          ((primeColor.z * primeWeight + channel01.z * weightX) +
           channel23.z * weightY) +
              channel45.z * weightZ};
      output.color[0] = ToColorByte(color.x);
      output.color[1] = ToColorByte(color.y);
      output.color[2] = ToColorByte(color.z);

      const idVec3 viewDelta = output.xyz - parms->viewOrg;
      const float fadeDistance =
          parms->viewDir * viewDelta - parms->fadeStartDist;
      output.color[3] =
          ToColorByte(1.0f - fadeDistance * inverseFadeDistance);
    }
    const idTransparencyVert* quadVertices = &parms->verts[quad * 4];
    const idVec3 center =
        (quadVertices[3].xyz +
         (quadVertices[2].xyz +
          (quadVertices[1].xyz + quadVertices[0].xyz))) *
        0.25f;
    parms->quadDepth[quad] = parms->viewDir * (center - parms->viewOrg);
  }
  *parms->quadsUsed = quadCount;
}

idParallelJobRegistration register_StaticTransparencyGenJob(
    reinterpret_cast<jobRun_t>(StaticTransparencyGenJob),
    "StaticTransparencyGenJob");
