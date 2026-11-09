#include "engine/renderer/imagedata.h"

#include "engine/renderer/imagetools/image_files.h"

void idImageData::WriteTGA(const char* filename) {
  R_WriteTGA(filename, data, width, height, 4, false, true, FSPATH_BASE);
}

