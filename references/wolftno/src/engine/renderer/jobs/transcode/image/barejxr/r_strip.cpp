#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

void dclphp_shuffle(int* data, int dclpCount);
void unblock_shuffle444(int* data);
void yuv444_to_rgb(jxr_image* image, int macroblockX);
void IPCT_level1_up1(jxr_image* image, int macroblockY, int channel);
void IPCT_level2_up2(jxr_image* image, int channel, int unused);
void overlap_level1_up2_444(jxr_image* image, int macroblockY, int channel);
void overlap_level1_up2_422(jxr_image* image, int macroblockY, int channel);
void overlap_level1_up2_420(jxr_image* image, int macroblockY, int channel);
void overlap_level2_up3_444(jxr_image* image, int macroblockY, int channel);
void overlap_level2_up3_422(jxr_image* image, int macroblockY, int channel);
void overlap_level2_up3_420(jxr_image* image, int macroblockY, int channel);
int _jxr_rcollect_tile_mb_rows(jxr_image* image, int tileX, int tileY,
                               int macroblockY);
void scale_and_emit_mb(jxr_image* image, int macroblockY, int macroblockX);
void shift_and_clip_regular(jxr_image* image, int macroblockX, int bias,
                            char shiftBits, char scale, int round, int clipLow,
                            int clipHigh);

namespace {

int OutputScale(const jxr_image* image) {
  return (image->component_mode & 1U) != 0 ? 3 : 0;
}

const int* OutputPlane(const jxr_image* image, int channel, int macroblockX) {
  return image->strip[channel].up3[macroblockX].data;
}

int StoredCoefficientCount(const jxr_image* image, unsigned int channel) {
  if (channel == 0)
    return 256;
  if (image->use_clr_fmt == 1)
    return 76;
  if (image->use_clr_fmt == 2)
    return 136;
  return 256;
}

void CopyStoredMacroblock(macroblock_s& destination,
                          const macroblock_s& source, int coefficientCount) {
  destination.lp_quant = source.lp_quant;
  destination.hp_quant = source.hp_quant;
  std::memcpy(destination.data, source.data,
              static_cast<std::size_t>(coefficientCount) * sizeof(int));
}

int* BlockSample(int* data, int x, int y, int blocksPerRow) {
  const int block = (y / 4) * blocksPerRow + x / 4;
  return data + block * 16 + 4 * (y % 4) + x % 4;
}

template <typename Sample>
void ApplyOverlap4(Sample sample) {
  _jxr_4OverlapFilter(sample(0), sample(1), sample(2), sample(3));
}

template <typename Sample>
void ApplyOverlap4x4(Sample sample) {
  _jxr_4x4OverlapFilter(
      sample(0, 0), sample(1, 0), sample(2, 0), sample(3, 0),
      sample(0, 1), sample(1, 1), sample(2, 1), sample(3, 1),
      sample(0, 2), sample(1, 2), sample(2, 2), sample(3, 2),
      sample(0, 3), sample(1, 3), sample(2, 3), sample(3, 3));
}

int* HorizontalJoinSample(int* left, int* right, int index, int y,
                          int width, int blocksPerRow) {
  return index < 2
             ? BlockSample(left, width - 2 + index, y, blocksPerRow)
             : BlockSample(right, index - 2, y, blocksPerRow);
}

int* VerticalJoinSample(int* top, int* bottom, int x, int index, int height,
                        int blocksPerRow) {
  return index < 2
             ? BlockSample(top, x, height - 2 + index, blocksPerRow)
             : BlockSample(bottom, x, index - 2, blocksPerRow);
}

int* FourWayJoinSample(int* topLeft, int* topRight, int* bottomLeft,
                       int* bottomRight, int x, int y, int width, int height,
                       int blocksPerRow) {
  int* rowLeft = y < 2 ? topLeft : bottomLeft;
  int* rowRight = y < 2 ? topRight : bottomRight;
  const int localY = y < 2 ? height - 2 + y : y - 2;
  return x < 2
             ? BlockSample(rowLeft, width - 2 + x, localY, blocksPerRow)
             : BlockSample(rowRight, x - 2, localY, blocksPerRow);
}

int* StripData(jxr_image* image, int channel, bool up2, unsigned int tileX,
               unsigned int localX) {
  const unsigned int column =
      image->tile_column_position[tileX] + localX;
  return (up2 ? image->strip[channel].up2[column]
              : image->strip[channel].up3[column])
      .data;
}

constexpr int kInterpolationWeights[5][4] = {
    {4, 4, 0, 8}, {5, 3, 1, 7}, {6, 2, 2, 6},
    {7, 1, 3, 5}, {8, 0, 4, 4}};

void Upsample(const int* input, int* output, int outputLength,
              int chromaCenter) {
  if (chromaCenter >= 5 && chromaCenter <= 7)
    chromaCenter = 0;
  const int* weights = kInterpolationWeights[chromaCenter];
  for (int k = 0; k <= (outputLength - 2) / 2; ++k) {
    output[2 * k + 1] =
        (weights[0] * input[k + 1] + weights[1] * input[k + 2] + 4) >> 3;
  }
  for (int k = -1; k <= (outputLength - 4) / 2; ++k) {
    output[2 * k + 2] =
        (weights[2] * input[k + 1] + weights[3] * input[k + 2] + 4) >> 3;
  }
}

}  // namespace

void dequantize_up_dclp(jxr_image* image, int macroblockY, int channel) {
  int coefficientCount = 16;
  if (channel > 0) {
    if (image->use_clr_fmt == 2)
      coefficientCount = 8;
    else if (image->use_clr_fmt == 1)
      coefficientCount = 4;
  }

  unsigned int tileRow = 0;
  const unsigned int row = static_cast<unsigned int>(macroblockY - 1);
  for (; tileRow < image->tile_rows; ++tileRow) {
    const unsigned int start = image->tile_row_position[tileRow];
    if (row >= start && row < start + image->tile_row_height[tileRow])
      break;
  }
  if (tileRow >= image->tile_rows)
    tileRow = 0;

  for (unsigned int tileColumn = 0; tileColumn < image->tile_columns;
       ++tileColumn) {
    const jxr_tile_qp& tile =
        image->tile_quant[tileColumn + tileRow * image->tile_columns];
    const int dcQuantizer =
        (image->component_mode & 2U) != 0
            ? image->dc_quant_ch[channel]
            : tile.dc_quant_ch[channel];
    const int mappedDc = _jxr_quant_map(image, dcQuantizer, 1);
    for (unsigned int localX = 0;
         localX < image->tile_column_width[tileColumn]; ++localX) {
      const unsigned int column =
          localX + image->tile_column_position[tileColumn];
      macroblock_s& macroblock = image->strip[channel].up1[column];
      const int lpQuantizer =
          (image->component_mode & 8U) != 0
              ? image->lp_quant_ch[channel][macroblock.lp_quant]
              : tile.lp_quant_ch[channel][macroblock.lp_quant];
      const int mappedLp = _jxr_quant_map(image, lpQuantizer, 1);
      macroblock.data[0] *= mappedDc;
      for (int coefficient = 1; coefficient < coefficientCount; ++coefficient)
        macroblock.data[coefficient] *= mappedLp;
    }
  }
}

void IPCT_level1_up1(jxr_image* image, int macroblockY, int channel) {
  dequantize_up_dclp(image, macroblockY, channel);
  const int macroblockCount = static_cast<int>(image->extended_width >> 4);
  for (int macroblockX = 0; macroblockX < macroblockCount; ++macroblockX) {
    int* data = image->strip[channel].up1[macroblockX].data;
    if (channel > 0 && image->use_clr_fmt == 1) {
      _jxr_2x2IPCT(data);
      _jxr_InvPermute2pt(data + 1, data + 2);
    } else if (channel > 0 && image->use_clr_fmt == 2) {
      _jxr_2ptT(data, data + 4);
      _jxr_2x2IPCT(data);
      _jxr_2x2IPCT(data + 4);
      _jxr_InvPermute2pt(data + 1, data + 2);
      _jxr_InvPermute2pt(data + 5, data + 6);
    } else {
      _jxr_4x4IPCT(data);
    }
  }
}

void IPCT_level2_up2(jxr_image* image, int channel, int) {
  int dclpCount = 16;
  if (channel > 0) {
    if (image->use_clr_fmt == 2)
      dclpCount = 8;
    else if (image->use_clr_fmt == 1)
      dclpCount = 4;
  }
  const int coefficientCount = 16 * dclpCount;
  const int macroblockCount = static_cast<int>(image->extended_width >> 4);
  for (int macroblockX = 0; macroblockX < macroblockCount; ++macroblockX) {
    macroblock_s& macroblock = image->strip[channel].up2[macroblockX];
    dclphp_shuffle(macroblock.data, dclpCount);
    const int quantizer = _jxr_quant_map(image, macroblock.hp_quant, 1);
    for (int coefficient = 0; coefficient < coefficientCount; ++coefficient)
      macroblock.data[coefficient] *= quantizer;
    for (int coefficient = 0; coefficient < coefficientCount;
         coefficient += 16) {
      _jxr_4x4IPCT(macroblock.data + coefficient);
    }
  }
}

