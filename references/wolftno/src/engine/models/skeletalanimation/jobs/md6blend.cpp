#include "engine/models/skeletalanimation/jobs/md6blend.h"

#include "engine/models/skeletalanimation/userchannelexpression.h"
#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace {

constexpr float kQuatRange = 0.70710678118654752440f;
constexpr float kInv255 = 1.0f / 255.0f;

template <typename To, typename From> To BitCopy(const From& from) {
  static_assert(sizeof(To) == sizeof(From));
  To to;
  std::memcpy(&to, &from, sizeof(to));
  return to;
}

float FrameFraction(std::uint32_t bits) { return BitCopy<float>(bits); }

void Copy4(const float* source, float* destination) {
  std::memcpy(destination, source, 4 * sizeof(float));
}

void SetIdentityRST(float* r, float* s, float* t) {
  r[0] = r[1] = r[2] = 0.0f;
  r[3] = 1.0f;
  s[0] = s[1] = s[2] = s[3] = 1.0f;
  t[0] = t[1] = t[2] = t[3] = 0.0f;
}

void NormalizeQuat(float* q) {
  const float lengthSquared =
      q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
  const float inverse =
      1.0f / std::sqrt((std::max)(lengthSquared,
                                  idMath::FLT_SMALLEST_NON_DENORMAL));
  for (int component = 0; component < 4; ++component)
    q[component] *= inverse;
}

void QuatMultiply(const float* a, const float* b, float* out) {
  const float result[4] = {
      a[0] * b[3] + a[3] * b[0] + b[1] * a[2] - b[2] * a[1],
      a[3] * b[1] + a[1] * b[3] + b[2] * a[0] - a[2] * b[0],
      a[3] * b[2] + a[2] * b[3] + a[1] * b[0] - b[1] * a[0],
      a[3] * b[3] - a[0] * b[0] - b[1] * a[1] - b[2] * a[2]};
  std::memcpy(out, result, sizeof(result));
}

void QuatConjugate(const float* q, float* out) {
  out[0] = -q[0];
  out[1] = -q[1];
  out[2] = -q[2];
  out[3] = q[3];
}

void QuatRotate(const float* q, const float* input, float* output) {
  const float tx = 2.0f * (q[1] * input[2] - q[2] * input[1]);
  const float ty = 2.0f * (q[2] * input[0] - q[0] * input[2]);
  const float tz = 2.0f * (q[0] * input[1] - q[1] * input[0]);
  output[0] = input[0] + q[3] * tx + q[1] * tz - q[2] * ty;
  output[1] = input[1] + q[3] * ty + q[2] * tx - q[0] * tz;
  output[2] = input[2] + q[3] * tz + q[0] * ty - q[1] * tx;
  output[3] = 0.0f;
}

void QuatNlerp(const float* a, const float* b, float amount, float* out) {
  const float dot =
      a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  const float sign = dot < 0.0f ? -1.0f : 1.0f;
  for (int component = 0; component < 4; ++component)
    out[component] = a[component] +
                     (sign * b[component] - a[component]) * amount;
  NormalizeQuat(out);
}

void QuatToRows(const float* q, const float* scale, const float* translation,
                float* rows) {
  const float xx = q[0] * q[0];
  const float yy = q[1] * q[1];
  const float zz = q[2] * q[2];
  const float xy = q[0] * q[1];
  const float xz = q[0] * q[2];
  const float yz = q[1] * q[2];
  const float wx = q[3] * q[0];
  const float wy = q[3] * q[1];
  const float wz = q[3] * q[2];
  rows[0] = (1.0f - 2.0f * (yy + zz)) * scale[0];
  rows[1] = (2.0f * (xy - wz)) * scale[1];
  rows[2] = (2.0f * (xz + wy)) * scale[2];
  rows[3] = translation[0];
  rows[4] = (2.0f * (xy + wz)) * scale[0];
  rows[5] = (1.0f - 2.0f * (xx + zz)) * scale[1];
  rows[6] = (2.0f * (yz - wx)) * scale[2];
  rows[7] = translation[1];
  rows[8] = (2.0f * (xz - wy)) * scale[0];
  rows[9] = (2.0f * (yz + wx)) * scale[1];
  rows[10] = (1.0f - 2.0f * (xx + yy)) * scale[2];
  rows[11] = translation[2];
}

void RowsToQuat(const float* rows, float* q) {
  const float trace = rows[0] + rows[5] + rows[10];
  if (trace > 0.0f) {
    const float root = std::sqrt(trace + 1.0f);
    q[3] = 0.5f * root;
    const float inverse = 0.5f / root;
    q[0] = (rows[9] - rows[6]) * inverse;
    q[1] = (rows[2] - rows[8]) * inverse;
    q[2] = (rows[4] - rows[1]) * inverse;
  } else {
    static constexpr int next[3] = {1, 2, 0};
    int i = rows[5] > rows[0] ? 1 : 0;
    if (rows[10] > rows[i * 4 + i])
      i = 2;
    const int j = next[i];
    const int k = next[j];
    const float root = std::sqrt((std::max)(
        rows[i * 4 + i] - rows[j * 4 + j] - rows[k * 4 + k] + 1.0f,
        idMath::FLT_SMALLEST_NON_DENORMAL));
    q[i] = 0.5f * root;
    const float inverse = 0.5f / root;
    q[3] = (rows[k * 4 + j] - rows[j * 4 + k]) * inverse;
    q[j] = (rows[j * 4 + i] + rows[i * 4 + j]) * inverse;
    q[k] = (rows[k * 4 + i] + rows[i * 4 + k]) * inverse;
  }
  NormalizeQuat(q);
}

void ComposeRows(const float* local, const float* parent, float* out) {
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      out[row * 4 + column] =
          local[row * 4 + 0] * parent[0 * 4 + column] +
          local[row * 4 + 1] * parent[1 * 4 + column] +
          local[row * 4 + 2] * parent[2 * 4 + column];
    }
    out[row * 4 + 3] =
        local[row * 4 + 3] +
        local[row * 4 + 0] * parent[0 * 4 + 3] +
        local[row * 4 + 1] * parent[1 * 4 + 3] +
        local[row * 4 + 2] * parent[2 * 4 + 3];
  }
}

float WeightedLerpAmount(std::uint8_t aWeight, std::uint8_t bWeight,
                         std::uint8_t filter, float alpha) {
  const float aw = static_cast<float>(aWeight) * filter;
  const float bw = static_cast<float>(bWeight) * filter;
  if (aw <= 0.0f && bw <= 0.0f)
    return 0.0f;
  if (bw <= aw)
    return aw > 0.0f ? bw * alpha / aw : 0.0f;
  return (bw - aw + aw * alpha) / bw;
}

float AdditiveAmount(std::uint8_t weight, std::uint8_t filter, float alpha) {
  return alpha * static_cast<float>(weight) * static_cast<float>(filter) *
         kInv255 * kInv255;
}

std::uint8_t SaturatingWeight(std::uint8_t a, std::uint8_t b) {
  return static_cast<std::uint8_t>((std::min)(255, static_cast<int>(a) + b));
}

const std::uint8_t* WeightTable(const idMD6SkelData* skeleton,
                                std::uint16_t offset) {
  return offset == 0 ? nullptr
                     : reinterpret_cast<const std::uint8_t*>(skeleton) +
                           offset;
}

std::uint8_t FilterAt(const std::uint8_t* filter, int index) {
  return filter == nullptr ? 255 : filter[index];
}

void DecodeSmallestThree(const std::uint16_t* packed, float* q) {
  const unsigned omitted = ((packed[0] >> 15) & 1u) |
                           (((packed[1] >> 15) & 1u) << 1u);
  const bool negative = (packed[2] & 0x8000u) != 0;
  float values[3];
  for (int i = 0; i < 3; ++i) {
    values[i] = (static_cast<float>(packed[i] & 0x7fffu) / 32767.0f) *
                    (2.0f * kQuatRange) -
                kQuatRange;
  }
  float sum = 0.0f;
  int source = 0;
  for (int component = 0; component < 4; ++component) {
    if (component == static_cast<int>(omitted))
      continue;
    q[component] = values[source++];
    sum += q[component] * q[component];
  }
  q[omitted] = std::sqrt((std::max)(0.0f, 1.0f - sum));
  if (negative)
    q[omitted] = -q[omitted];
  NormalizeQuat(q);
}

const frameSetData_t* FrameSetForFrame(const idMD6AnimData* animData,
                                       int frame) {
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(animData);
  const auto tableIndex = bytes[animData->frameSetTblOffset + frame];
  std::uint32_t shiftedOffset = 0;
  std::memcpy(&shiftedOffset,
              bytes + animData->frameSetOffsetTblOffset + tableIndex * 4,
              sizeof(shiftedOffset));
  return reinterpret_cast<const frameSetData_t*>(bytes + shiftedOffset * 16u);
}

int ClampFrame(const idMD6AnimData* animation, float frameValue,
               float& fraction) {
  fraction = 0.0f;
  if (animation == nullptr || animation->numFrames == 0)
    return 0;
  if (frameValue <= 0.0f)
    return 0;
  if (frameValue >= animation->numFrames)
    return animation->numFrames - 1;
  const int frame = static_cast<int>(frameValue);
  fraction = frameValue - static_cast<float>(frame);
  return frame;
}

} // namespace

bool idMath::IsValid(float f) {
  const std::uint32_t bits = BitCopy<std::uint32_t>(f);
  return (bits & 0x7f800000u) != 0 || (bits & 0x007fffffu) == 0;
}

idMat3& idMat3::OrthoNormalizeSelf(std::uint8_t prio1,
                                   std::uint8_t prio2) {
  const int third = 3 - prio1 - prio2;
  mat[prio1].Normalize();
  mat[third] = mat[prio1].Cross(mat[prio2]);
  mat[third].Normalize();
  mat[prio2] = mat[third].Cross(mat[prio1]);
  mat[prio2].Normalize();
  return *this;
}

