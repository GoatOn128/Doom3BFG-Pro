#include "engine/models/water/jobs/oceanjob.h"

#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
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
const __m128 DC_OFFSET_V128 =
    _mm_castsi128_ps(_mm_set1_epi32(0x26901d7d));

namespace {

void GetCubicWeights(float s, float A, float* bvals) {
  const float as3 = ((s * A) * s) * s;
  bvals[3] = (s * A) * s - as3;
  bvals[0] = (s * A) - ((A * 2.0f) * s) * s + as3;
  const float a2s3 = (((A + 2.0f) * s) * s) * s;
  bvals[2] = ((((A * 2.0f) + 3.0f) * s) * s - s * A) - a2s3;
  bvals[1] = 1.0f - (((A + 3.0f) * s) * s) + a2s3;
}

idVec4 Lerp(const idVec4& from, const idVec4& to, float fraction) {
  return {from.x + (to.x - from.x) * fraction,
          from.y + (to.y - from.y) * fraction,
          from.z + (to.z - from.z) * fraction,
          from.w + (to.w - from.w) * fraction};
}

idVec4 CubicSample(unsigned int sourceSide, const idVec4* source,
                   unsigned int sourceX, unsigned int sourceY,
                   const float* xWeights, const float* yWeights) {
  idVec4 rows[4];
  for (unsigned int y = 0; y != 4; ++y) {
    const idVec4* samples =
        source + (sourceY + y) * sourceSide + sourceX;
    rows[y].x = ((samples[0].x * xWeights[0] +
                  samples[1].x * xWeights[1]) +
                 samples[2].x * xWeights[2]) +
                samples[3].x * xWeights[3];
    rows[y].y = ((samples[0].y * xWeights[0] +
                  samples[1].y * xWeights[1]) +
                 samples[2].y * xWeights[2]) +
                samples[3].y * xWeights[3];
    rows[y].z = ((samples[0].z * xWeights[0] +
                  samples[1].z * xWeights[1]) +
                 samples[2].z * xWeights[2]) +
                samples[3].z * xWeights[3];
    rows[y].w = ((samples[0].w * xWeights[0] +
                  samples[1].w * xWeights[1]) +
                 samples[2].w * xWeights[2]) +
                samples[3].w * xWeights[3];
  }
  return {
      ((rows[0].x * yWeights[0] + rows[1].x * yWeights[1]) +
       rows[2].x * yWeights[2]) +
          rows[3].x * yWeights[3],
      ((rows[0].y * yWeights[0] + rows[1].y * yWeights[1]) +
       rows[2].y * yWeights[2]) +
          rows[3].y * yWeights[3],
      ((rows[0].z * yWeights[0] + rows[1].z * yWeights[1]) +
       rows[2].z * yWeights[2]) +
          rows[3].z * yWeights[3],
      ((rows[0].w * yWeights[0] + rows[1].w * yWeights[1]) +
       rows[2].w * yWeights[2]) +
          rows[3].w * yWeights[3]};
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
            (((((-0.0000002605f * square + 0.0000247609f) * square -
                 0.0013888397f) *
                    square +
                0.041666641f) *
                   square -
               0.5f) *
                  square +
              1.0f);
}

unsigned char PackNormal(float value, bool truncate) {
  const float encoded = value * 127.5f + 127.5f;
  const int rounded = truncate ? _mm_cvttss_si32(_mm_set_ss(encoded))
                               : _mm_cvtss_si32(_mm_set_ss(encoded));
  return static_cast<unsigned char>(std::clamp(rounded, 0, 255));
}

float WaveFade(float distance, float nearOffset, float farOffset) {
  return std::fmax(0.0f, std::fmin(std::fmin(farOffset - distance,
                                             distance - nearOffset),
                                  1.0f));
}

float MaxSmallestNonDenormal(float value) {
  return _mm_cvtss_f32(_mm_max_ss(
      _mm_set_ss(value), _mm_set_ss(idMath::FLT_SMALLEST_NON_DENORMAL)));
}

float RefinedReciprocalSqrt(float square, bool clampEstimate) {
  const float estimateSquare =
      clampEstimate ? MaxSmallestNonDenormal(square) : square;
  const __m128 estimate = _mm_rsqrt_ss(_mm_set_ss(estimateSquare));
  const __m128 correction = _mm_add_ss(
      _mm_mul_ss(_mm_mul_ss(estimate, estimate), _mm_set_ss(square)),
      _mm_set_ss(-3.0f));
  return _mm_cvtss_f32(
      _mm_mul_ss(correction, _mm_mul_ss(estimate, _mm_set_ss(-0.5f))));
}

} // namespace

