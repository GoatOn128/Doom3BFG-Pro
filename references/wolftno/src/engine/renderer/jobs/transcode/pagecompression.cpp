#include "engine/renderer/jobs/transcode/pagecompression.h"

#include <cstdint>
#include <immintrin.h>

void DefaultUncompressedPageData(std::uint8_t* pageData) {
  auto* diffuse = reinterpret_cast<std::uint32_t*>(pageData);
  auto* normal = reinterpret_cast<std::uint32_t*>(pageData + 0x10000);
  auto* specular = reinterpret_cast<std::uint32_t*>(pageData + 0x20000);
  for (int pixel = 0; pixel < 0x4000; ++pixel) {
    diffuse[pixel] = 0x00008080u;
    normal[pixel] = 0;
    specular[pixel] = 0xff000000u;
  }
}

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

idCVar vt_maxMonoSpecularError(
    "vt_maxMonoSpecularError", "10", 18,
    "specular is considered monochrome if the error is less than this value");
idCVar vt_useSpecularScale(
    "vt_useSpecularScale", "1", 17,
    "scale the specular to avoid DXT quantization errors");
idCVar vt_specularThreshold("vt_specularThreshold", "0", 20, "", 0.0f,
                            255.0f);
idCVar vt_specularScale("vt_specularScale", "0.2", 20, "", 0.0f, 255.0f);

idCVar vt_qualityDCTLuma(
    "vt_qualityDCTLuma", "90", 18,
    "quality in the range [0-100] percent where higher is better quality",
    0.0f, 100.0f);
idCVar vt_qualityDCTChroma("vt_qualityDCTChroma", "80", 18,
                           "quality in the range [0-100] percent", 0.0f,
                           100.0f);
idCVar vt_qualityDCTNormal("vt_qualityDCTNormal", "90", 18,
                           "quality in the range [0-100] percent", 0.0f,
                           100.0f);
idCVar vt_qualityDCTSpecular("vt_qualityDCTSpecular", "80", 18,
                             "quality in the range [0-100] percent", 0.0f,
                             100.0f);
idCVar vt_qualityDCTPower("vt_qualityDCTPower", "80", 18,
                          "quality in the range [0-100] percent", 0.0f,
                          100.0f);

namespace {

constexpr const char* kQuantizationDescription =
    "amount of quantization to apply in the range [0-255] where lower is "
    "better quality";

}  // namespace

idCVar vt_qualityHDPDiffuse("vt_qualityHDPDiffuse", "30", 18,
                            kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityHDPNormal("vt_qualityHDPNormal", "13", 18,
                           kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityHDPSpecular("vt_qualityHDPSpecular", "25", 18,
                             kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityHDPPower("vt_qualityHDPPower", "40", 18,
                          kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityHDPLossless("vt_qualityHDPLossless", "0", 17,
                             "force lossless HD-Photo compression", 0.0f,
                             255.0f);
idCVar vt_qualityJXRDiffuse("vt_qualityJXRDiffuse", "30", 18,
                            kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityJXRNormal("vt_qualityJXRNormal", "10", 18,
                           kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityJXRSpecular("vt_qualityJXRSpecular", "25", 18,
                             kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityJXRPower("vt_qualityJXRPower", "40", 18,
                          kQuantizationDescription, 0.0f, 255.0f);
idCVar vt_qualityJXRLossless("vt_qualityJXRLossless", "0", 17,
                             "force lossless JPEG-XR compression", 0.0f,
                             255.0f);
