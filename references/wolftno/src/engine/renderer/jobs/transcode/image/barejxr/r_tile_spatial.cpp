#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <cstdint>

unsigned int _jxr_DECODE_QP_INDEX(rbitstream*, unsigned int);
void _jxr_r_MB_DC(jxr_image*, rbitstream*, int, unsigned int, unsigned int,
                  unsigned int, unsigned int);
void _jxr_r_MB_LP(jxr_image*, rbitstream*, int, int, unsigned int,
                  unsigned int, unsigned int);
int _jxr_r_MB_CBP(jxr_image*, rbitstream*, int, unsigned int, unsigned int,
                  unsigned int, unsigned int);
int _jxr_r_MB_HP(jxr_image*, rbitstream*, int, int, unsigned int,
                 unsigned int, unsigned int);
void _jxr_complete_cur_dclp(jxr_image*, int, int, int);
void _jxr_r_TILE_HEADER_DC(jxr_image*, rbitstream*, int, unsigned int,
                           unsigned int);
void _jxr_r_TILE_HEADER_LOWPASS(jxr_image*, rbitstream*, int, unsigned int,
                                unsigned int);
void _jxr_r_TILE_HEADER_HIGHPASS(jxr_image*, rbitstream*, int, unsigned int,
                                 unsigned int);
void _jxr_r_rotate_mb_strip(jxr_image*, int);
int _jxr_r_transform_mb_strip(jxr_image*, int, int, int);

int _jxr_r_TILE_SPATIAL_decode_mb(jxr_image* image, rbitstream* stream,
                                  unsigned int tileX, unsigned int tileY,
                                  int macroblockX, int macroblockY) {
  const unsigned int planeCount = (image->header_flags2 & 1U) + 1U;
  for (unsigned int plane = 0; plane < planeCount; ++plane) {
    jxr_image* current = plane == 0 ? image : image->alpha;
    unsigned int lpIndex = 0;
    unsigned int hpIndex = 0;
    if (current->bands_present != 3) {
      if (current->num_lp_qps > 1 && (current->component_mode & 4U) == 0)
        lpIndex = _jxr_DECODE_QP_INDEX(stream, current->num_lp_qps);
      if (current->bands_present != 2 && current->num_hp_qps > 1) {
        hpIndex = (current->component_mode & 0x10U) != 0
                      ? lpIndex
                      : _jxr_DECODE_QP_INDEX(stream, current->num_hp_qps);
      }
    }

    const int block = macroblockX +
                      static_cast<int>(current->tile_column_position[tileX]);
    for (unsigned int channel = 0; channel < current->num_channels; ++channel) {
      current->strip[channel].cur[block].lp_quant = lpIndex;
      current->strip[channel].cur[block].hp_quant =
          current->hp_quant_ch[channel][hpIndex];
    }

    _jxr_r_MB_DC(current, stream, static_cast<int>(plane), tileX, tileY,
                 static_cast<unsigned int>(macroblockX),
                 static_cast<unsigned int>(macroblockY));
    if (current->bands_present == 3) {
      _jxr_complete_cur_dclp(current, static_cast<int>(tileX), macroblockX,
                             macroblockY);
      continue;
    }
    _jxr_r_MB_LP(current, stream, static_cast<int>(plane), tileX, tileY,
                 static_cast<unsigned int>(macroblockX),
                 static_cast<unsigned int>(macroblockY));
    _jxr_complete_cur_dclp(current, static_cast<int>(tileX), macroblockX,
                           macroblockY);
    if (current->bands_present != 2) {
      int result = _jxr_r_MB_CBP(
          current, stream, static_cast<int>(plane), tileX, tileY,
          static_cast<unsigned int>(macroblockX),
          static_cast<unsigned int>(macroblockY));
      if (result < 0)
        return result;
      result = _jxr_r_MB_HP(current, stream, static_cast<int>(plane), tileX,
                            tileY, static_cast<unsigned int>(macroblockX),
                            static_cast<unsigned int>(macroblockY));
      if (result < 0)
        return result;
    }
  }
  return 0;
}