void overlap_level1_up2_444(jxr_image* image, int macroblockY, int channel) {
  const unsigned int topRow = static_cast<unsigned int>(macroblockY - 2);
  unsigned int tileRow = 0;
  while (topRow > image->tile_row_position[tileRow] +
                      image->tile_row_height[tileRow] - 1) {
    ++tileRow;
  }
  const unsigned int width = image->extended_width >> 4;
  const unsigned int height = image->extended_height >> 4;
  const bool topImage = topRow == 0;
  const bool bottomImage = topRow + 1 == height;
  const bool tileTop = topRow == image->tile_row_position[tileRow];
  const bool tileBottom =
      topRow == image->tile_row_position[tileRow] +
                    image->tile_row_height[tileRow] - 1;

  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    const unsigned int start = image->tile_column_position[tileX];
    const unsigned int count = image->tile_column_width[tileX];
    if (topImage || (image->disableTileOverlapFlag != 0 && tileTop)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        if ((start + localX > 0 && image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX != 0)) {
          int* current = image->strip[channel].up2[start + localX].data;
          int* left = image->strip[channel].up2[start + localX - 1].data;
          _jxr_4OverlapFilter(left + 2, left + 3, current, current + 1);
          _jxr_4OverlapFilter(left + 6, left + 7, current + 4, current + 5);
        }
      }
      if (tileX == 0 || image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start].data;
        _jxr_4OverlapFilter(data, data + 1, data + 4, data + 5);
      }
      if (tileX + 1 == image->tile_columns ||
          image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start + count - 1].data;
        _jxr_4OverlapFilter(data + 2, data + 3, data + 6, data + 7);
      }
    }

    if (bottomImage || (image->disableTileOverlapFlag != 0 && tileBottom)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        if ((start + localX > 0 && image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX != 0)) {
          int* current = image->strip[channel].up2[start + localX].data;
          int* left = image->strip[channel].up2[start + localX - 1].data;
          _jxr_4OverlapFilter(left + 10, left + 11, current + 8,
                              current + 9);
          _jxr_4OverlapFilter(left + 14, left + 15, current + 12,
                              current + 13);
        }
      }
      if (tileX == 0 || image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start].data;
        _jxr_4OverlapFilter(data + 8, data + 9, data + 12, data + 13);
      }
      if (tileX + 1 == image->tile_columns ||
          image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start + count - 1].data;
        _jxr_4OverlapFilter(data + 10, data + 11, data + 14, data + 15);
      }
    }

    for (unsigned int localX = 0; localX < count && topRow + 1 < height;
         ++localX) {
      const unsigned int column = start + localX;
      if ((tileX == 0 && localX == 0 &&
           image->disableTileOverlapFlag == 0) ||
          (image->disableTileOverlapFlag != 0 && localX == 0 &&
           !tileBottom)) {
        int* top = image->strip[channel].up2[start].data;
        int* bottom = image->strip[channel].up1[start].data;
        _jxr_4OverlapFilter(top + 8, top + 12, bottom, bottom + 4);
        _jxr_4OverlapFilter(top + 9, top + 13, bottom + 1, bottom + 5);
      }
      if ((column + 1 < width && image->disableTileOverlapFlag == 0) ||
          (image->disableTileOverlapFlag != 0 && localX + 1 < count &&
           !tileBottom)) {
        int* topLeft = image->strip[channel].up2[column].data;
        int* topRight = image->strip[channel].up2[column + 1].data;
        int* bottomLeft = image->strip[channel].up1[column].data;
        int* bottomRight = image->strip[channel].up1[column + 1].data;
        _jxr_4x4OverlapFilter(
            topLeft + 10, topLeft + 11, topRight + 8, topRight + 9,
            topLeft + 14, topLeft + 15, topRight + 12, topRight + 13,
            bottomLeft + 2, bottomLeft + 3, bottomRight, bottomRight + 1,
            bottomLeft + 6, bottomLeft + 7, bottomRight + 4,
            bottomRight + 5);
      }
      if ((column + 1 == width && image->disableTileOverlapFlag == 0) ||
          (image->disableTileOverlapFlag != 0 && localX + 1 == count &&
           !tileBottom)) {
        int* top = image->strip[channel].up2[start + count - 1].data;
        int* bottom = image->strip[channel].up1[start + count - 1].data;
        _jxr_4OverlapFilter(top + 10, top + 14, bottom + 2, bottom + 6);
        _jxr_4OverlapFilter(top + 11, top + 15, bottom + 3, bottom + 7);
      }
    }
  }
}

void overlap_level1_up2_422(jxr_image* image, int macroblockY, int channel) {
  const unsigned int topRow = static_cast<unsigned int>(macroblockY - 2);
  unsigned int tileRow = 0;
  while (topRow > image->tile_row_position[tileRow] +
                      image->tile_row_height[tileRow] - 1)
    ++tileRow;
  const unsigned int width = image->extended_width >> 4;
  const unsigned int height = image->extended_height >> 4;
  const bool topEdge = topRow == 0 ||
                       (image->disableTileOverlapFlag != 0 &&
                        topRow == image->tile_row_position[tileRow]);
  const bool bottomEdge =
      topRow + 1 == height ||
      (image->disableTileOverlapFlag != 0 &&
       topRow == image->tile_row_position[tileRow] +
                     image->tile_row_height[tileRow] - 1);

  if (topEdge || bottomEdge) {
    for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
      const unsigned int start = image->tile_column_position[tileX];
      const unsigned int count = image->tile_column_width[tileX];
      if (tileX == 0 || image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start].data;
        if (topEdge)
          data[0] -= data[1];
        if (bottomEdge)
          data[6] -= data[7];
      }
      if (tileX + 1 == image->tile_columns ||
          image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start + count - 1].data;
        if (topEdge)
          data[1] -= data[0];
        if (bottomEdge)
          data[7] -= data[6];
      }
    }
  }

  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    const unsigned int start = image->tile_column_position[tileX];
    const unsigned int count = image->tile_column_width[tileX];
    if (tileX == 0 || image->disableTileOverlapFlag != 0)
      _jxr_2OverlapFilter(image->strip[channel].up2[start].data + 2,
                          image->strip[channel].up2[start].data + 4);
    if (tileX + 1 == image->tile_columns ||
        image->disableTileOverlapFlag != 0) {
      int* data = image->strip[channel].up2[start + count - 1].data;
      _jxr_2OverlapFilter(data + 3, data + 5);
    }
    for (unsigned int localX = 0; localX < count; ++localX) {
      const unsigned int column = start + localX;
      if (topEdge &&
          ((column > 0 && image->disableTileOverlapFlag == 0) ||
           (image->disableTileOverlapFlag != 0 && localX != 0))) {
        _jxr_2OverlapFilter(image->strip[channel].up2[column - 1].data + 1,
                            image->strip[channel].up2[column].data);
      }
      if (bottomEdge &&
          ((column > 0 && image->disableTileOverlapFlag == 0) ||
           (image->disableTileOverlapFlag != 0 && localX != 0))) {
        _jxr_2OverlapFilter(image->strip[channel].up2[column - 1].data + 7,
                            image->strip[channel].up2[column].data + 6);
      }
      if (topRow + 1 < height) {
        const bool tileBottom =
            topRow == image->tile_row_position[tileRow] +
                          image->tile_row_height[tileRow] - 1;
        if ((tileX == 0 && localX == 0 &&
             image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX == 0 &&
             !tileBottom)) {
          _jxr_2OverlapFilter(image->strip[channel].up2[start].data + 6,
                              image->strip[channel].up1[start].data);
        }
        if ((column + 1 == width && image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX + 1 == count &&
             !tileBottom)) {
          _jxr_2OverlapFilter(
              image->strip[channel].up2[start + count - 1].data + 7,
              image->strip[channel].up1[start + count - 1].data + 1);
        }
        if ((column + 1 < width && image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX + 1 < count &&
             !tileBottom)) {
          _jxr_2x2OverlapFilter(
              image->strip[channel].up2[column].data + 7,
              image->strip[channel].up2[column + 1].data + 6,
              image->strip[channel].up1[column].data + 1,
              image->strip[channel].up1[column + 1].data);
        }
      }
      if ((column + 1 < width && image->disableTileOverlapFlag == 0) ||
          (image->disableTileOverlapFlag != 0 && localX + 1 < count)) {
        _jxr_2x2OverlapFilter(
            image->strip[channel].up2[column].data + 3,
            image->strip[channel].up2[column + 1].data + 2,
            image->strip[channel].up2[column].data + 5,
            image->strip[channel].up2[column + 1].data + 4);
      }
    }
  }

  if (topEdge || bottomEdge) {
    for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
      const unsigned int start = image->tile_column_position[tileX];
      const unsigned int count = image->tile_column_width[tileX];
      if (tileX == 0 || image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start].data;
        if (topEdge)
          data[0] += data[1];
        if (bottomEdge)
          data[6] += data[7];
      }
      if (tileX + 1 == image->tile_columns ||
          image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start + count - 1].data;
        if (topEdge)
          data[1] += data[0];
        if (bottomEdge)
          data[7] += data[6];
      }
    }
  }
}

