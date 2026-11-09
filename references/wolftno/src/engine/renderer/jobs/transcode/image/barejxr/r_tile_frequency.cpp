#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <algorithm>

void _jxr_r_flush_rotate_mb_strip(jxr_image* image, int tileX, int tileY,
                                  int macroblockY);
void _jxr_r_TILE_HEADER_DC(jxr_image* image, rbitstream* stream, int plane,
                           unsigned int tileX, unsigned int tileY);
void _jxr_r_TILE_HEADER_LOWPASS(jxr_image* image, rbitstream* stream, int plane,
                                unsigned int tileX, unsigned int tileY);
void _jxr_r_TILE_HEADER_HIGHPASS(jxr_image* image, rbitstream* stream,
                                 int plane, unsigned int tileX,
                                 unsigned int tileY);
void _jxr_r_rotate_mb_strip(jxr_image* image, int macroblockY);
void _jxr_r_MB_DC(jxr_image* image, rbitstream* stream, int plane,
                  unsigned int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY);
void _jxr_r_MB_LP(jxr_image* image, rbitstream* stream, int plane,
                  int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY);
int _jxr_r_MB_CBP(jxr_image* image, rbitstream* stream, int plane,
                  unsigned int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY);
int _jxr_r_MB_HP(jxr_image* image, rbitstream* stream, int plane,
                 int tileX, unsigned int tileY,
                 unsigned int macroblockX, unsigned int macroblockY);
int _jxr_r_MB_FLEXBITS(jxr_image* image, rbitstream* stream, int plane,
                       int tileX, unsigned int tileY,
                       unsigned int macroblockX, unsigned int macroblockY);
unsigned int _jxr_DECODE_QP_INDEX(rbitstream* stream,
                                  unsigned int quantizerCount);
void _jxr_complete_cur_dclp(jxr_image* image, int tileX, int macroblockX,
                            int macroblockY);
void _jxr_propagate_hp_predictions(jxr_image* image, int channel, int tileX,
                                   int macroblockX, int predictionMode);

namespace {

int CoefficientGroupCount(const jxr_image* image, unsigned int channel) {
  if (channel == 0) {
    return 16;
  }
  if (image->use_clr_fmt == 2) {
    return 8;
  }
  if (image->use_clr_fmt == 1) {
    return 4;
  }
  return 16;
}

unsigned int RowBufferIndex(const jxr_image* image, unsigned int tileX,
                            unsigned int tileY, unsigned int macroblockY) {
  return image->tile_column_position[tileX] +
         (image->extended_width >> 4) *
             (macroblockY + image->tile_row_position[tileY]);
}

unsigned int TileHeight(const jxr_image* image, unsigned int tileY) {
  return (image->header_flags1 & 0x80) != 0
             ? image->tile_row_height[tileY]
             : image->extended_height >> 4;
}

unsigned int TileWidth(const jxr_image* image, unsigned int tileX) {
  return (image->header_flags1 & 0x80) != 0
             ? image->tile_column_width[tileX]
             : image->extended_width >> 4;
}

bool ReadTileMarker(rbitstream* stream) {
  const std::uint8_t first = _jxr_rbitstream_uint8(stream);
  const std::uint8_t second = _jxr_rbitstream_uint8(stream);
  const std::uint8_t third = _jxr_rbitstream_uint8(stream);
  _jxr_rbitstream_uint8(stream);
  return first == 0 && second == 0 && third == 1;
}

template <typename Function>
void ForEachPlane(jxr_image* image, Function function) {
  function(image);
  if ((image->header_flags2 & 1) != 0) {
    function(image->alpha);
  }
}

}  // namespace

void backup_dc_strip(jxr_image* image, int tileX, int tileY,
                     int macroblockY) {
  const unsigned int rowIndex = RowBufferIndex(image, tileX, tileY,
                                                macroblockY);
  const unsigned int width = image->tile_column_width[tileX];
  const unsigned int column = image->tile_column_position[tileX];
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    for (unsigned int x = 0; x < width; ++x) {
      image->mb_row_buffer[channel][rowIndex + x].data[0] =
          image->strip[channel].cur[column + x].data[0];
    }
  }
}