namespace quat_t {
void ToMat3(const float* q, idMat3* out) {
  float rows[12];
  const float scale[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  const float translation[4] = {};
  QuatToRows(q, scale, translation, rows);
  for (int row = 0; row < 3; ++row)
    for (int column = 0; column < 3; ++column)
      out->mat[row][column] = rows[row * 4 + column];
}
} // namespace quat_t

void BlendShapeFirst(__m128i* destMap, const __m128i* sourceData,
                     int numVerts, float blendValue) {
  auto* destination = reinterpret_cast<std::uint8_t*>(destMap);
  const auto* source = reinterpret_cast<const std::uint8_t*>(sourceData);
  const int blend = static_cast<int>(blendValue * 256.0f);
  for (int vertex = 0; vertex < numVerts; ++vertex) {
    for (int component = 0; component < 3; ++component) {
      const int value =
          (128 * (256 - blend) + source[vertex * 3 + component] * blend) >>
          8;
      destination[vertex * 4 + component] = static_cast<std::uint8_t>(
          (std::clamp)(value, 0, 255));
    }
    destination[vertex * 4 + 3] = 128;
  }
}

void BlendShapeAdd(__m128i* destMap, const __m128i* source2Data,
                   const __m128i* sourceData, int numVerts,
                   std::uint32_t blendValue) {
  auto* destination = reinterpret_cast<std::uint8_t*>(destMap);
  const auto* base = reinterpret_cast<const std::uint8_t*>(source2Data);
  const auto* source = reinterpret_cast<const std::uint8_t*>(sourceData);
  const int blend = static_cast<int>(BitCopy<float>(blendValue) * 256.0f);
  for (int vertex = 0; vertex < numVerts; ++vertex) {
    for (int component = 0; component < 4; ++component) {
      const int sourceValue =
          component < 3 ? source[vertex * 3 + component] : 128;
      const int value = base[vertex * 4 + component] +
                        ((sourceValue - 128) * blend >> 8);
      destination[vertex * 4 + component] = static_cast<std::uint8_t>(
          (std::clamp)(value, 0, 255));
    }
  }
}

namespace idMD6Blend {

void DecodeBasePoseRST(std::uint32_t numJoints, const float* inR,
                       const float* inS, const float* inT, float* outR,
                       float* outS, float* outT) {
  for (std::uint32_t joint = 0; joint < numJoints; ++joint) {
    Copy4(inR + joint * 4, outR + joint * 4);
    outS[joint * 4 + 0] = inS[joint * 3 + 0];
    outS[joint * 4 + 1] = inS[joint * 3 + 1];
    outS[joint * 4 + 2] = inS[joint * 3 + 2];
    outS[joint * 4 + 3] = 0.0f;
    outT[joint * 4 + 0] = inT[joint * 3 + 0];
    outT[joint * 4 + 1] = inT[joint * 3 + 1];
    outT[joint * 4 + 2] = inT[joint * 3 + 2];
    outT[joint * 4 + 3] = 0.0f;
  }
}

void DecodeBasePoseU(std::uint32_t numUserChannels, const float* inU,
                     float* outU) {
  std::memmove(outU, inU, numUserChannels * sizeof(float));
}

void DecodeConstantR(const std::uint8_t* jointsR, std::uint32_t numJointsR,
                     const char* inR, float* outR) {
  const auto* packed = reinterpret_cast<const std::uint16_t*>(inR);
  for (std::uint32_t index = 0; index < numJointsR; ++index)
    DecodeSmallestThree(packed + index * 3, outR + jointsR[index] * 4);
}

void DecodeConstantU(const std::uint8_t* channelsU,
                     std::uint32_t numUserChannels, const float* inU,
                     float* outU) {
  for (std::uint32_t index = 0; index < numUserChannels; ++index)
    outU[channelsU[index]] = inU[index];
}

std::int64_t DecodeFrameOffsets(int numJoints, std::uint32_t frame,
                                std::uint32_t numFrames,
                                std::uint32_t stride, const void* firstKeys,
                                const void* rangeKeys, const void* nextKeys,
                                const std::uint8_t* frameBits,
                                frameOffsets_t* offsets, std::uint32_t pad) {
  if (numJoints <= 0)
    return 0;
  const std::uint32_t bitBytes = (numFrames + 7u) >> 3u;
  const auto first = reinterpret_cast<std::uintptr_t>(firstKeys);
  const auto range = reinterpret_cast<std::uintptr_t>(rangeKeys);
  const auto next = reinterpret_cast<std::uintptr_t>(nextKeys);
  for (int channel = 0; channel < numJoints; ++channel) {
    const std::uint8_t* bits = frameBits + channel * bitBytes;
    int leftFrame = 0;
    int rightFrame = static_cast<int>(numFrames);
    int leftOrdinal = 0;
    int ordinal = 0;
    for (std::uint32_t keyFrame = 0; keyFrame < numFrames; ++keyFrame) {
      if ((bits[keyFrame >> 3u] & (1u << (keyFrame & 7u))) == 0)
        continue;
      if (keyFrame <= frame) {
        leftFrame = static_cast<int>(keyFrame);
        leftOrdinal = ordinal;
      } else {
        rightFrame = static_cast<int>(keyFrame);
        break;
      }
      ++ordinal;
    }
    if (rightFrame == static_cast<int>(numFrames))
      rightFrame = (std::max)(leftFrame + 1, static_cast<int>(numFrames));
    offsets->leftFrameOffset[channel] =
        static_cast<std::uint8_t>((std::min)(leftFrame, 255));
    offsets->rightFrameOffset[channel] =
        static_cast<std::uint8_t>((std::min)(rightFrame, 255));
    const std::uintptr_t leftAddress =
        leftOrdinal == 0 ? first + channel * stride
                         : range + (leftOrdinal - 1) * stride + channel * pad;
    const std::uintptr_t rightAddress =
        next + leftOrdinal * stride + channel * pad;
    offsets->leftKeyOffset[channel] =
        static_cast<std::uint16_t>(leftAddress - first);
    offsets->rightKeyOffset[channel] =
        static_cast<std::uint16_t>(rightAddress - first);
  }
  const int padded = (numJoints + 3) & ~3;
  for (int channel = numJoints; channel < padded; ++channel) {
    offsets->leftFrameOffset[channel] = 0;
    offsets->rightFrameOffset[channel] = 1;
    offsets->leftKeyOffset[channel] = 0;
    offsets->rightKeyOffset[channel] = 0;
  }
  return padded;
}

void DecodeFrameSetR(const std::uint8_t* joints, int numJoints,
                     std::uint32_t frame, std::uint32_t numFrames,
                     std::uint32_t fraction, const std::int16_t* firstKeys,
                     const std::int16_t* rangeKeys,
                     const std::int16_t* nextKeys,
                     const std::uint8_t* frameBits, float* out) {
  frameOffsets_t offsets{};
  DecodeFrameOffsets(numJoints, frame, numFrames, 6, firstKeys, rangeKeys,
                     nextKeys, frameBits, &offsets, 2);
  const float sample = static_cast<float>(frame) + FrameFraction(fraction);
  const auto* firstBytes = reinterpret_cast<const std::uint8_t*>(firstKeys);
  for (int index = 0; index < numJoints; ++index) {
    float left[4], right[4];
    DecodeSmallestThree(reinterpret_cast<const std::uint16_t*>(
                            firstBytes + offsets.leftKeyOffset[index]),
                        left);
    DecodeSmallestThree(reinterpret_cast<const std::uint16_t*>(
                            firstBytes + offsets.rightKeyOffset[index]),
                        right);
    const float denominator = static_cast<float>(
        offsets.rightFrameOffset[index] - offsets.leftFrameOffset[index]);
    const float amount = denominator > 0.0f
                             ? (sample - offsets.leftFrameOffset[index]) /
                                   denominator
                             : 0.0f;
    QuatNlerp(left, right, amount, out + joints[index] * 4);
  }
}

void DecodeFrameSetST(const std::uint8_t* joints, int numJoints,
                      std::uint32_t frame, std::uint32_t numFrames,
                      std::uint32_t fraction, const float* firstKeys,
                      const float* rangeKeys, const float* nextKeys,
                      const std::uint8_t* frameBits, float* out) {
  frameOffsets_t offsets{};
  DecodeFrameOffsets(numJoints, frame, numFrames, 12, firstKeys, rangeKeys,
                     nextKeys, frameBits, &offsets, 4);
  const float sample = static_cast<float>(frame) + FrameFraction(fraction);
  const auto* firstBytes = reinterpret_cast<const std::uint8_t*>(firstKeys);
  for (int index = 0; index < numJoints; ++index) {
    const auto* left = reinterpret_cast<const float*>(
        firstBytes + offsets.leftKeyOffset[index]);
    const auto* right = reinterpret_cast<const float*>(
        firstBytes + offsets.rightKeyOffset[index]);
    const float denominator = static_cast<float>(
        offsets.rightFrameOffset[index] - offsets.leftFrameOffset[index]);
    const float amount = denominator > 0.0f
                             ? (sample - offsets.leftFrameOffset[index]) /
                                   denominator
                             : 0.0f;
    float* destination = out + joints[index] * 4;
    for (int component = 0; component < 3; ++component)
      destination[component] =
          left[component] + (right[component] - left[component]) * amount;
    destination[3] = 0.0f;
  }
}

void DecodeFrameSetU(const std::uint8_t* channelsU, int numUserChannels,
                     std::uint32_t frame, std::uint32_t numFrames,
                     std::uint32_t fraction, const float* firstKeys,
                     const float* rangeKeys, const float* nextKeys,
                     const std::uint8_t* frameBits, float* out) {
  frameOffsets_t offsets{};
  DecodeFrameOffsets(numUserChannels, frame, numFrames, 4, firstKeys,
                     rangeKeys, nextKeys, frameBits, &offsets, 4);
  const float sample = static_cast<float>(frame) + FrameFraction(fraction);
  const auto* firstBytes = reinterpret_cast<const std::uint8_t*>(firstKeys);
  for (int index = 0; index < numUserChannels; ++index) {
    const float left = *reinterpret_cast<const float*>(
        firstBytes + offsets.leftKeyOffset[index]);
    const float right = *reinterpret_cast<const float*>(
        firstBytes + offsets.rightKeyOffset[index]);
    const float denominator = static_cast<float>(
        offsets.rightFrameOffset[index] - offsets.leftFrameOffset[index]);
    const float amount = denominator > 0.0f
                             ? (sample - offsets.leftFrameOffset[index]) /
                                   denominator
                             : 0.0f;
    out[channelsU[index]] = left + (right - left) * amount;
  }
}

void LerpRST(std::uint32_t numJoints, float alpha,
             const std::uint8_t* filter, const float* aR, const float* aS,
             const float* aT, const std::uint8_t* aW, const float* bR,
             const float* bS, const float* bT, std::uint32_t* bW,
             float* outR, float* outS, float* outT, std::uint8_t* outW) {
  const auto* bWeights = reinterpret_cast<const std::uint8_t*>(bW);
  for (std::uint32_t joint = 0; joint < numJoints; ++joint) {
    const float amount = WeightedLerpAmount(
        aW[joint], bWeights[joint], FilterAt(filter, joint), alpha);
    QuatNlerp(aR + joint * 4, bR + joint * 4, amount,
              outR + joint * 4);
    for (int component = 0; component < 4; ++component) {
      outS[joint * 4 + component] =
          aS[joint * 4 + component] +
          (bS[joint * 4 + component] - aS[joint * 4 + component]) * amount;
      outT[joint * 4 + component] =
          aT[joint * 4 + component] +
          (bT[joint * 4 + component] - aT[joint * 4 + component]) * amount;
    }
    outW[joint] = SaturatingWeight(aW[joint], bWeights[joint]);
  }
}

void AddRST(std::uint32_t numJoints, float alpha,
            const std::uint8_t* filter, const float* aR, const float* aS,
            const float* aT, const std::uint8_t* aW, const float* bR,
            const float* bS, const float* bT, const std::uint8_t* bW,
            float* outR, float* outS, float* outT, std::uint8_t* outW) {
  const float identity[4] = {0.0f, 0.0f, 0.0f, 1.0f};
  for (std::uint32_t joint = 0; joint < numJoints; ++joint) {
    const float amount =
        AdditiveAmount(bW[joint], FilterAt(filter, joint), alpha);
    float weighted[4], product[4];
    QuatNlerp(identity, bR + joint * 4, amount, weighted);
    QuatMultiply(aR + joint * 4, weighted, product);
    NormalizeQuat(product);
    Copy4(product, outR + joint * 4);
    for (int component = 0; component < 4; ++component) {
      outS[joint * 4 + component] =
          aS[joint * 4 + component] *
          (1.0f + (bS[joint * 4 + component] - 1.0f) * amount);
      outT[joint * 4 + component] =
          aT[joint * 4 + component] + bT[joint * 4 + component] * amount;
    }
    outW[joint] = aW[joint];
  }
}

void SubRST(std::uint32_t numJoints, float alpha,
            const std::uint8_t* filter, const float* aR, const float* aS,
            const float* aT, const std::uint8_t* aW, const float* bR,
            const float* bS, const float* bT, const std::uint8_t* bW,
            float* outR, float* outS, float* outT, std::uint8_t* outW) {
  std::vector<float> inverseR(numJoints * 4);
  std::vector<float> inverseS(numJoints * 4);
  std::vector<float> inverseT(numJoints * 4);
  for (std::uint32_t joint = 0; joint < numJoints; ++joint) {
    QuatConjugate(bR + joint * 4, inverseR.data() + joint * 4);
    for (int component = 0; component < 4; ++component) {
      inverseS[joint * 4 + component] =
          bS[joint * 4 + component] != 0.0f
              ? 1.0f / bS[joint * 4 + component]
              : 1.0f;
      inverseT[joint * 4 + component] = -bT[joint * 4 + component];
    }
  }
  AddRST(numJoints, alpha, filter, aR, aS, aT, aW, inverseR.data(),
         inverseS.data(), inverseT.data(), bW, outR, outS, outT, outW);
}

void AddSubU(std::uint32_t numUserChannels, float alpha,
             const std::uint8_t* filter, const float* aU,
             const std::uint8_t* aW, const float* bU,
             const std::uint8_t* bW, float* outU, std::uint8_t* outW) {
  for (std::uint32_t channel = 0; channel < numUserChannels; ++channel) {
    outU[channel] = aU[channel] +
                    bU[channel] * AdditiveAmount(
                                        bW[channel], FilterAt(filter, channel),
                                        alpha);
    outW[channel] = aW[channel];
  }
}

void LerpU(int numUserChannels, float alpha, const std::uint8_t* filter,
           const float* aU, const std::uint8_t* aW, const float* bU,
           const std::uint8_t* bW, float* outU, std::uint8_t* outW) {
  for (int channel = 0; channel < numUserChannels; ++channel) {
    const float amount = WeightedLerpAmount(
        aW[channel], bW[channel], FilterAt(filter, channel), alpha);
    outU[channel] = aU[channel] + (bU[channel] - aU[channel]) * amount;
    outW[channel] = SaturatingWeight(aW[channel], bW[channel]);
  }
}

void RetargetAdditiveRST(const idMD6SkelData* skeleton, float* outR,
                         float* outS, float* outT,
                         std::uint32_t numJoints) {
  const int padded = (skeleton->numJoints + 7) & ~7;
  const auto* baseR = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(skeleton) +
      skeleton->basePoseOffset);
  const float* baseS = baseR + padded * 4;
  const float* baseT = baseR + padded * 7;
  for (std::uint32_t joint = 0; joint < numJoints; ++joint) {
    float product[4];
    QuatMultiply(outR + joint * 4, baseR + joint * 4, product);
    NormalizeQuat(product);
    Copy4(product, outR + joint * 4);
    for (int component = 0; component < 3; ++component) {
      outS[joint * 4 + component] *= baseS[joint * 3 + component];
      outT[joint * 4 + component] += baseT[joint * 3 + component];
    }
    outS[joint * 4 + 3] = 0.0f;
    outT[joint * 4 + 3] = 0.0f;
  }
}

void RetargetAdditiveU(const idMD6SkelData* skeleton, float* outU,
                       int numUserChannels) {
  const int paddedJoints = (skeleton->numJoints + 7) & ~7;
  const auto* baseU = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(skeleton) +
      skeleton->basePoseOffset + paddedJoints * 40);
  for (int channel = 0; channel < numUserChannels; ++channel)
    outU[channel] += baseU[channel];
}