void overlap_level1_up2_420(jxr_image* image, int macroblockY, int channel) {
  const unsigned int topRow = static_cast<unsigned int>(macroblockY - 2);
  unsigned int tileRow = 0;
  while (topRow > image->tile_row_position[tileRow] +
                      image->tile_row_height[tileRow] - 1)
    ++tileRow;
  const unsigned int width = image->extended_width >> 4;
  const unsigned int height = image->extended_height >> 4;
  const bool tileTop = topRow == image->tile_row_position[tileRow];
  const bool tileBottom =
      topRow == image->tile_row_position[tileRow] +
                    image->tile_row_height[tileRow] - 1;
  const bool topEdge =
      topRow == 0 || (image->disableTileOverlapFlag != 0 && tileTop);
  const bool bottomEdge =
      topRow + 1 == height ||
      (image->disableTileOverlapFlag != 0 && tileBottom);

  if (topEdge || bottomEdge) {
    for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
      const unsigned int start = image->tile_column_position[tileX];
      const unsigned int count = image->tile_column_width[tileX];
      if (tileX == 0 || image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start].data;
        if (topEdge)
          data[0] -= data[1];
        if (bottomEdge)
          data[2] -= data[3];
      }
      if (tileX + 1 == image->tile_columns ||
          image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start + count - 1].data;
        if (topEdge)
          data[1] -= data[0];
        if (bottomEdge)
          data[3] -= data[2];
      }
    }
  }

  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    const unsigned int start = image->tile_column_position[tileX];
    const unsigned int count = image->tile_column_width[tileX];
    for (unsigned int localX = 0; localX < count; ++localX) {
      const unsigned int column = start + localX;
      if (topEdge &&
          ((column > 0 && image->disableTileOverlapFlag == 0) ||
           (image->disableTileOverlapFlag != 0 && localX != 0))) {
        _jxr_2OverlapFilter(image->strip[channel].up2[column - 1].data + 1,
                            image->strip[channel].up2[column].data);
      }
      if (bottomEdge &&
          ((column > 0 && image->disableTileOverlapFlag == 0) ||
           (image->disableTileOverlapFlag != 0 && localX != 0))) {
        _jxr_2OverlapFilter(image->strip[channel].up2[column - 1].data + 3,
                            image->strip[channel].up2[column].data + 2);
      }
      if (!bottomEdge) {
        if ((tileX == 0 && localX == 0 &&
             image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX == 0 &&
             !tileBottom)) {
          _jxr_2OverlapFilter(image->strip[channel].up2[start].data + 2,
                              image->strip[channel].up1[start].data);
        }
        if ((column + 1 == width && image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX + 1 == count &&
             !tileBottom)) {
          _jxr_2OverlapFilter(
              image->strip[channel].up2[start + count - 1].data + 3,
              image->strip[channel].up1[start + count - 1].data + 1);
        }
        if ((column + 1 < width && image->disableTileOverlapFlag == 0) ||
            (image->disableTileOverlapFlag != 0 && localX + 1 < count &&
             !tileBottom)) {
          _jxr_2x2OverlapFilter(
              image->strip[channel].up2[column].data + 3,
              image->strip[channel].up2[column + 1].data + 2,
              image->strip[channel].up1[column].data + 1,
              image->strip[channel].up1[column + 1].data);
        }
      }
    }
  }

  if (topEdge || bottomEdge) {
    for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
      const unsigned int start = image->tile_column_position[tileX];
      const unsigned int count = image->tile_column_width[tileX];
      if (tileX == 0 || image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start].data;
        if (topEdge)
          data[0] += data[1];
        if (bottomEdge)
          data[2] += data[3];
      }
      if (tileX + 1 == image->tile_columns ||
          image->disableTileOverlapFlag != 0) {
        int* data = image->strip[channel].up2[start + count - 1].data;
        if (topEdge)
          data[1] += data[0];
        if (bottomEdge)
          data[3] += data[2];
      }
    }
  }
}

void overlap_level2_up3_444(jxr_image* image, int macroblockY, int channel) {
  const unsigned int topRow = static_cast<unsigned int>(macroblockY - 3);
  unsigned int tileRow = 0;
  while (topRow > image->tile_row_position[tileRow] +
                      image->tile_row_height[tileRow] - 1) {
    ++tileRow;
  }
  const unsigned int imageWidth = image->extended_width >> 4;
  const unsigned int imageHeight = image->extended_height >> 4;
  const bool hardTiles = image->disableTileOverlapFlag != 0;
  const bool tileTop = topRow == image->tile_row_position[tileRow];
  const bool tileBottom =
      topRow == image->tile_row_position[tileRow] +
                    image->tile_row_height[tileRow] - 1;

  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    const unsigned int count = image->tile_column_width[tileX];
    const unsigned int start = image->tile_column_position[tileX];

    if (tileX == 0 || hardTiles) {
      int* data = StripData(image, channel, false, tileX, 0);
      for (int y = 2; y < 14; y += 4) {
        for (int x = 0; x < 2; ++x) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, x, y + i, 4);
          });
        }
      }
    }
    if (tileX + 1 == image->tile_columns || hardTiles) {
      int* data = StripData(image, channel, false, tileX, count - 1);
      for (int y = 2; y < 14; y += 4) {
        for (int x = 14; x < 16; ++x) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, x, y + i, 4);
          });
        }
      }
    }

    if (topRow == 0 || (hardTiles && tileTop)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        int* data = StripData(image, channel, false, tileX, localX);
        for (int y = 0; y < 2; ++y) {
          for (int x = 2; x < 14; x += 4) {
            ApplyOverlap4([&](int i) {
              return BlockSample(data, x + i, y, 4);
            });
          }
        }
        if ((start + localX > 0 && !hardTiles) ||
            (hardTiles && localX != 0)) {
          int* previous = StripData(image, channel, false, tileX, localX - 1);
          for (int y = 0; y < 2; ++y) {
            ApplyOverlap4([&](int i) {
              return HorizontalJoinSample(previous, data, i, y, 16, 4);
            });
          }
        }
      }
      if (tileX == 0 || hardTiles) {
        int* data = StripData(image, channel, false, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, i % 2, i / 2, 4);
        });
      }
      if (tileX + 1 == image->tile_columns || hardTiles) {
        int* data = StripData(image, channel, false, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, 14 + i % 2, i / 2, 4);
        });
      }
    }

    if (topRow + 1 == imageHeight || (hardTiles && tileBottom)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        int* data = StripData(image, channel, false, tileX, localX);
        for (int y = 14; y < 16; ++y) {
          for (int x = 2; x < 14; x += 4) {
            ApplyOverlap4([&](int i) {
              return BlockSample(data, x + i, y, 4);
            });
          }
        }
        if ((start + localX > 0 && !hardTiles) ||
            (hardTiles && localX != 0)) {
          int* previous = StripData(image, channel, false, tileX, localX - 1);
          for (int y = 14; y < 16; ++y) {
            ApplyOverlap4([&](int i) {
              return HorizontalJoinSample(previous, data, i, y, 16, 4);
            });
          }
        }
      }
      if (tileX == 0 || hardTiles) {
        int* data = StripData(image, channel, false, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, i % 2, 14 + i / 2, 4);
        });
      }
      if (tileX + 1 == image->tile_columns || hardTiles) {
        int* data = StripData(image, channel, false, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, 14 + i % 2, 14 + i / 2, 4);
        });
      }
    }

    for (unsigned int localX = 0; localX < count; ++localX) {
      int* data = StripData(image, channel, false, tileX, localX);
      for (int y = 2; y < 14; y += 4) {
        for (int x = 2; x < 14; x += 4) {
          ApplyOverlap4x4([&](int dx, int dy) {
            return BlockSample(data, x + dx, y + dy, 4);
          });
        }
        if ((start + localX + 1 < imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 < count)) {
          int* next = StripData(image, channel, false, tileX, localX + 1);
          ApplyOverlap4x4([&](int x, int dy) {
            return HorizontalJoinSample(data, next, x, y + dy, 16, 4);
          });
        }
      }

      if (topRow + 1 < imageHeight) {
        int* below = StripData(image, channel, true, tileX, localX);
        if ((tileX == 0 && localX == 0 && !hardTiles) ||
            (hardTiles && localX == 0 && !tileBottom)) {
          for (int x = 0; x < 2; ++x) {
            ApplyOverlap4([&](int y) {
              return VerticalJoinSample(data, below, x, y, 16, 4);
            });
          }
        }
        if (!hardTiles || !tileBottom) {
          for (int x = 2; x < 14; x += 4) {
            ApplyOverlap4x4([&](int dx, int y) {
              return VerticalJoinSample(data, below, x + dx, y, 16, 4);
            });
          }
        }
        if ((start + localX + 1 < imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 < count && !tileBottom)) {
          int* dataNext =
              StripData(image, channel, false, tileX, localX + 1);
          int* belowNext =
              StripData(image, channel, true, tileX, localX + 1);
          ApplyOverlap4x4([&](int x, int y) {
            return FourWayJoinSample(data, dataNext, below, belowNext, x, y,
                                     16, 16, 4);
          });
        }
        if ((start + localX + 1 == imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 == count && !tileBottom)) {
          for (int x = 14; x < 16; ++x) {
            ApplyOverlap4([&](int y) {
              return VerticalJoinSample(data, below, x, y, 16, 4);
            });
          }
        }
      }
    }
  }
}

