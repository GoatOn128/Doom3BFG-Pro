#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <cstdlib>
#include <cstring>
#include <utility>

void _jxr_output_mb_id_mono(jxr_image*, int, std::uint8_t*, int);
void _jxr_output_mb_id_grayscale(jxr_image*, int, std::uint8_t*, int);
void _jxr_output_mb_id_rgb(jxr_image*, int, std::uint8_t*, int);
void _jxr_output_mb_id_cocg_y(jxr_image*, int, std::uint8_t*, int);
void _jxr_output_mb_id_normal_y_x(jxr_image*, int, std::uint8_t*, int);

namespace {

constexpr int kBitsPerQpIndex[17] = {0, 0, 1, 1, 2, 2, 3, 3, 3,
                                     3, 4, 4, 4, 4, 4, 4, 4};
constexpr int kRunFixedLength[15] = {0, 0, 1, 1, 3, 0, 0, 1,
                                     1, 2, 0, 0, 0, 0, 1};
constexpr int kRunRemap[15] = {1, 2, 3, 5, 7, 1, 2, 3,
                               5, 7, 1, 2, 3, 4, 5};
constexpr int kRunBin[15] = {-1, -1, -1, -1, 2, 2, 2, 1,
                             1,  1,  1,  0,  0, 0, 0};
constexpr int kTranspose444[16] = {0, 4, 8, 12, 1, 5, 9, 13,
                                   2, 6, 10, 14, 3, 7, 11, 15};

constexpr std::uint8_t kFirstIndexCodeLengths0[64] = {
    6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr char kFirstIndexValues0[64] = {
    2, 3, 1, 1, 0, 0, 0, 0, 9, 9, 9, 9, 9, 9, 9, 9,
    4, 4, 4, 4, 6, 6, 6, 6, 8, 8, 8, 8, 10, 10, 10, 10,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11};
constexpr std::uint8_t kFirstIndexCodeLengths1[64] = {
    6, 6, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr char kFirstIndexValues1[64] = {
    2, 3, 10, 10, 1, 1, 6, 6, 0, 0, 0, 0, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9, 11, 11, 11, 11, 11, 11, 11, 11,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
constexpr std::uint8_t kFirstIndexCodeLengths2[64] = {
    7, 7, 7, 7, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
constexpr char kFirstIndexValues2[64] = {
    2, 3, 6, 10, 4, 4, 4, 4, 11, 11, 11, 11, 11, 11, 11, 11,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
constexpr std::uint8_t kFirstIndexCodeLengths3[64] = {
    7, 7, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
constexpr char kFirstIndexValues3[64] = {
    2, 6, 10, 10, 3, 3, 3, 3, 4, 4, 4, 4, 8, 8, 8, 8,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
constexpr std::uint8_t kFirstIndexCodeLengths4[64] = {
    7, 7, 6, 6, 6, 6, 6, 6, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};
constexpr char kFirstIndexValues4[64] = {
    6, 10, 2, 2, 4, 4, 8, 8, 11, 11, 11, 11, 11, 11, 11, 11,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
    9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};

constexpr int kFirstIndexDelta[4][12] = {
    {1, 1, 1, 1, 1, 0, 0, -1, 2, 1, 0, 0},
    {2, 2, -1, -1, -1, 0, -2, -1, 0, 0, -2, -1},
    {-1, 1, 0, 2, 0, 0, 0, 0, -2, 0, 1, 1},
    {0, 1, 0, 1, -2, 0, -1, -1, -2, -1, -2, -2}};
constexpr int kIndexDelta[3][6] = {{-1, 1, 1, 1, 0, 1},
                                   {-2, 0, 0, 2, 0, 0},
                                   {-1, -1, 0, 1, 0, 4}};
constexpr std::uint8_t kAbsLevelCodeLengths0[64] = {
    5, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr char kAbsLevelValues0[64] = {
    5, 5, 6, 6, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
constexpr std::uint8_t kAbsLevelCodeLengths1[64] = {
    6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr char kAbsLevelValues1[64] = {
    5, 6, 4, 4, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr int kAbsLevelIndexDelta[7] = {1, 0, -1, -1, -1, -1, -1};
constexpr std::uint8_t kBlockCbpCodeLengths6[64] = {
    6, 6, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr char kBlockCbpValues6[64] = {
    7, 4, 6, 6, 3, 3, 3, 3, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 5, 5, 5, 5, 5, 5, 5, 5,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t kBlockCbpCodeLengths5[32] = {
    5, 5, 5, 5, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
constexpr char kBlockCbpValues5[32] = {
    1, 3, 4, 7, 2, 2, 8, 8, 0, 0, 0, 0, 6, 6, 6, 6,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
constexpr int kBlockCbpOutput[16] = {0, 15, 3, 12, 1, 2, 4, 8,
                                     5, 6, 9, 10, 7, 11, 13, 14};
constexpr int kBlockCbpOffset[6] = {0, 4, 2, 8, 12, 1};
constexpr int kBlockCbpFixedLength[6] = {0, 2, 1, 2, 2, 0};
constexpr int kBlockCbpDelta9[9] = {2, 2, 1, 1, -1, -2, -2, -2, -3};
constexpr int kBlockCbpDelta5[5] = {0, -1, 0, 1, 1};
constexpr int kHpScanMap[16] = {0, 1, 4, 5, 2, 3, 6, 7,
                                8, 9, 12, 13, 10, 11, 14, 15};
constexpr int kSubsampledCbpPosition[4] = {0, 1, 4, 5};
constexpr std::uint8_t kLpCbpCodeLengths[16] = {
    1, 1, 1, 1, 1, 1, 1, 1, 3, 3, 4, 4, 4, 4, 4, 4};
constexpr char kLpCbpValues[16] = {0, 0, 0, 0, 0, 0, 0, 0,
                                   1, 1, 2, 3, 4, 5, 6, 7};
constexpr int kTranspose422[8] = {0, 2, 1, 3, 4, 6, 5, 7};
constexpr int kTranspose420[4] = {0, 2, 1, 3};
constexpr int kChromaLpRemap[7] = {4, 1, 2, 3, 5, 6, 7};

int RefineLowpassValue(rbitstream* stream, int value, int modelBits) {
  const unsigned int refinement = _jxr_rbitstream_uintN(stream, modelBits);
  if (value > 0)
    return (value << modelBits) + static_cast<int>(refinement);
  if (value < 0)
    return (value << modelBits) - static_cast<int>(refinement);
  int result = static_cast<int>(refinement);
  if (refinement != 0 && _jxr_rbitstream_uint1(stream))
    result = -result;
  return result;
}

void AdaptiveLPScan(jxr_image* image, int* input, int scanIndex, int value) {
  const int index = scanIndex - 1;
  input[image->lopass_scanorder[index]] = value;
  const unsigned int total = ++image->lopass_scantotals[index];
  if (scanIndex > 1 && total > image->lopass_scantotals[index - 1]) {
    std::swap(image->lopass_scantotals[index],
              image->lopass_scantotals[index - 1]);
    std::swap(image->lopass_scanorder[index],
              image->lopass_scanorder[index - 1]);
  }
}

void ReadQuantizers(jxr_image* image, rbitstream* stream,
                    std::uint8_t quantizers[16][16],
                    unsigned int quantizerCount) {
  for (unsigned int quantizer = 0; quantizer < quantizerCount; ++quantizer) {
    const int mode = image->num_channels == 1
                         ? 0
                         : static_cast<int>(_jxr_rbitstream_uint2(stream));
    if (mode == 0) {
      quantizers[0][quantizer] = _jxr_rbitstream_uint8(stream);
      for (unsigned int channel = 1; channel < image->num_channels; ++channel)
        quantizers[channel][quantizer] = quantizers[0][quantizer];
    } else if (mode == 1) {
      quantizers[0][quantizer] = _jxr_rbitstream_uint8(stream);
      quantizers[1][quantizer] = _jxr_rbitstream_uint8(stream);
      for (unsigned int channel = 2; channel < image->num_channels; ++channel)
        quantizers[channel][quantizer] = quantizers[1][quantizer];
    } else if (mode == 2) {
      for (unsigned int channel = 0; channel < image->num_channels; ++channel)
        quantizers[channel][quantizer] = _jxr_rbitstream_uint8(stream);
    }
  }
}

}  // namespace

void jxr_output_mb_id_mono(jxr_image* image, int macroblockX,
                           std::uint8_t* buffer, int byteStride) {
  _jxr_output_mb_id_mono(image, macroblockX, buffer, byteStride);
}

void jxr_output_mb_id_grayscale(jxr_image* image, int macroblockX,
                                std::uint8_t* buffer, int byteStride) {
  _jxr_output_mb_id_grayscale(image, macroblockX, buffer, byteStride);
}

void jxr_output_mb_id_rgb(jxr_image* image, int macroblockX,
                          std::uint8_t* buffer, int byteStride) {
  _jxr_output_mb_id_rgb(image, macroblockX, buffer, byteStride);
}

void jxr_output_mb_id_cocg_y(jxr_image* image, int macroblockX,
                             std::uint8_t* buffer, int byteStride) {
  _jxr_output_mb_id_cocg_y(image, macroblockX, buffer, byteStride);
}

void jxr_output_mb_id_normal_y_x(jxr_image* image, int macroblockX,
                                 std::uint8_t* buffer, int byteStride) {
  _jxr_output_mb_id_normal_y_x(image, macroblockX, buffer, byteStride);
}

int r_image_header(jxr_image* image, rbitstream* stream) {
  constexpr char signature[9] = "WMPHOTO";
  for (int index = 0; index < 8; ++index) {
    if (_jxr_rbitstream_uint8(stream) !=
        static_cast<std::uint8_t>(signature[index]))
      return -2;
  }
  (void)_jxr_rbitstream_uint4(stream);
  image->disableTileOverlapFlag = _jxr_rbitstream_uint1(stream);
  (void)_jxr_rbitstream_uint3(stream);
  image->header_flags1 = _jxr_rbitstream_uint8(stream);
  image->header_flags2 = _jxr_rbitstream_uint8(stream);
  image->header_flags_fmt = _jxr_rbitstream_uint8(stream);

  if ((image->header_flags2 & 0x80U) == 0) {
    image->width1 = _jxr_rbitstream_uint32(stream);
    image->height1 = _jxr_rbitstream_uint32(stream);
  } else {
    image->width1 = _jxr_rbitstream_uint16(stream);
    image->height1 = _jxr_rbitstream_uint16(stream);
  }

  if (jxr_get_TILING_FLAG(image) != 0) {
    image->tile_columns = _jxr_rbitstream_uint12(stream) + 1;
    image->tile_rows = _jxr_rbitstream_uint12(stream) + 1;
  } else {
    image->tile_columns = 1;
    image->tile_rows = 1;
  }

  image->tile_column_width = static_cast<unsigned int*>(
      _jxr_temp_calloc(image, 2ULL * image->tile_columns, 4));
  image->tile_column_position =
      image->tile_column_width + image->tile_columns;
  image->tile_row_height = static_cast<unsigned int*>(
      _jxr_temp_calloc(image, 2ULL * image->tile_rows, 4));
  image->tile_row_position = image->tile_row_height + image->tile_rows;

  unsigned int columnPosition = 0;
  for (unsigned int column = 0; column + 1 < image->tile_columns; ++column) {
    image->tile_column_width[column] =
        (image->header_flags2 & 0x80U) == 0
            ? _jxr_rbitstream_uint16(stream)
            : _jxr_rbitstream_uint8(stream);
    image->tile_column_position[column] = columnPosition;
    columnPosition += image->tile_column_width[column];
  }

  unsigned int rowPosition = 0;
  for (unsigned int row = 0; row + 1 < image->tile_rows; ++row) {
    image->tile_row_height[row] =
        (image->header_flags2 & 0x80U) == 0
            ? _jxr_rbitstream_uint16(stream)
            : _jxr_rbitstream_uint8(stream);
    image->tile_row_position[row] = rowPosition;
    rowPosition += image->tile_row_height[row];
  }

  if ((image->header_flags2 & 0x20U) != 0) {
    image->window_extra_top = _jxr_rbitstream_uint6(stream);
    image->window_extra_left = _jxr_rbitstream_uint6(stream);
    image->window_extra_bottom = _jxr_rbitstream_uint6(stream);
    image->window_extra_right = _jxr_rbitstream_uint6(stream);
  } else {
    image->window_extra_top = 0;
    image->window_extra_left = 0;
    const unsigned int heightRemainder = (image->height1 + 1) & 15U;
    const unsigned int widthRemainder = (image->width1 + 1) & 15U;
    image->window_extra_bottom =
        static_cast<std::uint16_t>(heightRemainder == 0 ? 0
                                                        : 16 - heightRemainder);
    image->window_extra_right =
        static_cast<std::uint16_t>(widthRemainder == 0 ? 0
                                                       : 16 - widthRemainder);
  }

  image->lwf_test = 0;
  image->extended_width = image->window_extra_left + image->window_extra_right +
                          image->width1 + 1;
  image->extended_height = image->window_extra_top + image->window_extra_bottom +
                           image->height1 + 1;
  const unsigned int lastColumn = image->tile_columns - 1;
  image->tile_column_width[lastColumn] =
      (image->extended_width >> 4) - columnPosition;
  image->tile_column_position[lastColumn] = columnPosition;
  const unsigned int lastRow = image->tile_rows - 1;
  image->tile_row_height[lastRow] =
      (image->extended_height >> 4) - rowPosition;
  image->tile_row_position[lastRow] = rowPosition;
  return 0;
}

int _jxr_r_DC_QP(jxr_image* image, rbitstream* stream) {
  const int mode = image->num_channels == 1
                       ? 0
                       : static_cast<int>(_jxr_rbitstream_uint2(stream));
  if (mode == 0) {
    image->dc_quant_ch[0] = _jxr_rbitstream_uint8(stream);
    for (unsigned int channel = 1; channel < image->num_channels; ++channel)
      image->dc_quant_ch[channel] = image->dc_quant_ch[0];
  } else if (mode == 1) {
    image->dc_quant_ch[0] = _jxr_rbitstream_uint8(stream);
    image->dc_quant_ch[1] = _jxr_rbitstream_uint8(stream);
    for (unsigned int channel = 2; channel < image->num_channels; ++channel)
      image->dc_quant_ch[channel] = image->dc_quant_ch[1];
  } else if (mode == 2) {
    for (unsigned int channel = 0; channel < image->num_channels; ++channel)
      image->dc_quant_ch[channel] = _jxr_rbitstream_uint8(stream);
  }
  return 0;
}

int _jxr_r_LP_QP(jxr_image* image, rbitstream* stream) {
  ReadQuantizers(image, stream, image->lp_quant_ch, image->num_lp_qps);
  return 0;
}

int r_HP_QP(jxr_image* image, rbitstream* stream) {
  ReadQuantizers(image, stream, image->hp_quant_ch, image->num_hp_qps);
  return 0;
}

int r_INDEX_TABLE(jxr_image* image, rbitstream* stream) {
  if ((image->header_flags1 & 4U) == 0)
    return 0;
  const std::uint8_t first = _jxr_rbitstream_uint8(stream);
  const std::uint8_t second = _jxr_rbitstream_uint8(stream);
  if (first != 0 || second != 1)
    return -1;

  int count = static_cast<int>(image->tile_rows * image->tile_columns);
  if ((image->header_flags1 & 0x40U) != 0)
    count *= image->bands_present == 4 ? 4 : 4 - image->bands_present;
  image->tile_index_table_length = count;
  image->tile_index_table = static_cast<std::int64_t*>(
      _jxr_temp_calloc(image, static_cast<std::uint64_t>(count), 8));
  for (int index = 0; index < count; ++index)
    image->tile_index_table[index] =
        static_cast<std::int64_t>(_jxr_rbitstream_intVLW(stream));
  return 0;
}

int r_PROFILE_LEVEL_INFO(jxr_image* image, rbitstream* stream) {
  int bytes = 0;
  do {
    image->profile_idc = _jxr_rbitstream_uint8(stream);
    image->level_idc = _jxr_rbitstream_uint8(stream);
    (void)_jxr_rbitstream_uint15(stream);
    bytes += 4;
  } while (_jxr_rbitstream_uint1(stream) == 0);
  return bytes;
}

int r_TILE_stripe(jxr_image* image, rbitstream* stream) {
  if (image->tile_quant == nullptr) {
    image->tile_quant = static_cast<jxr_tile_qp*>(_jxr_temp_calloc(
        image, image->tile_rows * image->tile_columns, sizeof(jxr_tile_qp)));
    if (image->tile_quant == nullptr)
      return -6;
  }

  auto finish = [image](int result) {
    // Retail calls the canonical no-op fold at RVA 0x1000 here; temporary
    // storage belongs to the image arena and is released by resetting it.
    image->tile_quant = nullptr;
    return result;
  };

  if ((image->header_flags1 & 0x40U) == 0) {
    for (;;) {
      const int result = _jxr_r_TILE_SPATIAL_stripe(
          image, stream, image->stripe_tx, image->stripe_ty);
      if (result < 0) {
        if (result != -256)
          return finish(result);
        ++image->stripe_ty;
        image->stripe_tx = 0;
        if (image->stripe_ty >= image->tile_rows)
          return finish(result);
        return 0;
      }
      if (result == 0)
        return 0;
      ++image->stripe_tx;
      if (image->stripe_tx >= image->tile_columns)
        return finish(result);
    }
  }

  if (image->freq_buffered_flag == 0) {
    int bandCount;
    switch (image->bands_present) {
      case 0:
        bandCount = 4;
        break;
      case 1:
        bandCount = 3;
        break;
      case 2:
        bandCount = 2;
        break;
      case 3:
        bandCount = 1;
        break;
      default:
        bandCount = 0;
        break;
    }

    int tileNumber = 0;
    for (unsigned int tileY = 0; tileY < image->tile_rows; ++tileY) {
      for (unsigned int tileX = 0; tileX < image->tile_columns;
           ++tileX, ++tileNumber) {
        _jxr_rbitstream_seek(stream,
                             image->tile_index_table[bandCount * tileNumber]);
        const int result = _jxr_r_TILE_DC(image, stream, tileX, tileY);
        if (result < 0)
          return finish(result);
      }
    }

    if (bandCount > 1) {
      tileNumber = 0;
      for (unsigned int tileY = 0; tileY < image->tile_rows; ++tileY) {
        for (unsigned int tileX = 0; tileX < image->tile_columns;
             ++tileX, ++tileNumber) {
          _jxr_rbitstream_seek(
              stream, image->tile_index_table[bandCount * tileNumber + 1]);
          const int result = _jxr_r_TILE_LP(image, stream, tileX, tileY);
          if (result < 0)
            return finish(result);
        }
      }
    }

    if (bandCount > 2) {
      tileNumber = 0;
      for (unsigned int tileY = 0; tileY < image->tile_rows; ++tileY) {
        for (unsigned int tileX = 0; tileX < image->tile_columns;
             ++tileX, ++tileNumber) {
          _jxr_rbitstream_seek(
              stream, image->tile_index_table[bandCount * tileNumber + 2]);
          const int result = _jxr_r_TILE_HP(image, stream, tileX, tileY);
          if (result < 0)
            return finish(result);
        }
      }
    }

    if (bandCount > 3) {
      tileNumber = 0;
      for (unsigned int tileY = 0; tileY < image->tile_rows; ++tileY) {
        for (unsigned int tileX = 0; tileX < image->tile_columns;
             ++tileX, ++tileNumber) {
          const std::int64_t offset =
              image->tile_index_table[bandCount * tileNumber + 3];
          if (offset < 0) {
            (void)_jxr_r_TILE_FLEXBITS_ESCAPE(image, tileX, tileY);
          } else {
            _jxr_rbitstream_seek(stream, static_cast<std::uint64_t>(offset));
            const int result =
                _jxr_r_TILE_FLEXBITS(image, stream, tileX, tileY);
            if (result < 0)
              return finish(result);
          }
        }
      }
    }
    image->freq_buffered_flag = 1;
    image->output_sent = 0;
  }

  const int result = _jxr_frequency_mode_render_stripe(image);
  return result == 0 ? 0 : finish(result);
}

void _jxr_r_TILE_HEADER_DC(jxr_image* image, rbitstream* stream, int,
                           unsigned int tileX, unsigned int tileY) {
  if ((image->component_mode & 2U) != 0)
    return;
  _jxr_r_DC_QP(image, stream);
  std::memcpy(image->tile_quant[tileX + tileY * image->tile_columns].dc_quant_ch,
              image->dc_quant_ch, sizeof(image->dc_quant_ch));
}

void _jxr_r_TILE_HEADER_LOWPASS(jxr_image* image, rbitstream* stream, int,
                                unsigned int tileX, unsigned int tileY) {
  if ((image->component_mode & 8U) != 0)
    return;
  image->component_mode =
      (image->component_mode & ~4U) | ((_jxr_rbitstream_uint1(stream) & 1) << 2);
  jxr_tile_qp& tile =
      image->tile_quant[tileX + tileY * image->tile_columns];
  if ((image->component_mode & 4U) != 0) {
    image->num_lp_qps = 1;
    for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
      image->lp_quant_ch[channel][0] = image->dc_quant_ch[channel];
      tile.lp_quant_ch[channel][0] = image->dc_quant_ch[channel];
    }
  } else {
    image->num_lp_qps = _jxr_rbitstream_uint4(stream) + 1;
    _jxr_r_LP_QP(image, stream);
    std::memcpy(tile.lp_quant_ch, image->lp_quant_ch,
                sizeof(image->lp_quant_ch));
  }
}

void _jxr_r_TILE_HEADER_HIGHPASS(jxr_image* image, rbitstream* stream, int,
                                 unsigned int tileX, unsigned int tileY) {
  if ((image->component_mode & 0x20U) != 0)
    return;
  image->component_mode = (image->component_mode & ~0x10U) |
                          ((_jxr_rbitstream_uint1(stream) & 1) << 4);
  jxr_tile_qp& tile =
      image->tile_quant[tileX + tileY * image->tile_columns];
  if ((image->component_mode & 0x10U) != 0) {
    image->num_hp_qps = image->num_lp_qps;
    for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
      std::memcpy(image->hp_quant_ch[channel], image->lp_quant_ch[channel], 16);
      std::memcpy(tile.hp_quant_ch[channel], image->lp_quant_ch[channel], 16);
    }
  } else {
    image->num_hp_qps = _jxr_rbitstream_uint4(stream) + 1;
    r_HP_QP(image, stream);
    std::memcpy(tile.hp_quant_ch, image->hp_quant_ch,
                sizeof(image->hp_quant_ch));
  }
}

unsigned int _jxr_DECODE_QP_INDEX(rbitstream* stream,
                                  unsigned int indexCount) {
  if (_jxr_rbitstream_uint1(stream) == 0)
    return 0;
  return _jxr_rbitstream_uintN(stream, kBitsPerQpIndex[indexCount]) + 1;
}

int r_REFINE_CBP(rbitstream* stream, int count) {
  if (count == 1)
    return 1 << _jxr_rbitstream_uint2(stream);
  if (count == 2) {
    if (_jxr_rbitstream_uint1(stream) == 0)
      return _jxr_rbitstream_uint1(stream) == 0 ? 3 : 5;
    if (_jxr_rbitstream_uint1(stream) == 0)
      return _jxr_rbitstream_uint1(stream) == 0 ? 6 : 9;
    return _jxr_rbitstream_uint1(stream) == 0 ? 10 : 12;
  }
  if (count == 3)
    return (1 << _jxr_rbitstream_uint2(stream)) ^ 15;
  return count == 4 ? 15 : 0;
}

int r_DECODE_FIRST_INDEX(jxr_image* image, rbitstream* stream,
                         int chromaFlag, int band) {
  int vlcIndex = 0;
  if (band == 1)
    vlcIndex = chromaFlag != 0 ? 9 : 2;
  else if (band == 2)
    vlcIndex = 16 + (chromaFlag != 0);

  int result = 0;
  const int table = image->vlc_table[vlcIndex].table;
  switch (table) {
    case 0:
      result = _jxr_rbitstream_uint1(stream)
                   ? 7
                   : _jxr_rbitstream_intE(stream, 6,
                                          kFirstIndexCodeLengths0,
                                          kFirstIndexValues0);
      break;
    case 1:
      result = _jxr_rbitstream_intE(stream, 6, kFirstIndexCodeLengths1,
                                    kFirstIndexValues1);
      break;
    case 2:
      result = _jxr_rbitstream_intE(stream, 7, kFirstIndexCodeLengths2,
                                    kFirstIndexValues2);
      break;
    case 3:
      result = _jxr_rbitstream_intE(stream, 7, kFirstIndexCodeLengths3,
                                    kFirstIndexValues3);
      break;
    case 4:
      result = _jxr_rbitstream_uint1(stream)
                   ? 1
                   : _jxr_rbitstream_intE(stream, 7,
                                          kFirstIndexCodeLengths4,
                                          kFirstIndexValues4);
      break;
    default:
      break;
  }

  jxr_vlc_table_t& vlc = image->vlc_table[vlcIndex];
  vlc.discriminant += kFirstIndexDelta[vlc.deltatable][result];
  vlc.discriminant2 += kFirstIndexDelta[vlc.delta2table][result];
  return result;
}

int r_DECODE_INDEX(jxr_image* image, rbitstream* stream, int location,
                   int chromaFlag, int band, int context) {
  int vlcIndex = 0;
  if (band == 1)
    vlcIndex = (chromaFlag != 0 ? 10 : 7) + (context != 0);
  else if (band == 2)
    vlcIndex = (chromaFlag != 0 ? 18 : 14) + (context != 0);

  if (location > 15)
    return _jxr_rbitstream_uint1(stream);
  if (location == 15) {
    int result = 0;
    if (_jxr_rbitstream_uint1(stream)) {
      if (!_jxr_rbitstream_uint1(stream))
        return 2;
      result = 3;
      if (!_jxr_rbitstream_uint1(stream))
        return 1;
    }
    return result;
  }

  int result = 0;
  switch (image->vlc_table[vlcIndex].table) {
    case 0:
      if (!_jxr_rbitstream_uint1(stream)) {
        if (_jxr_rbitstream_uint1(stream)) {
          result = 4;
        } else if (_jxr_rbitstream_uint1(stream)) {
          result = 2;
        } else if (_jxr_rbitstream_uint1(stream)) {
          result = 5;
        } else {
          result = _jxr_rbitstream_uint1(stream) ? 3 : 1;
        }
      }
      break;
    case 1: {
      const unsigned int code = _jxr_rbitstream_uint2(stream);
      if (code == 0) {
        result = _jxr_rbitstream_uint1(stream)
                     ? 5
                     : (_jxr_rbitstream_uint1(stream) ? 3 : 1);
      } else if (code == 2) {
        result = 2;
      } else if (code == 3) {
        result = 4;
      }
      break;
    }
    case 2: {
      const unsigned int code = _jxr_rbitstream_uint2(stream);
      if (code == 0) {
        result = _jxr_rbitstream_uint1(stream)
                     ? 5
                     : (_jxr_rbitstream_uint1(stream) ? 1 : 0);
      } else {
        result = static_cast<int>(code) + 1;
      }
      break;
    }
    case 3:
      if (_jxr_rbitstream_uint1(stream)) {
        result = 3;
      } else if (_jxr_rbitstream_uint1(stream)) {
        result = 2;
      } else if (_jxr_rbitstream_uint1(stream)) {
        result = 5;
      } else if (_jxr_rbitstream_uint1(stream)) {
        result = 4;
      } else {
        result = _jxr_rbitstream_uint1(stream) ? 1 : 0;
      }
      break;
    default:
      break;
  }

  jxr_vlc_table_t& vlc = image->vlc_table[vlcIndex];
  vlc.discriminant += kIndexDelta[vlc.deltatable][result];
  vlc.discriminant2 += kIndexDelta[vlc.delta2table][result];
  return result;
}

int r_DECODE_ABS_LEVEL(jxr_image* image, rbitstream* stream, int band,
                       int chromaFlag) {
  constexpr int remap[6] = {2, 3, 4, 6, 10, 14};
  constexpr int fixedLength[6] = {0, 0, 1, 2, 2, 2};
  const int vlcIndex = _jxr_vlc_select(band, chromaFlag);
  const bool secondTable = image->vlc_table[vlcIndex].table != 0;
  const int index = _jxr_rbitstream_intE(
      stream, 6,
      secondTable ? kAbsLevelCodeLengths1 : kAbsLevelCodeLengths0,
      secondTable ? kAbsLevelValues1 : kAbsLevelValues0);
  image->vlc_table[vlcIndex].discriminant += kAbsLevelIndexDelta[index];

  unsigned int suffix = 0;
  if (index < 6) {
    const int bitCount = fixedLength[index];
    for (int bit = 0; bit < bitCount; ++bit)
      suffix = (suffix << 1) |
               static_cast<unsigned int>(_jxr_rbitstream_uint1(stream));
    return remap[index] + static_cast<int>(suffix);
  }

  int bitCount = _jxr_rbitstream_uint4(stream) + 4;
  if (bitCount == 19) {
    bitCount = _jxr_rbitstream_uint2(stream) + 19;
    if (bitCount == 22)
      bitCount = _jxr_rbitstream_uint3(stream) + 22;
  }
  for (int bit = 0; bit < bitCount; ++bit)
    suffix = (suffix << 1) |
             static_cast<unsigned int>(_jxr_rbitstream_uint1(stream));
  return static_cast<int>((1U << bitCount) + suffix + 2);
}

int r_DECODE_BLOCK(jxr_image* image, rbitstream* stream, int chromaFlag,
                   int* coefficients, int band, int location) {
  unsigned int coefficientCount = 1;
  int index = r_DECODE_FIRST_INDEX(image, stream, chromaFlag, band);
  int continuation = index >> 2;
  int context = index & continuation & 1;

  const bool negative = _jxr_rbitstream_uint1(stream);
  int value = (index & 2) != 0
                  ? r_DECODE_ABS_LEVEL(image, stream, band, context)
                  : 1;
  if (negative)
    value = -value;
  coefficients[1] = value;
  coefficients[0] =
      (index & 1) != 0 ? 0 : r_DECODE_RUN(stream, 15 - location);
  location += coefficients[0] + 1;

  int* runOutput = coefficients + 2;
  int* valueOutput = coefficients + 3;
  while (continuation != 0) {
    const int run = (continuation & 1) != 0
                        ? 0
                        : r_DECODE_RUN(stream, 15 - location);
    location += run + 1;
    *runOutput = run;

    index = r_DECODE_INDEX(image, stream, location, chromaFlag, band, context);
    continuation = index >> 1;
    const int nextContext = continuation & context;
    const bool nextNegative = _jxr_rbitstream_uint1(stream);
    value = (index & 1) != 0
                ? r_DECODE_ABS_LEVEL(image, stream, band, nextContext)
                : 1;
    if (nextNegative)
      value = -value;
    *valueOutput = value;
    context = nextContext;
    ++coefficientCount;
    runOutput += 2;
    valueOutput += 2;
  }
  return static_cast<int>(coefficientCount);
}

int r_DECODE_BLOCK_ADAPTIVE(jxr_image* image, rbitstream* stream, int tileX,
                            unsigned int macroblockX, int cbpFlag,
                            int chromaFlag, int channel, int block,
                            int macroblockHpMode, char modelBits) {
  int runLevelCoefficients[32] = {};
  if (cbpFlag == 0)
    return 0;

  int hpInput[16] = {};
  const int coefficientCount = r_DECODE_BLOCK(
      image, stream, chromaFlag, runLevelCoefficients, 2, 1);
  int scanLocation = 1;
  for (int coefficient = 0; coefficient < coefficientCount; ++coefficient) {
    scanLocation += runLevelCoefficients[2 * coefficient];
    if (scanLocation >= 16)
      return -1;
    AdaptiveHPScan(image, hpInput, scanLocation,
                   runLevelCoefficients[2 * coefficient + 1],
                   macroblockHpMode);
    ++scanLocation;
  }

  int* destination =
      image->strip[channel]
          .cur[macroblockX + image->tile_column_position[tileX]]
          .data +
      16 + 15 * block;
  if ((image->user_flags & 1) != 0) {
    std::memset(destination, 0, 15 * sizeof(*destination));
  } else {
    for (int coefficient = 0; coefficient < 15; ++coefficient)
      destination[coefficient] = hpInput[coefficient + 1] << modelBits;
  }
  return coefficientCount;
}

void _jxr_r_MB_LP(jxr_image* image, rbitstream* stream, int, int tileX,
                  unsigned int tileY, unsigned int macroblockX,
                  unsigned int macroblockY) {
  int lpInput[16][16] = {};
  int lapMean[16] = {};
  if (_jxr_InitContext(image, tileX, tileY, macroblockX, macroblockY) != 0) {
    _jxr_InitializeCountCBPLP(image);
    _jxr_InitLPVLC(image);
    _jxr_InitializeAdaptiveScanLP(image);
    _jxr_InitializeModelMB(&image->model_lp, 1);
  }
  if (_jxr_ResetTotals(image, static_cast<char>(macroblockX)) != 0)
    _jxr_ResetTotalsAdaptiveScanLP(image);

  int logicalChannelCount = image->num_channels;
  int codedBlockPattern = 0;
  if (image->use_clr_fmt == 1 || image->use_clr_fmt == 2 ||
      image->use_clr_fmt == 3) {
    if (image->use_clr_fmt == 1 || image->use_clr_fmt == 2)
      logicalChannelCount = 2;
    const int maximumCbp = 4 * logicalChannelCount - 5;
    if (image->count_zero_CBPLP <= 0 || image->count_max_CBPLP < 0) {
      int symbol;
      if (image->use_clr_fmt <= 2) {
        if (!_jxr_rbitstream_uint1(stream)) {
          symbol = 0;
        } else if (!_jxr_rbitstream_uint1(stream)) {
          symbol = 1;
        } else {
          symbol = _jxr_rbitstream_uint1(stream) ? 3 : 2;
        }
      } else {
        symbol = _jxr_rbitstream_intE(stream, 4, kLpCbpCodeLengths,
                                      kLpCbpValues);
      }
      codedBlockPattern = image->count_max_CBPLP >= image->count_zero_CBPLP
                              ? symbol
                              : maximumCbp - symbol;
    } else {
      codedBlockPattern = static_cast<int>(
          _jxr_rbitstream_uintN(stream, logicalChannelCount));
    }
    _jxr_UpdateCountCBPLP(image, codedBlockPattern, maximumCbp);
  } else {
    for (unsigned int channel = 0; channel < image->num_channels; ++channel)
      codedBlockPattern |= _jxr_rbitstream_uint1(stream) << channel;
  }

  for (int logicalChannel = 0; logicalChannel < logicalChannelCount;
       ++logicalChannel) {
    const bool chroma = logicalChannel > 0;
    int nonzeroCount = 0;
    if (((codedBlockPattern >> logicalChannel) & 1) != 0) {
      int runLevelCoefficients[32] = {};
      int location = 1;
      if (chroma) {
        if (image->use_clr_fmt == 1)
          location = 10;
        else if (image->use_clr_fmt == 2)
          location = 2;
      }
      nonzeroCount = r_DECODE_BLOCK(image, stream, chroma,
                                    runLevelCoefficients, 1, location);
      if (chroma &&
          (image->use_clr_fmt == 1 || image->use_clr_fmt == 2)) {
        int temporary[14] = {};
        int scanLocation = 0;
        for (int coefficient = 0; coefficient < nonzeroCount; ++coefficient) {
          scanLocation += runLevelCoefficients[2 * coefficient];
          temporary[scanLocation] = runLevelCoefficients[2 * coefficient + 1];
          ++scanLocation;
        }
        const int remapOffset = image->use_clr_fmt == 1 ? 1 : 0;
        const int chromaCount = image->use_clr_fmt == 1 ? 6 : 14;
        for (int coefficient = 0; coefficient < chromaCount; ++coefficient) {
          const int channel = (coefficient & 1) + 1;
          const int remapped =
              kChromaLpRemap[remapOffset + coefficient / 2];
          const int destination = image->use_clr_fmt == 1
                                      ? kTranspose420[remapped]
                                      : kTranspose422[remapped];
          lpInput[channel][destination] = temporary[coefficient];
        }
      } else {
        int scanLocation = 1;
        for (int coefficient = 0; coefficient < nonzeroCount; ++coefficient) {
          scanLocation += runLevelCoefficients[2 * coefficient];
          AdaptiveLPScan(image, lpInput[logicalChannel], scanLocation,
                         runLevelCoefficients[2 * coefficient + 1]);
          ++scanLocation;
        }
      }
    }

    const int modelBits = image->model_lp.bits[logicalChannel];
    lapMean[logicalChannel] += nonzeroCount;
    if (modelBits != 0) {
      if (!chroma || (image->use_clr_fmt != 1 && image->use_clr_fmt != 2)) {
        for (int scanIndex = 1; scanIndex < 16; ++scanIndex) {
          const int coefficient = kTranspose444[scanIndex];
          lpInput[logicalChannel][coefficient] = RefineLowpassValue(
              stream, lpInput[logicalChannel][coefficient], modelBits);
        }
      } else if (image->use_clr_fmt == 1) {
        for (int scanIndex = 1; scanIndex < 4; ++scanIndex) {
          const int coefficient = kTranspose420[scanIndex];
          lpInput[1][coefficient] =
              RefineLowpassValue(stream, lpInput[1][coefficient], modelBits);
          lpInput[2][coefficient] =
              RefineLowpassValue(stream, lpInput[2][coefficient], modelBits);
        }
      } else {
        for (int scanIndex = 1; scanIndex < 8; ++scanIndex) {
          const int coefficient = kTranspose422[scanIndex];
          lpInput[1][coefficient] =
              RefineLowpassValue(stream, lpInput[1][coefficient], modelBits);
          lpInput[2][coefficient] =
              RefineLowpassValue(stream, lpInput[2][coefficient], modelBits);
        }
      }
    }

    const unsigned int column =
        macroblockX + image->tile_column_position[tileX];
    if (chroma && image->use_clr_fmt == 1) {
      for (int coefficient = 1; coefficient < 4; ++coefficient) {
        image->strip[1].cur[column].data[coefficient] =
            lpInput[1][coefficient];
        image->strip[2].cur[column].data[coefficient] =
            lpInput[2][coefficient];
      }
    } else if (chroma && image->use_clr_fmt == 2) {
      for (int coefficient = 1; coefficient < 8; ++coefficient) {
        image->strip[1].cur[column].data[coefficient] =
            lpInput[1][coefficient];
        image->strip[2].cur[column].data[coefficient] =
            lpInput[2][coefficient];
      }
    } else {
      std::memcpy(image->strip[logicalChannel].cur[column].data + 1,
                  lpInput[logicalChannel] + 1, 15 * sizeof(int));
    }
  }

  _jxr_UpdateModelMB(image, reinterpret_cast<char*>(lapMean),
                     &image->model_lp, 1);
  if (_jxr_ResetContext(image, tileX, macroblockX) != 0)
    _jxr_AdaptLP(image);
}

int _jxr_r_MB_HP(jxr_image* image, rbitstream* stream, int, int tileX,
                 unsigned int tileY, unsigned int macroblockX,
                 unsigned int macroblockY) {
  if (_jxr_InitContext(image, tileX, tileY, macroblockX, macroblockY) != 0) {
    _jxr_InitHPVLC(image);
    _jxr_InitializeAdaptiveScanHP(image);
    _jxr_InitializeModelMB(&image->model_hp, 2);
  }
  if (_jxr_ResetTotals(image, static_cast<char>(macroblockX)) != 0)
    _jxr_ResetTotalsAdaptiveScanHP(image);

  int lapMean[16] = {};
  const bool embeddedFlexbits = image->bands_present != 1 &&
                                (image->header_flags1 & 0x40U) == 0;
  const int macroblockHpMode =
      r_calculate_mbhp_mode(image, tileX, macroblockX, 0);
  const unsigned int column =
      macroblockX + image->tile_column_position[tileX];

  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    int blockScale = 4;
    const bool chroma = channel > 0;
    if (chroma) {
      if (image->use_clr_fmt == 1)
        blockScale = 1;
      else if (image->use_clr_fmt == 2)
        blockScale = 2;
    }
    const int modelChannel =
        (image->use_clr_fmt == 1 || image->use_clr_fmt == 2)
            ? static_cast<int>(chroma)
            : static_cast<int>(channel);
    const unsigned int modelBits = image->model_hp.bits[modelChannel];
    int codedBlockPattern = image->strip[channel].cur[column].hp_cbp;
    for (int scanBlock = 0; scanBlock < 4 * blockScale; ++scanBlock) {
      const unsigned int block = blockScale == 4
                                     ? kHpScanMap[scanBlock]
                                     : static_cast<unsigned int>(scanBlock);
      const int nonzeroCount = r_DECODE_BLOCK_ADAPTIVE(
          image, stream, tileX, macroblockX, codedBlockPattern & 1, chroma,
          static_cast<int>(channel), static_cast<int>(block),
          macroblockHpMode, static_cast<char>(modelBits));
      if (nonzeroCount < 0)
        return -1;
      if (embeddedFlexbits) {
        r_BLOCK_FLEXBITS(image, stream, tileX, macroblockX,
                         static_cast<int>(channel), block, modelBits);
      }
      lapMean[channel] += nonzeroCount;
      codedBlockPattern >>= 1;
    }
  }

  int predictionChannelCount = image->num_channels;
  if (image->use_clr_fmt == 1 || image->use_clr_fmt == 2)
    predictionChannelCount = 1;
  if ((image->header_flags1 & 0x40U) == 0 || image->bands_present == 1) {
    for (int channel = 0; channel < predictionChannelCount; ++channel) {
      _jxr_propagate_hp_predictions(image, channel, tileX, macroblockX,
                                    macroblockHpMode);
    }
  }

  image->strip[0].cur[column].mbhp_pred_mode = macroblockHpMode;
  for (int channel = 0; channel < 16; ++channel) {
    image->strip[0].cur[column].hp_model_bits[channel] =
        image->model_hp.bits[channel];
  }
  _jxr_UpdateModelMB(image, reinterpret_cast<char*>(lapMean),
                     &image->model_hp, 2);
  if (_jxr_ResetContext(image, tileX, macroblockX) != 0)
    _jxr_AdaptHP(image);
  return 0;
}

void _jxr_r_MB_DC(jxr_image* image, rbitstream* stream, int,
                  unsigned int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY) {
  int lapMean[16] = {};
  if (_jxr_InitContext(image, tileX, tileY, macroblockX, macroblockY) != 0) {
    _jxr_InitVLCTable(image, 0);
    _jxr_InitVLCTable(image, 1);
    _jxr_InitializeModelMB(&image->model_dc, 0);
  }

  const unsigned int column =
      macroblockX + image->tile_column_position[tileX];
  const auto decodeDc = [image, stream, &lapMean](int modelChannel,
                                                  bool nonzero,
                                                  bool chroma) {
    int value = 0;
    if (nonzero) {
      ++lapMean[modelChannel];
      value = r_DECODE_ABS_LEVEL(image, stream, 0, chroma) - 1;
    }
    const int modelBits = image->model_dc.bits[modelChannel];
    for (int bit = 0; bit < modelBits; ++bit) {
      value = static_cast<int>(_jxr_rbitstream_uint1(stream)) | (2 * value);
    }
    if (value != 0 && _jxr_rbitstream_uint1(stream))
      value = -value;
    return value;
  };

  if (image->use_clr_fmt == 0 || image->use_clr_fmt == 4 ||
      image->use_clr_fmt == 6) {
    for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
      const bool nonzero = _jxr_rbitstream_uint1(stream);
      image->strip[channel].cur[column].data[0] =
          decodeDc(static_cast<int>(channel), nonzero, false);
    }
  } else {
    const int dcMask = get_is_dc_yuv(stream);
    const int thirdModelChannel = image->use_clr_fmt == 3 ? 2 : 1;
    image->strip[0].cur[column].data[0] =
        decodeDc(0, (dcMask & 4) != 0, false);
    image->strip[1].cur[column].data[0] =
        decodeDc(1, (dcMask & 2) != 0, true);
    image->strip[2].cur[column].data[0] =
        decodeDc(thirdModelChannel, (dcMask & 1) != 0, true);
  }

  _jxr_UpdateModelMB(image, reinterpret_cast<char*>(lapMean),
                     &image->model_dc, 0);
  if (_jxr_ResetContext(image, tileX, macroblockX) != 0) {
    _jxr_AdaptVLCTable(image, 0);
    _jxr_AdaptVLCTable(image, 1);
  }
}

int r_DECODE_RUN(rbitstream* stream, int maximumRun) {
  unsigned int run = 0;
  if (maximumRun >= 5) {
    if (_jxr_rbitstream_uint1(stream) == 0) {
      if (_jxr_rbitstream_uint1(stream) != 0) {
        run = 1;
      } else if (_jxr_rbitstream_uint1(stream) != 0) {
        run = 2;
      } else {
        run = 3 + (_jxr_rbitstream_uint1(stream) != 0);
      }
    }
    const int tableIndex = 5 * kRunBin[maximumRun] + static_cast<int>(run);
    run = static_cast<unsigned int>(kRunRemap[tableIndex]);
    if (kRunFixedLength[tableIndex] != 0)
      run += _jxr_rbitstream_uintN(stream, kRunFixedLength[tableIndex]);
    return static_cast<int>(run);
  }
  switch (maximumRun) {
    case 1:
      return 1;
    case 2:
      return 2 - (_jxr_rbitstream_uint1(stream) != 0);
    case 3:
      return _jxr_rbitstream_uint1(stream) != 0
                 ? 1
                 : 3 - (_jxr_rbitstream_uint1(stream) != 0);
    case 4:
      if (_jxr_rbitstream_uint1(stream) != 0)
        return 1;
      if (_jxr_rbitstream_uint1(stream) != 0)
        return 2;
      return 4 - (_jxr_rbitstream_uint1(stream) != 0);
    default:
      return 0;
  }
}

void r_PredCBP(jxr_image* image, int* differentialCbp, unsigned int tileX,
               unsigned int tileY, unsigned int macroblockX,
               unsigned int macroblockY) {
  if (_jxr_InitContext(image, tileX, tileY, macroblockX, macroblockY) != 0)
    _jxr_InitializeCBPModel(image);
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  if (image->use_clr_fmt == 1) {
    image->strip[0].cur[column].hp_cbp = _jxr_PredCBP444(
        image, differentialCbp, 0, tileX, macroblockX, macroblockY);
    image->strip[1].cur[column].hp_cbp = _jxr_PredCBP420(
        image, differentialCbp, 1, tileX, macroblockX, macroblockY);
    image->strip[2].cur[column].hp_cbp = _jxr_PredCBP420(
        image, differentialCbp, 2, tileX, macroblockX, macroblockY);
  } else if (image->use_clr_fmt == 2) {
    image->strip[0].cur[column].hp_cbp = _jxr_PredCBP444(
        image, differentialCbp, 0, tileX, macroblockX, macroblockY);
    image->strip[1].cur[column].hp_cbp = _jxr_PredCBP422(
        image, differentialCbp, 1, tileX, macroblockX, macroblockY);
    image->strip[2].cur[column].hp_cbp = _jxr_PredCBP422(
        image, differentialCbp, 2, tileX, macroblockX, macroblockY);
  } else {
    for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
      image->strip[channel].cur[column].hp_cbp = _jxr_PredCBP444(
          image, differentialCbp, static_cast<int>(channel), tileX,
          macroblockX, macroblockY);
    }
  }
}

int _jxr_r_MB_CBP(jxr_image* image, rbitstream* stream, int,
                  unsigned int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY) {
  int differentialCbp[16] = {};
  if (_jxr_InitContext(image, tileX, tileY, macroblockX, macroblockY) != 0)
    _jxr_InitCBPVLC(image);

  const auto decodeFiveSymbol = [image, stream](int vlcIndex) {
    if (_jxr_rbitstream_uint1(stream))
      return 0;
    if (image->vlc_table[vlcIndex].table != 0)
      return static_cast<int>(_jxr_rbitstream_uint2(stream)) + 1;
    if (_jxr_rbitstream_uint1(stream))
      return 1;
    if (_jxr_rbitstream_uint1(stream))
      return 2;
    return _jxr_rbitstream_uint1(stream) ? 4 : 3;
  };
  const auto decodeThreeSymbol = [stream]() {
    if (_jxr_rbitstream_uint1(stream))
      return 0;
    return _jxr_rbitstream_uint1(stream) ? 1 : 2;
  };
  const auto decodeFourSymbol = [stream]() {
    if (_jxr_rbitstream_uint1(stream))
      return 0;
    if (_jxr_rbitstream_uint1(stream))
      return 1;
    return _jxr_rbitstream_uint1(stream) ? 3 : 2;
  };

  const int planeCount =
      image->use_clr_fmt == 4 || image->use_clr_fmt == 6
          ? image->num_channels
          : 1;
  for (int plane = 0; plane < planeCount; ++plane) {
    const int groupCount = decodeFiveSymbol(12);
    image->vlc_table[12].discriminant += kBlockCbpDelta5[groupCount];
    const int groupMask = r_REFINE_CBP(stream, groupCount);

    for (int group = 0; group < 4; ++group) {
      if ((groupMask & (1 << group)) == 0)
        continue;

      int symbol;
      if (image->use_clr_fmt == 0 || image->use_clr_fmt == 4 ||
          image->use_clr_fmt == 6) {
        symbol = decodeFiveSymbol(13);
      } else if (image->vlc_table[13].table != 0) {
        symbol = _jxr_rbitstream_intE(stream, 6, kBlockCbpCodeLengths6,
                                      kBlockCbpValues6);
        if (symbol == 7)
          symbol += _jxr_rbitstream_uint1(stream);
      } else {
        symbol = _jxr_rbitstream_intE(stream, 5, kBlockCbpCodeLengths5,
                                      kBlockCbpValues5);
      }
      image->vlc_table[13].discriminant +=
          image->use_clr_fmt == 0 || image->use_clr_fmt == 4 ||
                  image->use_clr_fmt == 6
              ? kBlockCbpDelta5[symbol]
              : kBlockCbpDelta9[symbol];

      int tableIndex = symbol + 1;
      int highMask = 0;
      if (tableIndex >= 6) {
        int prefix = 0;
        if (!_jxr_rbitstream_uint1(stream))
          prefix = _jxr_rbitstream_uint1(stream) ? 1 : 2;
        highMask = 16 * (prefix + 1);
        if (tableIndex >= 9)
          tableIndex += decodeThreeSymbol();
        tableIndex -= 6;
      }
      int outputIndex = kBlockCbpOffset[tableIndex];
      const int bitCount = kBlockCbpFixedLength[tableIndex];
      if (bitCount != 0)
        outputIndex += static_cast<int>(_jxr_rbitstream_uintN(stream, bitCount));
      const int blockPattern = kBlockCbpOutput[outputIndex] + highMask;
      const int lumaShift = 4 * group;

      switch (image->use_clr_fmt) {
        case 1:
          differentialCbp[0] |= (blockPattern & 15) << lumaShift;
          differentialCbp[1] |= ((blockPattern >> 4) & 1) << group;
          differentialCbp[2] |= ((blockPattern >> 5) & 1) << group;
          break;
        case 2:
          differentialCbp[0] |= (blockPattern & 15) << lumaShift;
          if ((blockPattern & 16) != 0) {
            const int position = decodeThreeSymbol();
            differentialCbp[1] |=
                kSubsampledCbpPosition[position + 1]
                << kSubsampledCbpPosition[group];
          }
          if ((blockPattern & 32) != 0) {
            const int position = decodeThreeSymbol();
            differentialCbp[2] |=
                kSubsampledCbpPosition[position + 1]
                << kSubsampledCbpPosition[group];
          }
          break;
        case 3:
          differentialCbp[0] |= (blockPattern & 15) << lumaShift;
          if ((blockPattern & 16) != 0) {
            const int count = decodeFourSymbol();
            differentialCbp[1] |= r_REFINE_CBP(stream, count + 1)
                                  << lumaShift;
          }
          if ((blockPattern & 32) != 0) {
            const int count = decodeFourSymbol();
            differentialCbp[2] |= r_REFINE_CBP(stream, count + 1)
                                  << lumaShift;
          }
          break;
        default:
          differentialCbp[plane] |= blockPattern << lumaShift;
          break;
      }
    }
  }
  r_PredCBP(image, differentialCbp, tileX, tileY, macroblockX,
            macroblockY);
  return 0;
}

int _jxr_r_MB_FLEXBITS(jxr_image* image, rbitstream* stream, int,
                       int tileX, unsigned int, unsigned int macroblockX,
                       unsigned int) {
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    int blockScale = 4;
    if (channel != 0) {
      if (image->use_clr_fmt == 1)
        blockScale = 1;
      else if (image->use_clr_fmt == 2)
        blockScale = 2;
    }
    const unsigned int modelBits =
        image->strip[0].cur[column].hp_model_bits[channel];
    const unsigned int trim = image->trim_flexbits & 15U;
    for (int scanBlock = 0; scanBlock < 4 * blockScale; ++scanBlock) {
      const unsigned int block = blockScale == 4
                                     ? kHpScanMap[scanBlock]
                                     : static_cast<unsigned int>(scanBlock);
      if (trim < modelBits) {
        const unsigned int flexbits = modelBits - trim;
        for (int scanIndex = 1; scanIndex < 16; ++scanIndex) {
          r_DECODE_FLEX(image, stream, tileX, macroblockX,
                        static_cast<int>(channel), block,
                        kTranspose444[scanIndex] - 1, flexbits);
        }
      }
    }
  }
  return 0;
}

void AdaptiveHPScan(jxr_image* image, int* hpInput, int scanIndex, int value,
                    int macroblockHpMode) {
  const int index = scanIndex - 1;
  unsigned int* totals = macroblockHpMode == 1
                             ? image->hipass_ver_scantotals
                             : image->hipass_hor_scantotals;
  unsigned int* order = macroblockHpMode == 1
                            ? image->hipass_ver_scanorder
                            : image->hipass_hor_scanorder;
  ++totals[index];
  hpInput[order[index]] = value;
  if (scanIndex > 1 && totals[index] > totals[index - 1]) {
    const unsigned int previousTotal = totals[index - 1];
    totals[index - 1] = totals[index];
    totals[index] = previousTotal;
    const unsigned int previousOrder = order[index - 1];
    order[index - 1] = order[index];
    order[index] = previousOrder;
  }
}

void r_DECODE_FLEX(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int macroblockX, int channel, unsigned int block,
                   unsigned int coefficient, unsigned int flexbits) {
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  int& destination =
      image->strip[channel].cur[column].data[15 * block + 16 + coefficient];
  const unsigned int refinement = _jxr_rbitstream_uintN(stream, flexbits);
  int value;
  const int shift = static_cast<int>(image->trim_flexbits & 15U);
  if (destination > 0) {
    value = destination + (static_cast<int>(refinement) << shift);
  } else if (destination < 0) {
    value = destination - (static_cast<int>(refinement) << shift);
  } else {
    int signedRefinement = static_cast<int>(refinement);
    if (refinement != 0 && _jxr_rbitstream_uint1(stream) != 0)
      signedRefinement = -signedRefinement;
    value = signedRefinement << shift;
  }
  if ((image->user_flags & 2) == 0)
    destination = value;
}

void r_BLOCK_FLEXBITS(jxr_image* image, rbitstream* stream, int tileX,
                      unsigned int macroblockX, int channel,
                      unsigned int block, unsigned int modelBits) {
  const unsigned int trim = image->trim_flexbits & 15U;
  if (trim > modelBits)
    return;
  const unsigned int flexbits = modelBits - trim;
  if (flexbits == 0)
    return;
  for (int scanIndex = 1; scanIndex < 16; ++scanIndex) {
    const unsigned int coefficient =
        15 * block + 16 + kTranspose444[scanIndex] - 1;
    const unsigned int column = image->tile_column_position[tileX] +
                                macroblockX;
    int& destination = image->strip[channel].cur[column].data[coefficient];
    const unsigned int refinement = _jxr_rbitstream_uintN(stream, flexbits);
    int value;
    const int shift = static_cast<int>(trim);
    if (destination > 0) {
      value = destination + (static_cast<int>(refinement) << shift);
    } else if (destination < 0) {
      value = destination - (static_cast<int>(refinement) << shift);
    } else {
      int signedRefinement = static_cast<int>(refinement);
      if (refinement != 0 && _jxr_rbitstream_uint1(stream) != 0)
        signedRefinement = -signedRefinement;
      value = signedRefinement << shift;
    }
    if ((image->user_flags & 2) == 0)
      destination = value;
  }
}

int r_calculate_mbhp_mode(jxr_image* image, int tileX, int macroblockX, int) {
  const unsigned int column = image->tile_column_position[tileX] + macroblockX;
  const int* luma = image->strip[0].cur[column].data;
  int vertical = std::abs(luma[1]) + std::abs(luma[2]) + std::abs(luma[3]);
  int horizontal =
      std::abs(luma[4]) + std::abs(luma[8]) + std::abs(luma[12]);
  if (image->use_clr_fmt == 1) {
    const int* first = image->strip[1].cur[column].data;
    const int* second = image->strip[2].cur[column].data;
    vertical += std::abs(first[1]) + std::abs(second[1]);
    horizontal += std::abs(first[2]) + std::abs(second[2]);
  } else if (image->use_clr_fmt == 2) {
    const int* first = image->strip[1].cur[column].data;
    const int* second = image->strip[2].cur[column].data;
    vertical += std::abs(second[5]) + std::abs(first[1]) +
                std::abs(first[5]) + std::abs(second[1]);
    horizontal += std::abs(second[6]) + std::abs(first[2]) +
                  std::abs(first[6]) + std::abs(second[2]);
  } else if (image->use_clr_fmt == 3 || image->use_clr_fmt == 4) {
    const int* first = image->strip[1].cur[column].data;
    const int* second = image->strip[2].cur[column].data;
    vertical += std::abs(first[1]) + std::abs(second[1]);
    horizontal += std::abs(first[4]) + std::abs(second[4]);
  }
  if (4 * vertical < horizontal)
    return 0;
  return 1 + (4 * horizontal >= vertical);
}

void _jxr_propagate_hp_predictions(jxr_image* image, int channel,
                                   int tileX, int macroblockX,
                                   int macroblockHpMode) {
  const unsigned int column =
      image->tile_column_position[tileX] + macroblockX;
  int* coefficients = image->strip[channel].cur[column].data;
  if (macroblockHpMode == 0) {
    int base = 34;
    for (int block = 1; block < 16; ++block, base += 15) {
      if ((block & 3) != 0) {
        coefficients[base] += coefficients[base - 15];
        coefficients[base + 4] += coefficients[base - 11];
        coefficients[base + 8] += coefficients[base - 7];
      }
    }
  } else if (macroblockHpMode == 1) {
    for (int base = 76; base <= 241; base += 15) {
      coefficients[base] += coefficients[base - 60];
      coefficients[base + 1] += coefficients[base - 59];
      coefficients[base + 2] += coefficients[base - 58];
    }
  }

  if (image->use_clr_fmt != 1 && image->use_clr_fmt != 2)
    return;
  const int verticalLast = image->use_clr_fmt == 1 ? 61 : 121;
  const int horizontalLast = image->use_clr_fmt == 1 ? 34 : 124;
  for (int chromaChannel = 1; chromaChannel <= 2; ++chromaChannel) {
    int* chroma = image->strip[chromaChannel].cur[column].data;
    if (macroblockHpMode == 0) {
      for (int base = 34; base <= horizontalLast; base += 30) {
        chroma[base] += chroma[base - 15];
        chroma[base + 4] += chroma[base - 11];
        chroma[base + 8] += chroma[base - 7];
      }
    } else if (macroblockHpMode == 1) {
      for (int base = 46; base <= verticalLast; base += 15) {
        chroma[base] += chroma[base - 30];
        chroma[base + 1] += chroma[base - 29];
        chroma[base + 2] += chroma[base - 28];
      }
    }
  }
}

int get_is_dc_yuv(rbitstream* stream) {
  if (_jxr_rbitstream_uint1(stream) == 1)
    return _jxr_rbitstream_uint1(stream) == 1 ? 4 : 0;
  switch (_jxr_rbitstream_uint2(stream)) {
    case 1:
      return 1;
    case 2:
      return 5;
    case 3:
      return 7;
    default:
      break;
  }
  if (_jxr_rbitstream_uint1(stream) == 1)
    return 3;
  return _jxr_rbitstream_uint1(stream) == 1 ? 2 : 6;
}

int jxr_read_stripe_bitstream(jxr_image* image) {
  return r_TILE_stripe(image, &image->rbits);
}

int r_image_plane_header(jxr_image* image, rbitstream* stream, int) {
  image->use_clr_fmt = _jxr_rbitstream_uint3(stream);
  image->component_mode =
      (image->component_mode & ~1U) | (_jxr_rbitstream_uint1(stream) & 1);
  image->bands_present = _jxr_rbitstream_uint4(stream);
  switch (image->use_clr_fmt) {
    case 0:
      image->num_channels = 1;
      break;
    case 1:
      (void)_jxr_rbitstream_uint1(stream);
      image->chroma_centering_x = _jxr_rbitstream_uint3(stream);
      (void)_jxr_rbitstream_uint1(stream);
      image->chroma_centering_y = _jxr_rbitstream_uint3(stream);
      image->num_channels = 3;
      break;
    case 2:
      (void)_jxr_rbitstream_uint1(stream);
      image->chroma_centering_x = _jxr_rbitstream_uint3(stream);
      (void)_jxr_rbitstream_uint4(stream);
      image->chroma_centering_y = 0;
      image->num_channels = 3;
      break;
    case 3:
      (void)_jxr_rbitstream_uint4(stream);
      (void)_jxr_rbitstream_uint4(stream);
      image->num_channels = 3;
      break;
    case 4:
      image->num_channels = 4;
      break;
    case 6: {
      const std::uint8_t channelCode = _jxr_rbitstream_uint4(stream);
      if (channelCode == 15) {
        image->num_channels = static_cast<std::uint8_t>(
            _jxr_rbitstream_uint12(stream) + 16);
      } else {
        image->num_channels = channelCode + 1;
        (void)_jxr_rbitstream_uint4(stream);
      }
      break;
    }
    default:
      break;
  }

  switch (image->header_flags_fmt & 15U) {
    case 0:
    case 1:
    case 4:
    case 8:
    case 9:
    case 15:
      image->shift_bits = 0;
      break;
    case 2:
    case 3:
    case 6:
      image->shift_bits = _jxr_rbitstream_uint8(stream);
      break;
    case 7:
      image->len_mantissa = _jxr_rbitstream_uint8(stream);
      image->exp_bias = static_cast<char>(_jxr_rbitstream_uint8(stream));
      break;
    default:
      break;
  }

  image->component_mode = (image->component_mode & ~2U) |
                          ((_jxr_rbitstream_uint1(stream) & 1) << 1);
  if ((image->component_mode & 2U) != 0)
    _jxr_r_DC_QP(image, stream);
  image->component_mode |= 0x28U;
  if (image->bands_present != 3) {
    (void)_jxr_rbitstream_uint1(stream);
    image->component_mode = (image->component_mode & ~8U) |
                            ((_jxr_rbitstream_uint1(stream) & 1) << 3);
    if ((image->component_mode & 8U) != 0) {
      image->num_lp_qps = 1;
      _jxr_r_LP_QP(image, stream);
    }
    if (image->bands_present != 2) {
      (void)_jxr_rbitstream_uint1(stream);
      image->component_mode = (image->component_mode & ~0x20U) |
                              ((_jxr_rbitstream_uint1(stream) & 1) << 5);
      if ((image->component_mode & 0x20U) != 0) {
        image->num_hp_qps = 1;
        r_HP_QP(image, stream);
      }
    }
  }
  _jxr_rbitstream_syncbyte(stream);
  return 0;
}

int jxr_init_read_stripe_bitstream(jxr_image* image, const std::uint8_t* data,
                                   std::uint64_t size) {
  image->rbits.data = data;
  image->rbits.cur_ptr = data;
  image->rbits.dataSize = size;
  image->rbits.bits_avail = 0;
  image->rbits.read_count = 0;
  image->rbits.mark_stream_position = 0;

  int result = r_image_header(image, &image->rbits);
  if (result < 0)
    return result;
  result = r_image_plane_header(image, &image->rbits, 0);
  if (result < 0)
    return result;
  _jxr_make_mbstore(image, 0);

  if ((image->header_flags2 & 1U) != 0) {
    void* storage = _jxr_temp_calloc(
        image, 1, static_cast<std::uint64_t>(jxr_image_size()));
    image->alpha = jxr_create_input(storage);
    std::memmove(image->alpha, image, sizeof(jxr_image));
    result = r_image_plane_header(image->alpha, &image->rbits, 0);
    if (result < 0)
      return result;
    for (unsigned int channel = 0; channel < image->num_channels; ++channel)
      image->alpha->strip[channel] = {};
    _jxr_make_mbstore(image->alpha, 0);
    image->alpha->primary = 0;
  }

  result = r_INDEX_TABLE(image, &image->rbits);
  if (result < 0)
    return result;
  image->profile_idc = 0x6F;
  image->level_idc = 0xFF;
  std::int64_t profileBytes =
      static_cast<std::int64_t>(_jxr_rbitstream_intVLW(&image->rbits));
  if (profileBytes > 0) {
    profileBytes -= r_PROFILE_LEVEL_INFO(image, &image->rbits);
    while (profileBytes-- > 0)
      (void)_jxr_rbitstream_uint8(&image->rbits);
  }
  _jxr_rbitstream_mark(&image->rbits);
  image->stripe_tx = 0;
  image->stripe_ty = 0;
  image->freq_buffered_flag = 0;
  image->spatial_buffered_flag = 0;
  image->stripe_my = 0;
  image->cleanup_state = 0;
  return result;
}
