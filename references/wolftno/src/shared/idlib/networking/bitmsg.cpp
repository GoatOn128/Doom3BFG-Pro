#include "bitmsg.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

idBitMsg::idBitMsg()
    : writeData(nullptr), readData(nullptr), maxSize(0), curSize(0), writeBit(0), readCount(0),
      readBit(0), allowOverflow(false), overflowed(false), tempValue(0) {}
void idBitMsg::InitWrite(void* data, int length) {
  writeData = static_cast<std::uint8_t*>(data);
  readData = writeData;
  maxSize = length;
  BeginWriting();
}
void idBitMsg::InitRead(const void* data, int length) {
  writeData = nullptr;
  readData = static_cast<const std::uint8_t*>(data);
  maxSize = curSize = length;
  BeginReading();
}
void idBitMsg::BeginWriting() {
  curSize = 0;
  writeBit = 0;
  readCount = 0;
  readBit = 0;
  overflowed = false;
  tempValue = 0;
  if (writeData && maxSize > 0)
    writeData[0] = 0;
}
void idBitMsg::BeginReading() {
  readCount = readBit = 0;
}
bool idBitMsg::CheckOverflow(int numBits) {
  if (numBits <= maxSize * 8 - curSize * 8 - writeBit)
    return false;
  overflowed = true;
  if (allowOverflow) {
    BeginWriting();
    return numBits > maxSize * 8;
  }
  return true;
}
void idBitMsg::WriteBits(int value, int numBits) {
  if (!writeData || numBits == 0)
    return;
  int bits = std::min(numBits < 0 ? -numBits : numBits, 32);
  if (CheckOverflow(bits))
    return;
  std::uint32_t encoded = static_cast<std::uint32_t>(value);
  if (bits < 32)
    encoded &= (1u << bits) - 1u;
  for (int i = 0; i < bits; ++i) {
    if (writeBit == 0)
      writeData[curSize] = 0;
    if (encoded & (1u << i))
      writeData[curSize] |= static_cast<std::uint8_t>(1u << writeBit);
    if (++writeBit == 8) {
      writeBit = 0;
      ++curSize;
    }
  }
  tempValue = writeBit && curSize < maxSize ? writeData[curSize] : 0;
}
int idBitMsg::ReadBits(int numBits) {
  if (!readData || numBits == 0)
    return 0;
  const bool sign = numBits < 0;
  int bits = std::min(sign ? -numBits : numBits, 32);
  std::uint32_t value = 0;
  for (int i = 0; i < bits; ++i) {
    if (readCount >= curSize) {
      overflowed = true;
      break;
    }
    if (readData[readCount] & (1u << readBit))
      value |= 1u << i;
    if (++readBit == 8) {
      readBit = 0;
      ++readCount;
    }
  }
  if (sign && bits < 32 && (value & (1u << (bits - 1))))
    value |= ~((1u << bits) - 1u);
  return static_cast<int>(value);
}
void idBitMsg::WriteData(const void* data, int length) {
  if (!writeData || !data || length <= 0)
    return;
  if (writeBit) {
    writeBit = 0;
    ++curSize;
    tempValue = 0;
  }
  if (CheckOverflow(length * 8))
    return;
  std::memcpy(writeData + curSize, data, length);
  curSize += length;
  if (curSize < maxSize)
    writeData[curSize] = 0;
}
int idBitMsg::ReadData(void* data, int length) {
  if (!readData || length <= 0)
    return 0;
  if (readBit) {
    readBit = 0;
    ++readCount;
  }
  const int count = std::min(length, std::max(0, curSize - readCount));
  if (data && count > 0)
    std::memcpy(data, readData + readCount, count);
  readCount += count;
  if (count != length)
    overflowed = true;
  return count;
}
void idBitMsg::WriteDelta(int oldValue, int newValue, int numBits) {
  if (oldValue == newValue)
    WriteBits(0, 1);
  else {
    WriteBits(1, 1);
    WriteBits(newValue, numBits);
  }
}
int idBitMsg::ReadDelta(int oldValue, int numBits) {
  return ReadBits(1) ? ReadBits(numBits) : oldValue;
}
void idBitMsg::WriteString(const char* text, int maximumLength, bool make7Bit) {
  if (!text) {
    WriteByte(0);
    return;
  }
  int length = idStr::Length(text);
  if (maximumLength >= 0)
    length = std::min(length, std::max(0, maximumLength - 1));
  for (int i = 0; i < length; ++i) {
    unsigned char value = static_cast<unsigned char>(text[i]);
    if (make7Bit && value > 127)
      value = '.';
    WriteByte(value);
  }
  WriteByte(0);
}
int idBitMsg::ReadString(char* buffer, int bufferSize) {
  if (!buffer || bufferSize <= 0)
    return 0;
  int length = 0;
  while (readCount < curSize) {
    const int value = ReadByte();
    if (value == 0)
      break;
    if (length + 1 < bufferSize)
      buffer[length++] = static_cast<char>(value);
  }
  buffer[length] = '\0';
  return length;
}
int idBitMsg::ReadString(idStr& text) {
  char buffer[4096];
  const int length = ReadString(buffer, sizeof(buffer));
  text = buffer;
  return length;
}
void idBitMsg::WriteNetadr(const netadr_t& address) {
  WriteData(address.ip, 4);
  WriteUShort(address.port);
  WriteByte(address.type);
}
void idBitMsg::ReadNetadr(netadr_t& address) {
  ReadData(address.ip, 4);
  address.port = static_cast<std::uint16_t>(ReadUShort());
  address.type = static_cast<netadrtype_t>(ReadByte());
}
namespace {
void WriteDeltaCounter(idBitMsg& msg, int oldValue, int newValue, int countBits) {
  const int delta = newValue - oldValue;
  int magnitude = std::abs(delta), valueBits = 1;
  while ((magnitude >>= 1) != 0)
    ++valueBits;
  msg.WriteBits(valueBits - 1, countBits);
  msg.WriteBits(delta, -(valueBits + 1));
}
int ReadDeltaCounter(idBitMsg& msg, int oldValue, int countBits) {
  const int valueBits = msg.ReadBits(countBits) + 1;
  return oldValue + msg.ReadBits(-(valueBits + 1));
}
} // namespace
void idBitMsg::WriteDeltaShortCounter(int oldValue, int newValue) {
  WriteDeltaCounter(*this, oldValue, newValue, 4);
}
void idBitMsg::WriteDeltaLongCounter(int oldValue, int newValue) {
  WriteDeltaCounter(*this, oldValue, newValue, 5);
}
int idBitMsg::ReadDeltaShortCounter(int oldValue) {
  return ReadDeltaCounter(*this, oldValue, 4);
}
int idBitMsg::ReadDeltaLongCounter(int oldValue) {
  return ReadDeltaCounter(*this, oldValue, 5);
}
