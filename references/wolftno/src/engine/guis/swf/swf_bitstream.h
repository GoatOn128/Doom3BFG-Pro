#pragma once

#include "engine/guis/swf/swf_types.h"

#include <cstdint>
#include <cstring>

class idSWFBitStream {
public:
  idSWFBitStream() { Free(); }
  idSWFBitStream(const std::uint8_t* data, std::uint32_t len, bool copy) : free(false) {
    Load(data, len, copy);
  }
  ~idSWFBitStream() { Free(); }
  idSWFBitStream& operator=(idSWFBitStream& other);

  void Load(const std::uint8_t* data, std::uint32_t len, bool copy);
  void Free();
  const std::uint8_t* Ptr() const { return startp; }
  std::uint32_t Length() const { return static_cast<std::uint32_t>(endp - startp); }
  std::uint32_t Tell() const { return static_cast<std::uint32_t>(readp - startp); }
  void Seek(std::int32_t offset) { readp += offset; }
  void Rewind() { readp = startp; }
  void ResetBits() { currentBit = currentByte = 0; }

  int ReadS(unsigned int numBits);
  unsigned int ReadU(unsigned int numBits);
  bool ReadBool() { return ReadU(1) != 0; }
  const std::uint8_t* ReadData(int size);
  std::uint8_t ReadU8() { ResetBits(); return *readp++; }
  std::uint16_t ReadU16() { ResetBits(); readp += 2; return readp[-2] | (readp[-1] << 8); }
  std::uint32_t ReadU32() { ResetBits(); readp += 4; return readp[-4] | (readp[-3]<<8) | (readp[-2]<<16) | (readp[-1]<<24); }
  std::int16_t ReadS16() { return static_cast<std::int16_t>(ReadU16()); }
  std::int32_t ReadS32() { return static_cast<std::int32_t>(ReadU32()); }
  std::uint32_t ReadEncodedU32();
  float ReadFixed8() { return static_cast<std::int16_t>(ReadU16()) / 256.0f; }
  float ReadFixed16() { return static_cast<std::int32_t>(ReadU32()) / 65536.0f; }
  float ReadFloat() { const std::uint32_t value=ReadU32(); float result; std::memcpy(&result,&value,4); return result; }
  double ReadDouble();
  const char* ReadString();
  void ReadRect(swfRect_t& rect);
  void ReadMatrix(swfMatrix_t& matrix);
  void ReadColorXFormRGBA(swfColorXform_t& cxf);
  void ReadColorRGB(swfColorRGB_t& color);
  void ReadColorRGBA(swfColorRGBA_t& color);
  void ReadGradient(swfGradient_t& grad, bool rgba);
  void ReadMorphGradient(swfGradient_t& grad);

private:
  int ReadInternalS(std::uint64_t& bit, std::uint64_t& byte, unsigned int numBits);
  unsigned int ReadInternalU(std::uint64_t& bit, std::uint64_t& byte, unsigned int numBits);
  bool free;
  const std::uint8_t* startp;
  const std::uint8_t* endp;
  const std::uint8_t* readp;
  std::uint64_t currentBit;
  std::uint64_t currentByte;
};

static_assert(sizeof(idSWFBitStream) == 48, "idSWFBitStream ABI drift");
