#pragma once

class idMaterial;

// Source-material provenance shared by static, discrete, and skeletal models.
// The retail binary formats serialize this record as five 32-bit/64-bit
// fields and require the 24-byte Win64 layout.
struct sourceSurface_t {
  const idMaterial* mtr{};
  int mtrChecksum{};
  int renderSurface{};
  int firstVertex{};
  int lastVertex{};
};

static_assert(sizeof(sourceSurface_t) == 24, "sourceSurface_t ABI drift");
