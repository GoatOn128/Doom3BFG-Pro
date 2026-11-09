#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr int kNumChromaBlocksForColorFormat[] = {
    0, 4, 8, 16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0,
};

}  // namespace

void* TempAlloc(hdpCodecParms_t* codecParms, std::uint64_t size,
                std::uint64_t alignment) {
  const auto base =
      reinterpret_cast<std::uint64_t>(codecParms->tempBuffer);
  const std::uint64_t mask = alignment - 1;
  const std::uint64_t baseMisalignment = mask & base;
  const std::uint64_t offset =
      (~mask & (baseMisalignment + codecParms->tempBufferUsed + mask)) -
      baseMisalignment;
  codecParms->tempBufferUsed = offset;
  if (offset + size > codecParms->tempBufferSize)
    return nullptr;
  codecParms->tempBufferUsed = offset + size;
  return reinterpret_cast<void*>(base + offset);
}

hdpError_t AllocateAdditionalBitIOs(hdpCodec_t* codec) {
  switch (codec->codecParms.subBands) {
    case SB_NO_LOWPASS:
      codec->numSubBands = 1;
      break;
    case SB_NO_HIGHPASS:
      codec->numSubBands = 2;
      break;
    case SB_NO_FLEXBITS:
      codec->numSubBands = 3;
      break;
    default:
      codec->numSubBands = 4;
      break;
  }

  std::uint64_t bitIOCount = 0;
  if (codec->codecParms.hasTileIndex) {
    bitIOCount = codec->codecParms.numTilesWideMinusOne + 1ULL;
    if (codec->codecParms.streamLayout != SL_SPATIAL)
      bitIOCount *= codec->numSubBands;
    if (bitIOCount != 0) {
      codec->tileBitIOs = static_cast<hdpBitIO_t*>(TempAlloc(
          &codec->codecParms, bitIOCount * sizeof(hdpBitIO_t), 16));
      if (codec->tileBitIOs == nullptr)
        return HDP_ERROR_OUT_OF_MEMORY;
      std::memset(codec->tileBitIOs, 0,
                  static_cast<std::size_t>(bitIOCount * sizeof(hdpBitIO_t)));

      const std::uint64_t tileCount =
          bitIOCount * (codec->codecParms.numTilesHighMinusOne + 1ULL);
      codec->tileIndex = static_cast<std::uint64_t*>(TempAlloc(
          &codec->codecParms, tileCount * sizeof(std::uint64_t), 16));
      if (codec->tileIndex == nullptr)
        return HDP_ERROR_OUT_OF_MEMORY;
    }
  }
  codec->numTileBitIOs = bitIOCount;
  return HDP_ERROR_NONE;
}

void UpdateTilePos(hdpCodec_t* codec, std::uint64_t macroblockX,
                   std::uint64_t macroblockY) {
  const std::uint64_t tileColumn = codec->currentTileColumn;
  const std::uint64_t tileStartX =
      codec->codecParms.tileMacroblockOffsetX[tileColumn];
  const std::uint64_t tileEndX =
      tileColumn >= codec->codecParms.numTilesWideMinusOne
          ? codec->widthInMacroblocks
          : codec->codecParms.tileMacroblockOffsetX[tileColumn + 1];
  const std::uint64_t tileRow = codec->currentTileRow;
  const std::uint64_t tileStartY =
      codec->codecParms.tileMacroblockOffsetY[tileRow];
  const std::uint64_t tileEndY =
      tileRow >= codec->codecParms.numTilesHighMinusOne
          ? codec->heightInMacroblocks
          : codec->codecParms.tileMacroblockOffsetY[tileRow + 1];

  if (macroblockX == 0)
    codec->currentTileColumn = 0;
  else if (macroblockX == tileEndX)
    codec->currentTileColumn = tileColumn + 1;
  if (macroblockY == 0)
    codec->currentTileRow = 0;
  else if (macroblockY == tileEndY)
    codec->currentTileRow = tileRow + 1;

  codec->atTileLeftMB = macroblockX == tileStartX;
  codec->atTileTopMB = macroblockY == tileStartY;
  codec->resetAdaptiveScanTotals =
      ((macroblockX - tileStartX) & 15ULL) == 0;
  codec->adaptContextHuffman = codec->resetAdaptiveScanTotals ||
                               macroblockX + 1 == tileEndX;
}

void InitMacroblockColumnPtrs(hdpCodec_t* firstCodec) {
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;
       codec = codec->nextCodec) {
    std::memcpy(codec->macroBlockColumnPtr0, codec->macroBlockRowBuffer0,
                sizeof(codec->macroBlockColumnPtr0));
    std::memcpy(codec->macroBlockColumnPtr1, codec->macroBlockRowBuffer1,
                sizeof(codec->macroBlockColumnPtr1));
  }
}

void AdvanceMacroblockColumnPtrs(hdpCodec_t* firstCodec) {
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;
       codec = codec->nextCodec) {
    codec->macroBlockPrevColumnPtr[0] = codec->macroBlockColumnPtr0[0];
    codec->macroBlockColumnPtr0[0] += 256;
    codec->macroBlockColumnPtr1[0] += 256;
    const int channelAdvance =
        16 * kNumChromaBlocksForColorFormat[codec->codecParms.colorFormat];
    for (std::uint64_t channel = 1;
         channel < codec->codecParms.numChannels; ++channel) {
      codec->macroBlockPrevColumnPtr[channel] =
          codec->macroBlockColumnPtr0[channel];
      codec->macroBlockColumnPtr0[channel] += channelAdvance;
      codec->macroBlockColumnPtr1[channel] += channelAdvance;
    }
  }
}

void SwapMacroblockRowPtrs(hdpCodec_t* firstCodec) {
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;
       codec = codec->nextCodec) {
    for (int channel = 0; channel < 16; ++channel)
      std::swap(codec->macroBlockRowBuffer0[channel],
                codec->macroBlockRowBuffer1[channel]);
  }
}

void SwapMacroblockPredPtrs(hdpCodec_t* firstCodec) {
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;
       codec = codec->nextCodec) {
    for (std::uint64_t channel = 0;
         channel < codec->codecParms.numChannels; ++channel) {
      std::swap(codec->predInfoCurRow[channel],
                codec->predInfoPrevRow[channel]);
    }
  }
}
