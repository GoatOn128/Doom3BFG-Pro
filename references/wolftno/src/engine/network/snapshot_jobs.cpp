#include "engine/network/snapshot_jobs.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace {

bool ObjectsSame(const objJobState_t& newer, const objJobState_t& older) {
  return newer.size == older.size && std::memcmp(newer.data, older.data, newer.size) == 0;
}

void FinishLZWStream(lzwParm_t* parameters, idLZWCompressor& compressor) {
  if (compressor.IsOverflowed()) {
    compressor.Restore();
  }
  lzwDelta_t& delta = parameters->ioData->lzwDeltas[parameters->ioData->numlzwDeltas];
  if (compressor.End() == -1) {
    delta.offset = -1;
    delta.size = -1;
    delta.snapSequence = -1;
  } else {
    delta.offset = parameters->ioData->lzwBytes;
    delta.size = compressor.Length();
    delta.snapSequence = parameters->ioData->snapSequence;
    parameters->ioData->lzwBytes += compressor.Length();
  }
  ++parameters->ioData->numlzwDeltas;
}

} // namespace

void SnapshotObjectJob(objParms_t* parameters) {
  objJobState_t& newer = parameters->newState;
  objJobState_t& older = parameters->oldState;
  objHeader_t& header = *parameters->destHeader;
  std::uint8_t* destination = parameters->dest;
  header.flags = 0;
  header.size = newer.valid ? newer.size : 0;
  header.csize = 0;
  header.data = destination;
  header.tag = static_cast<int>(0xABADF00Du);
  header.objID = -1;

  bool visibilityChanged = false;
  bool sendState = false;
  if (newer.valid && older.valid) {
    if (parameters->visIndex > 0) {
      const std::uint32_t visibilityBit = 1u << parameters->visIndex;
      const bool oldVisible = (older.visMask & visibilityBit) != 0;
      bool newVisible = (newer.visMask & visibilityBit) != 0;
      newVisible = newVisible || ((newer.size == 0) != (older.size == 0));
      if (!oldVisible && !newVisible) {
        header.flags |= OBJ_SAME;
        return;
      }
      if (oldVisible && !newVisible) {
        visibilityChanged = true;
      } else if (!oldVisible && newVisible) {
        visibilityChanged = true;
        sendState = true;
      }
    }
    if (!visibilityChanged && ObjectsSame(newer, older)) {
      header.flags |= OBJ_SAME;
      return;
    }
  }

  header.objID = newer.valid ? newer.objectNum : older.objectNum;
  idZeroRunLengthCompressor rle;
  if (!newer.valid) {
    header.flags |= OBJ_DELETED;
    return;
  }
  if (!older.valid) {
    rle.Start(destination, nullptr, ObjDestinationSize(newer.size));
    rle.WriteBytes(newer.data, newer.size);
    header.csize = rle.End();
    header.flags |= OBJ_NEW;
    if (header.csize == -1) {
      std::memmove(destination, newer.data, newer.size);
    }
    return;
  }

  header.flags |= OBJ_DIFFERENT;
  if (visibilityChanged) {
    header.flags |= sendState ? OBJ_VIS_NOT_STALE : OBJ_VIS_STALE;
  }
  if (visibilityChanged && !sendState) {
    return;
  }
  const int compareSize = std::min<int>(newer.size, older.size);
  rle.Start(destination, nullptr, ObjDestinationSize(newer.size));
  for (int i = 0; i < compareSize; ++i) {
    rle.WriteByte(static_cast<std::uint8_t>(newer.data[i] - older.data[i]));
  }
  if (newer.size > compareSize) {
    rle.WriteBytes(newer.data + compareSize, newer.size - compareSize);
  }
  header.csize = rle.End();
  if (header.csize == -1) {
    for (int i = 0; i < compareSize; ++i) {
      *destination++ = static_cast<std::uint8_t>(newer.data[i] - older.data[i]);
    }
    if (newer.size > compareSize) {
      std::memmove(destination, newer.data + compareSize, newer.size - compareSize);
    }
  }
}

void NewLZWStream(lzwParm_t* parameters, idLZWCompressor* compressor) {
  compressor->Start(parameters->ioData->lzwMem + parameters->ioData->lzwBytes,
                    parameters->ioData->maxlzwMem - parameters->ioData->lzwBytes);
  parameters->ioData->lastObjId = 0;
  ++parameters->ioData->snapSequence;
  compressor->WriteAgnostic(parameters->ioData->snapSequence);
  compressor->WriteAgnostic(parameters->baseSequence);
  compressor->WriteAgnostic(parameters->curTime);
}

void LZWJobInternal(lzwParm_t* parameters, unsigned int /*dmaTag*/) {
  if (parameters->ioData->numlzwDeltas > 0) {
    return;
  }
  idLZWCompressor compressor(parameters->ioData->lzwData);
  if (parameters->fragmented) {
    compressor.Start(parameters->ioData->lzwMem + parameters->ioData->lzwBytes,
                     parameters->ioData->maxlzwMem - parameters->ioData->lzwBytes, true);
  } else {
    NewLZWStream(parameters, &compressor);
  }

  int changedObjects = 0;
  for (int i = 0; i < parameters->numObjects; ++i) {
    if (compressor.IsOverflowed() ||
        compressor.Length() >= parameters->ioData->optimalLength) {
      FinishLZWStream(parameters, compressor);
      parameters->ioData->lzwDmaOut = parameters->ioData->lzwBytes;
      return;
    }
    if (changedObjects > 0) {
      compressor.Save();
    }
    const objHeader_t& header = parameters->headers[i];
    if (header.objID == -1) {
      continue;
    }
    ++changedObjects;
    const std::uint16_t objectDelta =
        static_cast<std::uint16_t>(header.objID - parameters->ioData->lastObjId);
    compressor.WriteAgnostic(objectDelta);
    parameters->ioData->lastObjId = static_cast<std::uint16_t>(header.objID);
    if ((header.flags & (OBJ_VIS_STALE | OBJ_VIS_NOT_STALE)) != 0) {
      const objectSize_t marker = (header.flags & OBJ_VIS_STALE) ? SIZE_STALE : SIZE_NOT_STALE;
      compressor.WriteAgnostic(marker);
    }
    if ((header.flags & OBJ_VIS_STALE) != 0) {
      continue;
    }
    if ((header.flags & OBJ_DELETED) != 0) {
      const objectSize_t deletedSize = 0;
      compressor.WriteAgnostic(deletedSize);
      continue;
    }
    const objectSize_t size = header.size;
    compressor.WriteAgnostic(size);
    if (header.csize == -1) {
      idZeroRunLengthCompressor rle;
      rle.Start(nullptr, &compressor, 0xFFFF);
      rle.WriteBytes(header.data, header.size);
      rle.End();
    } else {
      compressor.Write(header.data, header.csize);
    }
  }

  if (parameters->saveDictionary) {
    parameters->ioData->lzwDmaOut = parameters->ioData->lzwBytes + compressor.Length();
    return;
  }
  const std::uint16_t terminator =
      static_cast<std::uint16_t>(0xFFFFu - parameters->ioData->lastObjId);
  compressor.WriteAgnostic(terminator);
  FinishLZWStream(parameters, compressor);
  parameters->ioData->lzwDmaOut = parameters->ioData->lzwBytes;
  parameters->ioData->fullSnap = true;
}

void LZWJob(lzwParm_t* parameters) {
  LZWJobInternal(parameters, 0);
}