void backup_dclp_strip(jxr_image* image, int tileX, int tileY,
                       int macroblockY) {
  const unsigned int rowIndex = RowBufferIndex(image, tileX, tileY,
                                                macroblockY);
  const unsigned int width = image->tile_column_width[tileX];
  const unsigned int column = image->tile_column_position[tileX];
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    const int coefficientCount = CoefficientGroupCount(image, channel);
    for (unsigned int x = 0; x < width; ++x) {
      macroblock_s& destination =
          image->mb_row_buffer[channel][rowIndex + x];
      const macroblock_s& source = image->strip[channel].cur[column + x];
      std::copy_n(source.data, coefficientCount, destination.data);
      destination.lp_quant = source.lp_quant;
    }
  }
}

void backup_hp_strip(jxr_image* image, int tileX, int tileY,
                     int macroblockY) {
  const unsigned int rowIndex = RowBufferIndex(image, tileX, tileY,
                                                macroblockY);
  const unsigned int width = image->tile_column_width[tileX];
  const unsigned int column = image->tile_column_position[tileX];
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    const int groupCount = CoefficientGroupCount(image, channel);
    for (unsigned int x = 0; x < width; ++x) {
      macroblock_s& destination =
          image->mb_row_buffer[channel][rowIndex + x];
      const macroblock_s& source = image->strip[channel].cur[column + x];
      destination.data[0] = source.data[0];
      for (int group = 0; group < groupCount; ++group) {
        std::copy_n(source.data + 16 + group * 15, 15,
                    destination.data + groupCount + group * 15);
      }
      if (channel == 0) {
        std::copy_n(source.hp_model_bits, 16, destination.hp_model_bits);
        destination.mbhp_pred_mode = source.mbhp_pred_mode;
      }
      destination.hp_quant = source.hp_quant;
    }
  }
}

void recover_dc_strip(jxr_image* image, int tileX, int tileY,
                      int macroblockY) {
  const unsigned int rowIndex = RowBufferIndex(image, tileX, tileY,
                                                macroblockY);
  const unsigned int width = image->tile_column_width[tileX];
  const unsigned int column = image->tile_column_position[tileX];
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    for (unsigned int x = 0; x < width; ++x) {
      image->strip[channel].cur[column + x].data[0] =
          image->mb_row_buffer[channel][rowIndex + x].data[0];
    }
  }
}

void recover_dclp_strip(jxr_image* image, int tileX, int tileY,
                        int macroblockY) {
  const unsigned int rowIndex = RowBufferIndex(image, tileX, tileY,
                                                macroblockY);
  const unsigned int width = image->tile_column_width[tileX];
  const unsigned int column = image->tile_column_position[tileX];
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    const int coefficientCount = CoefficientGroupCount(image, channel);
    for (unsigned int x = 0; x < width; ++x) {
      macroblock_s& destination = image->strip[channel].cur[column + x];
      const macroblock_s& source =
          image->mb_row_buffer[channel][rowIndex + x];
      std::copy_n(source.data, coefficientCount, destination.data);
      destination.lp_quant = source.lp_quant;
    }
  }
}

void recover_dclphp_strip(jxr_image* image, int tileX, int tileY,
                          int macroblockY) {
  const unsigned int rowIndex = RowBufferIndex(image, tileX, tileY,
                                                macroblockY);
  const unsigned int width = image->tile_column_width[tileX];
  const unsigned int column = image->tile_column_position[tileX];
  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    const int groupCount = CoefficientGroupCount(image, channel);
    for (unsigned int x = 0; x < width; ++x) {
      macroblock_s& destination = image->strip[channel].cur[column + x];
      const macroblock_s& source =
          image->mb_row_buffer[channel][rowIndex + x];
      std::copy_n(source.data, groupCount, destination.data);
      for (int group = 0; group < groupCount; ++group) {
        std::copy_n(source.data + groupCount + group * 15, 15,
                    destination.data + 16 + group * 15);
      }
      if (channel == 0) {
        std::copy_n(source.hp_model_bits, 16, destination.hp_model_bits);
        destination.mbhp_pred_mode = source.mbhp_pred_mode;
      }
      destination.lp_quant = source.lp_quant;
      destination.hp_quant = source.hp_quant;
    }
  }
}

