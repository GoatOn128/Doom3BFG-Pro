#include "engine/models/skeletalanimation/md6morph.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00ff0000u) |
         ((value >> 8) & 0x0000ff00u) | (value >> 24);
}

void WriteBigInt(idFile* file, int value) {
  const std::uint32_t big = ByteSwap32(static_cast<std::uint32_t>(value));
  file->Write(&big, sizeof(big));
}

void SwapFloat(float& value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = ByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
}

void SwapVec3(idVec3& value) {
  SwapFloat(value.x);
  SwapFloat(value.y);
  SwapFloat(value.z);
}

void WriteBigVec3(idFile* file, const idVec3& value) {
  idVec3 big = value;
  SwapVec3(big);
  file->Write(&big, sizeof(big));
}

void WriteBigMat3(idFile* file, const idMat3& value) {
  idMat3 big = value;
  for (idVec3& row : big.mat)
    SwapVec3(row);
  file->Write(&big, sizeof(big));
}

int ReadBigInt(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  return static_cast<int>(ByteSwap32(value));
}

void ReadBigVec3(idFile* file, idVec3& value) {
  file->Read(&value, sizeof(value));
  SwapVec3(value);
}

void ReadBigMat3(idFile* file, idMat3& value) {
  file->Read(&value, sizeof(value));
  for (idVec3& row : value.mat)
    SwapVec3(row);
}

template <typename T, int Tag>
void SetSerializedCount(idList<T, Tag>& list, int count) {
  if (count > list.size)
    list.ResizeForPort(count);
  list.num = std::min(count, list.size);
}

} // namespace

void idMD6Morph::Write_Binary(idFile* file) const {
  WriteBigInt(file, sets.num);
  for (int setIndex = 0; setIndex < sets.num; ++setIndex) {
    const idMD6MorphSet& set = sets.list[setIndex];
    WriteBigInt(file, set.meshIndex);
    WriteBigInt(file, set.groupIndex);
    WriteBigInt(file, static_cast<int>(set.type));
    WriteBigVec3(file, set.pos);
    WriteBigVec3(file, set.size);
    WriteBigMat3(file, set.axis);
    WriteBigInt(file, set.indices.num);
    WriteBigInt(file, set.direction.num);
    for (int index = 0; index < set.indices.num; ++index) {
      const std::uint16_t value = ByteSwap16(set.indices.list[index]);
      file->Write(&value, sizeof(value));
    }
    for (int index = 0; index < set.direction.num; ++index)
      file->Write(&set.direction.list[index], sizeof(set.direction.list[index]));
  }
}

void idMD6Morph::Load_Binary(idFile* file, unsigned int magic) {
  (void)magic;
  SetSerializedCount(sets, ReadBigInt(file));
  for (int setIndex = 0; setIndex < sets.num; ++setIndex) {
    idMD6MorphSet& set = sets.list[setIndex];
    set.meshIndex = ReadBigInt(file);
    set.groupIndex = ReadBigInt(file);
    set.type = static_cast<morphDataType_t>(ReadBigInt(file));
    ReadBigVec3(file, set.pos);
    ReadBigVec3(file, set.size);
    ReadBigMat3(file, set.axis);
    SetSerializedCount(set.indices, ReadBigInt(file));
    SetSerializedCount(set.direction, ReadBigInt(file));
    if (set.indices.num > 0) {
      file->Read(set.indices.list,
                 sizeof(set.indices.list[0]) *
                     static_cast<std::size_t>(set.indices.num));
      for (int index = 0; index < set.indices.num; ++index)
        set.indices.list[index] = ByteSwap16(set.indices.list[index]);
    }
    if (set.direction.num > 0) {
      file->Read(set.direction.list,
                 sizeof(set.direction.list[0]) *
                     static_cast<std::size_t>(set.direction.num));
    }
  }
}

idMD6MorphSet& idMD6Morph::AllocSet() {
  return sets.Alloc();
}