void overlap_level2_up3_422(jxr_image* image, int macroblockY, int channel) {
  const unsigned int topRow = static_cast<unsigned int>(macroblockY - 3);
  unsigned int tileRow = 0;
  while (topRow > image->tile_row_position[tileRow] +
                      image->tile_row_height[tileRow] - 1) {
    ++tileRow;
  }
  const unsigned int imageWidth = image->extended_width >> 4;
  const unsigned int imageHeight = image->extended_height >> 4;
  const bool hardTiles = image->disableTileOverlapFlag != 0;
  const bool tileTop = topRow == image->tile_row_position[tileRow];
  const bool tileBottom =
      topRow == image->tile_row_position[tileRow] +
                    image->tile_row_height[tileRow] - 1;

  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    const unsigned int count = image->tile_column_width[tileX];
    const unsigned int start = image->tile_column_position[tileX];

    if (tileX == 0 || hardTiles) {
      int* data = StripData(image, channel, false, tileX, 0);
      for (int x = 0; x < 2; ++x) {
        for (int y = 2; y < 14; y += 4) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, x, y + i, 2);
          });
        }
      }
    }
    if (tileX + 1 == image->tile_columns || hardTiles) {
      int* data = StripData(image, channel, false, tileX, count - 1);
      for (int x = 6; x < 8; ++x) {
        for (int y = 2; y < 14; y += 4) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, x, y + i, 2);
          });
        }
      }
    }

    if (topRow == 0 || (hardTiles && tileTop)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        int* data = StripData(image, channel, false, tileX, localX);
        for (int y = 0; y < 2; ++y) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, 2 + i, y, 2);
          });
        }
        if ((start + localX > 0 && !hardTiles) ||
            (hardTiles && localX != 0)) {
          int* previous = StripData(image, channel, false, tileX, localX - 1);
          for (int y = 0; y < 2; ++y) {
            ApplyOverlap4([&](int i) {
              return HorizontalJoinSample(previous, data, i, y, 8, 2);
            });
          }
        }
      }
      if (tileX == 0 || hardTiles) {
        int* data = StripData(image, channel, false, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, i % 2, i / 2, 2);
        });
      }
      if (tileX + 1 == image->tile_columns || hardTiles) {
        int* data = StripData(image, channel, false, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, 6 + i % 2, i / 2, 2);
        });
      }
    }

    if (topRow + 1 == imageHeight || (hardTiles && tileBottom)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        int* data = StripData(image, channel, false, tileX, localX);
        for (int y = 14; y < 16; ++y) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, 2 + i, y, 2);
          });
        }
        if ((start + localX > 0 && !hardTiles) ||
            (hardTiles && localX != 0)) {
          int* previous = StripData(image, channel, false, tileX, localX - 1);
          for (int y = 14; y < 16; ++y) {
            ApplyOverlap4([&](int i) {
              return HorizontalJoinSample(previous, data, i, y, 8, 2);
            });
          }
        }
      }
      if (tileX == 0 || hardTiles) {
        int* data = StripData(image, channel, false, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, i % 2, 14 + i / 2, 2);
        });
      }
      if (tileX + 1 == image->tile_columns || hardTiles) {
        int* data = StripData(image, channel, false, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, 6 + i % 2, 14 + i / 2, 2);
        });
      }
    }

    for (unsigned int localX = 0; localX < count; ++localX) {
      int* data = StripData(image, channel, false, tileX, localX);
      for (int y = 2; y < 14; y += 4) {
        ApplyOverlap4x4([&](int x, int dy) {
          return BlockSample(data, 2 + x, y + dy, 2);
        });
      }
      if ((start + localX + 1 < imageWidth && !hardTiles) ||
          (hardTiles && localX + 1 < count)) {
        int* next = StripData(image, channel, false, tileX, localX + 1);
        for (int y = 2; y < 14; y += 4) {
          ApplyOverlap4x4([&](int x, int dy) {
            return HorizontalJoinSample(data, next, x, y + dy, 8, 2);
          });
        }
      }

      if (topRow + 1 < imageHeight) {
        int* below = StripData(image, channel, true, tileX, localX);
        if ((tileX == 0 && localX == 0 && !hardTiles) ||
            (hardTiles && localX == 0 && !tileBottom)) {
          for (int x = 0; x < 2; ++x) {
            ApplyOverlap4([&](int y) {
              return VerticalJoinSample(data, below, x, y, 16, 2);
            });
          }
        }
        if (!hardTiles || !tileBottom) {
          ApplyOverlap4x4([&](int x, int y) {
            return VerticalJoinSample(data, below, 2 + x, y, 16, 2);
          });
        }
        if ((start + localX + 1 < imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 < count && !tileBottom)) {
          int* dataNext =
              StripData(image, channel, false, tileX, localX + 1);
          int* belowNext =
              StripData(image, channel, true, tileX, localX + 1);
          ApplyOverlap4x4([&](int x, int y) {
            return FourWayJoinSample(data, dataNext, below, belowNext, x, y,
                                     8, 16, 2);
          });
        }
        if ((start + localX + 1 == imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 == count && !tileBottom)) {
          for (int x = 6; x < 8; ++x) {
            ApplyOverlap4([&](int y) {
              return VerticalJoinSample(data, below, x, y, 16, 2);
            });
          }
        }
      }
    }
  }
}

