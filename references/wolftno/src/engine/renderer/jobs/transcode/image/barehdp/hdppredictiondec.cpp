#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>

namespace {

std::int16_t AddLow16(std::int16_t first, std::int16_t second) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(first) +
                                   static_cast<std::uint16_t>(second));
}

int SaturateCount(int value) {
  return std::clamp(value, -16, 15);
}

int CountOnes16(int value) {
  unsigned bits = static_cast<unsigned>(value) & 0xFFFFU;
  int count = 0;
  while (bits != 0) {
    count += static_cast<int>(bits & 1U);
    bits >>= 1;
  }
  return count;
}

}  // namespace

void PredictDCLPDec(hdpCodec_t* codec) {
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  const int channels = format == CF_YUV_420 || format == CF_YUV_422
                           ? 1
                           : static_cast<int>(codec->codecParms.numChannels);
  const std::uint64_t column = codec->currentMacroblockColumn;
  int packedMode = GetPredictionModeDCLP(codec, 0, column);
  for (int channel = 0; channel < channels; ++channel) {
    if (format == CF_N_CHANNEL && channel > 0)
      packedMode = GetPredictionModeDCLP(codec, channel, column);
    const int dcMode = packedMode & 3;
    const int lpMode = packedMode & 12;
    std::int16_t* coefficients = codec->MBInfo.compactDCLP[channel];
    if (dcMode == 1) {
      coefficients[0] = AddLow16(
          coefficients[0], codec->predInfoPrevRow[channel][column].macroblockDC);
    } else if (dcMode == 0) {
      coefficients[0] = AddLow16(
          coefficients[0], codec->predInfoCurRow[channel][column - 1].macroblockDC);
    } else if (dcMode == 2) {
      const int average =
          (codec->predInfoCurRow[channel][column - 1].macroblockDC +
           codec->predInfoPrevRow[channel][column].macroblockDC) >>
          1;
      coefficients[0] =
          AddLow16(coefficients[0], static_cast<std::int16_t>(average));
    }
    if (lpMode == 4) {
      const auto& top = codec->predInfoPrevRow[channel][column];
      coefficients[4] = AddLow16(coefficients[4], top.macroblockLP[3]);
      coefficients[8] = AddLow16(coefficients[8], top.macroblockLP[4]);
      coefficients[12] = AddLow16(coefficients[12], top.macroblockLP[5]);
    } else if (lpMode == 0) {
      const auto& left = codec->predInfoCurRow[channel][column - 1];
      coefficients[1] = AddLow16(coefficients[1], left.macroblockLP[0]);
      coefficients[2] = AddLow16(coefficients[2], left.macroblockLP[1]);
      coefficients[3] = AddLow16(coefficients[3], left.macroblockLP[2]);
    }
  }
  for (int channel = 0; channel < channels; ++channel) {
    const int mode = format == CF_N_CHANNEL && channel > 0
                         ? GetPredictionModeHP(codec, channel)
                         : GetPredictionModeHP(codec, 0);
    codec->MBInfo.predOrientationHP[channel] = 2 - mode;
  }
}

void PredictHPDec(hdpCodec_t* codec) {
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  const std::uint64_t channels =
      format == CF_YUV_420 || format == CF_YUV_422
          ? 1
          : codec->codecParms.numChannels;
  for (std::uint64_t channel = 0; channel < channels; ++channel) {
    std::int16_t* coefficients = codec->macroBlockColumnPtr1[channel];
    const int mode = 2 - codec->MBInfo.predOrientationHP[channel];
    if (mode == 1) {
      for (int group = 0; group < 4; ++group) {
        const int base = 2 + group * 64;
        std::int16_t at0 = coefficients[base];
        std::int16_t at7 = coefficients[base + 7];
        std::int16_t at8 = coefficients[base + 8];
        for (int step = 1; step < 4; ++step) {
          const int target = base + step * 16;
          at0 = AddLow16(at0, coefficients[target]);
          at7 = AddLow16(at7, coefficients[target + 7]);
          at8 = AddLow16(at8, coefficients[target + 8]);
          coefficients[target] = at0;
          coefficients[target + 7] = at7;
          coefficients[target + 8] = at8;
        }
      }
    } else if (mode == 0) {
      for (int group = 0; group < 4; ++group) {
        const int base = 1 + group * 16;
        std::int16_t at0 = coefficients[base];
        std::int16_t at4 = coefficients[base + 4];
        std::int16_t at5 = coefficients[base + 5];
        for (int step = 1; step < 4; ++step) {
          const int target = base + step * 64;
          at0 = AddLow16(at0, coefficients[target]);
          at4 = AddLow16(at4, coefficients[target + 4]);
          at5 = AddLow16(at5, coefficients[target + 5]);
          coefficients[target] = at0;
          coefficients[target + 4] = at4;
          coefficients[target + 5] = at5;
        }
      }
    }
  }
}

void PredictHPCBPDec(hdpCodec_t* codec, hdpContext_t* context,
                     int macroblockX, int) {
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  const int channels = format == CF_YUV_420 || format == CF_YUV_422
                           ? 1
                           : static_cast<int>(codec->codecParms.numChannels);
  for (int channel = 0; channel < channels; ++channel) {
    int codedBlockPattern = codec->MBInfo.residualCBP[channel];
    const int state = context->adaptCBPModelHP.state[channel];
    if (state == 0) {
      if (codec->atTileLeftMB) {
        if (codec->atTileTopMB) {
          codedBlockPattern ^= 1;
        } else {
          codedBlockPattern ^=
              (codec->predInfoPrevRow[channel][macroblockX].macroblockHPCBP >>
               10) &
              1;
        }
      } else {
        codedBlockPattern ^=
            (codec->predInfoCurRow[channel][macroblockX - 1].macroblockHPCBP >>
             5) &
            1;
      }
      codedBlockPattern ^= 0x02 & (codedBlockPattern << 1);
      codedBlockPattern ^= 0x10 & (codedBlockPattern << 3);
      codedBlockPattern ^= 0x20 & (codedBlockPattern << 1);
      codedBlockPattern ^= (codedBlockPattern & 0x33) << 2;
      codedBlockPattern ^= (codedBlockPattern & 0xCC) << 6;
      codedBlockPattern ^= (codedBlockPattern & 0x3300) << 2;
    } else if (state == 2) {
      codedBlockPattern ^= 0xFFFF;
    }

    const int ones = CountOnes16(codedBlockPattern);
    int& countOnes = context->adaptCBPModelHP.countOnes[channel];
    int& countZeros = context->adaptCBPModelHP.countZeros[channel];
    countOnes = SaturateCount(countOnes + ones - 3);
    countZeros = SaturateCount(countZeros + 13 - ones);
    if (countOnes < 0)
      context->adaptCBPModelHP.state[channel] =
          countOnes < countZeros ? 1 : 2;
    else if (countZeros < 0)
      context->adaptCBPModelHP.state[channel] = 2;
    else
      context->adaptCBPModelHP.state[channel] = 0;

    codec->MBInfo.actualCBP[channel] = codedBlockPattern;
    codec->predInfoCurRow[channel][macroblockX].macroblockHPCBP =
        codedBlockPattern;
  }
}
