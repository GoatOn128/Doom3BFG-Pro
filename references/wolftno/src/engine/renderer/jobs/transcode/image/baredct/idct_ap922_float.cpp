#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace {

const float kTan1Over16 = std::tan(0.19634955f);
const float kTan2Over16 = std::tan(0.39269909f);
const float kTan3Over16 = std::tan(0.58904862f);
const float kCos4Over16 = std::cos(0.78539819f);

std::array<float, 32> MakeInverseRowTable(float verticalScale) {
  std::array<float, 32> table{};
  const float cos1 = std::cos(0.19634955f);
  const float cos2 = std::cos(0.39269909f);
  const float cos3 = std::cos(0.58904862f);
  const float cos4 = std::cos(0.78539819f);
  const float cos5 = std::cos(0.98174775f);
  const float cos6 = std::cos(1.1780972f);
  const float cos7 = std::cos(1.3744469f);

  const float dc = (cos4 * verticalScale) * 0.25f;
  const float even2 = (cos2 * verticalScale) * 0.25f;
  const float even6 = (cos6 * verticalScale) * 0.25f;
  table[0] = table[1] = table[2] = table[3] = dc;
  table[4] = even2;
  table[5] = even6;
  table[6] = -even6;
  table[7] = -even2;
  table[8] = dc;
  table[9] = -dc;
  table[10] = -dc;
  table[11] = dc;
  table[12] = even6;
  table[13] = -even2;
  table[14] = even2;
  table[15] = -even6;

  const float odd1 = (cos1 * verticalScale) * 0.25f;
  const float odd3 = (cos3 * verticalScale) * 0.25f;
  const float odd5 = (cos5 * verticalScale) * 0.25f;
  const float odd7 = (cos7 * verticalScale) * 0.25f;
  table[16] = odd1;
  table[17] = odd3;
  table[18] = odd5;
  table[19] = odd7;
  table[20] = odd3;
  table[21] = -odd7;
  table[22] = -odd1;
  table[23] = -odd5;
  table[24] = odd5;
  table[25] = -odd1;
  table[26] = odd7;
  table[27] = odd3;
  table[28] = odd7;
  table[29] = -odd5;
  table[30] = odd3;
  table[31] = -odd1;
  return table;
}

const std::array<float, 32> kInverseRow04 =
    MakeInverseRowTable(std::cos(0.78539819f));
const std::array<float, 32> kInverseRow17 =
    MakeInverseRowTable(std::cos(0.19634955f));
const std::array<float, 32> kInverseRow26 =
    MakeInverseRowTable(std::cos(0.39269909f));
const std::array<float, 32> kInverseRow35 =
    MakeInverseRowTable(std::cos(0.58904862f));

const std::array<float, 32>* const kInverseRowTables[8] = {
    &kInverseRow04, &kInverseRow17, &kInverseRow26, &kInverseRow35,
    &kInverseRow04, &kInverseRow35, &kInverseRow26, &kInverseRow17,
};

float Dequantize(std::int16_t coefficient, std::uint16_t quantizer) {
  const std::uint32_t product =
      static_cast<std::uint32_t>(static_cast<std::int32_t>(coefficient)) *
      static_cast<std::uint32_t>(quantizer);
  return static_cast<float>(static_cast<std::int32_t>(product));
}

std::int16_t RoundAndClamp(float value) {
  value = value <= 0.0f ? value - 0.5f : value + 0.5f;
  const int rounded = static_cast<int>(value);
  if (rounded < -32768)
    return -32768;
  if (rounded > 32767)
    return 32767;
  return static_cast<std::int16_t>(rounded);
}

}  // namespace

void IDCT_AP922_float(const std::int16_t* coefficients,
                      const std::uint16_t* quantizers,
                      std::int16_t* destination) {
  float temporary[64];
  for (int row = 0; row < 8; ++row) {
    const std::int16_t* coefficient = coefficients + row * 8;
    const std::uint16_t* quantizer = quantizers + row * 8;
    const float* table = kInverseRowTables[row]->data();
    const float c0 = Dequantize(coefficient[0], quantizer[0]);
    const float c1 = Dequantize(coefficient[1], quantizer[1]);
    const float c2 = Dequantize(coefficient[2], quantizer[2]);
    const float c3 = Dequantize(coefficient[3], quantizer[3]);
    const float c4 = Dequantize(coefficient[4], quantizer[4]);
    const float c5 = Dequantize(coefficient[5], quantizer[5]);
    const float c6 = Dequantize(coefficient[6], quantizer[6]);
    const float c7 = Dequantize(coefficient[7], quantizer[7]);

    float even[4];
    float odd[4];
    for (int output = 0; output < 4; ++output) {
      even[output] =
          ((c2 * table[4 + output] + c0 * table[output]) +
           c4 * table[8 + output]) +
          c6 * table[12 + output];
      odd[output] =
          ((c3 * table[20 + output] + c1 * table[16 + output]) +
           c5 * table[24 + output]) +
          c7 * table[28 + output];
    }
    float* output = temporary + row * 8;
    output[0] = odd[0] + even[0];
    output[1] = odd[1] + even[1];
    output[2] = odd[2] + even[2];
    output[3] = odd[3] + even[3];
    output[4] = even[3] - odd[3];
    output[5] = even[2] - odd[2];
    output[6] = even[1] - odd[1];
    output[7] = even[0] - odd[0];
  }

  for (int column = 0; column < 8; ++column) {
    const float row0 = temporary[column + 0 * 8];
    const float row1 = temporary[column + 1 * 8];
    const float row2 = temporary[column + 2 * 8];
    const float row3 = temporary[column + 3 * 8];
    const float row4 = temporary[column + 4 * 8];
    const float row5 = temporary[column + 5 * 8];
    const float row6 = temporary[column + 6 * 8];
    const float row7 = temporary[column + 7 * 8];

    const float odd17Plus = row7 * kTan1Over16 + row1;
    const float odd17Minus = row1 * kTan1Over16 - row7;
    const float odd35Plus = row5 * kTan3Over16 + row3;
    const float odd35Minus = row5 - row3 * kTan3Over16;
    const float oddA = odd35Plus + odd17Plus;
    const float oddB = odd17Plus - odd35Plus;
    const float oddC = odd35Minus + odd17Minus;
    const float oddD = odd17Minus - odd35Minus;

    const float even26Plus = row6 * kTan2Over16 + row2;
    const float rotatedA = (oddD + oddB) * kCos4Over16;
    const float rotatedB = (oddB - oddD) * kCos4Over16;
    const float even26Minus = row2 * kTan2Over16 - row6;
    const float even04Plus = row0 + row4;
    const float even04Minus = row0 - row4;
    const float evenA = even26Plus + even04Plus;
    const float evenB = even04Plus - even26Plus;
    const float evenC = even26Minus + even04Minus;
    const float evenD = even04Minus - even26Minus;

    destination[column + 0 * 8] = RoundAndClamp(evenA + oddA);
    destination[column + 1 * 8] = RoundAndClamp(evenC + rotatedA);
    destination[column + 2 * 8] = RoundAndClamp(evenD + rotatedB);
    destination[column + 3 * 8] = RoundAndClamp(evenB + oddC);
    destination[column + 4 * 8] = RoundAndClamp(evenB - oddC);
    destination[column + 5 * 8] = RoundAndClamp(evenD - rotatedB);
    destination[column + 6 * 8] = RoundAndClamp(evenC - rotatedA);
    destination[column + 7 * 8] = RoundAndClamp(evenA - oddA);
  }
}