void overlap_level2_up3_420(jxr_image* image, int macroblockY, int channel) {
  const unsigned int topRow = static_cast<unsigned int>(macroblockY - 3);
  unsigned int tileRow = 0;
  while (topRow > image->tile_row_position[tileRow] +
                      image->tile_row_height[tileRow] - 1) {
    ++tileRow;
  }
  const unsigned int imageWidth = image->extended_width >> 4;
  const unsigned int imageHeight = image->extended_height >> 4;
  const bool hardTiles = image->disableTileOverlapFlag != 0;
  const bool tileBottom =
      topRow == image->tile_row_position[tileRow] +
                    image->tile_row_height[tileRow] - 1;

  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    const unsigned int count = image->tile_column_width[tileX];
    const unsigned int start = image->tile_column_position[tileX];

    if (tileX == 0 || hardTiles) {
      int* data = StripData(image, channel, false, tileX, 0);
      for (int x = 0; x < 2; ++x) {
        ApplyOverlap4([&](int i) {
          return BlockSample(data, x, 2 + i, 2);
        });
      }
    }
    if (tileX + 1 == image->tile_columns || hardTiles) {
      int* data = StripData(image, channel, false, tileX, count - 1);
      for (int x = 6; x < 8; ++x) {
        ApplyOverlap4([&](int i) {
          return BlockSample(data, x, 2 + i, 2);
        });
      }
    }

    if (topRow == 0) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        int* data = StripData(image, channel, false, tileX, localX);
        for (int y = 0; y < 2; ++y) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, 2 + i, y, 2);
          });
        }
        if ((start + localX > 0 && !hardTiles) ||
            (hardTiles && localX != 0)) {
          int* previous = StripData(image, channel, false, tileX, localX - 1);
          for (int y = 0; y < 2; ++y) {
            ApplyOverlap4([&](int i) {
              return HorizontalJoinSample(previous, data, i, y, 8, 2);
            });
          }
        }
      }
      if (tileX == 0 || hardTiles) {
        int* data = StripData(image, channel, false, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, i % 2, i / 2, 2);
        });
      }
      if (tileX + 1 == image->tile_columns || hardTiles) {
        int* data = StripData(image, channel, false, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, 6 + i % 2, i / 2, 2);
        });
      }
    }

    if (topRow + 1 == imageHeight || (hardTiles && tileBottom)) {
      for (unsigned int localX = 0; localX < count; ++localX) {
        int* data = StripData(image, channel, false, tileX, localX);
        for (int y = 6; y < 8; ++y) {
          ApplyOverlap4([&](int i) {
            return BlockSample(data, 2 + i, y, 2);
          });
        }
        if ((start + localX > 0 && !hardTiles) ||
            (hardTiles && localX != 0)) {
          int* previous = StripData(image, channel, false, tileX, localX - 1);
          for (int y = 6; y < 8; ++y) {
            ApplyOverlap4([&](int i) {
              return HorizontalJoinSample(previous, data, i, y, 8, 2);
            });
          }
        }
      }
      if (tileX == 0 || hardTiles) {
        int* data = StripData(image, channel, false, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, i % 2, 6 + i / 2, 2);
        });
      }
      if (tileX + 1 == image->tile_columns || hardTiles) {
        int* data = StripData(image, channel, false, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(data, 6 + i % 2, 6 + i / 2, 2);
        });
      }

      if (hardTiles && tileBottom && topRow + 1 < imageHeight) {
        for (unsigned int localX = 0; localX < count; ++localX) {
          int* data = StripData(image, channel, true, tileX, localX);
          for (int y = 0; y < 2; ++y) {
            ApplyOverlap4([&](int i) {
              return BlockSample(data, 2 + i, y, 2);
            });
          }
          if (localX != 0) {
            int* previous = StripData(image, channel, true, tileX, localX - 1);
            for (int y = 0; y < 2; ++y) {
              ApplyOverlap4([&](int i) {
                return HorizontalJoinSample(previous, data, i, y, 8, 2);
              });
            }
          }
        }
        int* first = StripData(image, channel, true, tileX, 0);
        ApplyOverlap4([&](int i) {
          return BlockSample(first, i % 2, i / 2, 2);
        });
        int* last = StripData(image, channel, true, tileX, count - 1);
        ApplyOverlap4([&](int i) {
          return BlockSample(last, 6 + i % 2, i / 2, 2);
        });
      }
    }

    for (unsigned int localX = 0; localX < count; ++localX) {
      int* data = StripData(image, channel, false, tileX, localX);
      ApplyOverlap4x4([&](int x, int y) {
        return BlockSample(data, 2 + x, 2 + y, 2);
      });
      if ((start + localX + 1 < imageWidth && !hardTiles) ||
          (hardTiles && localX + 1 < count)) {
        int* next = StripData(image, channel, false, tileX, localX + 1);
        ApplyOverlap4x4([&](int x, int y) {
          return HorizontalJoinSample(data, next, x, 2 + y, 8, 2);
        });
      }

      if (topRow + 1 < imageHeight) {
        int* below = StripData(image, channel, true, tileX, localX);
        if ((tileX == 0 && localX == 0 && !hardTiles) ||
            (hardTiles && localX == 0 && !tileBottom)) {
          for (int x = 0; x < 2; ++x) {
            ApplyOverlap4([&](int y) {
              return VerticalJoinSample(data, below, x, y, 8, 2);
            });
          }
        }
        if (!hardTiles || !tileBottom) {
          ApplyOverlap4x4([&](int x, int y) {
            return VerticalJoinSample(data, below, 2 + x, y, 8, 2);
          });
        }
        if ((start + localX + 1 < imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 < count && !tileBottom)) {
          int* dataNext =
              StripData(image, channel, false, tileX, localX + 1);
          int* belowNext =
              StripData(image, channel, true, tileX, localX + 1);
          ApplyOverlap4x4([&](int x, int y) {
            return FourWayJoinSample(data, dataNext, below, belowNext, x, y,
                                     8, 8, 2);
          });
        }
        if ((start + localX + 1 == imageWidth && !hardTiles) ||
            (hardTiles && localX + 1 == count && !tileBottom)) {
          for (int x = 6; x < 8; ++x) {
            ApplyOverlap4([&](int y) {
              return VerticalJoinSample(data, below, x, y, 8, 2);
            });
          }
        }
      }
    }
  }
}

void dclphp_shuffle(int* data, int dclpCount) {
  int temporary[256];
  for (int block = 0; block < dclpCount; ++block) {
    temporary[16 * block] = data[block];
    std::memcpy(temporary + 16 * block + 1, data + 16 + 15 * block,
                15 * sizeof(int));
  }
  std::memmove(data, temporary, sizeof(temporary));
}

void unblock_shuffle444(int* data) {
  int temporary[256];
  for (int source = 0; source < 256; source += 4) {
    const int destination =
        4 * (((source >> 4) & 3) +
             4 * (((source >> 2) & 3) + 4 * (source >> 6)));
    std::memcpy(temporary + destination, data + source, 4 * sizeof(int));
  }
  std::memmove(data, temporary, sizeof(temporary));
}

void unblock_shuffle422(int* data) {
  int temporary[128];
  for (int source = 0; source < 128; source += 4) {
    const int destination =
        4 * (((source & 16) != 0) +
             2 * (((source >> 2) & 3) + 4 * (source / 32)));
    std::memcpy(temporary + destination, data + source, 4 * sizeof(int));
  }
  std::memmove(data, temporary, sizeof(temporary));
}

void unblock_shuffle420(int* data) {
  int temporary[64];
  for (int source = 0; source < 64; source += 4) {
    const int destination =
        4 * (((source & 16) != 0) +
             2 * (((source >> 2) & 3) + 4 * (source / 32)));
    std::memcpy(temporary + destination, data + source, 4 * sizeof(int));
  }
  std::memmove(data, temporary, sizeof(temporary));
}

void _jxr_r_rotate_mb_strip(jxr_image* image, int macroblockY) {
  image->cur_my = macroblockY;
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    jxr_strip_t& strip = image->strip[channel];
    macroblock_s* oldUp3 = strip.up3;
    strip.up3 = strip.up2;
    strip.up2 = strip.up1;
    strip.up1 = strip.cur;
    strip.cur = oldUp3;
  }
  _jxr_clear_strip_cur(image);
}

void yuv444_to_rgb(jxr_image* image, int macroblockX) {
  int* first = image->strip[0].up3[macroblockX].data;
  int* second = image->strip[1].up3[macroblockX].data;
  int* third = image->strip[2].up3[macroblockX].data;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int intermediate = first[pixel] - _jxr_floor_div2(-second[pixel]);
    const int red = intermediate - second[pixel] - _jxr_ceil_div2(third[pixel]);
    first[pixel] = red;
    second[pixel] = intermediate;
    third[pixel] = third[pixel] + red;
  }
}

void yuv422_to_yuv444(jxr_image* image, int macroblockX) {
  int output[256];
  const int macroblockCount = static_cast<int>(image->extended_width >> 4);
  for (int channel = 1; channel < 3; ++channel) {
    for (int y = 0; y < 16; ++y) {
      int input[10];
      int* data = image->strip[channel].up3[macroblockX].data;
      if (macroblockX == 0)
        image->strip[channel].upsample_memory_x[y] = data[8 * y];
      for (int x = 0; x < 8; ++x)
        input[x + 1] = data[8 * y + x];
      input[0] = image->strip[channel].upsample_memory_x[y];
      input[9] = macroblockX + 1 < macroblockCount
                     ? image->strip[channel].up3[macroblockX + 1].data[8 * y]
                     : input[8];
      Upsample(input, output + 16 * y, 16, image->chroma_centering_x);
      image->strip[channel].upsample_memory_x[y] = data[8 * y + 7];
    }
    std::memcpy(image->strip[channel].up3[macroblockX].data, output,
                sizeof(output));
  }
}

void yuv420_to_yuv444(jxr_image* image, int macroblockY, int macroblockX) {
  int output[256];
  int intermediate[2][128];
  int input[10];
  const int outputRow = macroblockY - 2;
  const int macroblockWidth = static_cast<int>(image->extended_width >> 4);
  const int macroblockHeight = static_cast<int>(image->extended_height >> 4);

  for (int channel = 1; channel < 3; ++channel) {
    for (int x = 0; x < 8; ++x) {
      int* data = image->strip[channel].up3[macroblockX].data;
      const int memoryIndex = 8 * macroblockX + x;
      if (outputRow == 1)
        image->strip[channel].upsample_memory_y[memoryIndex] = data[x];
      for (int y = 0; y < 8; ++y)
        input[y + 1] = data[8 * y + x];
      input[0] = image->strip[channel].upsample_memory_y[memoryIndex];
      if (outputRow < macroblockHeight) {
        input[9] = image->strip[channel].up2[macroblockX].data[
            x <= 3 ? x : x + 12];
      } else {
        input[9] = input[8];
      }
      Upsample(input, output, 16, image->chroma_centering_y);
      for (int y = 0; y < 16; ++y)
        intermediate[channel - 1][8 * y + x] = output[y];
      image->strip[channel].upsample_memory_y[memoryIndex] = data[8 * 7 + x];
    }
  }

  for (int channel = 1; channel < 3; ++channel) {
    int nextMacroblockColumn[16];
    for (int y = 0; y < 8; ++y) {
      input[y + 1] =
          macroblockX + 1 < macroblockWidth
              ? image->strip[channel].up3[macroblockX + 1].data[8 * y]
              : image->strip[channel].up3[macroblockX].data[8 * y];
    }
    if (outputRow < macroblockHeight && macroblockX + 1 < macroblockWidth) {
      input[9] = image->strip[channel].up2[macroblockX + 1].data[0];
    } else {
      input[9] = input[8];
    }
    if (outputRow != 1 && macroblockX + 1 < macroblockWidth) {
      input[0] = image->strip[channel].upsample_memory_y[8 * (macroblockX + 1)];
    } else {
      input[0] = input[1];
    }
    Upsample(input, nextMacroblockColumn, 16, image->chroma_centering_y);

    for (int y = 0; y < 16; ++y) {
      if (macroblockX == 0) {
        image->strip[channel].upsample_memory_x[y] =
            intermediate[channel - 1][8 * y];
      }
      for (int x = 0; x < 8; ++x)
        input[x + 1] = intermediate[channel - 1][8 * y + x];
      input[0] = image->strip[channel].upsample_memory_x[y];
      input[9] = macroblockX + 1 < macroblockWidth
                     ? nextMacroblockColumn[y]
                     : input[8];
      Upsample(input, output + 16 * y, 16, image->chroma_centering_x);
      image->strip[channel].upsample_memory_x[y] =
          intermediate[channel - 1][8 * y + 7];
    }
    std::memcpy(image->strip[channel].up3[macroblockX].data, output,
                sizeof(output));
  }
}

