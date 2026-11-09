#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

namespace {

constexpr unsigned int kScanTotals[15] = {32, 30, 28, 26, 24, 22, 20, 18,
                                           16, 14, 12, 10, 8,  6,  4};
constexpr unsigned int kScanOrderHorizontal[15] = {4,  1, 5,  8, 2, 9,  6, 12,
                                                   3, 10, 13, 7, 14, 11, 15};
constexpr unsigned int kScanOrderVertical[15] = {1,  2, 5,  4, 3, 6,  9, 8,
                                                 7, 12, 15, 13, 10, 11, 14};
constexpr int kModelWeights0[3] = {240, 12, 1};
constexpr int kModelWeights2[3] = {120, 37, 2};
constexpr int kModelWeights3[3] = {120, 18, 1};
constexpr int kInversePermutation[16] = {0, 8, 4, 13, 2, 15, 3, 14,
                                         1, 12, 5, 9,  7, 11, 6, 10};

int CountBits(int value) {
  int count = 0;
  while (value > 0) {
    count += value & 1;
    value >>= 1;
  }
  return count;
}

void InitializeAdaptiveVlc(jxr_vlc_table_t& table) {
  table.discriminant = 0;
  table.discriminant2 = 0;
  table.table = 1;
  table.deltatable = 0;
  table.delta2table = 1;
}

void InitializeSimpleVlc(jxr_vlc_table_t& table) {
  table.discriminant = 0;
  table.table = 0;
  table.deltatable = 0;
}

}  // namespace

int _jxr_floor_div2(int x) {
  return x < 0 ? (1 - x) / -2 : x / 2;
}

int _jxr_ceil_div2(int x) {
  return (x < 0 ? x : x + 1) / 2;
}

int _jxr_quant_map(jxr_image* image, int x, int shift) {
  if (x == 0)
    return 1;
  if ((image->component_mode & 1U) != 0) {
    if (x >= 16)
      return (x % 16 + 16) << ((x >> 4) + shift - 1);
    return x << shift;
  }
  if (x < 32)
    return (x + 3) >> 2;
  if (x < 48)
    return (((x & 15) + 17) >> 1) << ((x >> 4) - 2);
  return ((x & 15) + 16) << ((x >> 4) - 3);
}

int _jxr_vlc_select(int band, int chromaFlag) {
  const int chroma = chromaFlag != 0;
  if (band == 0)
    return chroma;
  if (band == 1)
    return chroma + 3;
  if (band == 2)
    return chroma + 5;
  return 0;
}

void _jxr_InitVLCTable(jxr_image* image, int vlcSelect) {
  auto& table = image->vlc_table[vlcSelect];
  table.discriminant = 0;
  table.table = 0;
  table.deltatable = 0;
}

void _jxr_AdaptVLCTable(jxr_image* image, int vlcSelect) {
  auto& vlc = image->vlc_table[vlcSelect];
  vlc.deltatable = 0;
  if (vlc.discriminant < -8 && vlc.table != 0) {
    vlc.discriminant = 0;
    --vlc.table;
  } else if (vlc.discriminant > 8 && vlc.table != 1) {
    vlc.discriminant = 0;
    ++vlc.table;
  } else {
    vlc.discriminant = std::clamp(vlc.discriminant, -64, 64);
  }
}

void AdaptVLCTable2(jxr_image* image, int vlcSelect, int maxIndex) {
  auto& vlc = image->vlc_table[vlcSelect];
  bool changed = false;
  if (vlc.discriminant < -8 && vlc.table > 0) {
    --vlc.table;
    changed = true;
  } else if (vlc.discriminant2 > 8 && vlc.table < maxIndex) {
    ++vlc.table;
    changed = true;
  }

  if (changed) {
    vlc.discriminant = 0;
    vlc.discriminant2 = 0;
    if (vlc.table == maxIndex) {
      vlc.deltatable = maxIndex - 1;
      vlc.delta2table = maxIndex - 1;
    } else if (vlc.table != 0) {
      vlc.deltatable = vlc.table - 1;
      vlc.delta2table = vlc.table;
    } else {
      vlc.deltatable = 0;
      vlc.delta2table = 0;
    }
    return;
  }

  vlc.discriminant = std::clamp(vlc.discriminant, -64, 64);
  vlc.discriminant2 = std::clamp(vlc.discriminant2, -64, 64);
}

