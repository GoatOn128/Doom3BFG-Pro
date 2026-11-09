#include "engine/models/water/jobs/oceanjob2.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <immintrin.h>

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));

void EvalVertices(const idVec4& position, const idVec4& step,
                  COceanVertex* destination, unsigned int vertexCount) {
  float x = position.x;
  float y = position.y;
  float z = position.z;
  for (unsigned int index = 0; index < vertexCount; ++index) {
    destination[index].x = x;
    destination[index].y = y;
    destination[index].z = z;
    destination[index].s0 = 0;
    destination[index].t0 = 0;
    x += step.x;
    y += step.y;
    z += step.z;
  }
}

void Ocean2GenJob(idRenderModelOcean2_TileJobParams* params) {
  const Ocean2TessParams tess = params->m_Tess;
  const idVec2i subRectSize = params->m_SubRectSize;
  const idVec2i subRectOffset = params->m_SubRectOfs;
  const float inverseWidth =
      1.0f / static_cast<float>(params->m_VBDim.x - 1);
  const float inverseHeight =
      1.0f / static_cast<float>(params->m_VBDim.y - 1);
  if (subRectSize.y <= 0)
    return;

  const float perspectiveRange = tess.m_FarPersp - tess.m_NearPersp;
  const float normalizedStartX =
      static_cast<float>(subRectOffset.x) * inverseWidth - 0.5f;
  const float offsetX = tess.m_TessU.z * normalizedStartX;
  const float offsetY = tess.m_TessU.w * normalizedStartX;
  const float offsetZ = tess.m_TessV.x * normalizedStartX;
  const float offsetW = tess.m_TessV.y * normalizedStartX;

  for (int row = 0; row < subRectSize.y; ++row) {
    const int y = subRectOffset.y + row;
    COceanVertex* destination =
        &params->m_pVB[subRectOffset.x + params->m_VBDim.x * y];
    const float normalizedY = static_cast<float>(y) * inverseHeight;
    const float perspectiveStep =
        (perspectiveRange * normalizedY + tess.m_NearPersp) * inverseWidth;
    idVec4 position;
    position.x =
        (tess.m_TessV.z * normalizedY + tess.m_TessBase.z) + offsetX;
    position.y =
        (tess.m_TessBase.w + tess.m_TessV.w * normalizedY) + offsetY;
    position.z =
        (tess.m_TessU.x + tess.m_Near * normalizedY) + offsetZ;
    position.w =
        (tess.m_TessU.y + tess.m_Far * normalizedY) + offsetW;
    idVec4 step;
    step.x = tess.m_TessU.z * perspectiveStep;
    step.y = tess.m_TessU.w * perspectiveStep;
    step.z = tess.m_TessV.x * perspectiveStep;
    step.w = tess.m_TessV.y * perspectiveStep;
    EvalVertices(position, step, destination,
                 static_cast<unsigned int>(subRectSize.x));
  }
}

idParallelJobRegistration register_Ocean2GenJob(
    reinterpret_cast<jobRun_t>(Ocean2GenJob), "Ocean2GenJob");