void yuvk_to_cmyk(jxr_image* image, int macroblockX) {
  int* first = image->strip[0].up3[macroblockX].data;
  int* second = image->strip[1].up3[macroblockX].data;
  int* third = image->strip[2].up3[macroblockX].data;
  int* fourth = image->strip[3].up3[macroblockX].data;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int black = _jxr_floor_div2(first[pixel]) + fourth[pixel];
    const int magenta = black - first[pixel] - _jxr_floor_div2(second[pixel]);
    const int cyan = second[pixel] + magenta + _jxr_floor_div2(third[pixel]);
    first[pixel] = cyan;
    second[pixel] = magenta;
    third[pixel] = cyan - third[pixel];
    fourth[pixel] = black;
  }
}

void yuvk_to_cmykdirect(jxr_image* image, int macroblockX) {
  int* first = image->strip[0].up3[macroblockX].data;
  int* second = image->strip[1].up3[macroblockX].data;
  int* third = image->strip[2].up3[macroblockX].data;
  int* fourth = image->strip[3].up3[macroblockX].data;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int saved = first[pixel];
    first[pixel] = second[pixel];
    second[pixel] = third[pixel];
    third[pixel] = fourth[pixel];
    fourth[pixel] = saved;
  }
}

void shift_and_clip_CMYK(jxr_image* image, int macroblockX, int bias,
                         char shiftBits, char scale, int round, int clipLow,
                         int clipHigh) {
  const int scaledBias = (bias >> (shiftBits + 1)) << scale;
  for (int channel = 0; channel < 3; ++channel) {
    int* data = image->strip[channel].up3[macroblockX].data;
    for (int pixel = 0; pixel < 256; ++pixel) {
      const int value = ((data[pixel] + scaledBias + round) >> scale)
                        << shiftBits;
      data[pixel] = std::clamp(value, clipLow, clipHigh);
    }
  }
  int* black = image->strip[3].up3[macroblockX].data;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int value = ((black[pixel] + round - scaledBias) >> scale)
                      << shiftBits;
    black[pixel] = std::clamp(value, clipLow, clipHigh);
  }
}

void shift_and_clip_BD565(jxr_image* image, int macroblockX, int bias,
                          char scale, int round) {
  int* first = image->strip[0].up3[macroblockX].data;
  int* second = image->strip[1].up3[macroblockX].data;
  int* third = image->strip[2].up3[macroblockX].data;
  const int scaledBias = bias << scale;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int firstValue =
        std::clamp((first[pixel] + scaledBias + round) >> (scale + 1), 0, 31);
    const int secondValue =
        std::clamp((second[pixel] + scaledBias + round) >> scale, 0, 63);
    const int thirdValue =
        std::clamp((third[pixel] + scaledBias + round) >> (scale + 1), 0, 31);
    if ((image->header_flags2 & 4U) != 0) {
      first[pixel] = firstValue;
      second[pixel] = secondValue;
      third[pixel] = thirdValue;
    } else {
      first[pixel] = thirdValue;
      second[pixel] = secondValue;
      third[pixel] = firstValue;
    }
  }
}

void shift_and_clip_BDxxx(jxr_image* image, int macroblockX, int bias,
                          char shiftBits, char scale, int round, int clipLow,
                          int clipHigh) {
  int* first = image->strip[0].up3[macroblockX].data;
  int* second = image->strip[1].up3[macroblockX].data;
  int* third = image->strip[2].up3[macroblockX].data;
  const int scaledBias = (bias >> shiftBits) << scale;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int firstValue = std::clamp(
        ((first[pixel] + scaledBias + round) >> scale) << shiftBits,
        clipLow, clipHigh);
    const int secondValue = std::clamp(
        ((second[pixel] + scaledBias + round) >> scale) << shiftBits,
        clipLow, clipHigh);
    const int thirdValue = std::clamp(
        ((third[pixel] + scaledBias + round) >> scale) << shiftBits,
        clipLow, clipHigh);
    if ((image->header_flags2 & 4U) != 0) {
      first[pixel] = firstValue;
      second[pixel] = secondValue;
      third[pixel] = thirdValue;
    } else {
      first[pixel] = thirdValue;
      second[pixel] = secondValue;
      third[pixel] = firstValue;
    }
  }
}

void shift_and_clip_regular(jxr_image* image, int macroblockX, int bias,
                            char shiftBits, char scale, int round, int clipLow,
                            int clipHigh) {
  const int scaledBias = (bias >> shiftBits) << scale;
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    int* data = image->strip[channel].up3[macroblockX].data;
    for (int pixel = 0; pixel < 256; ++pixel) {
      const int value = ((data[pixel] + scaledBias + round) >> scale)
                        << shiftBits;
      data[pixel] = std::clamp(value, clipLow, clipHigh);
    }
  }
}

void shift_and_clip_FLOAT(jxr_image* image, int macroblockX, char scale,
                          int round) {
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    int* data = image->strip[channel].up3[macroblockX].data;
    for (int pixel = 0; pixel < 256; ++pixel) {
      int value = (data[pixel] + round) >> scale;
      std::uint32_t encoded;
      if ((image->header_flags_fmt & 15U) == 4) {
        const std::uint32_t sign = value < 0 ? 0x8000U : 0;
        std::uint32_t magnitude = value < 0
                                      ? 0U - static_cast<std::uint32_t>(value)
                                      : static_cast<std::uint32_t>(value);
        magnitude = std::min(magnitude, 0x7FFFU);
        encoded = sign | magnitude;
      } else {
        const std::uint32_t sign =
            static_cast<std::uint32_t>(value) & 0x80000000U;
        const int mantissaBits = image->len_mantissa;
        const std::uint32_t hiddenBit = 1U << mantissaBits;
        const std::uint32_t magnitude =
            value < 0 ? 0U - static_cast<std::uint32_t>(value)
                      : static_cast<std::uint32_t>(value);
        int exponent = static_cast<int>(magnitude >> mantissaBits);
        std::uint32_t mantissa = hiddenBit | (magnitude & (hiddenBit - 1));
        if (exponent == 0) {
          mantissa ^= hiddenBit;
          exponent = 1;
        }
        exponent += 127 - image->exp_bias;
        while (exponent > 1 && mantissa > 0 && mantissa < hiddenBit) {
          mantissa *= 2;
          --exponent;
        }
        if (mantissa >= hiddenBit)
          mantissa ^= hiddenBit;
        else
          exponent = 0;
        encoded = sign | (static_cast<std::uint32_t>(exponent) << 23) |
                  (mantissa << (23 - mantissaBits));
      }
      data[pixel] = static_cast<int>(encoded);
    }
  }
}

void shift_and_clip_RGBE(jxr_image* image, int macroblockX, char scale,
                         int round, int* buffer) {
  int* red = image->strip[0].up3[macroblockX].data;
  int* green = image->strip[1].up3[macroblockX].data;
  int* blue = image->strip[2].up3[macroblockX].data;
  const int stride = image->num_channels + 1;
  for (int pixel = 0; pixel < 256; ++pixel) {
    const int inputs[3] = {(red[pixel] + round) >> scale,
                           (green[pixel] + round) >> scale,
                           (blue[pixel] + round) >> scale};
    int exponents[3] = {};
    for (int channel = 0; channel < 3; ++channel) {
      int value = inputs[channel];
      if (value > 0) {
        exponents[channel] = std::max(value >> 7, 1);
        if ((value >> 7) > 1)
          value = (value & 127) + 128;
        buffer[pixel * stride + channel] = value;
      } else {
        buffer[pixel * stride + channel] = 0;
      }
    }
    const int exponent =
        std::max(exponents[0], std::max(exponents[1], exponents[2]));
    buffer[pixel * stride + 3] = exponent;
    for (int channel = 0; channel < 3; ++channel) {
      if (exponent > exponents[channel]) {
        buffer[pixel * stride + channel] = static_cast<std::uint8_t>(
            (2 * buffer[pixel * stride + channel] + 1) >>
            (exponent - exponents[channel] + 1));
      }
    }
  }
}

