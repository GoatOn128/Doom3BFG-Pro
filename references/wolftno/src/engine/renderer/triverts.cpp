#include "engine/renderer/triverts.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <limits>

namespace {

template <typename T>
T Read(const std::uint8_t*& cursor) {
  T value;
  std::memcpy(&value, cursor, sizeof(value));
  cursor += sizeof(value);
  return value;
}

template <typename T>
void Write(std::uint8_t*& cursor, const T& value) {
  std::memcpy(cursor, &value, sizeof(value));
  cursor += sizeof(value);
}

void Swap16(void* value) {
  auto* bytes = static_cast<std::uint8_t*>(value);
  std::swap(bytes[0], bytes[1]);
}

void Swap32(void* value) {
  auto* bytes = static_cast<std::uint8_t*>(value);
  std::swap(bytes[0], bytes[3]);
  std::swap(bytes[1], bytes[2]);
}

int TruncateFloatToInt(float value) {
  return _mm_cvtt_ss2si(_mm_set_ss(value));
}

std::int16_t ClampToShort(int value) {
  if (value < -32768) {
    return std::numeric_limits<std::int16_t>::min();
  }
  if (value > 32767) {
    return std::numeric_limits<std::int16_t>::max();
  }
  return static_cast<std::int16_t>(value);
}

struct TransferVertex {
  float xyz[3]{};
  float xyz1[3]{};
  std::int16_t xyzShort[3]{};
  float st[2]{};
  float st1[2]{};
  std::int16_t stShort[2]{};
  std::int16_t st1Short[2]{};
  std::uint32_t normal{};
  std::uint32_t tangent{};
  std::uint32_t color{};
};

void SwapNumericFields(TransferVertex& vertex) {
  for (float& component : vertex.xyz) {
    Swap32(&component);
  }
  for (float& component : vertex.xyz1) {
    Swap32(&component);
  }
  for (std::int16_t& component : vertex.xyzShort) {
    Swap16(&component);
  }
  for (float& component : vertex.st) {
    Swap32(&component);
  }
  for (float& component : vertex.st1) {
    Swap32(&component);
  }
  for (std::int16_t& component : vertex.stShort) {
    Swap16(&component);
  }
  for (std::int16_t& component : vertex.st1Short) {
    Swap16(&component);
  }
}

}  // namespace

int SwapVertexBuffer(void* buf, int numVerts, int vertexMask) {
  auto* const start = reinterpret_cast<std::uint8_t*>(buf);
  auto* cursor = start;
  if (numVerts <= 0) {
    return 0;
  }

  const std::uint32_t mask = static_cast<std::uint32_t>(vertexMask);
  for (int vertex = 0; vertex < numVerts; ++vertex) {
    if ((mask & VERTEX_MASK_XYZ_SHORT) != 0) {
      Swap16(cursor);
      Swap16(cursor + 2);
      Swap16(cursor + 4);
      cursor += 8;
    } else if ((mask & VERTEX_MASK_XYZ) != 0) {
      Swap32(cursor);
      Swap32(cursor + 4);
      Swap32(cursor + 8);
      cursor += 12;
    }

    if ((mask & VERTEX_MASK_XYZ1) != 0) {
      Swap32(cursor);
      Swap32(cursor + 4);
      Swap32(cursor + 8);
      cursor += 12;
    }

    if ((mask & VERTEX_MASK_ST_SHORT) != 0) {
      Swap16(cursor);
      Swap16(cursor + 2);
      cursor += 4;
    } else if ((mask & VERTEX_MASK_ST) != 0) {
      Swap32(cursor);
      Swap32(cursor + 4);
      cursor += 8;
    }

    if ((mask & VERTEX_MASK_ST1_SHORT) != 0) {
      Swap16(cursor);
      Swap16(cursor + 2);
      cursor += 4;
    } else if ((mask & VERTEX_MASK_ST1) != 0) {
      Swap32(cursor);
      Swap32(cursor + 4);
      cursor += 8;
    }

    if ((mask & VERTEX_MASK_NORMAL) != 0) {
      cursor += 4;
    }
    if ((mask & VERTEX_MASK_TANGENT) != 0) {
      cursor += 4;
    }
    if ((mask & VERTEX_MASK_COLOR) != 0) {
      cursor += 4;
    }
  }

  return static_cast<int>(cursor - start);
}

