#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <array>
#include <cmath>
#include <cstdint>

namespace {

const float kForwardTan1Over16 = std::tan(0.19634955f);
const float kForwardTan2Over16 = std::tan(0.39269909f);
const float kForwardTan3Over16 = std::tan(0.58904862f);
const float kForwardCos4Over16 = std::cos(0.78539819f);

std::array<float, 32> MakeForwardRowTable(float verticalScale) {
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
  table[0] = dc;
  table[1] = even2;
  table[2] = dc;
  table[3] = even6;
  table[4] = dc;
  table[5] = -even6;
  table[6] = -dc;
  table[7] = even2;
  table[8] = dc;
  table[9] = even6;
  table[10] = -dc;
  table[11] = -even2;
  table[12] = dc;
  table[13] = -even2;
  table[14] = dc;
  table[15] = -even6;

  const float odd1 = (cos1 * verticalScale) * 0.25f;
  const float odd3 = (cos3 * verticalScale) * 0.25f;
  const float odd5 = (cos5 * verticalScale) * 0.25f;
  const float odd7 = (cos7 * verticalScale) * 0.25f;
  table[16] = odd1;
  table[17] = odd3;
  table[18] = odd5;
  table[19] = odd7;
  table[20] = odd5;
  table[21] = -odd1;
  table[22] = odd7;
  table[23] = odd3;
  table[24] = odd3;
  table[25] = -odd7;
  table[26] = -odd1;
  table[27] = -odd5;
  table[28] = odd7;
  table[29] = -odd5;
  table[30] = odd3;
  table[31] = -odd1;
  return table;
}

const std::array<float, 32> kForwardRow04 =
    MakeForwardRowTable(std::cos(0.78539819f));
const std::array<float, 32> kForwardRow17 =
    MakeForwardRowTable(std::cos(0.19634955f));
const std::array<float, 32> kForwardRow26 =
    MakeForwardRowTable(std::cos(0.39269909f));
const std::array<float, 32> kForwardRow35 =
    MakeForwardRowTable(std::cos(0.58904862f));

const std::array<float, 32>* const kForwardRowTables[8] = {
    &kForwardRow04, &kForwardRow17, &kForwardRow26, &kForwardRow35,
    &kForwardRow04, &kForwardRow35, &kForwardRow26, &kForwardRow17,
};

std::int16_t Quantize(float value, std::uint16_t quantizer) {
  float quotient = value / static_cast<float>(quantizer);
  quotient = value >= 0.0f ? quotient + 0.5f : quotient - 0.5f;
  const int rounded = static_cast<int>(quotient);
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(rounded));
}

}  // namespace

void FDCT_AP922_float(const std::int16_t* source,
                      const std::uint16_t* quantizers,
                      std::int16_t* coefficients) {
  float temporary[64];
  for (int column = 0; column < 8; ++column) {
    const int row0 = source[column + 0 * 8];
    const int row1 = source[column + 1 * 8];
    const int row2 = source[column + 2 * 8];
    const int row3 = source[column + 3 * 8];
    const int row4 = source[column + 4 * 8];
    const int row5 = source[column + 5 * 8];
    const int row6 = source[column + 6 * 8];
    const int row7 = source[column + 7 * 8];

    const float sum07 = static_cast<float>(row0 + row7);
    const float difference16 = static_cast<float>(row1 - row6);
    const float difference07 = static_cast<float>(row0 - row7);
    const float sum16 = static_cast<float>(row1 + row6);
    const float difference25 = static_cast<float>(row2 - row5);
    const float sum25 = static_cast<float>(row2 + row5);
    const float sum34 = static_cast<float>(row3 + row4);
    const float difference34 = static_cast<float>(row3 - row4);

    const float evenA = sum25 + sum16;
    const float evenB = sum16 - sum25;
    const float evenC = sum34 + sum07;
    const float evenD = sum07 - sum34;
    temporary[column + 0 * 8] = evenA + evenC;
    temporary[column + 4 * 8] = evenC - evenA;
    temporary[column + 6 * 8] = kForwardTan2Over16 * evenD - evenB;
    temporary[column + 2 * 8] = kForwardTan2Over16 * evenB + evenD;

    const float rotationA =
        (difference16 + difference25) * kForwardCos4Over16;
    const float rotationB =
        (difference16 - difference25) * kForwardCos4Over16;
    const float oddA = rotationA + difference07;
    const float oddB = difference07 - rotationA;
    const float oddC = rotationB + difference34;
    const float oddD = difference34 - rotationB;
    temporary[column + 1 * 8] = kForwardTan1Over16 * oddC + oddA;
    temporary[column + 7 * 8] = kForwardTan1Over16 * oddA - oddC;
    temporary[column + 5 * 8] = kForwardTan3Over16 * oddB + oddD;
    temporary[column + 3 * 8] = oddB - kForwardTan3Over16 * oddD;
  }

  for (int row = 0; row < 8; ++row) {
    const float* input = temporary + row * 8;
    const float* table = kForwardRowTables[row]->data();
    const float evenInput[4] = {
        input[0] + input[7], input[5] + input[2],
        input[6] + input[1], input[4] + input[3],
    };
    const float oddInput[4] = {
        input[0] - input[7], input[2] - input[5],
        input[1] - input[6], input[3] - input[4],
    };
    float even[4];
    float odd[4];
    for (int output = 0; output < 4; ++output) {
      even[output] =
          ((evenInput[1] * table[4 + output] +
            evenInput[0] * table[output]) +
           evenInput[2] * table[8 + output]) +
          evenInput[3] * table[12 + output];
      odd[output] =
          ((oddInput[1] * table[20 + output] +
            oddInput[0] * table[16 + output]) +
           oddInput[2] * table[24 + output]) +
          oddInput[3] * table[28 + output];
    }
    std::int16_t* output = coefficients + row * 8;
    output[0] = Quantize(even[0], quantizers[row * 8 + 0]);
    output[1] = Quantize(odd[0], quantizers[row * 8 + 1]);
    output[2] = Quantize(even[1], quantizers[row * 8 + 2]);
    output[3] = Quantize(odd[1], quantizers[row * 8 + 3]);
    output[4] = Quantize(even[2], quantizers[row * 8 + 4]);
    output[5] = Quantize(odd[2], quantizers[row * 8 + 5]);
    output[6] = Quantize(even[3], quantizers[row * 8 + 6]);
    output[7] = Quantize(odd[3], quantizers[row * 8 + 7]);
  }
}
