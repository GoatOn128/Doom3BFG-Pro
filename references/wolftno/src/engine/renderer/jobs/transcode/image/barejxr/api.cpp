#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

void _jxr_send_mb_to_output(jxr_image* image, int mx, int my, int* data) {
  if (image->out_fun != nullptr)
    image->out_fun(image, mx, my, data);
}

