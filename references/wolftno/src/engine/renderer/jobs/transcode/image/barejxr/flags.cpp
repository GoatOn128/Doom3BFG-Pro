#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

int jxr_get_TILING_FLAG(jxr_image* image) {
  return image->header_flags1 >> 7;
}

