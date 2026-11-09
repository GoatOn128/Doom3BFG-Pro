#include "engine/renderer/jobs/transcode/image/barehdp/hdpbitioread.h"

hdpError_t StreamReadAttach(hdpBitIO_t* __restrict io,
                            hdpStream_t* __restrict stream) {
  stream->GetPos(stream, &io->streamOffset);
  io->byteBufStart = static_cast<std::uint8_t*>(stream->ReadPtr(stream));
  io->byteBufEnd = static_cast<std::uint8_t*>(stream->EndPtr(stream));
  io->stream = stream;
  io->byteBufCur = io->byteBufStart;
  io->byteCount = 0;
  io->bitBuff = 0;
  io->bitCnt = 0;
  return HDP_ERROR_NONE;
}

hdpError_t StreamReadDetach(hdpBitIO_t* __restrict io) {
  io->bitCnt -= io->bitCnt & 7;
  io->streamOffset += io->byteCount - (io->bitCnt >> 3);
  io->stream->SetPos(io->stream, io->streamOffset);
  io->stream = nullptr;
  return HDP_ERROR_NONE;
}