void _jxr_AdaptLP(jxr_image* image) {
  AdaptVLCTable2(image, 2, 4);
  AdaptVLCTable2(image, 7, 3);
  AdaptVLCTable2(image, 8, 3);
  AdaptVLCTable2(image, 9, 4);
  AdaptVLCTable2(image, 10, 3);
  AdaptVLCTable2(image, 11, 3);
  _jxr_AdaptVLCTable(image, 3);
  _jxr_AdaptVLCTable(image, 4);
}

void _jxr_AdaptHP(jxr_image* image) {
  AdaptVLCTable2(image, 16, 4);
  AdaptVLCTable2(image, 14, 3);
  AdaptVLCTable2(image, 15, 3);
  AdaptVLCTable2(image, 17, 4);
  AdaptVLCTable2(image, 18, 3);
  AdaptVLCTable2(image, 19, 3);
  _jxr_AdaptVLCTable(image, 5);
  _jxr_AdaptVLCTable(image, 6);
  _jxr_AdaptVLCTable(image, 12);
  _jxr_AdaptVLCTable(image, 13);
}

int _jxr_InitContext(jxr_image*, unsigned int, unsigned int,
                     unsigned int macroblockX, unsigned int macroblockY) {
  return macroblockX == 0 && macroblockY == 0;
}

int _jxr_ResetContext(jxr_image* image, unsigned int tileX,
                      unsigned int macroblockX) {
  return (macroblockX & 15U) == 0 ||
         image->tile_column_width[tileX] == macroblockX + 1;
}

int _jxr_ResetTotals(jxr_image*, char macroblockX) {
  return (macroblockX & 15) == 0;
}

void _jxr_InitializeModelMB(model_s* model, int band) {
  for (int channel = 0; channel < 16; ++channel) {
    model->state[channel] = 0;
    model->bits[channel] = 4 * (2 - band);
  }
}

void _jxr_UpdateModelMB(jxr_image* image, char* lapMeanBytes, model_s* model,
                        int band) {
  int* lapMean = reinterpret_cast<int*>(lapMeanBytes);
  lapMean[0] *= kModelWeights0[band];
  if (image->use_clr_fmt == 1) {
    lapMean[1] *= kModelWeights2[band];
  } else if (image->use_clr_fmt == 2) {
    lapMean[1] *= kModelWeights3[band];
  } else {
    for (unsigned int channel = 1; channel < image->num_channels; ++channel)
      lapMean[channel] *= kModelWeights0[band];
  }

  const int channelCount = image->use_clr_fmt == 0 ? 1 : 16;
  for (int channel = 0; channel < channelCount; ++channel) {
    int state = model->state[channel];
    const int delta = (lapMean[channel] - 70) >> 2;
    if (delta <= -8) {
      state += std::max(delta + 4, -16);
      if (state < -8) {
        if (model->bits[channel] != 0) {
          state = 0;
          --model->bits[channel];
        } else {
          state = -8;
        }
      }
    } else if (delta >= 8) {
      state += std::min(delta - 4, 15);
      if (state > 8) {
        if (model->bits[channel] < 15) {
          state = 0;
          ++model->bits[channel];
        } else {
          model->bits[channel] = 15;
          state = 8;
        }
      }
    }
    model->state[channel] = state;
  }
}

void _jxr_InitializeCountCBPLP(jxr_image* image) {
  image->count_max_CBPLP = 1;
  image->count_zero_CBPLP = 1;
}

void _jxr_UpdateCountCBPLP(jxr_image* image, int cbplp, int maximum) {
  image->count_zero_CBPLP =
      std::clamp(image->count_zero_CBPLP + 1 - (cbplp == 0 ? 4 : 0), -8, 7);
  image->count_max_CBPLP = std::clamp(
      image->count_max_CBPLP + 1 - (cbplp == maximum ? 4 : 0), -8, 7);
}

void _jxr_InitLPVLC(jxr_image* image) {
  for (const int index : {2, 7, 8, 9, 10, 11})
    InitializeAdaptiveVlc(image->vlc_table[index]);
  InitializeSimpleVlc(image->vlc_table[3]);
  InitializeSimpleVlc(image->vlc_table[4]);
}

void _jxr_InitializeAdaptiveScanLP(jxr_image* image) {
  std::copy_n(kScanOrderHorizontal, 15, image->lopass_scanorder);
  std::copy_n(kScanTotals, 15, image->lopass_scantotals);
}