void DecodeSkeletonFrame(const idMD6SkelData* skeleton, float* stackR,
                         float* stackS, float* stackT, float* stackU) {
  const int paddedJoints = (skeleton->numJoints + 7) & ~7;
  const auto* baseR = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(skeleton) +
      skeleton->basePoseOffset);
  DecodeBasePoseRST(paddedJoints, baseR, baseR + paddedJoints * 4,
                    baseR + paddedJoints * 7, stackR, stackS, stackT);
  if (stackU != nullptr) {
    const int paddedChannels = (skeleton->numUserChannels + 7) & ~7;
    DecodeBasePoseU(paddedChannels, baseR + paddedJoints * 10, stackU);
  }
}

void TransformLocalJointToModelJoint(const std::int16_t* parentTable,
                                     int joint, const float* inR,
                                     const float* inS, const float* inT,
                                     float* outR, float* outS, float* outT) {
  if (joint < 0) {
    SetIdentityRST(outR, outS, outT);
    return;
  }
  const int parent = parentTable[joint];
  if (parent < 0) {
    Copy4(inR + joint * 4, outR);
    Copy4(inS + joint * 4, outS);
    Copy4(inT + joint * 4, outT);
    return;
  }
  float parentR[4], parentS[4], parentT[4];
  TransformLocalJointToModelJoint(parentTable, parent, inR, inS, inT,
                                  parentR, parentS, parentT);
  QuatMultiply(inR + joint * 4, parentR, outR);
  NormalizeQuat(outR);
  for (int component = 0; component < 3; ++component)
    outS[component] = parentS[component] * inS[joint * 4 + component];
  outS[3] = 0.0f;
  float scaled[4] = {parentS[0] * inT[joint * 4 + 0],
                     parentS[1] * inT[joint * 4 + 1],
                     parentS[2] * inT[joint * 4 + 2], 0.0f};
  float rotated[4];
  QuatRotate(parentR, scaled, rotated);
  for (int component = 0; component < 3; ++component)
    outT[component] = parentT[component] + rotated[component];
  outT[3] = 0.0f;
}

int GetAnimMapIndex(const idMD6AnimData* animData,
                    idCrcHandle_t targetParentTableCRC) {
  const auto* maps = reinterpret_cast<const std::uint16_t*>(animData + 1);
  for (int index = 0; index < animData->numAnimMaps; ++index)
    if (maps[index] == targetParentTableCRC.value)
      return index;
  return -1;
}

int DecodeRLE(const std::uint8_t* rle, std::uint16_t sentinel,
              std::uint8_t* indices) {
  const int count = (std::min)(static_cast<int>(rle[0]),
                               static_cast<int>(sentinel));
  const std::uint8_t* source = rle + 1;
  int output = 0;
  while (output < count) {
    const std::uint8_t control = *source++;
    int run = control & 0x7f;
    if ((control & 0x80u) != 0) {
      while (run-- > 0 && output < count)
        indices[output++] = static_cast<std::uint8_t>(sentinel);
      continue;
    }
    std::uint8_t value = *source++;
    while (run-- > 0 && output < count) {
      indices[output++] = value < sentinel
                              ? value++
                              : static_cast<std::uint8_t>(sentinel);
    }
  }
  const int padded = (count + 7) & ~7;
  std::fill(indices + count, indices + padded,
            static_cast<std::uint8_t>(sentinel));
  return count;
}