int _jxr_frequency_mode_render_stripe(jxr_image* image) {
  bool enterCleanup = false;
  if (image->cleanup_state == 0) {
    const unsigned int tileY = static_cast<unsigned int>(image->stripe_ty);
    while (true) {
      const unsigned int stripeY = image->stripe_my;
      ForEachPlane(image, [tileY, stripeY](jxr_image* plane) {
        _jxr_r_flush_rotate_mb_strip(
            plane, -1, -1, stripeY + plane->tile_row_position[tileY]);
      });
      for (unsigned int tileX = 0; tileX < image->tile_columns; ++tileX) {
        recover_dclphp_strip(image, tileX, tileY, image->stripe_my);
        if ((image->header_flags2 & 1) != 0) {
          recover_dclphp_strip(image->alpha, tileX, tileY, image->stripe_my);
        }
      }
      ++image->stripe_my;
      if (static_cast<unsigned int>(image->stripe_my) >=
              image->tile_row_height[tileY] ||
          image->output_sent != 0) {
        break;
      }
    }
    if (static_cast<unsigned int>(image->stripe_my) <
        image->tile_row_height[tileY]) {
      image->output_sent = 0;
      return 0;
    }
    ++image->stripe_ty;
    image->stripe_my = 0;
    if (static_cast<unsigned int>(image->stripe_ty) == image->tile_rows) {
      ++image->cleanup_state;
    }
    if (image->output_sent != 0) {
      image->output_sent = 0;
      return 0;
    }
    enterCleanup = true;
  }

  unsigned int cleanupStage =
      enterCleanup ? 1U : static_cast<unsigned int>(image->cleanup_state);
  while (cleanupStage >= 1 && cleanupStage <= 4) {
    const unsigned int extra = cleanupStage - 1;
    ForEachPlane(image, [extra](jxr_image* plane) {
      _jxr_r_flush_rotate_mb_strip(
          plane, -1, -1, static_cast<int>((plane->extended_height >> 4) + extra));
    });
    ++image->cleanup_state;
    if (image->output_sent != 0) {
      image->output_sent = 0;
      return 0;
    }
    ++cleanupStage;
  }
  return -256;
}

int _jxr_r_TILE_DC(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int tileY) {
  if (!ReadTileMarker(stream)) {
    return -1;
  }
  _jxr_r_TILE_HEADER_DC(image, stream, 0, tileX, tileY);
  if ((image->header_flags2 & 1) != 0) {
    _jxr_r_TILE_HEADER_DC(image->alpha, stream, 1, tileX, tileY);
  }
  const unsigned int height = TileHeight(image, tileY);
  const unsigned int width = TileWidth(image, tileX);
  for (unsigned int y = 0; y < height; ++y) {
    ForEachPlane(image,
                 [y](jxr_image* plane) { _jxr_r_rotate_mb_strip(plane, y); });
    for (unsigned int x = 0; x < width; ++x) {
      _jxr_r_MB_DC(image, stream, 0, tileX, tileY, x, y);
      if (image->bands_present == 3) {
        _jxr_complete_cur_dclp(image, tileX, x, y);
      }
      if ((image->header_flags2 & 1) != 0) {
        _jxr_r_MB_DC(image->alpha, stream, 1, tileX, tileY, x, y);
        if (image->alpha->bands_present == 3) {
          _jxr_complete_cur_dclp(image->alpha, tileX, x, y);
        }
      }
    }
    if ((image->header_flags2 & 1) != 0) {
      backup_dc_strip(image->alpha, tileX, tileY, y);
    }
    backup_dc_strip(image, tileX, tileY, y);
  }
  _jxr_rbitstream_syncbyte(stream);
  return 0;
}