void _jxr_InitializeAdaptiveScanHP(jxr_image* image) {
  std::copy_n(kScanOrderHorizontal, 15, image->hipass_hor_scanorder);
  std::copy_n(kScanTotals, 15, image->hipass_hor_scantotals);
  std::copy_n(kScanOrderVertical, 15, image->hipass_ver_scanorder);
  std::copy_n(kScanTotals, 15, image->hipass_ver_scantotals);
}

void _jxr_InitializeCBPModel(jxr_image* image) {
  for (int channel = 0; channel < 16; ++channel) {
    image->hp_cbp_model.state[channel] = 0;
    image->hp_cbp_model.count0[channel] = -4;
    image->hp_cbp_model.count1[channel] = 4;
  }
}

void _jxr_InitHPVLC(jxr_image* image) {
  for (const int index : {16, 14, 15, 17, 18, 19})
    InitializeAdaptiveVlc(image->vlc_table[index]);
  InitializeSimpleVlc(image->vlc_table[5]);
  InitializeSimpleVlc(image->vlc_table[6]);
}

void _jxr_InitCBPVLC(jxr_image* image) {
  InitializeSimpleVlc(image->vlc_table[12]);
  InitializeSimpleVlc(image->vlc_table[13]);
}

void update_cbp_model(jxr_image* image, int channel, int originalCount) {
  auto& model = image->hp_cbp_model;
  model.count0[channel] =
      std::clamp(model.count0[channel] + originalCount - 3, -16, 15);
  model.count1[channel] =
      std::clamp(model.count1[channel] - originalCount + 13, -16, 15);
  if (model.count0[channel] >= 0) {
    model.state[channel] = model.count1[channel] < 0 ? 2 : 0;
  } else {
    model.state[channel] =
        1 + (model.count0[channel] >= model.count1[channel] ? 1 : 0);
  }
}

int _jxr_PredCBP444(jxr_image* image, int* diffCbp, int channel,
                    unsigned int tileX, unsigned int macroblockX,
                    unsigned int macroblockY) {
  unsigned int cbp = static_cast<unsigned int>(diffCbp[channel]);
  if (image->hp_cbp_model.state[channel] == 0) {
    unsigned int predictor;
    if (macroblockX != 0) {
      predictor = static_cast<unsigned int>(
          image->strip[channel]
              .cur[image->tile_column_position[tileX] + macroblockX - 1]
              .hp_cbp >>
          5);
    } else if (macroblockY != 0) {
      predictor = static_cast<unsigned int>(
          image->strip[channel].up1[image->tile_column_position[tileX]].hp_cbp >>
          10);
    } else {
      predictor = 1;
    }
    unsigned int value = cbp ^ (predictor & 1U);
    value = (8U * (((2U * (value & 1U)) ^ value) & 2U)) ^
            (2U * (value & 1U)) ^ value;
    value = (4U * (((2U * (value & 0x10U)) ^ value) & 0x33U)) ^
            (2U * (value & 0x10U)) ^ value;
    cbp = (4U * ((((value & 0xCCU) << 6U) ^ value) & 0x3300U)) ^
          ((value & 0xCCU) << 6U) ^ value;
  } else if (image->hp_cbp_model.state[channel] == 2) {
    cbp ^= 0xFFFFU;
  }
  update_cbp_model(image, channel, CountBits(static_cast<int>(cbp)));
  return static_cast<int>(cbp);
}

int _jxr_PredCBP422(jxr_image* image, int* diffCbp, int channel,
                    unsigned int tileX, unsigned int macroblockX,
                    unsigned int macroblockY) {
  unsigned int cbp = static_cast<unsigned int>(diffCbp[channel]);
  if (image->hp_cbp_model.state[1] == 0) {
    unsigned int predictor;
    if (macroblockX != 0) {
      predictor = static_cast<unsigned int>(
          image->strip[channel]
              .cur[image->tile_column_position[tileX] + macroblockX - 1]
              .hp_cbp >>
          1);
    } else if (macroblockY != 0) {
      predictor = static_cast<unsigned int>(
          image->strip[channel].up1[image->tile_column_position[tileX]].hp_cbp >>
          6);
    } else {
      predictor = 1;
    }
    unsigned int value = cbp ^ (predictor & 1U);
    value = (4U * (((2U * (value & 1U)) ^ value) & 3U)) ^
            (2U * (value & 1U)) ^ value;
    cbp = (4U * (((4U * (value & 0xCU)) ^ value) & 0x30U)) ^
          (4U * (value & 0xCU)) ^ value;
  } else if (image->hp_cbp_model.state[1] == 2) {
    cbp ^= 0xFFU;
  }
  update_cbp_model(image, 1, 2 * CountBits(static_cast<int>(cbp)));
  return static_cast<int>(cbp);
}