void DecodeAnimFrame(const idMD6SkelData* skeleton,
                     std::uint16_t numPaddedJoints,
                     const idMD6AnimData* animData,
                     const frameSetData_t* frameSetData, int frame,
                     std::uint32_t frameFraction, float* stackR,
                     float* stackS, float* stackT, float* stackU) {
  const int paddedChannels = (skeleton->numUserChannels + 7) & ~7;
  if ((animData->flags & 0x101u) != 0) {
    for (int joint = 0; joint < numPaddedJoints; ++joint)
      SetIdentityRST(stackR + joint * 4, stackS + joint * 4,
                     stackT + joint * 4);
    if (stackU != nullptr)
      std::fill(stackU, stackU + paddedChannels, 0.0f);
  } else {
    DecodeSkeletonFrame(skeleton, stackR, stackS, stackT, stackU);
  }

  int mapIndex = GetAnimMapIndex(animData, skeleton->parentTblCrc);
  if (mapIndex < 0)
    mapIndex = 0;
  const auto* mapOffsets = reinterpret_cast<const std::uint16_t*>(animData + 1);
  const int mapBase = animData->numAnimMaps + mapIndex * 8;
  std::array<std::uint8_t, 272> jointsR{};
  std::array<std::uint8_t, 272> jointsS{};
  std::array<std::uint8_t, 272> jointsT{};
  std::array<std::uint8_t, 272> channelsU{};
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(animData);

  const int constantR = DecodeRLE(bytes + mapOffsets[mapBase + 0],
                                  numPaddedJoints, jointsR.data());
  const int constantS = DecodeRLE(bytes + mapOffsets[mapBase + 1],
                                  numPaddedJoints, jointsS.data());
  const int constantT = DecodeRLE(bytes + mapOffsets[mapBase + 2],
                                  numPaddedJoints, jointsT.data());
  const int constantU = stackU == nullptr
                            ? 0
                            : DecodeRLE(bytes + mapOffsets[mapBase + 3],
                                        paddedChannels, channelsU.data());
  DecodeConstantR(jointsR.data(), constantR,
                  reinterpret_cast<const char*>(animData) +
                      animData->constROffset,
                  stackR);
  const auto* constantScale = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(animData) + animData->constSOffset);
  const auto* constantTranslation = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(animData) + animData->constTOffset);
  for (int index = 0; index < constantS; ++index) {
    float* destination = stackS + jointsS[index] * 4;
    std::memcpy(destination, constantScale + index * 3, 3 * sizeof(float));
    destination[3] = 0.0f;
  }
  for (int index = 0; index < constantT; ++index) {
    float* destination = stackT + jointsT[index] * 4;
    std::memcpy(destination, constantTranslation + index * 3,
                3 * sizeof(float));
    destination[3] = 0.0f;
  }
  if (constantU != 0) {
    const auto* constantChannels = reinterpret_cast<const float*>(
        reinterpret_cast<const std::uint8_t*>(&animData->numFrames) +
        animData->constUOffset);
    DecodeConstantU(channelsU.data(), constantU, constantChannels, stackU);
  }

  const int animatedR = DecodeRLE(bytes + mapOffsets[mapBase + 4],
                                  numPaddedJoints, jointsR.data());
  const int animatedS = DecodeRLE(bytes + mapOffsets[mapBase + 5],
                                  numPaddedJoints, jointsS.data());
  const int animatedT = DecodeRLE(bytes + mapOffsets[mapBase + 6],
                                  numPaddedJoints, jointsT.data());
  const int animatedU = stackU == nullptr
                            ? 0
                            : DecodeRLE(bytes + mapOffsets[mapBase + 7],
                                        paddedChannels, channelsU.data());
  const auto* frameBytes = reinterpret_cast<const std::uint8_t*>(frameSetData);
  DecodeFrameSetR(jointsR.data(), animatedR, frame, frameSetData->frameRange,
                  frameFraction,
                  reinterpret_cast<const std::int16_t*>(
                      frameBytes + frameSetData->firstROffset),
                  reinterpret_cast<const std::int16_t*>(
                      frameBytes + frameSetData->rangeROffset),
                  reinterpret_cast<const std::int16_t*>(
                      frameBytes + frameSetData->nextROffset),
                  frameBytes + frameSetData->RBitsOffset, stackR);
  DecodeFrameSetST(jointsS.data(), animatedS, frame,
                   frameSetData->frameRange, frameFraction,
                   reinterpret_cast<const float*>(
                       frameBytes + frameSetData->firstSOffset),
                   reinterpret_cast<const float*>(
                       frameBytes + frameSetData->rangeSOffset),
                   reinterpret_cast<const float*>(
                       frameBytes + frameSetData->nextSOffset),
                   frameBytes + frameSetData->SBitsOffset, stackS);
  DecodeFrameSetST(jointsT.data(), animatedT, frame,
                   frameSetData->frameRange, frameFraction,
                   reinterpret_cast<const float*>(
                       frameBytes + frameSetData->firstTOffset),
                   reinterpret_cast<const float*>(
                       frameBytes + frameSetData->rangeTOffset),
                   reinterpret_cast<const float*>(
                       frameBytes + frameSetData->nextTOffset),
                   frameBytes + frameSetData->TBitsOffset, stackT);
  if (stackU != nullptr)
    DecodeFrameSetU(channelsU.data(), animatedU, frame,
                    frameSetData->frameRange, frameFraction,
                    reinterpret_cast<const float*>(
                        frameBytes + frameSetData->firstUOffset),
                    reinterpret_cast<const float*>(
                        frameBytes + frameSetData->rangeUOffset),
                    reinterpret_cast<const float*>(
                        frameBytes + frameSetData->nextUOffset),
                    frameBytes + frameSetData->UBitsOffset, stackU);

  for (int joint = skeleton->numJoints; joint < numPaddedJoints; ++joint)
    SetIdentityRST(stackR + joint * 4, stackS + joint * 4,
                   stackT + joint * 4);
  if ((animData->flags & 0x100u) != 0) {
    RetargetAdditiveRST(skeleton, stackR, stackS, stackT, numPaddedJoints);
    if (stackU != nullptr)
      RetargetAdditiveU(skeleton, stackU, paddedChannels);
  }
}

void DecodeAnimFrameU(const idMD6SkelData* skeleton,
                      const idMD6AnimData* animData,
                      const frameSetData_t* frameSetData,
                      const md6FrameInfo_t* frameInfo, float* stackU) {
  const int paddedChannels = (skeleton->numUserChannels + 7) & ~7;
  const int paddedJoints = (skeleton->numJoints + 7) & ~7;
  if ((animData->flags & 0x101u) != 0) {
    std::fill(stackU, stackU + paddedChannels, 0.0f);
  } else {
    const auto* baseU = reinterpret_cast<const float*>(
        reinterpret_cast<const std::uint8_t*>(skeleton) +
        skeleton->basePoseOffset + paddedJoints * 40);
    DecodeBasePoseU(paddedChannels, baseU, stackU);
  }
  int mapIndex = GetAnimMapIndex(animData, skeleton->parentTblCrc);
  if (mapIndex < 0)
    mapIndex = 0;
  const auto* offsets = reinterpret_cast<const std::uint16_t*>(animData + 1);
  const int mapBase = animData->numAnimMaps + mapIndex * 8;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(animData);
  std::array<std::uint8_t, 272> channels{};
  int count = DecodeRLE(bytes + offsets[mapBase + 3], paddedChannels,
                        channels.data());
  DecodeConstantU(channels.data(), count,
                  reinterpret_cast<const float*>(
                      reinterpret_cast<const std::uint8_t*>(
                          &animData->numFrames) +
                      animData->constUOffset),
                  stackU);
  count = DecodeRLE(bytes + offsets[mapBase + 7], paddedChannels,
                    channels.data());
  const auto* frameBytes = reinterpret_cast<const std::uint8_t*>(frameSetData);
  DecodeFrameSetU(channels.data(), count, frameInfo->frameSetFrame,
                  frameSetData->frameRange,
                  BitCopy<std::uint32_t>(frameInfo->frameFraction),
                  reinterpret_cast<const float*>(
                      frameBytes + frameSetData->firstUOffset),
                  reinterpret_cast<const float*>(
                      frameBytes + frameSetData->rangeUOffset),
                  reinterpret_cast<const float*>(
                      frameBytes + frameSetData->nextUOffset),
                  frameBytes + frameSetData->UBitsOffset, stackU);
  if ((animData->flags & 0x100u) != 0)
    RetargetAdditiveU(skeleton, stackU, paddedChannels);
}

void DecodeUserChannelsForAnimFrame(const idMD6SkelData* skeleton,
                                    const idMD6AnimData* animData,
                                    float frame, float* outUserChannels) {
  float fraction;
  const int frameNumber = ClampFrame(animData, frame, fraction);
  const frameSetData_t* frameSet = FrameSetForFrame(animData, frameNumber);
  md6FrameInfo_t info{};
  info.animData = animData;
  info.frameSetData = frameSet;
  info.frame = static_cast<std::int16_t>(frameNumber);
  info.frameFraction = fraction;
  info.frameSetFrame =
      static_cast<std::int16_t>(frameNumber - frameSet->frameStart);
  DecodeAnimFrameU(skeleton, animData, frameSet, &info, outUserChannels);
}

namespace local {

void SubD(const float* a, const float* b, float* r) {
  float inverse[4];
  QuatConjugate(b, inverse);
  QuatMultiply(a, inverse, r);
  NormalizeQuat(r);
  r[4] = a[4] / b[4];
  r[5] = a[5] / b[5];
  r[6] = a[6] / b[6];
  r[7] = 1.0f;
  r[8] = a[8] - b[8];
  r[9] = a[9] - b[9];
  r[10] = a[10] - b[10];
  r[11] = 0.0f;
}

void AddD(const float* a, const float* b, float* r) {
  QuatMultiply(a, b, r);
  NormalizeQuat(r);
  r[4] = a[4] * b[4];
  r[5] = a[5] * b[5];
  r[6] = a[6] * b[6];
  r[7] = 1.0f;
  r[8] = a[8] + b[8];
  r[9] = a[9] + b[9];
  r[10] = a[10] + b[10];
  r[11] = 0.0f;
}

bool SolveTwoBones(const idVec3* startPos, idVec3* endPos,
                   const idVec3* direction, float length0, float length1,
                   idVec3* jointPos) {
  idVec3 delta = *endPos - *startPos;
  float distance = delta.Length();
  if (distance <= idMath::FLT_SMALLEST_NON_DENORMAL)
    return false;
  const float maximum = length0 + length1;
  if (distance > maximum) {
    delta *= maximum / distance;
    *endPos = *startPos + delta;
    distance = maximum;
  }
  const idVec3 forward = delta / distance;
  idVec3 bend = *direction - forward * (*direction * forward);
  if (bend.Normalize() <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    idVec3 unused;
    forward.NormalVectors(bend, unused);
  }
  const float along = (length0 * length0 - length1 * length1 +
                       distance * distance) /
                      (2.0f * distance);
  const float height = std::sqrt((std::max)(
      0.0f, length0 * length0 - along * along));
  *jointPos = *startPos + forward * along + bend * height;
  return true;
}

void EvaluateArm(float* finalPose, const float* refPose,
                 const jointMod_t* target, idVec3* slaveOffset) {
  (void)refPose;
  if (finalPose == nullptr || target == nullptr)
    return;
  slaveOffset->x = target->mat[3] - finalPose[3];
  slaveOffset->y = target->mat[7] - finalPose[7];
  slaveOffset->z = target->mat[11] - finalPose[11];
  std::memcpy(finalPose, target->mat, sizeof(target->mat));
}

void HandsIK(float* finalPose, const jointMod_t* jointMod,
             const jointMod_t* targetRight, const jointMod_t* targetLeft,
             const float* refPose) {
  if (finalPose == nullptr || jointMod == nullptr || targetRight == nullptr ||
      targetLeft == nullptr)
    return;
  idVec3 offset{};
  EvaluateArm(finalPose, refPose, targetRight, &offset);
  EvaluateArm(finalPose + 12, refPose, targetLeft, &offset);
}

void QuatMult(const float* a, const float* b, float* out) {
  QuatMultiply(a, b, out);
  NormalizeQuat(out);
}

void RotateVector(const float* quat, const float* vector, float* out) {
  QuatRotate(quat, vector, out);
}

void QuatFromMat(float* result, float* matrix) { RowsToQuat(matrix, result); }

} // namespace local