void UpSampleVertices_BiLinear(unsigned int sourceSide,
                               unsigned int destinationSide,
                               const idVec4* source, idVec4* destination,
                               unsigned int debugFlags, unsigned int x0,
                               unsigned int y0, unsigned int x1,
                               unsigned int y1) {
  static constexpr float fractions[4] = {0.0f, 0.25f, 0.5f, 0.75f};
  for (unsigned int y = y0; y < y1; ++y) {
    const unsigned int shiftedY = y + 3;
    const unsigned int sourceY = shiftedY >> 2;
    const float fractionY = fractions[shiftedY & 3];
    for (unsigned int x = x0; x < x1; ++x) {
      const unsigned int shiftedX = x + 3;
      const unsigned int sourceX = shiftedX >> 2;
      const float fractionX = fractions[shiftedX & 3];
      const idVec4* upper = source + sourceY * sourceSide + sourceX;
      const idVec4 top = Lerp(upper[0], upper[1], fractionX);
      const idVec4 bottom = Lerp(upper[sourceSide], upper[sourceSide + 1],
                                 fractionX);
      idVec4 value = Lerp(top, bottom, fractionY);
      if ((debugFlags & 0x10u) != 0)
        value.w = 1.0f;
      destination[y * destinationSide + x] = value;
    }
  }
}

void UpSampleVertices_BiCubic(unsigned int sourceSide,
                              unsigned int destinationSide,
                              const idVec4* source, idVec4* destination,
                              unsigned int debugFlags) {
  if ((debugFlags & 0x40u) != 0) {
    UpSampleVertices_BiLinear(sourceSide, destinationSide, source,
                              destination, debugFlags, 1, 1,
                              destinationSide - 1, destinationSide - 1);
    return;
  }

  float weights[4][4];
  for (unsigned int fraction = 0; fraction != 4; ++fraction)
    GetCubicWeights(static_cast<float>(fraction) * 0.25f, -0.75f,
                    weights[fraction]);

  for (unsigned int y = 1; y + 1 < destinationSide; ++y) {
    const unsigned int relativeY = y - 1;
    const unsigned int sourceY = relativeY >> 2;
    for (unsigned int x = 1; x + 1 < destinationSide; ++x) {
      const unsigned int relativeX = x - 1;
      const unsigned int sourceX = relativeX >> 2;
      idVec4 value = CubicSample(sourceSide, source, sourceX, sourceY,
                                 weights[relativeX & 3],
                                 weights[relativeY & 3]);
      if ((debugFlags & 0x10u) != 0)
        value.w = 1.0f;
      destination[y * destinationSide + x] = value;
    }
  }

  UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                            debugFlags, 0, 0, destinationSide, 1);
  UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                            debugFlags, 0, destinationSide - 1,
                            destinationSide, destinationSide);
  UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                            debugFlags, 0, 1, 1, destinationSide - 1);
  UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                            debugFlags, destinationSide - 1, 1,
                            destinationSide, destinationSide - 1);

  if ((debugFlags & 0x80u) != 0 && destinationSide >= 9) {
    UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                              debugFlags, 0, 0, destinationSide, 3);
    UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                              debugFlags, 0, destinationSide - 3,
                              destinationSide, destinationSide);
    UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                              debugFlags, 0, 3, 3, destinationSide - 3);
    UpSampleVertices_BiLinear(sourceSide, destinationSide, source, destination,
                              debugFlags, destinationSide - 3, 3,
                              destinationSide, destinationSide - 3);
  }
}

