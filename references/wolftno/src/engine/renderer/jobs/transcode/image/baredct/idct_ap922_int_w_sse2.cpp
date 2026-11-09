#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <cstdint>
#include <emmintrin.h>

namespace {

alignas(16) constexpr std::int16_t kTan1[8] = {
    13036, 13036, 13036, 13036, 13036, 13036, 13036, 13036};
alignas(16) constexpr std::int16_t kTan2[8] = {
    27146, 27146, 27146, 27146, 27146, 27146, 27146, 27146};
alignas(16) constexpr std::int16_t kTan3[8] = {
    -21746, -21746, -21746, -21746, -21746, -21746, -21746, -21746};
alignas(16) constexpr std::int16_t kCos4[8] = {
    -19195, -19195, -19195, -19195, -19195, -19195, -19195, -19195};

alignas(16) constexpr std::int16_t kInverse04[4][8] = {
    {16384, 21407, 16384, 8867, 16384, -8867, 16384, -21407},
    {16384, 8867, -16384, -21407, -16384, 21407, 16384, -8867},
    {22725, 19266, 19266, -4520, 12873, -22725, 4520, -12873},
    {12873, 4520, -22725, -12873, 4520, 19266, 19266, -22725},
};
alignas(16) constexpr std::int16_t kInverse17[4][8] = {
    {22725, 29692, 22725, 12299, 22725, -12299, 22725, -29692},
    {22725, 12299, -22725, -29692, -22725, 29692, 22725, -12299},
    {31521, 26722, 26722, -6270, 17855, -31521, 6270, -17855},
    {17855, 6270, -31521, -17855, 6270, 26722, 26722, -31521},
};
alignas(16) constexpr std::int16_t kInverse26[4][8] = {
    {21407, 27969, 21407, 11585, 21407, -11585, 21407, -27969},
    {21407, 11585, -21407, -27969, -21407, 27969, 21407, -11585},
    {29692, 25172, 25172, -5906, 16819, -29692, 5906, -16819},
    {16819, 5906, -29692, -16819, 5906, 25172, 25172, -29692},
};
alignas(16) constexpr std::int16_t kInverse35[4][8] = {
    {19266, 25172, 19266, 10426, 19266, -10426, 19266, -25172},
    {19266, 10426, -19266, -25172, -19266, 25172, 19266, -10426},
    {26722, 22654, 22654, -5315, 15137, -26722, 5315, -15137},
    {15137, 5315, -26722, -15137, 5315, 22654, 22654, -26722},
};

__m128i Load(const std::int16_t* values) {
  return _mm_load_si128(reinterpret_cast<const __m128i*>(values));
}

__m128i DequantizeAndShuffle(const __m128i* coefficients,
                             const __m128i* quantizers, int row) {
  __m128i values = _mm_mullo_epi16(_mm_load_si128(coefficients + row),
                                   _mm_load_si128(quantizers + row));
  values = _mm_shufflelo_epi16(values, 216);
  return _mm_shufflehi_epi16(values, 216);
}

void FirstPass(const __m128i& values, const std::int16_t table[4][8],
               int rounder, __m128i& low, __m128i& high) {
  const __m128i base = _mm_add_epi32(
      _mm_add_epi32(_mm_madd_epi16(_mm_shuffle_epi32(values, 0),
                                   Load(table[0])),
                    _mm_set1_epi32(rounder)),
      _mm_madd_epi16(_mm_shuffle_epi32(values, 170), Load(table[1])));
  const __m128i cross = _mm_add_epi32(
      _mm_madd_epi16(_mm_shuffle_epi32(values, 255), Load(table[3])),
      _mm_madd_epi16(_mm_shuffle_epi32(values, 85), Load(table[2])));
  low = _mm_srai_epi32(_mm_add_epi32(cross, base), 11);
  high = _mm_shuffle_epi32(_mm_srai_epi32(_mm_sub_epi32(base, cross), 11),
                           27);
}

}  // namespace

