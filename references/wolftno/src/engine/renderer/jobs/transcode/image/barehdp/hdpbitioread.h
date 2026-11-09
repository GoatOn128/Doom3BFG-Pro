#pragma once

#include <cstddef>
#include <cstdint>

enum hdpError_t : int {
  HDP_ERROR_NONE = 0,
  HDP_ERROR_INVALID_PARM = -1,
  HDP_ERROR_OUT_OF_MEMORY = -2,
  HDP_ERROR_BUFFER_OVERFLOW = -3,
  HDP_ERROR_FILE_IO = -4,
  HDP_ERROR_DECODE = -5
};

struct hdpStream_t {
  union state_t {
    struct file_t {
      void* fp;
    } file;
    struct mem_t {
      std::uint8_t* buffer;
      std::uint64_t bufferSize;
      std::uint64_t bufferOffset;
    } mem;
  } state;
  hdpError_t (*Close)(hdpStream_t*);
  hdpError_t (*Read)(hdpStream_t*, void*, std::uint64_t);
  hdpError_t (*Write)(hdpStream_t*, const void*, std::uint64_t);
  void* (*ReadPtr)(hdpStream_t*);
  void* (*WritePtr)(hdpStream_t*);
  void* (*EndPtr)(hdpStream_t*);
  hdpError_t (*SetPos)(hdpStream_t*, std::uint64_t);
  hdpError_t (*GetPos)(hdpStream_t*, std::uint64_t*);
};

struct hdpBitIO_t {
  std::uint64_t bitBuff;
  std::uint64_t bitCnt;
  std::uint8_t* byteBufStart;
  std::uint8_t* byteBufEnd;
  std::uint8_t* byteBufCur;
  std::uint64_t byteCount;
  hdpStream_t* stream;
  std::uint64_t streamOffset;
};

hdpError_t StreamReadAttach(hdpBitIO_t* __restrict io,
                            hdpStream_t* __restrict stream);
hdpError_t StreamReadDetach(hdpBitIO_t* __restrict io);

hdpError_t ReadWS_Memory(hdpStream_t* stream, void* output,
                         std::uint64_t byteCount);
hdpError_t WriteWS_Memory(hdpStream_t* stream, const void* input,
                          std::uint64_t byteCount);
void* ReadPtrWS_Memory(hdpStream_t* stream);
void* EndPtrWS_Memory(hdpStream_t* stream);
hdpError_t SetPosWS_Memory(hdpStream_t* stream, std::uint64_t offset);
hdpError_t GetPosWS_Memory(hdpStream_t* stream, std::uint64_t* offset);
hdpError_t CreateWS_Memory(hdpStream_t* stream, void* buffer,
                           std::uint64_t byteCount);
void FillBitBuffer(hdpBitIO_t* io, std::uint64_t* bitBuffer,
                   std::uint64_t* bitCount);

static_assert(offsetof(hdpStream_t, ReadPtr) == 0x30,
              "hdpStream_t ReadPtr offset drift");
static_assert(offsetof(hdpStream_t, EndPtr) == 0x40,
              "hdpStream_t EndPtr offset drift");
static_assert(offsetof(hdpStream_t, GetPos) == 0x50,
              "hdpStream_t GetPos offset drift");
static_assert(sizeof(hdpStream_t) == 0x58, "hdpStream_t ABI drift");
static_assert(offsetof(hdpBitIO_t, byteBufStart) == 0x10,
              "hdpBitIO_t byte buffer offset drift");
static_assert(offsetof(hdpBitIO_t, stream) == 0x30,
              "hdpBitIO_t stream offset drift");
static_assert(sizeof(hdpBitIO_t) == 0x40, "hdpBitIO_t ABI drift");