int _jxr_PredCBP420(jxr_image* image, int* diffCbp, int channel,
                    unsigned int tileX, unsigned int macroblockX,
                    unsigned int macroblockY) {
  unsigned int cbp = static_cast<unsigned int>(diffCbp[channel]);
  if (image->hp_cbp_model.state[1] == 0) {
    unsigned int predictor;
    if (macroblockX != 0) {
      predictor = static_cast<unsigned int>(
          image->strip[channel]
              .cur[image->tile_column_position[tileX] + macroblockX - 1]
              .hp_cbp >>
          1);
    } else if (macroblockY != 0) {
      predictor = static_cast<unsigned int>(
          image->strip[channel].up1[image->tile_column_position[tileX]].hp_cbp >>
          2);
    } else {
      predictor = 1;
    }
    const unsigned int value = cbp ^ (predictor & 1U);
    cbp = (4U * (((2U * (value & 1U)) ^ value) & 3U)) ^
          (2U * (value & 1U)) ^ value;
  } else if (image->hp_cbp_model.state[1] == 2) {
    cbp ^= 0xFU;
  }
  update_cbp_model(image, 1, 4 * CountBits(static_cast<int>(cbp)));
  return static_cast<int>(cbp);
}

void _jxr_ResetTotalsAdaptiveScanLP(jxr_image* image) {
  std::copy_n(kScanTotals, 15, image->lopass_scantotals);
}

void _jxr_ResetTotalsAdaptiveScanHP(jxr_image* image) {
  std::copy_n(kScanTotals, 15, image->hipass_hor_scantotals);
  std::copy_n(kScanTotals, 15, image->hipass_ver_scantotals);
}

int calculate_mbdc_mode(jxr_image* image, int channel, int tileX,
                        int macroblockX, int macroblockY) {
  if (macroblockX == 0)
    return macroblockY == 0 ? 3 : 1;
  if (macroblockY == 0)
    return 0;

  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  const unsigned int leftColumn = column - 1;
  const int left = image->strip[channel].cur[leftColumn].data[0];
  const int above = image->strip[channel].up1[column].data[0];
  const int northwest = image->strip[channel].up1[leftColumn].data[0];

  int horizontal;
  int vertical;
  if (image->use_clr_fmt == 0 || image->use_clr_fmt == 6) {
    horizontal = std::abs(northwest - left);
    vertical = std::abs(northwest - above);
  } else {
    const int left1 = image->strip[1].cur[leftColumn].data[0];
    const int above1 = image->strip[1].up1[column].data[0];
    const int northwest1 = image->strip[1].up1[leftColumn].data[0];
    const int left2 = image->strip[2].cur[leftColumn].data[0];
    const int above2 = image->strip[2].up1[column].data[0];
    const int northwest2 = image->strip[2].up1[leftColumn].data[0];
    const int lumaWeight = image->use_clr_fmt == 1
                               ? 8
                               : (image->use_clr_fmt == 2 ? 4 : 2);
    horizontal = std::abs(northwest1 - left1) +
                 std::abs(northwest2 - left2) +
                 lumaWeight * std::abs(northwest - left);
    vertical = std::abs(northwest1 - above1) +
               std::abs(northwest2 - above2) +
               lumaWeight * std::abs(northwest - above);
  }

  if (4 * horizontal < vertical)
    return 1;
  if (4 * vertical < horizontal)
    return 0;
  return 2;
}

