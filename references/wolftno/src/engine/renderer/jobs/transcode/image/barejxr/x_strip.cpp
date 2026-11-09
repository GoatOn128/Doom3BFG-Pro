#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <algorithm>

void _jxr_clear_strip_cur(jxr_image* image) {
  for (int channel = 0; channel < image->num_channels; ++channel) {
    for (unsigned int block = 0; block < image->extended_width >> 4; ++block) {
      std::fill_n(image->strip[channel].cur[block].data, 256, 0);
    }
  }
}