void CreateNormals(unsigned int sourceSide, const idVec4* source,
                   unsigned char* texels, unsigned int pitch,
                   unsigned int debugFlags) {
  if (sourceSide == 2)
    return;
  const bool truncate = (debugFlags & 8u) != 0;
  const unsigned int outputSide = sourceSide - 2;
  const unsigned int vectorizedColumns = outputSide & ~3u;
  for (unsigned int y = 0; y != outputSide; ++y) {
    unsigned char* output = texels + y * pitch;
    for (unsigned int x = 0; x != outputSide; ++x) {
      const idVec4* corner = source + y * sourceSide + x;
      const idVec4& center = corner[sourceSide + 1];
      const idVec3 right{corner[sourceSide + 2].x - center.x,
                         corner[sourceSide + 2].y - center.y,
                         corner[sourceSide + 2].z - center.z};
      const idVec3 down{corner[2 * sourceSide + 1].x - center.x,
                        corner[2 * sourceSide + 1].y - center.y,
                        corner[2 * sourceSide + 1].z - center.z};
      const idVec3 left{corner[sourceSide].x - center.x,
                        corner[sourceSide].y - center.y,
                        corner[sourceSide].z - center.z};
      const idVec3 up{corner[1].x - center.x,
                      corner[1].y - center.y,
                      corner[1].z - center.z};
      idVec3 normal = right.Cross(down) + left.Cross(up);
      const float squareLength =
          (normal.y * normal.y + normal.x * normal.x) + normal.z * normal.z;
      const float inverseLength = truncate
                                      ? 1.0f / std::sqrt(
                                            MaxSmallestNonDenormal(squareLength))
                                      : RefinedReciprocalSqrt(
                                            squareLength,
                                            x >= vectorizedColumns);
      normal *= inverseLength;
      output[4 * x + 0] = PackNormal(normal.x, truncate);
      output[4 * x + 1] = PackNormal(normal.y, truncate);
      output[4 * x + 2] = 0;
      output[4 * x + 3] = 0;
    }
  }
}

template <int AddToExisting>
void ApplyWavesSOA(
    const idVec3& offset, float time, float vertexStep,
    unsigned int vertexSide,
    const idRenderModelOcean_WaveParams::CWaveSOA& waves,
    unsigned int waveCount, idVec4* destination, unsigned int debugFlags,
    const idVec4& lodCameraPosition, float lodFadeReciprocal) {
  const unsigned int vectorizedColumns = vertexSide & ~3u;
  for (unsigned int y = 0; y != vertexSide; ++y) {
    for (unsigned int x = 0; x != vertexSide; ++x) {
      const idVec3 base{offset.x + static_cast<float>(x) * vertexStep,
                        offset.y + static_cast<float>(y) * vertexStep,
                        offset.z};
      idVec4 value;
      if constexpr (AddToExisting != 0)
        value = destination[y * vertexSide + x];
      else
        value = {base.x, base.y, base.z, 1.0f};

      const float dx = base.x - lodCameraPosition.x;
      const float dy = base.y - lodCameraPosition.y;
      const float dz = base.z - lodCameraPosition.z;
      const float squareDistance =
          (dy * dy + dx * dx) + dz * dz;
      const bool scalarDebugPath =
          (debugFlags & 4u) != 0 && x >= vectorizedColumns;
      const float distance =
          (scalarDebugPath
               ? std::sqrt(MaxSmallestNonDenormal(squareDistance))
               : RefinedReciprocalSqrt(squareDistance, true) *
                     squareDistance) *
          lodFadeReciprocal;
      for (unsigned int wave = 0; wave != waveCount; ++wave) {
        const float fade = WaveFade(distance,
                                    waves.m_pLodFadeNearOfs_v[wave],
                                    waves.m_pLodFadeFarOfs_v[wave]);
        float sine;
        float cosine;
        SinCos16(((base.y * waves.m_pWaveVec_y[wave] +
                   base.x * waves.m_pWaveVec_x[wave]) *
                  waves.m_pWaveVecMag[wave]) +
                     time * waves.m_pFreq[wave],
                 &sine, &cosine);
        sine *= fade;
        cosine *= fade;
        const float amplitude = waves.m_pAmp[wave];
        value.x += sine * waves.m_pWaveVec_x[wave] * amplitude;
        value.y += sine * waves.m_pWaveVec_y[wave] * amplitude;
        value.z -= cosine * amplitude;
      }
      value.w = 1.0f;
      destination[y * vertexSide + x] = value;
    }
  }
}