void ApplyAnimMods(const idMD6SkelData* skeleton, const jointMod_t* animMods,
                   std::uint32_t numAnimMods, float* R, float* S, float* T,
                   float* U) {
  if (animMods == nullptr)
    return;
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(skeleton) +
      skeleton->parentTblOffset);
  for (std::uint32_t index = 0; index < numAnimMods; ++index) {
    const jointMod_t& mod = animMods[index];
    const int joint = mod.joint.value;
    if (joint < 0 || joint >= skeleton->numJoints || (mod.flags & 0x40u) != 0)
      continue;
    float modR[4];
    RowsToQuat(mod.mat, modR);
    const float modS[4] = {mod.s[0], mod.s[1], mod.s[2], 1.0f};
    const float modT[4] = {mod.mat[3], mod.mat[7], mod.mat[11], 0.0f};
    float* destinationR = R + joint * 4;
    float* destinationS = S + joint * 4;
    float* destinationT = T + joint * 4;
    const bool modelSpace = (mod.flags & 1u) != 0;
    const bool overrideValue = (mod.flags & 0x20u) != 0;
    if (modelSpace) {
      float parentR[4], parentS[4], parentT[4];
      TransformLocalJointToModelJoint(parents, parents[joint], R, S, T,
                                      parentR, parentS, parentT);
      if ((mod.flags & 2u) != 0) {
        float parentInverse[4];
        QuatConjugate(parentR, parentInverse);
        QuatMultiply(modR, parentInverse, destinationR);
        NormalizeQuat(destinationR);
      }
      if ((mod.flags & 4u) != 0)
        for (int component = 0; component < 3; ++component)
          destinationS[component] =
              modS[component] / (parentS[component] == 0.0f
                                     ? 1.0f
                                     : parentS[component]);
      if ((mod.flags & 8u) != 0) {
        float delta[4] = {modT[0] - parentT[0], modT[1] - parentT[1],
                          modT[2] - parentT[2], 0.0f};
        float inverse[4], localT[4];
        QuatConjugate(parentR, inverse);
        QuatRotate(inverse, delta, localT);
        for (int component = 0; component < 3; ++component)
          destinationT[component] =
              localT[component] / (parentS[component] == 0.0f
                                       ? 1.0f
                                       : parentS[component]);
      }
    } else if (overrideValue) {
      if ((mod.flags & 2u) != 0)
        Copy4(modR, destinationR);
      if ((mod.flags & 4u) != 0)
        Copy4(modS, destinationS);
      if ((mod.flags & 8u) != 0)
        Copy4(modT, destinationT);
    } else {
      if ((mod.flags & 2u) != 0) {
        float product[4];
        QuatMultiply(destinationR, modR, product);
        NormalizeQuat(product);
        Copy4(product, destinationR);
      }
      if ((mod.flags & 4u) != 0)
        for (int component = 0; component < 3; ++component)
          destinationS[component] *= modS[component];
      if ((mod.flags & 8u) != 0)
        for (int component = 0; component < 3; ++component)
          destinationT[component] += modT[component];
    }
    if (U != nullptr && (mod.flags & 0x10u) != 0) {
      for (int component = 0; component < 3; ++component) {
        const int channel = joint + component;
        if (channel >= skeleton->numUserChannels)
          break;
        if (overrideValue)
          U[channel] += (mod.s[component] - U[channel]) * mod.mat[component];
        else
          U[channel] += mod.s[component];
      }
    }
  }
}

void CalculatePoseBounds(const idMD6SkelData* skeleton, __m128* finalPose,
                         idJointIndex_t boundsSkipJoint,
                         md6BlendOutputs_t* blendOutputs) {
  float minimum[3] = {std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max()};
  float maximum[3] = {-std::numeric_limits<float>::max(),
                      -std::numeric_limits<float>::max(),
                      -std::numeric_limits<float>::max()};
  const float* matrices = reinterpret_cast<const float*>(finalPose);
  for (int joint = 0; joint < skeleton->numJoints; ++joint) {
    if (joint == boundsSkipJoint.value)
      continue;
    for (int component = 0; component < 3; ++component) {
      const float value = matrices[joint * 12 + component * 4 + 3];
      minimum[component] = (std::min)(minimum[component], value);
      maximum[component] = (std::max)(maximum[component], value);
    }
  }
  for (int component = 0; component < 3; ++component) {
    blendOutputs->jointBounds[component] = minimum[component];
    blendOutputs->jointBounds[component + 3] = maximum[component];
  }
}

void TransformModelMatrices(const idMD6SkelData* skeleton,
                            const std::uint8_t* remap,
                            const float* inMatrices1,
                            const float* inMatrices2,
                            const float* invertedRefPoseQuat,
                            float* outMatrices, bool useDualQuat,
                            float* quatTemp) {
  const int padded = (skeleton->numJoints + 7) & ~7;
  for (int joint = 0; joint < padded; ++joint) {
    const int destination = remap == nullptr ? joint : remap[joint];
    if (useDualQuat) {
      float rotation[4];
      RowsToQuat(inMatrices1 + joint * 12, rotation);
      float transformed[4];
      QuatMultiply(rotation, invertedRefPoseQuat + joint * 4, transformed);
      NormalizeQuat(transformed);
      float* output = outMatrices + destination * 8;
      Copy4(transformed, output);
      const float translation[4] = {inMatrices1[joint * 12 + 3],
                                    inMatrices1[joint * 12 + 7],
                                    inMatrices1[joint * 12 + 11], 0.0f};
      float dual[4];
      QuatMultiply(translation, transformed, dual);
      for (int component = 0; component < 4; ++component)
        output[4 + component] = dual[component] * 0.5f;
      if (quatTemp != nullptr) {
        Copy4(transformed, quatTemp + joint * 8);
        Copy4(translation, quatTemp + joint * 8 + 4);
      }
    } else {
      ComposeRows(inMatrices1 + joint * 12, inMatrices2 + joint * 12,
                  outMatrices + destination * 12);
    }
  }
}

void TransformLocalJointsToModelMatrices(
    const idMD6SkelData* skeleton, float* temp, std::uint32_t tempSize,
    const jointMod_t* mods, std::uint32_t numMods, std::uint32_t flags,
    const float* originOffset, bool addOffset, const float* inR,
    __m128i* inS, const float* inT, __m128* outMatrices, float* quatTemp,
    const float* refPose) {
  (void)temp;
  (void)tempSize;
  (void)refPose;
  const int padded = (skeleton->numJoints + 7) & ~7;
  std::vector<float> rotations(inR, inR + padded * 4);
  const float* scaleInput = reinterpret_cast<const float*>(inS);
  std::vector<float> scales(scaleInput, scaleInput + padded * 4);
  std::vector<float> translations(inT, inT + padded * 4);
  if (mods != nullptr && numMods != 0) {
    std::vector<jointMod_t> selected;
    selected.reserve(numMods);
    for (std::uint32_t index = 0; index < numMods; ++index)
      if ((mods[index].flags & flags) != 0)
        selected.push_back(mods[index]);
    ApplyAnimMods(skeleton, selected.data(),
                  static_cast<std::uint32_t>(selected.size()),
                  rotations.data(), scales.data(), translations.data(),
                  nullptr);
    for (std::size_t index = 0; index < selected.size(); ++index) {
      if ((selected[index].flags & 0x40u) != 0 && index + 4 < selected.size())
        local::HandsIK(reinterpret_cast<float*>(outMatrices), &selected[index],
                       &selected[index + 1], &selected[index + 4], refPose);
    }
  }
  if (originOffset != nullptr) {
    if (addOffset) {
      translations[0] += originOffset[0];
      translations[1] += originOffset[1];
      translations[2] += originOffset[2];
    } else {
      translations[0] = originOffset[0];
      translations[1] = originOffset[1];
      translations[2] = originOffset[2];
    }
  }
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(skeleton) +
      skeleton->parentTblOffset);
  float* matrices = reinterpret_cast<float*>(outMatrices);
  for (int joint = 0; joint < padded; ++joint) {
    float localRows[12];
    QuatToRows(rotations.data() + joint * 4, scales.data() + joint * 4,
               translations.data() + joint * 4, localRows);
    const int parent = joint < skeleton->numJoints ? parents[joint] : -1;
    if (parent >= 0)
      ComposeRows(localRows, matrices + parent * 12, matrices + joint * 12);
    else
      std::memcpy(matrices + joint * 12, localRows, sizeof(localRows));
    if (quatTemp != nullptr) {
      RowsToQuat(matrices + joint * 12, quatTemp + joint * 8);
      quatTemp[joint * 8 + 4] = matrices[joint * 12 + 3];
      quatTemp[joint * 8 + 5] = matrices[joint * 12 + 7];
      quatTemp[joint * 8 + 6] = matrices[joint * 12 + 11];
      quatTemp[joint * 8 + 7] = 0.0f;
    }
  }
}

void DecodeBasePose(const idMD6SkelData* skeleton, float* temp,
                    std::uint32_t tempSize, const float* originOffset,
                    bool offsetAdd, __m128* outPose) {
  const int padded = (skeleton->numJoints + 7) & ~7;
  const int poseCount = padded + 1;
  const std::size_t needed =
      static_cast<std::size_t>(poseCount * 12) * sizeof(float);
  if (temp == nullptr || tempSize < needed)
    return;
  float* outR = temp;
  float* outS = outR + poseCount * 4;
  float* outT = outS + poseCount * 4;
  DecodeSkeletonFrame(skeleton, outR, outS, outT, nullptr);
  SetIdentityRST(outR + padded * 4, outS + padded * 4,
                 outT + padded * 4);
  TransformLocalJointsToModelMatrices(skeleton, nullptr, 0, nullptr, 0, 0,
                                      originOffset, offsetAdd, outR,
                                      reinterpret_cast<__m128i*>(outS), outT,
                                      outPose, nullptr, nullptr);
}

