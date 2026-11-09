#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <cstring>

namespace {

constexpr std::uint64_t kAlignment = 16;

int SamplesPerChromaMacroblock(const jxr_image* image) {
  if (image->use_clr_fmt == 2) {
    return 136;
  }
  if (image->use_clr_fmt == 1) {
    return 76;
  }
  return 256;
}

void BindMacroblocks(macroblock_s* blocks, unsigned int count, int* data,
                     int dataStride, int* prediction) {
  for (unsigned int index = 0; index < count; ++index) {
    blocks[index].data = data;
    data += dataStride;
    if (prediction != nullptr) {
      blocks[index].pred_dclp = prediction;
      prediction += 7;
    }
  }
}

}  // namespace

jxr_image* __make_jxr(void* buffer) {
  if (buffer == nullptr) {
    return nullptr;
  }

  auto* image = static_cast<jxr_image*>(buffer);
  std::memset(image, 0, sizeof(*image));
  image->header_flags2 = 0x80;
  image->default_output_flag = 1;
  image->component_mode |= 1;
  return image;
}

int jxr_image_size() { return sizeof(jxr_image); }

jxr_image* jxr_create_input(void* buffer) { return __make_jxr(buffer); }

void* _jxr_temp_calloc(jxr_image* image, std::uint64_t count,
                       std::uint64_t size) {
  const std::uint64_t allocationSize =
      ((count * size) + (kAlignment - 1)) & ~(kAlignment - 1);
  const int used = static_cast<int>(image->temp_buffer_used);
  const int next = used + static_cast<int>(allocationSize);
  if (next > static_cast<int>(image->temp_buffer_size)) {
    *static_cast<volatile int*>(nullptr) = 0;
    return nullptr;
  }

  auto* result = static_cast<std::uint8_t*>(image->temp_buffer) + used;
  std::memset(result, 0, static_cast<std::size_t>(allocationSize));
  image->temp_buffer_used += static_cast<unsigned int>(allocationSize);
  return result;
}

void jxr_set_temp_memory(jxr_image* image, void* buffer, std::uint64_t size,
                         std::uint64_t used) {
  image->temp_buffer = buffer;
  image->temp_buffer_size = static_cast<unsigned int>(size);
  image->temp_buffer_used = static_cast<unsigned int>(used);
}

void make_mb_row_buffer(jxr_image* image, unsigned int use_height) {
  const unsigned int blockCount =
      use_height * (image->extended_width >> 4);

  image->mb_row_buffer[0] = static_cast<macroblock_s*>(
      _jxr_temp_calloc(image, blockCount, sizeof(macroblock_s)));
  auto* lumaData = static_cast<int*>(
      _jxr_temp_calloc(image, static_cast<std::uint64_t>(blockCount) << 8,
                       sizeof(int)));
  auto* lumaPrediction = static_cast<int*>(_jxr_temp_calloc(
      image, static_cast<std::uint64_t>(blockCount) * 7, sizeof(int)));
  BindMacroblocks(image->mb_row_buffer[0], blockCount, lumaData, 256,
                  lumaPrediction);

  const int chromaSamples = SamplesPerChromaMacroblock(image);
  for (unsigned int channel = 1; channel < image->num_channels; ++channel) {
    image->mb_row_buffer[channel] = static_cast<macroblock_s*>(
        _jxr_temp_calloc(image, blockCount, sizeof(macroblock_s)));
    auto* data = static_cast<int*>(_jxr_temp_calloc(
        image, blockCount, static_cast<std::uint64_t>(chromaSamples) * 4));
    auto* prediction = static_cast<int*>(_jxr_temp_calloc(
        image, static_cast<std::uint64_t>(blockCount) * 7, sizeof(int)));
    BindMacroblocks(image->mb_row_buffer[channel], blockCount, data,
                    chromaSamples, prediction);
  }
}

void _jxr_make_mbstore(jxr_image* image, int up4_flag) {
  const unsigned int blockCount = image->extended_width >> 4;
  constexpr macroblock_s* jxr_strip_t::* kRows[] = {
      &jxr_strip_t::up4, &jxr_strip_t::up3, &jxr_strip_t::up2,
      &jxr_strip_t::up1, &jxr_strip_t::cur};
  const unsigned int firstRow = up4_flag != 0 ? 0 : 1;

  for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
    jxr_strip_t& strip = image->strip[channel];

    for (unsigned int row = firstRow; row < 5; ++row) {
      strip.*kRows[row] = static_cast<macroblock_s*>(
          _jxr_temp_calloc(image, blockCount, sizeof(macroblock_s)));
    }
    for (unsigned int row = firstRow; row < 5; ++row) {
      auto* data = static_cast<int*>(_jxr_temp_calloc(
          image, static_cast<std::uint64_t>(blockCount) << 8, sizeof(int)));
      BindMacroblocks(strip.*kRows[row], blockCount, data, 256, nullptr);
    }
    for (unsigned int row = firstRow; row < 5; ++row) {
      auto* prediction = static_cast<int*>(_jxr_temp_calloc(
          image, static_cast<std::uint64_t>(blockCount) * 7, sizeof(int)));
      BindMacroblocks(strip.*kRows[row], blockCount,
                      (strip.*kRows[row])->data, 256, prediction);
    }

    if (channel != 0 &&
        (image->use_clr_fmt == 1 || image->use_clr_fmt == 2)) {
      strip.upsample_memory_x =
          static_cast<int*>(_jxr_temp_calloc(image, 1, 64));
    }
    if (channel != 0 && image->use_clr_fmt == 1) {
      strip.upsample_memory_y = static_cast<int*>(
          _jxr_temp_calloc(image, blockCount, 8 * sizeof(int)));
    }
  }

  if ((image->header_flags1 & 0x40) != 0) {
    make_mb_row_buffer(image, image->extended_height >> 4);
  } else if ((image->header_flags1 & 4) != 0) {
    unsigned int maximumTileRowHeight = 0;
    for (unsigned int row = 0; row < image->tile_rows; ++row) {
      if (image->tile_row_height[row] > maximumTileRowHeight) {
        maximumTileRowHeight = image->tile_row_height[row];
      }
    }
    make_mb_row_buffer(image, maximumTileRowHeight);

    const unsigned int contextCount = 4 * blockCount;
    const int chromaSamples = SamplesPerChromaMacroblock(image);
    for (unsigned int channel = 0; channel < image->num_channels; ++channel) {
      const int sampleCount = channel == 0 ? 256 : chromaSamples;
      image->mb_row_context[channel] = static_cast<macroblock_s*>(
          _jxr_temp_calloc(image, contextCount, sizeof(macroblock_s)));
      auto* data = static_cast<int*>(_jxr_temp_calloc(
          image, contextCount, static_cast<std::uint64_t>(sampleCount) * 4));
      BindMacroblocks(image->mb_row_context[channel], contextCount, data,
                      sampleCount, nullptr);
    }
  }

  image->model_hp_buffer = nullptr;
  image->hp_cbp_model_buffer = nullptr;
  if (image->tile_columns > 1) {
    image->model_hp_buffer = static_cast<model_s*>(_jxr_temp_calloc(
        image, image->tile_columns, sizeof(model_s)));
    image->hp_cbp_model_buffer = static_cast<cbp_model_s*>(_jxr_temp_calloc(
        image, image->tile_columns, sizeof(cbp_model_s)));
  }
  image->cur_my = -1;
}