void IDCT_AP922_int_w_SSE2(const std::int16_t* coefficients,
                           const std::uint16_t* quantizers,
                           std::int16_t* destination) {
  const auto* coefficientVectors =
      reinterpret_cast<const __m128i*>(coefficients);
  const auto* quantizerVectors =
      reinterpret_cast<const __m128i*>(quantizers);
  auto* destinationVectors = reinterpret_cast<__m128i*>(destination);

  __m128i low;
  __m128i high;
  __m128i values =
      DequantizeAndShuffle(coefficientVectors, quantizerVectors, 0);
  FirstPass(values, kInverse04, 65536, low, high);
  destinationVectors[0] = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 2);
  FirstPass(values, kInverse26, 1024, low, high);
  destinationVectors[2] = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 4);
  FirstPass(values, kInverse04, 1024, low, high);
  destinationVectors[4] = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 6);
  FirstPass(values, kInverse26, 512, low, high);
  destinationVectors[6] = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 3);
  FirstPass(values, kInverse35, 1704, low, high);
  destinationVectors[3] = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 1);
  FirstPass(values, kInverse17, 1024, low, high);
  destinationVectors[1] = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 5);
  FirstPass(values, kInverse35, 455, low, high);
  const __m128i oddFive = _mm_packs_epi32(low, high);

  values = DequantizeAndShuffle(coefficientVectors, quantizerVectors, 7);
  FirstPass(values, kInverse17, 374, low, high);
  const __m128i oddSeven = _mm_packs_epi32(low, high);

  const __m128i tangent3 = Load(kTan3);
  const __m128i oddThree = _mm_load_si128(destinationVectors + 3);
  const __m128i rotation35 = _mm_adds_epi16(
      _mm_adds_epi16(_mm_mulhi_epi16(oddFive, tangent3), oddThree), oddFive);
  const __m128i rotation53 = _mm_subs_epi16(
      oddFive,
      _mm_adds_epi16(_mm_mulhi_epi16(oddThree, tangent3), oddThree));

  const __m128i tangent1 = Load(kTan1);
  const __m128i oddOne = _mm_load_si128(destinationVectors + 1);
  const __m128i rotation17 =
      _mm_adds_epi16(_mm_mulhi_epi16(oddSeven, tangent1), oddOne);
  const __m128i rotation71 =
      _mm_subs_epi16(_mm_mulhi_epi16(oddOne, tangent1), oddSeven);
  const __m128i oddLowDifference = _mm_subs_epi16(rotation71, rotation53);
  const __m128i oddHighSum = _mm_adds_epi16(rotation35, rotation17);
  const __m128i oddHighDifference = _mm_subs_epi16(rotation17, rotation35);
  const __m128i oddMiddleDifference =
      _mm_subs_epi16(oddHighDifference, oddLowDifference);
  const __m128i oddMiddleSum =
      _mm_adds_epi16(oddHighDifference, oddLowDifference);
  const __m128i oddLowSum = _mm_adds_epi16(rotation71, rotation53);

  const __m128i evenZero = _mm_load_si128(destinationVectors);
  const __m128i evenFour = _mm_load_si128(destinationVectors + 4);
  const __m128i evenLow = _mm_adds_epi16(evenFour, evenZero);
  const __m128i evenHigh = _mm_subs_epi16(evenZero, evenFour);
  const __m128i cosine4 = Load(kCos4);
  const __m128i oddCosDifference = _mm_adds_epi16(
      _mm_mulhi_epi16(oddMiddleDifference, cosine4), oddMiddleDifference);
  const __m128i oddCosSum = _mm_adds_epi16(
      oddMiddleSum, _mm_mulhi_epi16(oddMiddleSum, cosine4));

  const __m128i tangent2 = Load(kTan2);
  const __m128i evenTwo = _mm_load_si128(destinationVectors + 2);
  const __m128i evenSix = _mm_load_si128(destinationVectors + 6);
  const __m128i rotation26 =
      _mm_adds_epi16(_mm_mulhi_epi16(evenSix, tangent2), evenTwo);
  const __m128i rotation62 =
      _mm_subs_epi16(_mm_mulhi_epi16(evenTwo, tangent2), evenSix);

  const __m128i evenSum = _mm_adds_epi16(evenLow, rotation26);
  const __m128i evenDifference = _mm_subs_epi16(evenLow, rotation26);
  const __m128i evenHighSum = _mm_adds_epi16(evenHigh, rotation62);
  const __m128i evenHighDifference = _mm_subs_epi16(evenHigh, rotation62);

  destinationVectors[0] =
      _mm_srai_epi16(_mm_adds_epi16(oddHighSum, evenSum), 6);
  destinationVectors[1] =
      _mm_srai_epi16(_mm_adds_epi16(evenHighSum, oddCosSum), 6);
  destinationVectors[2] =
      _mm_srai_epi16(_mm_adds_epi16(evenHighDifference, oddCosDifference), 6);
  destinationVectors[3] =
      _mm_srai_epi16(_mm_adds_epi16(oddLowSum, evenDifference), 6);
  destinationVectors[4] =
      _mm_srai_epi16(_mm_subs_epi16(evenDifference, oddLowSum), 6);
  destinationVectors[5] = _mm_srai_epi16(
      _mm_subs_epi16(evenHighDifference, oddCosDifference), 6);
  destinationVectors[6] =
      _mm_srai_epi16(_mm_subs_epi16(evenHighSum, oddCosSum), 6);
  destinationVectors[7] =
      _mm_srai_epi16(_mm_subs_epi16(evenSum, oddHighSum), 6);
}
