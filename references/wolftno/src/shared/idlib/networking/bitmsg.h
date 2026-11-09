#pragma once

#include "shared/idlib/text/str.h"
#include <cstdint>

enum netadrtype_t { NA_BAD, NA_LOOPBACK, NA_BROADCAST, NA_IP };
struct netadr_t {
  netadrtype_t type;
  std::uint8_t ip[4];
  std::uint16_t port;
};

class idBitMsg {
public:
  idBitMsg();
  void InitWrite(void* data, int length);
  void InitRead(const void* data, int length);
  void BeginWriting();
  void BeginReading();
  bool CheckOverflow(int numBits);
  void WriteBits(int value, int numBits);
  int ReadBits(int numBits);
  // Retail serializer callers use the const-qualified ABI entry point. The
  // non-const declaration remains for the current idlib implementation.
  int ReadBits(int numBits) const;
  void WriteData(const void* data, int length);
  int ReadData(void* data, int length);
  void WriteDelta(int oldValue, int newValue, int numBits);
  int ReadDelta(int oldValue, int numBits);
  void WriteString(const char* text, int maximumLength = -1, bool make7Bit = true);
  int ReadString(idStr& text);
  int ReadString(char* buffer, int bufferSize);
  void WriteNetadr(const netadr_t& address);
  void ReadNetadr(netadr_t& address);
  void WriteDeltaShortCounter(int oldValue, int newValue);
  void WriteDeltaLongCounter(int oldValue, int newValue);
  int ReadDeltaShortCounter(int oldValue);
  int ReadDeltaLongCounter(int oldValue);
  template <typename VectorType, int Maximum, int NumBits>
  void WriteQuantizedVector(const VectorType& vector);
  void WriteByte(int value) { WriteBits(value, 8); }
  void WriteUShort(int value) { WriteBits(value, 16); }
  int ReadByte() { return ReadBits(8); }
  int ReadUShort() { return ReadBits(16); }
  int GetSize() const { return curSize + (writeBit != 0 ? 1 : 0); }
  int GetReadCount() const { return readCount; }
  bool IsOverflowed() const { return overflowed; }
  std::uint8_t* writeData;
  const std::uint8_t* readData;
  int maxSize;
  int curSize;
  int writeBit;
  int readCount;
  int readBit;
  bool allowOverflow;
  bool overflowed;
  std::uint64_t tempValue;
};
static_assert(sizeof(netadr_t) == 12, "net address ABI drift");
static_assert(sizeof(idBitMsg) == 48, "idBitMsg ABI drift");