void scale_and_emit_mb(jxr_image* image, int macroblockY, int macroblockX) {
  const bool scaled = (image->component_mode & 1U) != 0;
  const char scale = static_cast<char>(scaled ? 3 : 0);
  const char shiftBits = static_cast<char>(image->shift_bits);
  const int sourceBitDepth = image->header_flags_fmt & 0x0F;
  const int sourceColorFormat = image->header_flags_fmt >> 4;
  int bias = 0;
  int round = scaled ? 3 : 0;
  int clipLow = 0;
  int clipHigh = 255;
  int buffer[17 * 256] = {};

  switch (sourceBitDepth) {
    case 0:
    case 15:
      round = scaled ? 4 : 0;
      clipHigh = 1;
      break;
    case 1:
      bias = 128;
      break;
    case 2:
      bias = 32768;
      round = scaled ? 4 : 0;
      clipHigh = 65535;
      break;
    case 3:
      clipLow = -32768;
      clipHigh = 32767;
      break;
    case 6:
      clipLow = (-2147483647 - 1);
      clipHigh = 2147483647;
      break;
    case 8:
      bias = 16;
      clipHigh = 31;
      break;
    case 9:
      bias = 512;
      clipHigh = 1023;
      break;
    case 10:
      bias = 32;
      clipHigh = 31;
      break;
    default:
      break;
  }

  unblock_shuffle444(image->strip[0].up3[macroblockX].data);
  for (unsigned int channel = 1; channel < image->num_channels; ++channel) {
    int* data = image->strip[channel].up3[macroblockX].data;
    if (image->use_clr_fmt == 1)
      unblock_shuffle420(data);
    else if (image->use_clr_fmt == 2)
      unblock_shuffle422(data);
    else
      unblock_shuffle444(data);
  }

  const bool skipColorTransform =
      sourceColorFormat == 1 || sourceColorFormat == 2 ||
      sourceColorFormat == 3 || sourceColorFormat == 5;
  if (!skipColorTransform) {
    if (image->use_clr_fmt == 1) {
      yuv420_to_yuv444(image, macroblockY, macroblockX);
      yuv444_to_rgb(image, macroblockX);
    } else if (image->use_clr_fmt == 2) {
      yuv422_to_yuv444(image, macroblockX);
      yuv444_to_rgb(image, macroblockX);
    } else if (image->use_clr_fmt == 3) {
      yuv444_to_rgb(image, macroblockX);
    } else if (image->use_clr_fmt == 4) {
      yuvk_to_cmyk(image, macroblockX);
    }
  } else if (sourceColorFormat == 5 && image->primary != 0) {
    yuvk_to_cmykdirect(image, macroblockX);
  } else if (sourceColorFormat == 3) {
    if (image->use_clr_fmt == 1)
      yuv420_to_yuv444(image, macroblockY, macroblockX);
    else if (image->use_clr_fmt == 2)
      yuv422_to_yuv444(image, macroblockX);
  }

  if (image->use_clr_fmt == 4 && sourceColorFormat != 5) {
    shift_and_clip_CMYK(image, macroblockX, bias, shiftBits, scale, round,
                        clipLow, clipHigh);
  } else if (sourceBitDepth == 10) {
    if (image->num_channels == 1) {
      shift_and_clip_regular(image, macroblockX, bias, shiftBits, scale,
                             round, 0, 63);
    } else {
      shift_and_clip_BD565(image, macroblockX, bias, scale, round);
    }
  } else if (!skipColorTransform &&
             (sourceBitDepth == 8 || sourceBitDepth == 9) &&
             image->num_channels != 1) {
    shift_and_clip_BDxxx(image, macroblockX, bias, shiftBits, scale, round,
                         clipLow, clipHigh);
  } else if (sourceBitDepth == 4 || sourceBitDepth == 7) {
    shift_and_clip_FLOAT(image, macroblockX, scale, round);
  } else if (sourceColorFormat == 8) {
    shift_and_clip_RGBE(image, macroblockX, scale, round, buffer);
  } else {
    shift_and_clip_regular(image, macroblockX, bias, shiftBits, scale, round,
                           clipLow, clipHigh);
  }

  if (image->primary == 1) {
    const bool hasAlpha = (image->header_flags2 & 1U) != 0;
    const int outputChannels =
        static_cast<int>(image->num_channels) + (hasAlpha ? 1 : 0);
    if (sourceColorFormat != 8) {
      for (int pixel = 0; pixel < 256; ++pixel) {
        for (unsigned int channel = 0; channel < image->num_channels;
             ++channel) {
          buffer[outputChannels * pixel + channel] =
              image->strip[channel].up3[macroblockX].data[pixel];
        }
      }
    }
    if (hasAlpha) {
      const int* alpha = image->alpha->strip[0].up3[macroblockX].data;
      for (int pixel = 0; pixel < 256; ++pixel) {
        buffer[outputChannels * pixel + image->num_channels] = alpha[pixel];
      }
    }
    _jxr_send_mb_to_output(image, macroblockX, macroblockY - 3, buffer);
    image->output_sent = 1;
  }
}

void rflush_to_tile_buffer(jxr_image* image, int tileX, int macroblockY) {
  const unsigned int rowWidth = image->extended_width >> 4;
  for (unsigned int localX = 0; localX < image->tile_column_width[tileX];
       ++localX) {
    const unsigned int column = localX + image->tile_column_position[tileX];
    const unsigned int destination = column + macroblockY * rowWidth;
    for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
      CopyStoredMacroblock(image->mb_row_buffer[channel][destination],
                           image->strip[channel].cur[column],
                           StoredCoefficientCount(image, channel));
    }
  }
}

void rflush_collect_mb_strip_data(jxr_image* image, int macroblockY) {
  const unsigned int rowWidth = image->extended_width >> 4;
  for (unsigned int tileX = 0; tileX + 1 < image->tile_columns; ++tileX) {
    for (unsigned int localX = 0; localX < image->tile_column_width[tileX];
         ++localX) {
      const unsigned int column = localX + image->tile_column_position[tileX];
      const unsigned int source = column + macroblockY * rowWidth;
      for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
        CopyStoredMacroblock(image->strip[channel].cur[column],
                             image->mb_row_buffer[channel][source],
                             StoredCoefficientCount(image, channel));
      }
    }
  }
}

void rflush_save_context(jxr_image* image) {
  const unsigned int width = image->extended_width >> 4;
  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    for (unsigned int localX = 0; localX < image->tile_column_width[tileX];
         ++localX) {
      const unsigned int column = localX + image->tile_column_position[tileX];
      for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
        const int coefficientCount = StoredCoefficientCount(image, channel);
        macroblock_s* context = image->mb_row_context[channel];
        const jxr_strip_t& strip = image->strip[channel];
        CopyStoredMacroblock(context[column], strip.cur[column],
                             coefficientCount);
        CopyStoredMacroblock(context[width + column], strip.up1[column],
                             coefficientCount);
        CopyStoredMacroblock(context[2 * width + column], strip.up2[column],
                             coefficientCount);
        CopyStoredMacroblock(context[3 * width + column], strip.up3[column],
                             coefficientCount);
      }
    }
  }
}

void rflush_recover_context(jxr_image* image) {
  const unsigned int width = image->extended_width >> 4;
  for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
    for (unsigned int localX = 0; localX < image->tile_column_width[tileX];
         ++localX) {
      const unsigned int column = localX + image->tile_column_position[tileX];
      for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
        const int coefficientCount = StoredCoefficientCount(image, channel);
        const macroblock_s* context = image->mb_row_context[channel];
        jxr_strip_t& strip = image->strip[channel];
        CopyStoredMacroblock(strip.cur[column], context[column],
                             coefficientCount);
        CopyStoredMacroblock(strip.up1[column], context[width + column],
                             coefficientCount);
        CopyStoredMacroblock(strip.up2[column], context[2 * width + column],
                             coefficientCount);
        CopyStoredMacroblock(strip.up3[column], context[3 * width + column],
                             coefficientCount);
      }
    }
  }
}

int _jxr_rcollect_tile_mb_rows(jxr_image* image, int tileX, int tileY,
                               int macroblockY) {
  if (image->tile_columns > 1 && tileX >= 0) {
    if (tileX + 1 < static_cast<int>(image->tile_columns)) {
      if (image->cur_my >= 0) {
        if (macroblockY == 0) {
          if (tileX == 0) {
            if (tileY > 0) {
              rflush_collect_mb_strip_data(image, image->cur_my);
              rflush_save_context(image);
              rflush_to_tile_buffer(
                  image, static_cast<int>(image->tile_columns) - 1,
                  image->cur_my);
            }
            return 0;
          }
          --tileX;
        }
        rflush_to_tile_buffer(image, tileX, image->cur_my);
      }
      return 0;
    }
    if (macroblockY != 0) {
      if (macroblockY <= static_cast<int>(image->tile_row_height[tileY]))
        rflush_collect_mb_strip_data(image, image->cur_my);
    } else {
      rflush_to_tile_buffer(image, tileX - 1, image->cur_my);
      image->cur_my = -1;
      if (tileY > 0) {
        rflush_recover_context(image);
        return 1;
      }
    }
  }
  return 1;
}