void predict_lp444(jxr_image* image, int tileX, int macroblockX, int channel,
                   int mode) {
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  macroblock_s& current = image->strip[channel].cur[column];
  if (mode == 1) {
    current.data[1] += image->strip[channel].up1[column].pred_dclp[1];
    current.data[2] += image->strip[channel].up1[column].pred_dclp[2];
    current.data[3] += image->strip[channel].up1[column].pred_dclp[3];
  } else if (mode == 0) {
    const macroblock_s& left = image->strip[channel].cur[column - 1];
    current.data[4] += left.pred_dclp[4];
    current.data[8] += left.pred_dclp[5];
    current.data[12] += left.pred_dclp[6];
  }
  current.pred_dclp[1] = current.data[1];
  current.pred_dclp[2] = current.data[2];
  current.pred_dclp[3] = current.data[3];
  current.pred_dclp[4] = current.data[4];
  current.pred_dclp[5] = current.data[8];
  current.pred_dclp[6] = current.data[12];
}

void predict_lp422(jxr_image* image, int tileX, int macroblockX, int channel,
                   int mode, int dcMode) {
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  macroblock_s& current = image->strip[channel].cur[column];
  if (mode == 1) {
    current.data[4] += image->strip[channel].up1[column].pred_dclp[4];
    current.data[1] += image->strip[channel].up1[column].pred_dclp[5];
    current.data[5] += current.data[1];
  } else if (mode == 2 && dcMode == 1) {
    current.data[5] += current.data[1];
  } else if (mode == 0) {
    const macroblock_s& left = image->strip[channel].cur[column - 1];
    current.data[4] += left.pred_dclp[4];
    current.data[2] += left.pred_dclp[2];
    current.data[6] += left.pred_dclp[6];
  }
  current.pred_dclp[1] = current.data[1];
  current.pred_dclp[2] = current.data[2];
  current.pred_dclp[4] = current.data[4];
  current.pred_dclp[5] = current.data[5];
  current.pred_dclp[6] = current.data[6];
}

void predict_lp420(jxr_image* image, int tileX, int macroblockX, int channel,
                   int mode) {
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  macroblock_s& current = image->strip[channel].cur[column];
  if (mode == 1) {
    current.data[1] += image->strip[channel].up1[column].pred_dclp[1];
  } else if (mode == 0) {
    current.data[2] +=
        image->strip[channel].cur[column - 1].pred_dclp[2];
  }
  current.pred_dclp[1] = current.data[1];
  current.pred_dclp[2] = current.data[2];
}

void _InvPermute(int* coefficients) {
  int permuted[16];
  for (int index = 0; index < 16; ++index)
    permuted[kInversePermutation[index]] = coefficients[index];
  std::memcpy(coefficients, permuted, sizeof(permuted));
}

void _jxr_InvPermute2pt(int* a, int* b) {
  std::swap(*a, *b);
}

void _jxr_2ptT(int* a, int* b) {
  *a -= (*b + 1) >> 1;
  *b += *a;
}

void _jxr_2x2IPCT(int* coefficients) {
  coefficients[0] += coefficients[3];
  coefficients[1] -= coefficients[2];
  const int halfDifference = (coefficients[0] - coefficients[1]) >> 1;
  const int newFourth = halfDifference - coefficients[2];
  coefficients[2] = halfDifference - coefficients[3];
  coefficients[3] = newFourth;
  coefficients[0] -= newFourth;
  coefficients[1] += coefficients[2];
}