void PackToVertexFormat(const idDrawVert* dv, void* packed, int numVerts,
                        int vertexMask) {
  if (numVerts <= 0) {
    return;
  }

  const std::uint32_t mask = static_cast<std::uint32_t>(vertexMask);
  auto* output = reinterpret_cast<std::uint8_t*>(packed);
  for (int vertex = 0; vertex < numVerts; ++vertex, ++dv) {
    if ((mask & VERTEX_MASK_XYZ) != 0) {
      if ((mask & VERTEX_MASK_XYZ_SHORT) != 0) {
        const std::int16_t xyzShort[4] = {
            ClampToShort(TruncateFloatToInt(dv->xyz.x)),
            ClampToShort(TruncateFloatToInt(dv->xyz.y)),
            ClampToShort(TruncateFloatToInt(dv->xyz.z)), 1};
        Write(output, xyzShort);
      } else {
        Write(output, dv->xyz);
      }
    }

    if ((mask & VERTEX_MASK_ST) != 0) {
      if ((mask & VERTEX_MASK_ST_SHORT) != 0) {
        const std::int16_t stShort[2] = {
            ClampToShort(TruncateFloatToInt(dv->st.x)),
            ClampToShort(TruncateFloatToInt(dv->st.y))};
        Write(output, stShort);
      } else {
        Write(output, dv->st);
      }
    }

    if ((mask & VERTEX_MASK_NORMAL) != 0) {
      std::memcpy(output, dv->normal, 4);
      output += 4;
    }
    if ((mask & VERTEX_MASK_TANGENT) != 0) {
      std::memcpy(output, dv->tangent, 4);
      output += 4;
    }
    if ((mask & VERTEX_MASK_COLOR) != 0) {
      std::memcpy(output, dv->color, 4);
      output += 4;
    }
  }
}