void DecodeSingleFrame(const idMD6SkelData* skeleton, char* temp,
                       std::uint32_t tempSize,
                       const idMD6AnimData* animData, int timeMSec,
                       int ticksPerSec, bool clearOriginTransform,
                       const float* originOffset, bool offsetAdd,
                       __m128* outPose, float* outUserChannels,
                       const jointMod_t* jointMods, int numJointMods) {
  float fraction;
  const float frameValue = ticksPerSec != 0
                               ? static_cast<float>(timeMSec) /
                                     static_cast<float>(ticksPerSec) *
                                     animData->frameRate
                               : 0.0f;
  const int frame = ClampFrame(animData, frameValue, fraction);
  const frameSetData_t* frameSet = FrameSetForFrame(animData, frame);
  const int padded = (skeleton->numJoints + 7) & ~7;
  const int poseCount = padded + 1;
  float* R = reinterpret_cast<float*>(temp);
  float* S = R + poseCount * 4;
  float* T = S + poseCount * 4;
  float* U = T + poseCount * 4;
  DecodeAnimFrame(skeleton, padded, animData, frameSet,
                  frame - frameSet->frameStart,
                  BitCopy<std::uint32_t>(fraction), R, S, T, U);
  SetIdentityRST(R + padded * 4, S + padded * 4, T + padded * 4);
  if (clearOriginTransform) {
    if ((animData->flags & 2u) != 0)
      T[0] = 0.0f;
    if ((animData->flags & 4u) != 0)
      T[1] = 0.0f;
    if ((animData->flags & 8u) != 0)
      T[2] = 0.0f;
    if ((animData->flags & 0x10u) != 0) {
      R[0] = R[1] = R[2] = 0.0f;
      R[3] = 1.0f;
    }
  }
  float* modTemp = jointMods != nullptr && numJointMods > 0
                       ? reinterpret_cast<float*>(temp + tempSize / 2)
                       : nullptr;
  TransformLocalJointsToModelMatrices(
      skeleton, modTemp, modTemp == nullptr ? 0u : tempSize / 2, jointMods,
      static_cast<std::uint32_t>((std::max)(numJointMods, 0)), 0x8000u,
      originOffset, offsetAdd, R, reinterpret_cast<__m128i*>(S), T, outPose,
      nullptr, nullptr);
  if (outUserChannels != nullptr)
    std::memmove(outUserChannels, U,
                 skeleton->numUserChannels * sizeof(float));
}

void DecodeSingleFrameWithOriginDeltaScale(
    const idMD6SkelData* skeleton, char* temp, std::uint32_t tempSize,
    const idMD6AnimData* animData, int timeMSec, int ticksPerSec,
    bool clearOriginTransform, const float* originOffset, bool offsetAdd,
    __m128* outPose, float* outUserChannels,
    const float* originDeltaScale) {
  float fraction;
  const float frameValue = ticksPerSec != 0
                               ? static_cast<float>(timeMSec) /
                                     static_cast<float>(ticksPerSec) *
                                     animData->frameRate
                               : 0.0f;
  const int frame = ClampFrame(animData, frameValue, fraction);
  const frameSetData_t* frameSet = FrameSetForFrame(animData, frame);
  const int padded = (skeleton->numJoints + 7) & ~7;
  const int poseCount = padded + 1;
  float* R = reinterpret_cast<float*>(temp);
  float* S = R + poseCount * 4;
  float* T = S + poseCount * 4;
  float* U = T + poseCount * 4;
  DecodeAnimFrame(skeleton, padded, animData, frameSet,
                  frame - frameSet->frameStart,
                  BitCopy<std::uint32_t>(fraction), R, S, T, U);
  SetIdentityRST(R + padded * 4, S + padded * 4, T + padded * 4);
  if (clearOriginTransform) {
    if ((animData->flags & 2u) != 0)
      T[0] = 0.0f;
    if ((animData->flags & 4u) != 0)
      T[1] = 0.0f;
    if ((animData->flags & 8u) != 0)
      T[2] = 0.0f;
    if ((animData->flags & 0x10u) != 0) {
      R[0] = R[1] = R[2] = 0.0f;
      R[3] = 1.0f;
    }
  }
  if (originDeltaScale != nullptr)
    for (int component = 0; component < 3; ++component)
      T[component] *= originDeltaScale[component];
  TransformLocalJointsToModelMatrices(skeleton, nullptr, 0, nullptr, 0, 0,
                                      originOffset, offsetAdd, R,
                                      reinterpret_cast<__m128i*>(S), T,
                                      outPose, nullptr, nullptr);
  if (outUserChannels != nullptr)
    std::memmove(outUserChannels, U,
                 skeleton->numUserChannels * sizeof(float));
}

namespace {

struct BlendFrame {
  std::vector<float> r;
  std::vector<float> s;
  std::vector<float> t;
  std::vector<float> u;
  std::vector<std::uint8_t> jointWeights;
  std::vector<std::uint8_t> userWeights;
};

BlendFrame MakeFrame(int poseCount, int userCount) {
  BlendFrame frame;
  frame.r.resize(poseCount * 4);
  frame.s.resize(poseCount * 4);
  frame.t.resize(poseCount * 4);
  frame.u.resize(userCount);
  frame.jointWeights.resize(poseCount);
  frame.userWeights.resize(userCount);
  return frame;
}

void SetFrameWeights(const idMD6SkelData* skeleton,
                     const md6FrameInfo_t& info, BlendFrame& frame) {
  const int weightGroup = (std::min)(static_cast<int>(info.weightGroup), 7);
  const std::uint8_t* joints =
      WeightTable(skeleton, skeleton->jointWeightOffsets[weightGroup]);
  const std::uint8_t* users =
      WeightTable(skeleton, skeleton->userWeightOffsets[weightGroup]);
  for (std::size_t index = 0; index < frame.jointWeights.size(); ++index)
    frame.jointWeights[index] =
        index < skeleton->numJoints && joints != nullptr ? joints[index] : 255;
  for (std::size_t index = 0; index < frame.userWeights.size(); ++index)
    frame.userWeights[index] =
        index < skeleton->numUserChannels && users != nullptr ? users[index]
                                                               : 255;
}

void DecodeCommandFrame(const idMD6SkelData* skeleton,
                        const md6FrameInfo_t& info,
                        bool clearOriginTransform, BlendFrame& frame) {
  const int padded = (skeleton->numJoints + 7) & ~7;
  DecodeAnimFrame(skeleton, static_cast<std::uint16_t>(padded), info.animData,
                  info.frameSetData, info.frameSetFrame,
                  BitCopy<std::uint32_t>(info.frameFraction), frame.r.data(),
                  frame.s.data(), frame.t.data(), frame.u.data());
  SetIdentityRST(frame.r.data() + padded * 4,
                 frame.s.data() + padded * 4,
                 frame.t.data() + padded * 4);
  SetFrameWeights(skeleton, info, frame);
  if (clearOriginTransform) {
    if ((info.animData->flags & 2u) != 0)
      frame.t[0] = 0.0f;
    if ((info.animData->flags & 4u) != 0)
      frame.t[1] = 0.0f;
    if ((info.animData->flags & 8u) != 0)
      frame.t[2] = 0.0f;
    if ((info.animData->flags & 0x10u) != 0) {
      frame.r[0] = frame.r[1] = frame.r[2] = 0.0f;
      frame.r[3] = 1.0f;
    }
  }
  if (info.animMods != nullptr && info.animModsSize != 0)
    ApplyAnimMods(skeleton, info.animMods, info.animModsSize / 64,
                  frame.r.data(), frame.s.data(), frame.t.data(),
                  frame.u.data());
}

void BlendCommandFrames(const idMD6SkelData* skeleton,
                        const md6BlendInfo_t& info, const BlendFrame& left,
                        const BlendFrame& right, BlendFrame& output) {
  const int poseCount = static_cast<int>(output.jointWeights.size());
  const int userCount = static_cast<int>(output.userWeights.size());
  const int group = (std::min)(static_cast<int>(info.filterGroup), 7);
  const std::uint8_t* jointFilter =
      WeightTable(skeleton, skeleton->jointWeightOffsets[group]);
  const std::uint8_t* userFilter =
      WeightTable(skeleton, skeleton->userWeightOffsets[group]);
  const float originAlpha[3] = {info.currentAlpha, 0.0f, 1.0f};
  const float originAmount = info.originBlend < 3
                                 ? originAlpha[info.originBlend]
                                 : info.currentAlpha;
  switch (info.op) {
  case BOP_LERP:
  case BOP_REF_LERP:
  case BOP_BLEND:
  case BOP_FUSION:
    LerpRST(poseCount, info.currentAlpha, jointFilter, left.r.data(),
            left.s.data(), left.t.data(), left.jointWeights.data(),
            right.r.data(), right.s.data(), right.t.data(),
            reinterpret_cast<std::uint32_t*>(
                const_cast<std::uint8_t*>(right.jointWeights.data())),
            output.r.data(), output.s.data(), output.t.data(),
            output.jointWeights.data());
    LerpU(userCount, info.currentAlpha, userFilter, left.u.data(),
          left.userWeights.data(), right.u.data(), right.userWeights.data(),
          output.u.data(), output.userWeights.data());
    break;
  case BOP_ADD_LEFT:
    AddRST(poseCount, info.currentAlpha, jointFilter, right.r.data(),
           right.s.data(), right.t.data(), right.jointWeights.data(),
           left.r.data(), left.s.data(), left.t.data(),
           left.jointWeights.data(), output.r.data(), output.s.data(),
           output.t.data(), output.jointWeights.data());
    AddSubU(userCount, info.currentAlpha, userFilter, right.u.data(),
            right.userWeights.data(), left.u.data(), left.userWeights.data(),
            output.u.data(), output.userWeights.data());
    break;
  case BOP_ADD_RIGHT:
  case BOP_BLENDA:
    AddRST(poseCount, info.currentAlpha, jointFilter, left.r.data(),
           left.s.data(), left.t.data(), left.jointWeights.data(),
           right.r.data(), right.s.data(), right.t.data(),
           right.jointWeights.data(), output.r.data(), output.s.data(),
           output.t.data(), output.jointWeights.data());
    AddSubU(userCount, info.currentAlpha, userFilter, left.u.data(),
            left.userWeights.data(), right.u.data(),
            right.userWeights.data(), output.u.data(),
            output.userWeights.data());
    break;
  case BOP_SUB_LEFT:
    SubRST(poseCount, info.currentAlpha, jointFilter, right.r.data(),
           right.s.data(), right.t.data(), right.jointWeights.data(),
           left.r.data(), left.s.data(), left.t.data(),
           left.jointWeights.data(), output.r.data(), output.s.data(),
           output.t.data(), output.jointWeights.data());
    AddSubU(userCount, -info.currentAlpha, userFilter, right.u.data(),
            right.userWeights.data(), left.u.data(), left.userWeights.data(),
            output.u.data(), output.userWeights.data());
    break;
  case BOP_SUB_RIGHT:
    SubRST(poseCount, info.currentAlpha, jointFilter, left.r.data(),
           left.s.data(), left.t.data(), left.jointWeights.data(),
           right.r.data(), right.s.data(), right.t.data(),
           right.jointWeights.data(), output.r.data(), output.s.data(),
           output.t.data(), output.jointWeights.data());
    AddSubU(userCount, -info.currentAlpha, userFilter, left.u.data(),
            left.userWeights.data(), right.u.data(),
            right.userWeights.data(), output.u.data(),
            output.userWeights.data());
    break;
  case BOP_CLEARADDU: {
    const int padded = (skeleton->numJoints + 7) & ~7;
    const auto* baseU = reinterpret_cast<const float*>(
        reinterpret_cast<const std::uint8_t*>(skeleton) +
        skeleton->basePoseOffset + padded * 40);
    LerpU(userCount, 1.0f, userFilter, left.u.data(),
          left.userWeights.data(), baseU, right.userWeights.data(),
          output.u.data(), output.userWeights.data());
    AddSubU(userCount, info.currentAlpha, userFilter, output.u.data(),
            output.userWeights.data(), right.u.data(),
            right.userWeights.data(), output.u.data(),
            output.userWeights.data());
    output.r = left.r;
    output.s = left.s;
    output.t = left.t;
    output.jointWeights = left.jointWeights;
    break;
  }
  default:
    output = left;
    break;
  }
  // The two synthetic origin transforms use the branch's origin policy.
  if (poseCount > 0 && originAmount != info.currentAlpha) {
    const int origin = poseCount - 1;
    float blended[4];
    QuatNlerp(left.r.data() + origin * 4,
              right.r.data() + origin * 4, originAmount, blended);
    Copy4(blended, output.r.data() + origin * 4);
    for (int component = 0; component < 4; ++component) {
      output.s[origin * 4 + component] =
          left.s[origin * 4 + component] +
          (right.s[origin * 4 + component] -
           left.s[origin * 4 + component]) *
              originAmount;
      output.t[origin * 4 + component] =
          left.t[origin * 4 + component] +
          (right.t[origin * 4 + component] -
           left.t[origin * 4 + component]) *
              originAmount;
    }
  }
}

void EvaluateBlendShapes(const idMD6SkelData* skeleton, const float* channels,
                         blendShapeEval_t* evaluations,
                         std::uint32_t count) {
  if (evaluations == nullptr || count == 0)
    return;
  std::vector<std::array<float, MAX_BLENDSHAPES>> weights(count);
  float total = 0.0f;
  for (std::uint32_t evalIndex = 0; evalIndex < count; ++evalIndex) {
    for (int shape = 0; shape < evaluations[evalIndex].numBlendShapes;
         ++shape) {
      const idUserChannelExpression* expression =
          evaluations[evalIndex].expressions[shape];
      float value = expression == nullptr
                        ? 0.0f
                        : const_cast<idUserChannelExpression*>(expression)
                              ->Eval(channels, skeleton->numUserChannels);
      value = (std::clamp)(value, 0.0f, 1.0f);
      weights[evalIndex][shape] = value;
      if (value > 0.02f)
        total += value;
    }
  }
  const float scale = total <= idMath::FLT_SMALLEST_NON_DENORMAL
                          ? 0.0f
                          : (std::min)(1.0f, 1.0f / total);
  for (std::uint32_t evalIndex = 0; evalIndex < count; ++evalIndex) {
    blendShapeEval_t& evaluation = evaluations[evalIndex];
    bool first = true;
    for (int shape = 0; shape < evaluation.numBlendShapes; ++shape) {
      const float value = weights[evalIndex][shape] * scale;
      if (value <= 0.02f)
        continue;
      if (first) {
        BlendShapeFirst(reinterpret_cast<__m128i*>(evaluation.destData),
                        reinterpret_cast<const __m128i*>(
                            evaluation.sourceDatas[shape]),
                        evaluation.numVerts, value);
        first = false;
      } else {
        BlendShapeAdd(reinterpret_cast<__m128i*>(evaluation.destData),
                      reinterpret_cast<const __m128i*>(evaluation.destData),
                      reinterpret_cast<const __m128i*>(
                          evaluation.sourceDatas[shape]),
                      evaluation.numVerts, BitCopy<std::uint32_t>(value));
      }
    }
    if (first && evaluation.destData != nullptr)
      std::memset(evaluation.destData, 128,
                  static_cast<std::size_t>(evaluation.numVerts) * 4);
  }
}

} // namespace