int _jxr_r_TILE_LP(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int tileY) {
  if (!ReadTileMarker(stream)) {
    return -1;
  }
  _jxr_r_TILE_HEADER_LOWPASS(image, stream, 0, tileX, tileY);
  if ((image->header_flags2 & 1) != 0) {
    _jxr_r_TILE_HEADER_LOWPASS(image->alpha, stream, 1, tileX, tileY);
  }
  const unsigned int height = TileHeight(image, tileY);
  const unsigned int width = TileWidth(image, tileX);
  const unsigned int planeCount = (image->header_flags2 & 1) != 0 ? 2 : 1;
  for (unsigned int y = 0; y < height; ++y) {
    ForEachPlane(image,
                 [y](jxr_image* plane) { _jxr_r_rotate_mb_strip(plane, y); });
    recover_dc_strip(image, tileX, tileY, y);
    if (planeCount == 2) {
      recover_dc_strip(image->alpha, tileX, tileY, y);
    }
    for (unsigned int x = 0; x < width; ++x) {
      for (unsigned int planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        jxr_image* plane = planeIndex == 0 ? image : image->alpha;
        int quantizer = 0;
        if ((plane->component_mode & 4) == 0 && plane->num_lp_qps > 1) {
          quantizer = _jxr_DECODE_QP_INDEX(stream, plane->num_lp_qps);
        }
        const unsigned int column = plane->tile_column_position[tileX] + x;
        for (unsigned int channel = 0; channel < plane->num_channels; ++channel) {
          plane->strip[channel].cur[column].lp_quant = quantizer;
        }
        _jxr_r_MB_LP(plane, stream, 0, tileX, tileY, x, y);
        if (plane->bands_present != 3) {
          _jxr_complete_cur_dclp(plane, tileX, x, y);
        }
      }
    }
    backup_dclp_strip(image, tileX, tileY, y);
    if (planeCount == 2) {
      backup_dclp_strip(image->alpha, tileX, tileY, y);
    }
  }
  _jxr_rbitstream_syncbyte(stream);
  return 0;
}

int _jxr_r_TILE_HP(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int tileY) {
  if (!ReadTileMarker(stream)) {
    return -1;
  }
  _jxr_r_TILE_HEADER_HIGHPASS(image, stream, 0, tileX, tileY);
  if ((image->header_flags2 & 1) != 0) {
    _jxr_r_TILE_HEADER_HIGHPASS(image->alpha, stream, 1, tileX, tileY);
  }
  const unsigned int height = TileHeight(image, tileY);
  const unsigned int width = TileWidth(image, tileX);
  const unsigned int planeCount = (image->header_flags2 & 1) != 0 ? 2 : 1;
  for (unsigned int y = 0; y < height; ++y) {
    ForEachPlane(image,
                 [y](jxr_image* plane) { _jxr_r_rotate_mb_strip(plane, y); });
    recover_dclp_strip(image, tileX, tileY, y);
    if (planeCount == 2) {
      recover_dclp_strip(image->alpha, tileX, tileY, y);
    }
    for (unsigned int x = 0; x < width; ++x) {
      for (unsigned int planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        jxr_image* plane = planeIndex == 0 ? image : image->alpha;
        const unsigned int column = plane->tile_column_position[tileX] + x;
        int quantizer = 0;
        if (plane->num_hp_qps > 1) {
          quantizer = (plane->component_mode & 0x10) != 0
                          ? plane->strip[0].cur[column].lp_quant
                          : _jxr_DECODE_QP_INDEX(stream, plane->num_hp_qps);
        }
        for (unsigned int channel = 0; channel < plane->num_channels; ++channel) {
          plane->strip[channel].cur[column].hp_quant =
              plane->hp_quant_ch[channel][quantizer];
        }
        int result = _jxr_r_MB_CBP(plane, stream, 0, tileX, tileY, x, y);
        if (result < 0) {
          return result;
        }
        result = _jxr_r_MB_HP(plane, stream, 0, tileX, tileY, x, y);
        if (result < 0) {
          return result;
        }
      }
    }
    backup_hp_strip(image, tileX, tileY, y);
    if (planeCount == 2) {
      backup_hp_strip(image->alpha, tileX, tileY, y);
    }
  }
  _jxr_rbitstream_syncbyte(stream);
  return 0;
}