void _jxr_4x4IPCT(int* c) {
  _InvPermute(c);
  c[0] += c[5];
  c[1] -= c[4];
  int half = (c[0] - c[1] + 1) >> 1;
  int temporary = half - c[4];
  c[4] = half - c[5];
  c[5] = temporary;
  c[0] -= temporary;
  c[1] += c[4];

  c[3] += c[7];
  c[2] -= c[6];
  c[7] -= c[3] >> 1;
  c[6] += (c[2] + 1) >> 1;
  c[2] -= (3 * c[3] + 4) >> 3;
  c[3] += (3 * c[2] + 4) >> 3;
  c[6] -= (3 * c[7] + 4) >> 3;
  c[7] += (3 * c[6] + 4) >> 3;
  c[6] -= (c[3] + 1) >> 1;
  c[7] = ((c[2] + 1) >> 1) - c[7];
  c[3] += c[6];
  c[2] -= c[7];

  c[12] += c[13];
  c[8] -= c[9];
  c[13] -= c[12] >> 1;
  c[9] += (c[8] + 1) >> 1;
  c[8] -= (3 * c[12] + 4) >> 3;
  c[12] += (3 * c[8] + 4) >> 3;
  c[9] -= (3 * c[13] + 4) >> 3;
  c[13] += (3 * c[9] + 4) >> 3;
  c[9] -= (c[12] + 1) >> 1;
  c[13] = ((c[8] + 1) >> 1) - c[13];
  c[12] += c[9];
  c[8] -= c[13];

  c[15] += c[10];
  c[14] -= c[11];
  const int half15 = c[15] >> 1;
  const int half14 = c[14] >> 1;
  c[10] -= half15;
  c[11] += half14;
  c[10] -= (3 * c[11] + 3) >> 3;
  c[11] += (3 * c[10] + 3) >> 2;
  c[10] -= (3 * c[11] + 4) >> 3;
  c[11] -= half14;
  c[10] += half15;
  c[14] += c[11];
  c[15] -= c[10];
  c[11] = -c[11];
  c[14] = -c[14];

  c[0] += c[15];
  c[3] -= c[12];
  half = (c[0] - c[3]) >> 1;
  temporary = half - c[12];
  c[12] = half - c[15];
  c[15] = temporary;
  c[0] -= temporary;
  c[3] += c[12];

  c[5] += c[10];
  c[6] -= c[9];
  temporary = ((c[5] - c[6]) >> 1) - c[9];
  c[9] = ((c[5] - c[6]) >> 1) - c[10];
  c[10] = temporary;
  c[5] -= temporary;
  c[6] += c[9];

  c[1] += c[14];
  c[2] -= c[13];
  half = (c[1] - c[2]) >> 1;
  temporary = half - c[13];
  c[13] = half - c[14];
  c[14] = temporary;
  c[1] -= temporary;
  c[2] += c[13];

  c[4] += c[11];
  c[7] -= c[8];
  half = (c[4] - c[7]) >> 1;
  temporary = half - c[8];
  c[8] = half - c[11];
  c[11] = temporary;
  c[4] -= temporary;
  c[7] += c[8];
}

void _jxr_4x4OverlapFilter(int* a, int* b, int* c, int* d, int* e, int* f,
                           int* g, int* h, int* i, int* j, int* k, int* l,
                           int* m, int* n, int* o, int* p) {
  *a += *p;
  *d -= *m;
  int half = (*a - *d) >> 1;
  int temporary = half - *m;
  *m = half - *p;
  *p = temporary;
  *a -= temporary;
  *d += *m;

  *b += *o;
  *c -= *n;
  half = (*b - *c) >> 1;
  temporary = half - *n;
  *n = half - *o;
  *o = temporary;
  *b -= temporary;
  *c += *n;

  *e += *l;
  *h -= *i;
  half = (*e - *h) >> 1;
  temporary = half - *i;
  *i = half - *l;
  *l = temporary;
  *e -= temporary;
  *h += *i;

  *f += *k;
  *g -= *j;
  half = (*f - *g) >> 1;
  temporary = half - *j;
  *j = half - *k;
  *k = temporary;
  *f -= temporary;
  *g += *j;

  *n -= (*m + 1) >> 1;
  *m += (*n + 1) >> 1;
  *j -= (*i + 1) >> 1;
  *i += (*j + 1) >> 1;
  *h -= (*d + 1) >> 1;
  *d += (*h + 1) >> 1;
  *g -= (*c + 1) >> 1;
  *c += (*g + 1) >> 1;

  *p += *k;
  *o -= *l;
  const int halfP = *p >> 1;
  const int halfO = *o >> 1;
  *k -= halfP;
  *l += halfO;
  *k -= (3 * *l + 6) >> 3;
  *l += (3 * *k + 2) >> 2;
  *k -= (3 * *l + 4) >> 3;
  *l -= halfO;
  *k += halfP;
  *o += *l;
  *p -= *k;

  *a += *p;
  *p = (*a >> 1) - *p;
  *a += (3 * *p + 8) >> 4;
  *p += (3 * *a + 4) >> 5;
  *b += *o;
  *o = (*b >> 1) - *o;
  *b += (3 * *o + 8) >> 4;
  *o += (3 * *b + 4) >> 5;
  *e += *l;
  *l = (*e >> 1) - *l;
  *e += (3 * *l + 8) >> 4;
  *l += (3 * *e + 4) >> 5;
  *f += *k;
  *k = (*f >> 1) - *k;
  *f += (3 * *k + 8) >> 4;
  *k += (3 * *f + 4) >> 5;

  *d -= *m;
  *a += (3 * *p + 8) >> 4;
  *p -= *d >> 1;
  temporary = ((*a - *d) >> 1) - *m;
  *m = *p;
  *p = temporary;
  *a -= temporary;
  *d += *m;

  *c -= *n;
  *b += (3 * *o + 8) >> 4;
  *o -= *c >> 1;
  temporary = ((*b - *c) >> 1) - *n;
  *n = *o;
  *o = temporary;
  *b -= temporary;
  *c += *n;

  *h -= *i;
  *e += (3 * *l + 8) >> 4;
  *l -= *h >> 1;
  temporary = ((*e - *h) >> 1) - *i;
  *i = *l;
  *l = temporary;
  *e -= temporary;
  *h += *i;

  *g -= *j;
  *f += (3 * *k + 8) >> 4;
  *k -= *g >> 1;
  temporary = ((*f - *g) >> 1) - *j;
  *j = *k;
  *k = temporary;
  *f -= temporary;
  *g += *j;
}