template void ApplyWavesSOA<0>(
    const idVec3&, float, float, unsigned int,
    const idRenderModelOcean_WaveParams::CWaveSOA&, unsigned int, idVec4*,
    unsigned int, const idVec4&, float);
template void ApplyWavesSOA<1>(
    const idVec3&, float, float, unsigned int,
    const idRenderModelOcean_WaveParams::CWaveSOA&, unsigned int, idVec4*,
    unsigned int, const idVec4&, float);

void OceanGenJob(idRenderModelOcean_TileJobParams* params) {
  idRenderModelOcean_WaveParams& waveParams = *params->m_pParams;
  const unsigned int tessellation = params->m_TileTess;
  const unsigned int coarseSide = tessellation + 3;
  const unsigned int tileSide = tessellation + 1;
  const unsigned int fineSide = 4 * tessellation + 3;
  const float coarseStep = params->m_TileDim / static_cast<float>(tessellation);
  const float scaledMinimum =
      waveParams.m_WaveLodFadeRangeRcp * params->m_MinDist;
  const float scaledMaximum =
      waveParams.m_WaveLodFadeRangeRcp * params->m_MaxDist;
  const unsigned int maximumWaveCount = waveParams.m_nWavesMax_v;

  unsigned int coarseWaveCount = 0;
  while (coarseWaveCount < maximumWaveCount &&
         scaledMinimum <
             waveParams.m_WavesSOA.m_lLodFadeFarOfs_v[coarseWaveCount])
    ++coarseWaveCount;

  unsigned int detailFirst = maximumWaveCount;
  while (detailFirst != 0 &&
         waveParams.m_WavesSOA.m_lLodFadeNearOfs_n[detailFirst - 1] <
             scaledMaximum)
    --detailFirst;
  unsigned int detailWaveCount = 0;
  while (detailFirst + detailWaveCount < maximumWaveCount &&
         scaledMinimum < waveParams.m_WavesSOA
                             .m_lLodFadeFarOfs_n[detailFirst + detailWaveCount])
    ++detailWaveCount;
  if ((waveParams.m_DebugFlags & 0x200u) != 0) {
    detailFirst = 0;
    detailWaveCount = maximumWaveCount;
  }

  std::array<idVec4, 4858> work;
  idVec4* coarseVertices = work.data();
  idVec4* fineVertices = coarseVertices + coarseSide * coarseSide;
  reinterpret_cast<unsigned char*>(work.data())[
      16 * (coarseSide * coarseSide + fineSide * fineSide) + 127] = 0xdd;

  const idRenderModelOcean_WaveParams::CWaveSOA coarseWaves{
      coarseWaveCount,
      waveParams.m_WavesSOA.m_lWaveVec_x,
      waveParams.m_WavesSOA.m_lWaveVec_y,
      waveParams.m_WavesSOA.m_lWaveVecMag,
      waveParams.m_WavesSOA.m_lAmp,
      waveParams.m_WavesSOA.m_lFreq,
      waveParams.m_WavesSOA.m_lLodFadeNearOfs_v,
      waveParams.m_WavesSOA.m_lLodFadeFarOfs_v,
      waveParams.m_WavesSOA.m_lLodFadeNearOfs_n,
      waveParams.m_WavesSOA.m_lLodFadeFarOfs_n};
  const idVec3 coarseOffset{
      params->m_TileOfs.x - coarseStep + waveParams.m_TilesOrigin.x,
      params->m_TileOfs.y - coarseStep + waveParams.m_TilesOrigin.y,
      waveParams.m_TilesOrigin.z + 0.0f};

  std::int64_t ticks = Sys_GetClockTicks();
  ApplyWavesSOA<0>(coarseOffset, waveParams.m_Time, coarseStep, coarseSide,
                   coarseWaves, coarseWaveCount, coarseVertices,
                   waveParams.m_DebugFlags, waveParams.m_LODCamPos,
                   waveParams.m_WaveLodFadeRangeRcp);
  params->m_Timers.m_TicksWave_v = Sys_GetClockTicks() - ticks;

  ticks = Sys_GetClockTicks();
  for (unsigned int y = 0; y != tileSide; ++y) {
    for (unsigned int x = 0; x != tileSide; ++x) {
      const idVec4& source = coarseVertices[(y + 1) * coarseSide + x + 1];
      COceanVertex& destination = params->m_pVB[y * tileSide + x];
      destination.x = source.x;
      destination.y = source.y;
      destination.z = source.z;
      destination.s0 = static_cast<std::uint16_t>(
          params->m_TileST1Ofs.x + static_cast<int>(x * params->m_TileSTScale));
      destination.t0 = static_cast<std::uint16_t>(
          params->m_TileST1Ofs.y + static_cast<int>(y * params->m_TileSTScale));
    }
  }
  params->m_Timers.m_TicksVertices = Sys_GetClockTicks() - ticks;

  ticks = Sys_GetClockTicks();
  if ((waveParams.m_DebugFlags & 0x20u) != 0) {
    UpSampleVertices_BiLinear(coarseSide, fineSide, coarseVertices,
                              fineVertices, waveParams.m_DebugFlags, 0, 0,
                              fineSide, fineSide);
  } else {
    UpSampleVertices_BiCubic(coarseSide, fineSide, coarseVertices,
                             fineVertices, waveParams.m_DebugFlags);
  }
  params->m_Timers.m_TicksUpSample = Sys_GetClockTicks() - ticks;

  ticks = Sys_GetClockTicks();
  if (detailWaveCount != 0) {
    const idRenderModelOcean_WaveParams::CWaveSOA detailWaves{
        detailWaveCount,
        waveParams.m_WavesSOA.m_lWaveVec_x + detailFirst,
        waveParams.m_WavesSOA.m_lWaveVec_y + detailFirst,
        waveParams.m_WavesSOA.m_lWaveVecMag + detailFirst,
        waveParams.m_WavesSOA.m_lAmp + detailFirst,
        waveParams.m_WavesSOA.m_lFreq + detailFirst,
        waveParams.m_WavesSOA.m_lLodFadeNearOfs_n + detailFirst,
        waveParams.m_WavesSOA.m_lLodFadeFarOfs_n + detailFirst,
        waveParams.m_WavesSOA.m_lLodFadeNearOfs_n + detailFirst,
        waveParams.m_WavesSOA.m_lLodFadeFarOfs_n + detailFirst};
    const float fineStep = coarseStep * 0.25f;
    const idVec3 fineOffset{
        params->m_TileOfs.x - fineStep + waveParams.m_TilesOrigin.x,
        params->m_TileOfs.y - fineStep + waveParams.m_TilesOrigin.y,
        waveParams.m_TilesOrigin.z + 0.0f};
    ApplyWavesSOA<1>(fineOffset, waveParams.m_Time, fineStep, fineSide,
                     detailWaves, detailWaveCount, fineVertices,
                     waveParams.m_DebugFlags, waveParams.m_LODCamPos,
                     waveParams.m_WaveLodFadeRangeRcp);
  }
  params->m_Timers.m_TicksWave_n = Sys_GetClockTicks() - ticks;

  ticks = Sys_GetClockTicks();
  CreateNormals(fineSide, fineVertices,
                static_cast<unsigned char*>(params->m_pNormalMap),
                params->m_NormalMapPitch, waveParams.m_DebugFlags);
  params->m_Timers.m_TicksNormals = Sys_GetClockTicks() - ticks;
}

idParallelJobRegistration register_OceanGenJob(
    reinterpret_cast<jobRun_t>(OceanGenJob), "OceanGenJob");