int _jxr_r_TILE_SPATIAL_stripe(jxr_image* image, rbitstream* stream,
                               unsigned int tileX, unsigned int tileY) {
  if (image->spatial_buffered_flag == 0) {
    if ((image->header_flags1 & 4U) != 0)
      _jxr_rbitstream_seek(
          stream, image->tile_index_table[tileX + tileY * image->tile_columns]);
    const std::uint8_t marker0 = _jxr_rbitstream_uint8(stream);
    const std::uint8_t marker1 = _jxr_rbitstream_uint8(stream);
    const std::uint8_t marker2 = _jxr_rbitstream_uint8(stream);
    _jxr_rbitstream_uint8(stream);
    if (marker0 != 0 || marker1 != 0 || marker2 != 1)
      return -1;

    image->user_flags &= ~0xFU;
    if ((image->header_flags2 & 0x10U) != 0)
      image->user_flags = (image->user_flags & ~0xF) |
                          (_jxr_rbitstream_uint4(stream) & 0xF);
    _jxr_r_TILE_HEADER_DC(image, stream, 0, tileX, tileY);
    if (image->bands_present != 3) {
      _jxr_r_TILE_HEADER_LOWPASS(image, stream, 0, tileX, tileY);
      if (image->bands_present != 2)
        _jxr_r_TILE_HEADER_HIGHPASS(image, stream, 0, tileX, tileY);
    }
    if ((image->header_flags2 & 1U) != 0) {
      _jxr_r_TILE_HEADER_DC(image->alpha, stream, 1, tileX, tileY);
      if (image->bands_present != 3) {
        _jxr_r_TILE_HEADER_LOWPASS(image->alpha, stream, 1, tileX, tileY);
        if (image->bands_present != 2)
          _jxr_r_TILE_HEADER_HIGHPASS(image->alpha, stream, 1, tileX, tileY);
      }
    }
    image->spatial_mb_height = image->extended_height >> 4;
    image->spatial_mb_width = image->extended_width >> 4;
    if ((image->header_flags1 & 0x80U) != 0) {
      image->spatial_mb_height = image->tile_row_height[tileY];
      image->spatial_mb_width = image->tile_column_width[tileX];
    }
    image->spatial_buffered_flag = 1;
    image->stripe_my = 0;
    image->cleanup_state = 0;
  }

  if (image->cleanup_state == 0) {
    int transformPending = 0;
    do {
      _jxr_r_rotate_mb_strip(image, image->stripe_my);
      if ((image->header_flags2 & 1U) != 0)
        _jxr_r_rotate_mb_strip(image->alpha, image->stripe_my);
      for (unsigned int x = 0; x < image->spatial_mb_width; ++x) {
        const int result = _jxr_r_TILE_SPATIAL_decode_mb(
            image, stream, tileX, tileY, static_cast<int>(x), image->stripe_my);
        if (result < 0)
          return result;
      }
      ++image->stripe_my;
      transformPending =
          _jxr_r_transform_mb_strip(image, tileX, tileY, image->stripe_my);
      if ((image->header_flags2 & 1U) != 0)
        _jxr_r_transform_mb_strip(image->alpha, tileX, tileY, image->stripe_my);
    } while (image->stripe_my < static_cast<int>(image->spatial_mb_height) &&
             transformPending == 0);

    if (image->stripe_my == static_cast<int>(image->spatial_mb_height))
      _jxr_rbitstream_syncbyte(stream);
    if (tileX + 1 != image->tile_columns) {
      image->spatial_buffered_flag = 0;
      image->stripe_my = 0;
      return 1;
    }
    if (image->stripe_my < static_cast<int>(image->spatial_mb_height) &&
        transformPending != 0)
      return 0;
    if (tileY + 1 != image->tile_rows) {
      image->spatial_buffered_flag = 0;
      image->stripe_my = 0;
      return -256;
    }
    image->cleanup_state = 1;
    if (transformPending != 0)
      return 0;
  }

  if (tileX + 1 != image->tile_columns || tileY + 1 != image->tile_rows)
    return 1;
  while (image->cleanup_state <= 3) {
    _jxr_r_rotate_mb_strip(image, image->stripe_my);
    if ((image->header_flags2 & 1U) != 0)
      _jxr_r_rotate_mb_strip(image->alpha, image->stripe_my);
    ++image->stripe_my;
    const int transformPending =
        _jxr_r_transform_mb_strip(image, tileX, tileY, image->stripe_my);
    if ((image->header_flags2 & 1U) != 0)
      _jxr_r_transform_mb_strip(image->alpha, tileX, tileY, image->stripe_my);
    ++image->cleanup_state;
    if (transformPending != 0)
      return 0;
  }
  image->spatial_buffered_flag = 0;
  image->stripe_my = 0;
  return -256;
}
