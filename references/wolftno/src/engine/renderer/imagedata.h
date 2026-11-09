#pragma once

#include <cstddef>
#include <cstdint>

struct idImageData {
  idImageData(int w, int h);

  int width;
  int height;
  std::uint8_t* data;
  float* floatData;

  void WriteTGA(const char* filename);
};

static_assert(offsetof(idImageData, data) == 0x8,
              "idImageData data offset drift");
static_assert(offsetof(idImageData, floatData) == 0x10,
              "idImageData floatData offset drift");
static_assert(sizeof(idImageData) == 0x18, "idImageData ABI drift");
