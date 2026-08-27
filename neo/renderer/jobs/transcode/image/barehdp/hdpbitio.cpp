#include "hdpbitioread.h"

#include <cstdint>
#include <cstring>

namespace {

hdpError_t CloseWS_Memory(hdpStream_t*) {
  return HDP_ERROR_NONE;
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
  // The retail routine used branchless pointer arithmetic tied to its x64
  // stream layout. Reconstructing that literally on the 32-bit BFG target
  // could read before/after the supplied memory stream once a malformed or
  // partially reconstructed packet exhausted its bytes. Preserve the six-byte
  // refill contract while clamping every physical read to the stream bounds.
  io->byteCount += 6;
  std::uint64_t value = *bitBuffer;
  for (int lane = 0; lane < 6; ++lane) {
    const std::uint8_t byte = io->byteBufCur < io->byteBufEnd
        ? *io->byteBufCur++
        : 0;
    value = (value << 8) | byte;
  }
  *bitBuffer = value;
  *bitCount += 48;
}
