#pragma once

#include "engine/network/lightweightcompression.h"

#include <cstdint>

using objectSize_t = std::int32_t;

constexpr objectSize_t SIZE_STALE = 0x7FFFFFFF;
constexpr objectSize_t SIZE_NOT_STALE = 0x7FFFFFFE;
constexpr std::uint32_t OBJ_VIS_STALE = 1u << 0;
constexpr std::uint32_t OBJ_VIS_NOT_STALE = 1u << 1;
constexpr std::uint32_t OBJ_NEW = 1u << 2;
constexpr std::uint32_t OBJ_DELETED = 1u << 3;
constexpr std::uint32_t OBJ_DIFFERENT = 1u << 4;
constexpr std::uint32_t OBJ_SAME = 1u << 5;

constexpr int ObjDestinationSize(int size) { return (size + 15) & ~15; }

struct objJobState_t {
  std::uint8_t valid{};
  std::uint8_t* data{};
  std::uint16_t size{};
  std::uint16_t objectNum{};
  std::uint32_t visMask{};
};

struct objHeader_t {
  int tag{};
  int objID{};
  int size{};
  int csize{};
  std::uint32_t flags{};
  std::uint8_t* data{};
};

struct alignas(16) objParms_t {
  std::uint8_t visIndex{};
  objJobState_t newState{};
  objJobState_t oldState{};
  objHeader_t* destHeader{};
  std::uint8_t* dest{};
};

struct alignas(8) lzwDelta_t {
  int offset{};
  int size{};
  int snapSequence{};
};

struct lzwInOutData_t {
  int numlzwDeltas{};
  bool fullSnap{};
  lzwDelta_t* lzwDeltas{};
  int maxlzwDeltas{};
  std::uint8_t* lzwMem{};
  int maxlzwMem{};
  int lzwDmaOut{};
  int lzwBytes{};
  int optimalLength{};
  int snapSequence{};
  std::uint16_t lastObjId{};
  lzwCompressionData_t* lzwData{};
};

struct alignas(16) lzwParm_t {
  int numObjects{};
  objHeader_t* headers{};
  int curTime{};
  int baseTime{};
  int baseSequence{};
  bool saveDictionary{};
  bool fragmented{};
  lzwInOutData_t* ioData{};
};

void SnapshotObjectJob(objParms_t* parameters);
void NewLZWStream(lzwParm_t* parameters, idLZWCompressor* compressor);
void LZWJobInternal(lzwParm_t* parameters, unsigned int dmaTag);
void LZWJob(lzwParm_t* parameters);

static_assert(sizeof(objJobState_t) == 24, "Wolf retail snapshot job state ABI drift");
static_assert(sizeof(objHeader_t) == 32, "Wolf retail snapshot object header ABI drift");
static_assert(sizeof(objParms_t) == 80, "Wolf retail snapshot object parameters ABI drift");
static_assert(sizeof(lzwDelta_t) == 16, "Wolf retail snapshot LZW delta ABI drift");
static_assert(sizeof(lzwInOutData_t) == 64, "Wolf retail snapshot LZW state ABI drift");
static_assert(sizeof(lzwParm_t) == 48, "Wolf retail snapshot LZW parameters ABI drift");
