#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

#include <cstdint>

namespace {

bool ReadBit(rbitstream* stream) {
  if (stream->bits_avail == 0) {
    stream->byte = *stream->cur_ptr++;
    ++stream->read_count;
    stream->bits_avail = 8;
  }
  --stream->bits_avail;
  return (stream->byte & (1U << stream->bits_avail)) != 0;
}

std::uint32_t ReadBits(rbitstream* stream, int bitCount) {
  std::uint32_t value = 0;
  for (int bit = 0; bit < bitCount; ++bit)
    value = (value << 1) | static_cast<std::uint32_t>(ReadBit(stream));
  return value;
}

}  // namespace

void _jxr_rbitstream_mark(rbitstream* stream) {
  stream->read_count = 0;
  stream->mark_stream_position =
      static_cast<std::uint64_t>(stream->cur_ptr - stream->data);
}

void _jxr_rbitstream_seek(rbitstream* stream, std::uint64_t offset) {
  stream->read_count = offset;
  stream->cur_ptr = stream->data + stream->mark_stream_position +
                    static_cast<std::int32_t>(offset);
}

void _jxr_rbitstream_syncbyte(rbitstream* stream) {
  stream->bits_avail = 0;
}

bool _jxr_rbitstream_uint1(rbitstream* stream) {
  return ReadBit(stream);
}

std::uint8_t _jxr_rbitstream_uint2(rbitstream* stream) {
  return static_cast<std::uint8_t>(ReadBits(stream, 2));
}

std::uint8_t _jxr_rbitstream_uint3(rbitstream* stream) {
  return static_cast<std::uint8_t>(ReadBits(stream, 3));
}

std::uint8_t _jxr_rbitstream_uint4(rbitstream* stream) {
  return static_cast<std::uint8_t>(ReadBits(stream, 4));
}

std::uint8_t _jxr_rbitstream_uint6(rbitstream* stream) {
  return static_cast<std::uint8_t>(ReadBits(stream, 6));
}

std::uint8_t _jxr_rbitstream_uint8(rbitstream* stream) {
  return static_cast<std::uint8_t>(ReadBits(stream, 8));
}

std::uint16_t _jxr_rbitstream_uint12(rbitstream* stream) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(_jxr_rbitstream_uint8(stream)) << 4) |
      _jxr_rbitstream_uint4(stream));
}

std::uint16_t _jxr_rbitstream_uint15(rbitstream* stream) {
  const std::uint16_t high = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(_jxr_rbitstream_uint8(stream)) << 4) |
      _jxr_rbitstream_uint4(stream));
  return static_cast<std::uint16_t>((high << 3) |
                                    _jxr_rbitstream_uint3(stream));
}

std::uint16_t _jxr_rbitstream_uint16(rbitstream* stream) {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(_jxr_rbitstream_uint8(stream)) << 8) |
      _jxr_rbitstream_uint8(stream));
}

std::uint32_t _jxr_rbitstream_uint32(rbitstream* stream) {
  return (static_cast<std::uint32_t>(_jxr_rbitstream_uint8(stream)) << 24) |
         (static_cast<std::uint32_t>(_jxr_rbitstream_uint8(stream)) << 16) |
         (static_cast<std::uint32_t>(_jxr_rbitstream_uint8(stream)) << 8) |
         _jxr_rbitstream_uint8(stream);
}

std::uint32_t _jxr_rbitstream_uintN(rbitstream* stream, int bitCount) {
  return ReadBits(stream, bitCount);
}

int _jxr_rbitstream_intE(rbitstream* stream, int codeSize,
                         const std::uint8_t* codeLengths,
                         const char* values) {
  int bitsRead = 0;
  std::uint32_t code = 0;
  if (codeLengths[0] != 0) {
    do {
      code = (code << 1) |
             static_cast<std::uint32_t>(_jxr_rbitstream_uint1(stream));
      ++bitsRead;
    } while (codeLengths[code << (codeSize - bitsRead)] != bitsRead);
  }
  return values[code << (codeSize - bitsRead)];
}

std::uint64_t _jxr_rbitstream_intVLW(rbitstream* stream) {
  const std::uint8_t tag = _jxr_rbitstream_uint8(stream);
  if (tag < 251)
    return (static_cast<std::uint64_t>(tag) << 8) |
           _jxr_rbitstream_uint8(stream);
  if (tag == 251)
    return _jxr_rbitstream_uint32(stream);
  if (tag == 252) {
    const std::uint64_t high = _jxr_rbitstream_uint32(stream);
    return (high << 32) | _jxr_rbitstream_uint32(stream);
  }
  return 0;
}
