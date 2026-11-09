#pragma once

#include "shared/idlib/math/vector.h"
#include "shared/idlib/networking/bitmsg.h"

#include <algorithm>
#include <cmath>
#include <cstring>

class idSerializer {
public:
  bool writing = false;
  std::uint8_t padding[7]{};
  idBitMsg* msg = nullptr;

  void SerializePacked(int* original);

  void Serialize(std::uint64_t* value) {
    if (writing) {
      msg->WriteBits(static_cast<int>(*value), 32);
      msg->WriteBits(static_cast<int>(*value >> 32u), 32);
    } else {
      const std::uint64_t low =
          static_cast<std::uint32_t>(msg->ReadBits(32));
      const std::uint64_t high =
          static_cast<std::uint32_t>(msg->ReadBits(32));
      *value = low | (high << 32u);
    }
  }

  template <int Maximum, int Bits> void SerializeUQ(float* value) {
    constexpr int quantizedMaximum = (1U << Bits) - 1U;
    constexpr int integerScale = quantizedMaximum / Maximum;
    if (writing) {
      const int quantized = static_cast<int>(*value * integerScale);
      msg->WriteBits(std::max(0, std::min(quantizedMaximum, quantized)),
                     Bits);
    } else {
      *value = static_cast<float>(msg->ReadBits(Bits)) *
               (static_cast<float>(Maximum) /
                static_cast<float>(quantizedMaximum));
    }
  }

  template <int Maximum, int Bits> void SerializeQ(float* value) {
    constexpr int quantizedMaximum = (1U << (Bits - 1)) - 1U;
    constexpr int integerScale = quantizedMaximum / Maximum;
    if (writing) {
      const int quantized = static_cast<int>(*value * integerScale);
      msg->WriteBits(std::max(-quantizedMaximum,
                             std::min(quantizedMaximum, quantized)),
                     -Bits);
    } else {
      *value = static_cast<float>(msg->ReadBits(-Bits)) *
               (static_cast<float>(Maximum) /
                static_cast<float>(quantizedMaximum));
    }
  }

  void Serialize(idVec3* value) {
    float* components = &value->x;
    for (int index = 0; index < 3; ++index) {
      int bits = 0;
      if (writing) {
        std::memcpy(&bits, &components[index], sizeof(bits));
        msg->WriteBits(bits, 32);
      } else {
        bits = msg->ReadBits(32);
        std::memcpy(&components[index], &bits, sizeof(bits));
      }
    }
  }

  void Serialize(idVec3& value) { Serialize(&value); }
  void Serialize(idVec6& value);
  void Serialize(idMat3& value);

  void SerializeQ(idMat3* axis, int bits);
};

static_assert(sizeof(idSerializer) == 16, "idSerializer ABI drift");
