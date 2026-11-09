#include "engine/renderer/jobs/transcode/image/barehdp/hdpbitioread.h"

#include <cstdint>
#include <cstring>

namespace {

hdpError_t CloseWS_Memory(hdpStream_t*) {
  return HDP_ERROR_NONE;
}

std::int64_t RefillByteIndex(int remaining, int lane) {
  const auto delta = static_cast<std::int64_t>(
      static_cast<std::int32_t>(remaining - lane));
  const std::int64_t signBias =
      static_cast<std::int64_t>(lane - 1) * 0x80000000LL;
  return (delta ^ signBias) >> 31;
}

}  // namespace

hdpError_t ReadWS_Memory(hdpStream_t* stream, void* output,
                         std::uint64_t byteCount) {
  const std::uint64_t offset = stream->state.mem.bufferOffset;
  if (offset + byteCount > stream->state.mem.bufferSize)
    return HDP_ERROR_BUFFER_OVERFLOW;
  std::memmove(output, stream->state.mem.buffer + offset,
               static_cast<std::size_t>(byteCount));
  stream->state.mem.bufferOffset += byteCount;
  return HDP_ERROR_NONE;
}

hdpError_t WriteWS_Memory(hdpStream_t* stream, const void* input,
                          std::uint64_t byteCount) {
  const std::uint64_t offset = stream->state.mem.bufferOffset;
  if (offset + byteCount > stream->state.mem.bufferSize)
    return HDP_ERROR_BUFFER_OVERFLOW;
  std::memmove(stream->state.mem.buffer + offset, input,
               static_cast<std::size_t>(byteCount));
  stream->state.mem.bufferOffset += byteCount;
  return HDP_ERROR_NONE;
}

void* ReadPtrWS_Memory(hdpStream_t* stream) {
  return stream->state.mem.buffer + stream->state.mem.bufferOffset;
}

void* EndPtrWS_Memory(hdpStream_t* stream) {
  return stream->state.mem.buffer + stream->state.mem.bufferSize;
}

hdpError_t SetPosWS_Memory(hdpStream_t* stream, std::uint64_t offset) {
  stream->state.mem.bufferOffset = offset;
  return HDP_ERROR_NONE;
}

hdpError_t GetPosWS_Memory(hdpStream_t* stream, std::uint64_t* offset) {
  *offset = stream->state.mem.bufferOffset;
  return HDP_ERROR_NONE;
}

hdpError_t CreateWS_Memory(hdpStream_t* stream, void* buffer,
                           std::uint64_t byteCount) {
  stream->state.mem.buffer = static_cast<std::uint8_t*>(buffer);
  stream->state.mem.bufferSize = byteCount;
  stream->state.mem.bufferOffset = 0;
  stream->Close = CloseWS_Memory;
  stream->Read = ReadWS_Memory;
  stream->Write = WriteWS_Memory;
  stream->ReadPtr = ReadPtrWS_Memory;
  stream->WritePtr = ReadPtrWS_Memory;
  stream->EndPtr = EndPtrWS_Memory;
  stream->SetPos = SetPosWS_Memory;
  stream->GetPos = GetPosWS_Memory;
  return HDP_ERROR_NONE;
}

void FillBitBuffer(hdpBitIO_t* io, std::uint64_t* bitBuffer,
                   std::uint64_t* bitCount) {
  const std::uint32_t endLow = static_cast<std::uint32_t>(
      reinterpret_cast<std::uintptr_t>(io->byteBufEnd));
  const std::uint32_t currentLow = static_cast<std::uint32_t>(
      reinterpret_cast<std::uintptr_t>(io->byteBufCur));
  const int remaining =
      static_cast<std::int32_t>(endLow - currentLow);
  std::uint8_t* const current = io->byteBufCur;
  io->byteCount += 6;
  std::uint64_t value = *bitBuffer;
  for (int lane = 1; lane <= 6; ++lane) {
    value = (value << 8) |
            current[RefillByteIndex(remaining, lane)];
  }
  *bitBuffer = value;
  *bitCount += 48;
  if (remaining >= 0)
    io->byteBufCur += 6;
}
