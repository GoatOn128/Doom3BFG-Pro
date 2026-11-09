#pragma once

#include "engine/renderer/triangles.h"

#include <cstdint>

enum vertexMask_t : std::int32_t {
  VERTEX_MASK_XYZ = 0x1,
  VERTEX_MASK_XYZ1 = 0x1000,
  VERTEX_MASK_ST = 0x2,
  VERTEX_MASK_ST1 = 0x400,
  VERTEX_MASK_NORMAL = 0x4,
  VERTEX_MASK_COLOR = 0x8,
  VERTEX_MASK_TANGENT = 0x10,
  VERTEX_MASK_MORPH = 0x80,
  VERTEX_MASK_XYZ_SHORT = 0x20,
  VERTEX_MASK_ST_SHORT = 0x40,
  VERTEX_MASK_ST1_SHORT = 0x800,
  VERTEX_MASK_ST1_EXTERNAL = 0x2000,
  VERTEX_MASK_INDEX_LONG = 0x100,
  VERTEX_MASK_INDEX_STRIP = 0x200,
};

int SwapVertexBuffer(void* buf, int numVerts, int vertexMask);
void PackToVertexFormat(const idDrawVert* dv, void* packed, int numVerts,
                        int vertexMask);
void TransferVerts(int inputMask, int outputMask,
                   const idVertexScaleAndBias* vsb, int vertexCount,
                   bool byteSwap, const void* inputBuffer, void* outputBuffer);