void ExecuteCommandsInternal(
    const idMD6SkelData* skeleton, char* temp, doubleStack_t* stack,
    const md6AnimCommand_t* cmds, std::uint32_t numCmds,
    bool clearOriginTransform, idJointIndex_t boundsSkipJoint,
    const float* originOffset, bool addOffset, const jointMod_t* jointMods,
    std::uint32_t numJointMods, const std::uint8_t* jointRemap,
    bool* generatedReferencePose, float* refPose,
    md6BlendOutputs_t* blendOutputs, float* userChannels,
    blendShapeEval_t* blendShapeEvals, std::uint32_t numBlendShapeEvals) {
  (void)boundsSkipJoint;
  (void)originOffset;
  (void)addOffset;
  (void)jointMods;
  (void)numJointMods;
  (void)jointRemap;
  (void)generatedReferencePose;
  (void)refPose;
  const int padded = (skeleton->numJoints + 7) & ~7;
  const int poseCount = padded + 1;
  const int paddedUsers = (skeleton->numUserChannels + 7) & ~7;
  std::vector<BlendFrame> frames;
  frames.reserve(16);
  for (std::uint32_t commandIndex = 0;
       commandIndex < numCmds && cmds[commandIndex].op != md6AnimCommand_t::OP_END;
       ++commandIndex) {
    const md6AnimCommand_t& command = cmds[commandIndex];
    if (command.op == md6AnimCommand_t::OP_DECODE_AND_PUSH ||
        command.op == md6AnimCommand_t::OP_DECODE_FACECOMPOSITE_AND_BLEND) {
      BlendFrame frame = MakeFrame(poseCount, paddedUsers);
      DecodeCommandFrame(skeleton, command.frameInfo, clearOriginTransform,
                         frame);
      if (command.op == md6AnimCommand_t::OP_DECODE_FACECOMPOSITE_AND_BLEND &&
          !frames.empty()) {
        BlendFrame output = MakeFrame(poseCount, paddedUsers);
        md6BlendInfo_t faceBlend{};
        faceBlend.op = BOP_LERP;
        faceBlend.currentAlpha = 1.0f;
        BlendCommandFrames(skeleton, faceBlend, frames.back(), frame, output);
        frames.back() = std::move(output);
      } else {
        frames.push_back(std::move(frame));
      }
    } else if (command.op == md6AnimCommand_t::OP_POP_AND_BLEND &&
               frames.size() >= 2) {
      BlendFrame right = std::move(frames.back());
      frames.pop_back();
      BlendFrame left = std::move(frames.back());
      frames.pop_back();
      BlendFrame output = MakeFrame(poseCount, paddedUsers);
      BlendCommandFrames(skeleton, command.blendInfo, left, right, output);
      frames.push_back(std::move(output));
    }
  }
  if (frames.empty()) {
    BlendFrame base = MakeFrame(poseCount, paddedUsers);
    DecodeSkeletonFrame(skeleton, base.r.data(), base.s.data(), base.t.data(),
                        base.u.data());
    SetIdentityRST(base.r.data() + padded * 4,
                   base.s.data() + padded * 4,
                   base.t.data() + padded * 4);
    std::fill(base.jointWeights.begin(), base.jointWeights.end(), 255);
    std::fill(base.userWeights.begin(), base.userWeights.end(), 255);
    frames.push_back(std::move(base));
  }
  BlendFrame& result = frames.back();
  const std::size_t poseBytes = static_cast<std::size_t>(poseCount) * 16;
  const std::size_t required = poseBytes * 3 +
                               static_cast<std::size_t>(paddedUsers) * 4;
  if (stack->end >= stack->start &&
      static_cast<std::size_t>(stack->end - stack->start) >= required) {
    stack->bot = stack->start;
    std::memcpy(stack->bot, result.r.data(), poseBytes);
    std::memcpy(stack->bot + poseBytes, result.s.data(), poseBytes);
    std::memcpy(stack->bot + poseBytes * 2, result.t.data(), poseBytes);
    std::memcpy(stack->bot + poseBytes * 3, result.u.data(),
                static_cast<std::size_t>(paddedUsers) * sizeof(float));
    stack->top = stack->end;
  }
  if (blendOutputs != nullptr) {
    Copy4(result.r.data(), blendOutputs->animRotation);
    Copy4(result.s.data(), blendOutputs->animScale);
    Copy4(result.t.data(), blendOutputs->animTranslation);
    Copy4(result.r.data() + padded * 4, blendOutputs->deltaRotation);
    Copy4(result.s.data() + padded * 4, blendOutputs->deltaScale);
    Copy4(result.t.data() + padded * 4, blendOutputs->deltaTranslation);
    blendOutputs->done = true;
  }
  if (userChannels != nullptr)
    std::memmove(userChannels, result.u.data(),
                 skeleton->numUserChannels * sizeof(float));
  EvaluateBlendShapes(skeleton, result.u.data(), blendShapeEvals,
                      numBlendShapeEvals);
}