void TransferVerts(int inputMask, int outputMask,
                   const idVertexScaleAndBias* vsb, int vertexCount,
                   bool byteSwap, const void* inputBuffer,
                   void* outputBuffer) {
  TransferVertex vertex;
  if (vertexCount <= 0) {
    return;
  }

  const auto* input = static_cast<const std::uint8_t*>(inputBuffer);
  auto* output = static_cast<std::uint8_t*>(outputBuffer);
  for (int vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
    if ((inputMask & VERTEX_MASK_XYZ_SHORT) != 0) {
      std::memcpy(vertex.xyzShort, input, sizeof(vertex.xyzShort));
      input += 8;
    } else if ((inputMask & VERTEX_MASK_XYZ) != 0) {
      std::memcpy(vertex.xyz, input, sizeof(vertex.xyz));
      input += sizeof(vertex.xyz);
    }

    if ((inputMask & VERTEX_MASK_XYZ1) != 0) {
      std::memcpy(vertex.xyz1, input, sizeof(vertex.xyz1));
      input += sizeof(vertex.xyz1);
    }

    if ((inputMask & VERTEX_MASK_ST_SHORT) != 0) {
      std::memcpy(vertex.stShort, input, sizeof(vertex.stShort));
      input += sizeof(vertex.stShort);
    } else if ((inputMask & VERTEX_MASK_ST) != 0) {
      std::memcpy(vertex.st, input, sizeof(vertex.st));
      input += sizeof(vertex.st);
    }

    // The retail routine reads compressed ST1 but has no uncompressed ST1
    // input branch. The zero-initialized uncompressed value is still emitted.
    if ((inputMask & VERTEX_MASK_ST1_SHORT) != 0) {
      std::memcpy(vertex.st1Short, input, sizeof(vertex.st1Short));
      input += sizeof(vertex.st1Short);
    }

    if ((inputMask & VERTEX_MASK_NORMAL) != 0) {
      vertex.normal = Read<std::uint32_t>(input);
    }
    if ((inputMask & VERTEX_MASK_TANGENT) != 0) {
      vertex.tangent = Read<std::uint32_t>(input);
    }
    if ((inputMask & VERTEX_MASK_COLOR) != 0) {
      vertex.color = Read<std::uint32_t>(input);
    }

    if (byteSwap) {
      SwapNumericFields(vertex);
    }

    if ((outputMask & VERTEX_MASK_XYZ_SHORT) != 0) {
      if ((inputMask & VERTEX_MASK_XYZ_SHORT) == 0) {
        const float* scale = &vsb->xyzScale.x;
        const float* bias = &vsb->xyzBias.x;
        for (int component = 0; component < 3; ++component) {
          if (scale[component] == 0.0f) {
            vertex.xyzShort[component] = 0;
          } else {
            const float snapped =
                std::floor(vertex.xyz[component] * 4.0f) * 0.25f;
            const float quantized =
                std::floor((snapped - bias[component]) / scale[component] +
                           0.5f);
            vertex.xyzShort[component] =
                ClampToShort(TruncateFloatToInt(quantized));
          }
        }
      }
    } else if ((inputMask & VERTEX_MASK_XYZ_SHORT) != 0) {
      const float* scale = &vsb->xyzScale.x;
      const float* bias = &vsb->xyzBias.x;
      for (int component = 0; component < 3; ++component) {
        vertex.xyz[component] =
            static_cast<float>(vertex.xyzShort[component]) *
                scale[component] +
            bias[component];
      }
    }

    if ((outputMask & VERTEX_MASK_ST_SHORT) != 0) {
      if ((inputMask & VERTEX_MASK_ST_SHORT) == 0) {
        const float* scale = &vsb->stScale.x;
        const float* bias = &vsb->stBias.x;
        for (int component = 0; component < 2; ++component) {
          if (scale[component] == 0.0f) {
            vertex.stShort[component] = 0;
          } else {
            const float quantized =
                std::floor((vertex.st[component] - bias[component]) /
                                   scale[component] +
                           0.5f);
            vertex.stShort[component] =
                ClampToShort(TruncateFloatToInt(quantized));
          }
        }
      }
    } else if ((inputMask & VERTEX_MASK_ST_SHORT) != 0) {
      const float* scale = &vsb->stScale.x;
      const float* bias = &vsb->stBias.x;
      for (int component = 0; component < 2; ++component) {
        vertex.st[component] =
            static_cast<float>(vertex.stShort[component]) * scale[component] +
            bias[component];
      }
    }

    if ((outputMask & VERTEX_MASK_XYZ_SHORT) != 0) {
      std::memcpy(output, vertex.xyzShort, sizeof(vertex.xyzShort));
      output += 8;
    } else if ((outputMask & VERTEX_MASK_XYZ) != 0) {
      std::memcpy(output, vertex.xyz, sizeof(vertex.xyz));
      output += sizeof(vertex.xyz);
    }

    if ((outputMask & VERTEX_MASK_XYZ1) != 0) {
      std::memcpy(output, vertex.xyz1, sizeof(vertex.xyz1));
      output += sizeof(vertex.xyz1);
    }

    if ((outputMask & VERTEX_MASK_ST_SHORT) != 0) {
      Write(output, vertex.stShort);
    } else if ((outputMask & VERTEX_MASK_ST) != 0) {
      Write(output, vertex.st);
    }

    if ((outputMask & VERTEX_MASK_ST1_SHORT) != 0) {
      Write(output, vertex.st1Short);
    } else if ((outputMask & VERTEX_MASK_ST1) != 0) {
      Write(output, vertex.st1);
    }

    if ((outputMask & VERTEX_MASK_NORMAL) != 0) {
      Write(output, vertex.normal);
    }
    if ((outputMask & VERTEX_MASK_TANGENT) != 0) {
      Write(output, vertex.tangent);
    }
    if ((outputMask & VERTEX_MASK_COLOR) != 0) {
      Write(output, vertex.color);
    }
  }
}