void _jxr_4OverlapFilter(int* a, int* b, int* c, int* d) {
  *a += *d;
  *b += *c;
  *d -= (*a + 1) >> 1;
  *c -= (*b + 1) >> 1;
  *c -= (*d + 1) >> 1;
  *d += (*c + 1) >> 1;
  *d += (*a + 1) >> 1;
  *c += (*b + 1) >> 1;
  *a += ((3 * *d + 16) >> 5) - *d;
  *b += ((3 * *c + 16) >> 5) - *c;
  *d += (3 * *a + 8) >> 4;
  *c += (3 * *b + 8) >> 4;
  *a += (3 * *d + 16) >> 5;
  *b += (3 * *c + 16) >> 5;
}

void _jxr_2x2OverlapFilter(int* a, int* b, int* c, int* d) {
  *a += *d;
  *b += *c;
  *d -= (*a + 1) >> 1;
  *c -= (*b + 1) >> 1;
  *b += (*a + 2) >> 2;
  *a += (*b + 1) >> 1;
  *b += (*a + 2) >> 2;
  *d += (*a + 1) >> 1;
  *c += (*b + 1) >> 1;
  *a -= *d;
  *b -= *c;
}

void _jxr_2OverlapFilter(int* a, int* b) {
  *b += (*a + 4) >> 3;
  *a += (*b + 2) >> 2;
  *b += (*a + 4) >> 3;
}

std::uint8_t long_word_flag;

int _jxr_read_lwf_test_flag() { return long_word_flag; }

void _jxr_complete_cur_dclp(jxr_image* image, int tileX, int macroblockX,
                            int /*macroblockY*/) {
  const int dcMode =
      calculate_mbdc_mode(image, 0, tileX, macroblockX, image->cur_my);
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;

  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    macroblock_s& current = image->strip[channel].cur[column];
    const int left =
        macroblockX <= 0 ? 0 : image->strip[channel].cur[column - 1].data[0];
    const int above = image->strip[channel].up1[column].data[0];
    current.pred_dclp[0] = current.data[0];
    if (dcMode == 0) {
      current.data[0] += left;
    } else if (dcMode == 1) {
      current.data[0] += above;
    } else if (dcMode == 2) {
      int sum = left + above;
      if (channel > 0 &&
          (image->use_clr_fmt == 1 || image->use_clr_fmt == 2))
        ++sum;
      current.data[0] +=
          (sum & static_cast<int>(0x80000000U)) | (sum >> 1);
    }
  }

  int lpMode;
  if (dcMode == 0) {
    lpMode = image->strip[0].cur[column].lp_quant ==
                     image->strip[0].cur[column - 1].lp_quant
                 ? 0
                 : 2;
  } else if (dcMode == 1) {
    lpMode = image->strip[0].up1[column].lp_quant ==
                     image->strip[0].cur[column].lp_quant
                 ? 1
                 : 2;
  } else {
    lpMode = 2;
  }

  predict_lp444(image, tileX, macroblockX, 0, lpMode);
  for (unsigned int channel = 1; channel < image->num_channels; ++channel) {
    if (image->use_clr_fmt == 1) {
      predict_lp420(image, tileX, macroblockX, static_cast<int>(channel),
                    lpMode);
    } else if (image->use_clr_fmt == 2) {
      predict_lp422(image, tileX, macroblockX, static_cast<int>(channel),
                    lpMode, dcMode);
    } else {
      predict_lp444(image, tileX, macroblockX, static_cast<int>(channel),
                    lpMode);
    }
  }
}