void ExecuteCommands(
    const idMD6SkelData* skeleton, char* temp, std::uint32_t tempSize,
    const md6AnimCommand_t* cmds, std::uint32_t numCmds,
    bool clearOriginTransform, idJointIndex_t boundsSkipJoint,
    bool useDualQuat, const float* originOffset, bool addOffset,
    const jointMod_t* jointMods, std::uint32_t numJointMods,
    const float* invertedBasePose, const float* invertedBasePoseQuat,
    const std::uint8_t* jointRemap, __m128* refPose, __m128* finalPose,
    float* renderPose, md6BlendOutputs_t* blendOutputs,
    float* userChannels, blendShapeEval_t* blendShapeEvals,
    std::uint32_t numBlendShapeEvals) {
  doubleStack_t stack{temp, temp + tempSize, temp + tempSize, temp};
  bool generatedReferencePose = false;
  ExecuteCommandsInternal(
      skeleton, temp, &stack, cmds, numCmds, clearOriginTransform,
      boundsSkipJoint, originOffset, addOffset, jointMods, numJointMods,
      jointRemap, &generatedReferencePose,
      refPose == nullptr ? nullptr : reinterpret_cast<float*>(refPose),
      blendOutputs, userChannels, blendShapeEvals, numBlendShapeEvals);
  const int padded = (skeleton->numJoints + 7) & ~7;
  const int poseCount = padded + 1;
  const std::size_t poseBytes = static_cast<std::size_t>(poseCount) * 16;
  const float* R = reinterpret_cast<const float*>(stack.bot);
  auto* S = reinterpret_cast<__m128i*>(stack.bot + poseBytes);
  const float* T = reinterpret_cast<const float*>(stack.bot + poseBytes * 2);
  float* work = reinterpret_cast<float*>(stack.top - MIN_BLEND_STACK_SIZE);
  if (!generatedReferencePose && refPose != nullptr)
    TransformLocalJointsToModelMatrices(
        skeleton, work, MIN_BLEND_STACK_SIZE, jointMods, numJointMods, 0x4000,
        originOffset, addOffset, R, S, T, refPose, nullptr, nullptr);
  TransformLocalJointsToModelMatrices(
      skeleton, work, MIN_BLEND_STACK_SIZE, jointMods, numJointMods, 0x8000,
      originOffset, addOffset, R, S, T, finalPose,
      reinterpret_cast<float*>(stack.bot + poseBytes * 3),
      refPose == nullptr ? nullptr : reinterpret_cast<float*>(refPose));
  CalculatePoseBounds(skeleton, finalPose, boundsSkipJoint, blendOutputs);
  TransformModelMatrices(skeleton, jointRemap,
                         reinterpret_cast<const float*>(finalPose),
                         invertedBasePose, invertedBasePoseQuat, renderPose,
                         useDualQuat,
                         reinterpret_cast<float*>(stack.bot + poseBytes * 3));
}

void ExecuteCommandsToRSTU(
    const idMD6SkelData* skeleton, char* temp, std::uint32_t tempSize,
    const md6AnimCommand_t* cmds, std::uint32_t numCmds,
    bool clearOriginTransform, idJointIndex_t boundsSkipJoint,
    const float* originOffset, bool addOffset, const jointMod_t* jointMods,
    std::uint32_t numJointMods, const std::uint8_t* jointRemap,
    __m128* finalPose, float* localR, float* localS, float* localT,
    md6BlendOutputs_t* blendOutputs, float* userChannels,
    blendShapeEval_t* blendShapeEvals, std::uint32_t numBlendShapeEvals) {
  doubleStack_t stack{temp, temp + tempSize, temp + tempSize, temp};
  bool generatedReferencePose = true;
  ExecuteCommandsInternal(
      skeleton, temp, &stack, cmds, numCmds, clearOriginTransform,
      boundsSkipJoint, originOffset, addOffset, jointMods, numJointMods,
      jointRemap, &generatedReferencePose, nullptr, blendOutputs, userChannels,
      blendShapeEvals, numBlendShapeEvals);
  const int padded = (skeleton->numJoints + 7) & ~7;
  const int poseCount = padded + 1;
  const std::size_t poseBytes = static_cast<std::size_t>(poseCount) * 16;
  const float* R = reinterpret_cast<const float*>(stack.bot);
  auto* S = reinterpret_cast<__m128i*>(stack.bot + poseBytes);
  const float* T = reinterpret_cast<const float*>(stack.bot + poseBytes * 2);
  TransformLocalJointsToModelMatrices(
      skeleton, reinterpret_cast<float*>(stack.top - MIN_BLEND_STACK_SIZE),
      MIN_BLEND_STACK_SIZE, jointMods, numJointMods, 0x8000, originOffset,
      addOffset, R, S, T, finalPose, nullptr, nullptr);
  CalculatePoseBounds(skeleton, finalPose, boundsSkipJoint, blendOutputs);
  std::memmove(localR, R, poseBytes);
  std::memmove(localS, reinterpret_cast<const float*>(S), poseBytes);
  std::memmove(localT, T, poseBytes);
}

// The retail compiland emitted one initializer for the expression fixture and
// one for each TU-local SIMD constant.  constexpr storage preserves all 40
// records without depending on unrelated renderer-owned aggregate constants.
namespace static_initializers {
idUserChannelExpression exprt{};
float foo = exprt.Eval(nullptr, 0);
alignas(16) constexpr std::int32_t vector_int_zero[4] = {};
alignas(16) constexpr std::int32_t vector_int_32[4] = {32, 32, 32, 32};
alignas(16) constexpr std::uint32_t vector_uint32_mask_keep_first[4] = {
    0xffffffffu, 0u, 0u, 0u};
alignas(16) constexpr std::uint32_t vector_uint32_0x55555555[4] = {
    0x55555555u, 0x55555555u, 0x55555555u, 0x55555555u};
alignas(16) constexpr std::uint32_t vector_uint32_0x33333333[4] = {
    0x33333333u, 0x33333333u, 0x33333333u, 0x33333333u};
alignas(16) constexpr std::uint32_t vector_uint32_0x0F0F0F0F[4] = {
    0x0f0f0f0fu, 0x0f0f0f0fu, 0x0f0f0f0fu, 0x0f0f0f0fu};
alignas(16) constexpr std::uint16_t vector_uint16_0x0101[8] = {
    0x0101u, 0x0101u, 0x0101u, 0x0101u,
    0x0101u, 0x0101u, 0x0101u, 0x0101u};
alignas(16) constexpr std::uint32_t vector_lsb_mask[4] = {1, 1, 1, 1};
alignas(16) constexpr std::uint32_t vector_mse_base[4] = {
    0x7fffffffu, 0x3fffffffu, 0x1fffffffu, 0x0fffffffu};
alignas(16) constexpr std::uint32_t vector_lse_base[4] = {31, 31, 31, 31};
alignas(16) constexpr std::uint32_t vector_olb_mask[4] = {
    0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u};
alignas(16) constexpr std::uint32_t vector_flt_abs_mask[4] = {
    0x7fffffffu, 0x7fffffffu, 0x7fffffffu, 0x7fffffffu};
alignas(16) constexpr std::uint32_t vector_flt_exp_bias[4] = {
    0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u};
alignas(16) constexpr std::int16_t vector_uint16_quat_rotate_madd[8] = {
    1, 2, 1, 2, 1, 2, 1, 2};
alignas(16) constexpr std::uint16_t vector_uint16_quat_mask[8] = {
    0x7fffu, 0x7fffu, 0x7fffu, 0x7fffu,
    0x7fffu, 0x7fffu, 0x7fffu, 0x7fffu};
alignas(16) constexpr float vector_float_quat_scale[4] = {
    2.0f * kQuatRange / 32767.0f, 2.0f * kQuatRange / 32767.0f,
    2.0f * kQuatRange / 32767.0f, 2.0f * kQuatRange / 32767.0f};
alignas(16) constexpr float vector_float_quat_bias[4] = {
    -kQuatRange, -kQuatRange, -kQuatRange, -kQuatRange};
alignas(16) constexpr std::uint32_t vector_float_mask_keep_last[4] = {
    0u, 0u, 0u, 0xffffffffu};
alignas(16) constexpr std::uint32_t vector_float_mask_clear_last[4] = {
    0xffffffffu, 0xffffffffu, 0xffffffffu, 0u};
alignas(16) constexpr std::uint32_t vector_float_sign_bit[4] = {
    0x80000000u, 0x80000000u, 0x80000000u, 0x80000000u};
alignas(16) constexpr std::uint32_t vector_float_sign_0[4] = {
    0x80000000u, 0u, 0u, 0u};
alignas(16) constexpr std::uint32_t vector_float_sign_1[4] = {
    0u, 0x80000000u, 0u, 0u};
alignas(16) constexpr std::uint32_t vector_float_sign_2[4] = {
    0u, 0u, 0x80000000u, 0u};
alignas(16) constexpr std::uint32_t vector_float_sign_3[4] = {
    0u, 0u, 0u, 0x80000000u};
alignas(16) constexpr std::uint32_t vector_float_quat_inverse[4] = {
    0x80000000u, 0x80000000u, 0x80000000u, 0u};
alignas(16) constexpr std::uint8_t vector_uint8_permute0[16] = {
    0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
alignas(16) constexpr std::uint8_t vector_uint8_permute10[16] = {
    0, 1, 2, 0x80, 3, 4, 5, 0x80, 6, 7, 8, 0x80, 9, 10, 11, 0x80};
alignas(16) constexpr std::uint8_t vector_uint8_permute11[16] = {
    12, 13, 14, 0x80, 15, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
alignas(16) constexpr std::uint8_t vector_uint8_permute20[16] = {
    0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};
alignas(16) constexpr std::uint8_t vector_uint8_permute21[16] = {
    4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7};
alignas(16) constexpr std::uint8_t vector_uint8_permute3[16] = {
    0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
alignas(16) constexpr std::uint16_t vector_uint16_vector_offset[8] = {
    128, 128, 128, 128, 128, 128, 128, 128};
alignas(16) constexpr std::uint16_t vector_uint16_0x8000[8] = {
    0x8000u, 0x8000u, 0x8000u, 0x8000u,
    0x8000u, 0x8000u, 0x8000u, 0x8000u};
alignas(16) constexpr std::uint32_t vector_uint32_0x00008000[4] = {
    0x00008000u, 0x00008000u, 0x00008000u, 0x00008000u};
alignas(16) constexpr std::uint8_t vector_uint128_mask0[16] = {
    0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0};
alignas(16) constexpr std::uint8_t vector_uint128_mask1[16] = {
    0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0};
alignas(16) constexpr std::uint8_t vector_uint128_mask2[16] = {
    0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0};
alignas(16) constexpr std::uint8_t vector_uint128_mask3[16] = {
    0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff, 0, 0, 0, 0xff};
alignas(16) constexpr std::uint8_t vector_uint128_mask4[16] = {
    0, 0, 0, 0, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr std::uint8_t vector_uint128_mask5[16] = {
    0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr std::uint8_t vector_uint128_mask6[16] = {
    0, 0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0, 0, 0, 0, 0, 0};
alignas(16) constexpr std::uint8_t vector_uint128_mask7[16] = {
    0, 0, 0, 0, 0, 0, 0, 0xff, 0, 0, 0, 0, 0, 0, 0, 0};
} // namespace static_initializers

} // namespace idMD6Blend
