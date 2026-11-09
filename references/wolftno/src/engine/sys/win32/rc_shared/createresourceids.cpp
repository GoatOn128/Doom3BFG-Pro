#include <immintrin.h>

#include <cstdint>

// CreateResourceIDs was linked into the retail image through the shared RC
// tool support library. Its only PDB-owned runtime records are initialization
// of these SIMD constants; the resource-ID generator itself is a build tool
// and has no callable function in WolfNewOrder_x64.exe.
namespace resourceIdToolSimd {

alignas(16) const __m128 vectorFloatAbsMask = _mm_castsi128_ps(
    _mm_set1_epi32(static_cast<int>(0x7FFFFFFFU)));
alignas(16) const __m128 vectorFloatSignBit = _mm_castsi128_ps(
    _mm_set1_epi32(static_cast<int>(0x80000000U)));
alignas(16) const __m128 vectorFloatLastSignBit = _mm_castsi128_ps(
    _mm_set_epi32(static_cast<int>(0x80000000U), 0, 0, 0));
alignas(16) const __m128 vectorFloatFirstSignBit = _mm_castsi128_ps(
    _mm_set_epi32(0, 0, 0, static_cast<int>(0x80000000U)));
alignas(16) const __m128 vectorFloatClearLast = _mm_castsi128_ps(
    _mm_set_epi32(0, -1, -1, -1));
alignas(16) const __m128 dcOffsetV128 = _mm_set1_ps(128.0F);

}  // namespace resourceIdToolSimd