int _jxr_r_transform_mb_strip(jxr_image* image, int tileX, int tileY,
                              int macroblockY) {
  const int collected =
      _jxr_rcollect_tile_mb_rows(image, tileX, tileY, macroblockY);
  if (collected == 0)
    return 0;

  const unsigned int rowBase =
      tileY < 0 ? 0U : image->tile_row_position[tileY];
  const int outputRow = static_cast<int>(rowBase) + macroblockY - 1;
  int result = 0;
  if (outputRow >= 1) {
    for (unsigned int channel = 0; channel < image->num_channels; ++channel)
      IPCT_level1_up1(image, outputRow, static_cast<int>(channel));
    if (outputRow >= 2) {
      if ((image->header_flags1 & 3U) >= 2) {
        for (unsigned int channel = 0; channel < image->num_channels;
             ++channel) {
          if (channel == 0 ||
              (image->use_clr_fmt != 1 && image->use_clr_fmt != 2)) {
            overlap_level1_up2_444(image, outputRow,
                                   static_cast<int>(channel));
          } else if (image->use_clr_fmt == 1) {
            overlap_level1_up2_420(image, outputRow,
                                   static_cast<int>(channel));
          } else {
            overlap_level1_up2_422(image, outputRow,
                                   static_cast<int>(channel));
          }
        }
      }
      for (unsigned int channel = 0; channel < image->num_channels; ++channel)
        IPCT_level2_up2(image, static_cast<int>(channel), 0);
      if (outputRow >= 3) {
        if ((image->header_flags1 & 3U) != 0) {
          for (unsigned int channel = 0; channel < image->num_channels;
               ++channel) {
            if (channel == 0 ||
                (image->use_clr_fmt != 1 && image->use_clr_fmt != 2)) {
              overlap_level2_up3_444(image, outputRow,
                                     static_cast<int>(channel));
            } else if (image->use_clr_fmt == 1) {
              overlap_level2_up3_420(image, outputRow,
                                     static_cast<int>(channel));
            } else {
              overlap_level2_up3_422(image, outputRow,
                                     static_cast<int>(channel));
            }
          }
        }
        result = 1;
      }
    }
    if (image->lwf_test == 0)
      image->lwf_test = static_cast<std::uint8_t>(_jxr_read_lwf_test_flag());
  }
  return result;
}

int _jxr_r_output_mb_strip(jxr_image* image, int, int tileY,
                           int macroblockY) {
  const unsigned int rowBase =
      tileY < 0 ? 0U : image->tile_row_position[tileY];
  const int outputRow = static_cast<int>(rowBase) + macroblockY - 1;
  const int macroblockCount = static_cast<int>(image->extended_width >> 4);
  for (int macroblockX = 0; macroblockX < macroblockCount; ++macroblockX)
    scale_and_emit_mb(image, outputRow, macroblockX);
  return 0;
}

void _jxr_r_flush_rotate_mb_strip(jxr_image* image, int tileX, int tileY,
                                  int macroblockY) {
  if (_jxr_r_transform_mb_strip(image, tileX, tileY, macroblockY) != 0)
    _jxr_r_output_mb_strip(image, tileX, tileY, macroblockY);
  image->cur_my = macroblockY;
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    jxr_strip_t& strip = image->strip[channel];
    macroblock_s* oldUp3 = strip.up3;
    strip.up3 = strip.up2;
    strip.up2 = strip.up1;
    strip.up1 = strip.cur;
    strip.cur = oldUp3;
  }
  _jxr_clear_strip_cur(image);
}

void _jxr_output_mb_id_mono(jxr_image* image, int macroblockX,
                            std::uint8_t* buffer, int byteStride) {
  unblock_shuffle444(image->strip[0].up3[macroblockX].data);
  const int scale = OutputScale(image);
  shift_and_clip_regular(image, macroblockX, 128,
                         static_cast<char>(image->shift_bits),
                         static_cast<char>(scale), scale, 0, 255);
  const int* mono = OutputPlane(image, 0, macroblockX);
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x)
      buffer[y * byteStride + x] = static_cast<std::uint8_t>(mono[16 * y + x]);
  }
}

void _jxr_output_mb_id_grayscale(jxr_image* image, int macroblockX,
                                 std::uint8_t* buffer, int byteStride) {
  unblock_shuffle444(image->strip[0].up3[macroblockX].data);
  const int scale = OutputScale(image);
  shift_and_clip_regular(image, macroblockX, 128,
                         static_cast<char>(image->shift_bits),
                         static_cast<char>(scale), scale, 0, 255);
  const int* grayscale = OutputPlane(image, 0, macroblockX);
  for (int y = 0; y < 16; ++y) {
    std::uint8_t* output = buffer + 4 * byteStride * y;
    for (int x = 0; x < 16; ++x) {
      const std::uint8_t value =
          static_cast<std::uint8_t>(grayscale[16 * y + x]);
      output[4 * x + 0] = value;
      output[4 * x + 1] = value;
      output[4 * x + 2] = value;
      output[4 * x + 3] = 255;
    }
  }
}

void _jxr_output_mb_id_rgb(jxr_image* image, int macroblockX,
                           std::uint8_t* buffer, int byteStride) {
  for (int channel = 0; channel < 3; ++channel)
    unblock_shuffle444(image->strip[channel].up3[macroblockX].data);
  yuv444_to_rgb(image, macroblockX);
  const int scale = OutputScale(image);
  shift_and_clip_regular(image, macroblockX, 128,
                         static_cast<char>(image->shift_bits),
                         static_cast<char>(scale), scale, 0, 255);
  const int* red = OutputPlane(image, 0, macroblockX);
  const int* green = OutputPlane(image, 1, macroblockX);
  const int* blue = OutputPlane(image, 2, macroblockX);
  for (int y = 0; y < 16; ++y) {
    std::uint8_t* output = buffer + 4 * byteStride * y;
    for (int x = 0; x < 16; ++x) {
      const int pixel = 16 * y + x;
      output[4 * x + 0] = static_cast<std::uint8_t>(red[pixel]);
      output[4 * x + 1] = static_cast<std::uint8_t>(green[pixel]);
      output[4 * x + 2] = static_cast<std::uint8_t>(blue[pixel]);
      output[4 * x + 3] = 255;
    }
  }
}

void _jxr_output_mb_id_cocg_y(jxr_image* image, int macroblockX,
                              std::uint8_t* buffer, int byteStride) {
  for (int channel = 0; channel < 3; ++channel)
    unblock_shuffle444(image->strip[channel].up3[macroblockX].data);
  const int scale = OutputScale(image);
  shift_and_clip_regular(image, macroblockX, 128,
                         static_cast<char>(image->shift_bits),
                         static_cast<char>(scale), scale, 0, 255);
  const int* yPlane = OutputPlane(image, 0, macroblockX);
  const int* coPlane = OutputPlane(image, 1, macroblockX);
  const int* cgPlane = OutputPlane(image, 2, macroblockX);
  for (int y = 0; y < 16; ++y) {
    std::uint8_t* output = buffer + 4 * byteStride * y;
    for (int x = 0; x < 16; ++x) {
      const int pixel = 16 * y + x;
      output[4 * x + 0] = static_cast<std::uint8_t>(coPlane[pixel]);
      output[4 * x + 1] = static_cast<std::uint8_t>(cgPlane[pixel]);
      output[4 * x + 2] = 0;
      output[4 * x + 3] = static_cast<std::uint8_t>(yPlane[pixel]);
    }
  }
}

void _jxr_output_mb_id_normal_y_x(jxr_image* image, int macroblockX,
                                  std::uint8_t* buffer, int byteStride) {
  unblock_shuffle444(image->strip[0].up3[macroblockX].data);
  unblock_shuffle444(image->strip[1].up3[macroblockX].data);
  const int scale = OutputScale(image);
  shift_and_clip_regular(image, macroblockX, 128,
                         static_cast<char>(image->shift_bits),
                         static_cast<char>(scale), scale, 0, 255);
  const int* yPlane = OutputPlane(image, 0, macroblockX);
  const int* xPlane = OutputPlane(image, 1, macroblockX);
  for (int y = 0; y < 16; ++y) {
    std::uint8_t* output = buffer + 4 * byteStride * y;
    for (int x = 0; x < 16; ++x) {
      const int pixel = 16 * y + x;
      output[4 * x + 0] = 255;
      output[4 * x + 1] = static_cast<std::uint8_t>(xPlane[pixel]);
      output[4 * x + 2] = 255;
      output[4 * x + 3] = static_cast<std::uint8_t>(yPlane[pixel]);
    }
  }
}