int _jxr_r_TILE_FLEXBITS(jxr_image* image, rbitstream* stream,
                         unsigned int tileX, unsigned int tileY) {
  if (!ReadTileMarker(stream)) {
    return -1;
  }
  image->trim_flexbits &= ~0xFU;
  if ((image->header_flags2 & 0x10) != 0) {
    image->trim_flexbits |= _jxr_rbitstream_uint4(stream) & 0xF;
  }
  const int mainChannelCount =
      image->use_clr_fmt == 1 || image->use_clr_fmt == 2
          ? 1
          : image->num_channels;
  const int height = static_cast<int>(TileHeight(image, tileY));
  const int width = static_cast<int>(TileWidth(image, tileX));
  const int planeCount = (image->header_flags2 & 1) != 0 ? 2 : 1;
  for (int y = 0; y < height; ++y) {
    ForEachPlane(image,
                 [y](jxr_image* plane) { _jxr_r_rotate_mb_strip(plane, y); });
    recover_dclphp_strip(image, tileX, tileY, y);
    if (planeCount == 2) {
      recover_dclphp_strip(image->alpha, tileX, tileY, y);
    }
    for (int x = 0; x < width; ++x) {
      for (int planeIndex = 0; planeIndex < planeCount; ++planeIndex) {
        jxr_image* plane = planeIndex == 0 ? image : image->alpha;
        const int channelCount = planeIndex == 0 ? mainChannelCount : 1;
        const int result = _jxr_r_MB_FLEXBITS(plane, stream, 0, tileX, tileY,
                                               x, y);
        if (result < 0) {
          return result;
        }
        const unsigned int column = plane->tile_column_position[tileX] + x;
        const int predictionMode = plane->strip[0].cur[column].mbhp_pred_mode;
        for (int channel = 0; channel < channelCount; ++channel) {
          _jxr_propagate_hp_predictions(plane, channel, tileX, x,
                                        predictionMode);
        }
      }
    }
    backup_hp_strip(image, tileX, tileY, y);
    if (planeCount == 2) {
      backup_hp_strip(image->alpha, tileX, tileY, y);
    }
  }
  _jxr_rbitstream_syncbyte(stream);
  return 0;
}

int _jxr_r_TILE_FLEXBITS_ESCAPE(jxr_image* image, unsigned int tileX,
                                unsigned int tileY) {
  const int channelCount =
      image->use_clr_fmt == 1 || image->use_clr_fmt == 2
          ? 1
          : image->num_channels;
  const int height = static_cast<int>(TileHeight(image, tileY));
  const int width = static_cast<int>(TileWidth(image, tileX));
  for (int y = 0; y < height; ++y) {
    _jxr_r_rotate_mb_strip(image, y);
    recover_dclphp_strip(image, tileX, tileY, y);
    for (int x = 0; x < width; ++x) {
      const unsigned int column = image->tile_column_position[tileX] + x;
      const int predictionMode = image->strip[0].cur[column].mbhp_pred_mode;
      for (int channel = 0; channel < channelCount; ++channel) {
        _jxr_propagate_hp_predictions(image, channel, tileX, x,
                                      predictionMode);
      }
    }
    backup_hp_strip(image, tileX, tileY, y);
  }
  return 0;
}
