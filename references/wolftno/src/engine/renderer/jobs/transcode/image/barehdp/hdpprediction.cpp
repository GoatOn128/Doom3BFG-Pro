#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <cstdlib>

namespace {

int Absolute(std::int16_t value) {
  return std::abs(static_cast<int>(value));
}

int Absolute(int value) {
  return value < 0 ? -value : value;
}

}  // namespace

hdpError_t AllocatePredictionInfo(hdpCodec_t* codec) {
  const std::uint64_t channels = codec->codecParms.numChannels;
  const std::uint64_t width = codec->widthInMacroblocks;
  codec->predInfoMemory = static_cast<hdpPredictionInfo_t*>(TempAlloc(
      &codec->codecParms,
      2 * channels * width * sizeof(hdpPredictionInfo_t), 16));
  if (codec->predInfoMemory == nullptr)
    return HDP_ERROR_OUT_OF_MEMORY;
  hdpPredictionInfo_t* cursor = codec->predInfoMemory;
  for (std::uint64_t channel = 0; channel < channels; ++channel) {
    codec->predInfoCurRow[channel] = cursor;
    cursor += width;
    codec->predInfoPrevRow[channel] = cursor;
    cursor += width;
  }
  return HDP_ERROR_NONE;
}

void FreePredictionInfo(hdpCodec_t* codec) {
  if (codec->predInfoMemory != nullptr)
    codec->predInfoMemory = nullptr;
}

int GetPredictionModeHP(hdpCodec_t* codec, int channel) {
  const std::int16_t* coefficients = codec->MBInfo.compactDCLP[channel];
  int horizontal = Absolute(coefficients[1]) + Absolute(coefficients[2]) +
                   Absolute(coefficients[3]);
  int vertical = Absolute(coefficients[4]) + Absolute(coefficients[8]) +
                 Absolute(coefficients[12]);
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  if (format != CF_Y_ONLY && format != CF_N_CHANNEL) {
    horizontal += Absolute(codec->MBInfo.compactDCLP[1][1]) +
                  Absolute(codec->MBInfo.compactDCLP[2][1]);
    if (format == CF_YUV_420) {
      vertical += Absolute(codec->MBInfo.compactDCLP[1][2]) +
                  Absolute(codec->MBInfo.compactDCLP[2][2]);
    } else if (format == CF_YUV_422) {
      vertical += Absolute(codec->MBInfo.compactDCLP[1][2]) +
                  Absolute(codec->MBInfo.compactDCLP[1][6]) +
                  Absolute(codec->MBInfo.compactDCLP[2][2]) +
                  Absolute(codec->MBInfo.compactDCLP[2][6]);
      horizontal += Absolute(codec->MBInfo.compactDCLP[1][5]) +
                    Absolute(codec->MBInfo.compactDCLP[2][5]);
    } else {
      vertical += Absolute(codec->MBInfo.compactDCLP[1][4]) +
                  Absolute(codec->MBInfo.compactDCLP[2][4]);
    }
  }
  if (4 * horizontal < vertical)
    return 1;
  if (4 * vertical < horizontal)
    return 0;
  return 2;
}

int GetPredictionModeDCLP(hdpCodec_t* codec, int channel,
                          std::uint64_t macroblockX) {
  if (codec->atTileLeftMB && codec->atTileTopMB)
    return 11;
  if (codec->atTileLeftMB) {
    const int adMode = codec->MBInfo.quantizerIndexLP ==
                               codec->predInfoPrevRow[channel][macroblockX]
                                   .quantizerIndexLP
                           ? 1
                           : 2;
    return 1 + 4 * adMode;
  }
  if (codec->atTileTopMB) {
    const int adMode = codec->MBInfo.quantizerIndexLP ==
                               codec->predInfoCurRow[channel][macroblockX - 1]
                                   .quantizerIndexLP
                           ? 0
                           : 2;
    return 4 * adMode;
  }

  int horizontal = Absolute(
      codec->predInfoPrevRow[channel][macroblockX - 1].macroblockDC -
      codec->predInfoCurRow[channel][macroblockX - 1].macroblockDC);
  int vertical = Absolute(
      codec->predInfoPrevRow[channel][macroblockX - 1].macroblockDC -
      codec->predInfoPrevRow[channel][macroblockX].macroblockDC);
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  if (format != CF_Y_ONLY && format != CF_N_CHANNEL) {
    int weight = 2;
    if (format == CF_YUV_420)
      weight = 8;
    else if (format == CF_YUV_422)
      weight = 4;
    horizontal = weight * horizontal + Absolute(
        codec->predInfoPrevRow[1][macroblockX - 1].macroblockDC -
        codec->predInfoCurRow[1][macroblockX - 1].macroblockDC) + Absolute(
        codec->predInfoPrevRow[2][macroblockX - 1].macroblockDC -
        codec->predInfoCurRow[2][macroblockX - 1].macroblockDC);
    vertical = weight * vertical + Absolute(
        codec->predInfoPrevRow[1][macroblockX - 1].macroblockDC -
        codec->predInfoPrevRow[1][macroblockX].macroblockDC) + Absolute(
        codec->predInfoPrevRow[2][macroblockX - 1].macroblockDC -
        codec->predInfoPrevRow[2][macroblockX].macroblockDC);
  }

  if (4 * horizontal < vertical) {
    const int adMode = codec->MBInfo.quantizerIndexLP ==
                               codec->predInfoPrevRow[channel][macroblockX]
                                   .quantizerIndexLP
                           ? 1
                           : 2;
    return 1 + 4 * adMode;
  }
  if (4 * vertical < horizontal) {
    const int adMode = codec->MBInfo.quantizerIndexLP ==
                               codec->predInfoCurRow[channel][macroblockX - 1]
                                   .quantizerIndexLP
                           ? 0
                           : 2;
    return 4 * adMode;
  }
  return 10;
}

void UpdatePredictionInfo(hdpCodec_t* codec, std::uint64_t macroblockX) {
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  const std::uint64_t channels =
      format == CF_YUV_420 || format == CF_YUV_422
          ? 1
          : codec->codecParms.numChannels;
  for (std::uint64_t channel = 0; channel < channels; ++channel) {
    hdpPredictionInfo_t& prediction =
        codec->predInfoCurRow[channel][macroblockX];
    const std::int16_t* coefficients = codec->MBInfo.compactDCLP[channel];
    prediction.quantizerIndexLP = codec->MBInfo.quantizerIndexLP;
    prediction.macroblockDC = coefficients[0];
    prediction.macroblockLP[0] = coefficients[1];
    prediction.macroblockLP[1] = coefficients[2];
    prediction.macroblockLP[2] = coefficients[3];
    prediction.macroblockLP[3] = coefficients[4];
    prediction.macroblockLP[4] = coefficients[8];
    prediction.macroblockLP[5] = coefficients[12];
  }
}
