#include "engine/network/snapshot.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

idSnapShot::objectBuffer_t::~objectBuffer_t() { _Release(); }

void idSnapShot::objectBuffer_t::_Release() {
  if (data != nullptr) {
    if (data[size]-- == 1) {
      delete[] data;
    }
    data = nullptr;
    size = 0;
  }
}

void idSnapShot::objectBuffer_t::_AddRef() {
  if (data != nullptr) ++data[size];
}

idSnapShot::objectBuffer_t& idSnapShot::objectBuffer_t::operator=(const objectBuffer_t& other) {
  if (this != &other) {
    _Release();
    data = other.data;
    size = other.size;
    _AddRef();
  }
  return *this;
}

void idSnapShot::objectBuffer_t::Alloc(unsigned int requestedSize) {
  if (requestedSize >= static_cast<unsigned int>(SIZE_NOT_STALE)) {
    idLib::FatalError("s >= SIZE_NOT_STALE");
  }
  _Release();
  data = new std::uint8_t[static_cast<std::size_t>(requestedSize) + 1];
  size = requestedSize;
  data[size] = 1;
}

void idSnapShot::objectState_t::Print(const char* name) const {
  idLib::Printf("%25s: [sz %u] 0x", name, buffer.size);
  for (unsigned int i = 0; i < buffer.size; ++i) idLib::Printf("%02X", buffer.data[i]);
  idLib::Printf("\n");
}

idSnapShot::idSnapShot() {
  allocatedObjs.allowAllocs = true;
  allocatedObjs.clearAllocs = true;
}

idSnapShot::idSnapShot(const idSnapShot& other) : idSnapShot() { *this = other; }

idSnapShot::~idSnapShot() { Clear(); }

void idSnapShot::SetSerializerLog(idSerializerLog* log) {
  if (log != nullptr) log->AddRef();
  if (serializeLog != nullptr) serializeLog->Release();
  serializeLog = log;
}

int idSnapShot::BinarySearch(int objectNum) const {
  int lower = 0;
  int upper = objectStates.Num();
  while (lower < upper) {
    const int middle = (lower + upper) >> 1;
    if (objectStates[middle]->objectNum == objectNum) return middle;
    if (objectStates[middle]->objectNum < objectNum) lower = middle + 1;
    else upper = middle;
  }
  return lower;
}

int idSnapShot::FindObjectIndexByID(int objectNum) const {
  const int index = BinarySearch(objectNum);
  return index >= 0 && index < objectStates.Num() &&
                 objectStates[index]->objectNum == objectNum
             ? index
             : -1;
}

idSnapShot::objectState_t* idSnapShot::FindObjectByID(int objectNum) const {
  const int index = FindObjectIndexByID(objectNum);
  return index == -1 ? nullptr : objectStates[index];
}

idSnapShot::objectState_t* idSnapShot::FindOrCreateObjectByID(int objectNum) {
  const int index = BinarySearch(objectNum);
  if (index < objectStates.Num() && objectStates[index]->objectNum == objectNum)
    return objectStates[index];
  objectState_t* state = allocatedObjs.Alloc();
  state->objectNum = static_cast<std::uint16_t>(objectNum);
  objectStates.Insert(state, index);
  return state;
}

void idSnapShot::FreeObjectState(int index) {
  if (index >= 0 && index < objectStates.Num() && objectStates[index] != nullptr) {
    allocatedObjs.Free(objectStates[index]);
    objectStates[index] = nullptr;
  }
}

void idSnapShot::Clear() {
  time = 0;
  recvTime = 0;
  for (int i = 0; i < objectStates.Num(); ++i) FreeObjectState(i);
  objectStates.ClearFree();
  allocatedObjs.Shutdown();
  if (serializeLog != nullptr) {
    serializeLog->Release();
    serializeLog = nullptr;
  }
}

idSnapShot& idSnapShot::operator=(const idSnapShot& other) {
  if (this == &other) return *this;
  while (objectStates.Num() > other.objectStates.Num()) {
    FreeObjectState(objectStates.Num() - 1);
    objectStates.RemoveIndex(objectStates.Num() - 1);
  }
  for (int i = 0; i < other.objectStates.Num(); ++i) {
    objectState_t* destination = i < objectStates.Num() ? objectStates[i] : allocatedObjs.Alloc();
    if (i >= objectStates.Num()) objectStates.Append(destination);
    *destination = *other.objectStates[i];
  }
  time = other.time;
  recvTime = other.recvTime;
  SetSerializerLog(other.serializeLog);
  return *this;
}

int idSnapShot::ObjectChangedCountByIndex(int index) const {
  return index < 0 || index >= objectStates.Num() ? 0 : objectStates[index]->changedCount;
}

void idSnapShot::UpdateExpectedSeq(int sequence) {
  for (int i = 0; i < objectStates.Num(); ++i)
    if (objectStates[i]->expectedSequence == -2) objectStates[i]->expectedSequence = sequence;
}

int idSnapShot::GetObjectMsgByIndex(int index, idBitMsg& message, bool ignoreIfStale) const {
  if (index < 0 || index >= objectStates.Num()) return -1;
  const objectState_t* state = objectStates[index];
  if (ignoreIfStale && state->stale) return -1;
  message.InitRead(state->buffer.data, static_cast<int>(state->buffer.size));
  return state->objectNum;
}

void idSnapShot::PeekDeltaSequence(char* memory, int size, int& sequence, int& baseSequence) {
  lzwCompressionData_t state{};
  idLZWCompressor compressor(&state);
  compressor.Start(reinterpret_cast<std::uint8_t*>(memory), size);
  compressor.ReadAgnostic(sequence);
  compressor.ReadAgnostic(baseSequence);
}

idSnapShot::objectState_t* idSnapShot::S_AddObject(int objectNum, std::uint32_t visMask,
                                                   const char* bytes, unsigned int size) {
  objectState_t* state = FindOrCreateObjectByID(objectNum);
  state->visMask = visMask;
  if (state->buffer.size == size && state->buffer.data != nullptr &&
      state->buffer.NumRefs() == 1) {
    std::memmove(state->buffer.data, bytes, size);
  } else {
    objectBuffer_t buffer(size);
    std::memmove(buffer.data, bytes, size);
    state->buffer = buffer;
  }
  return state;
}

bool idSnapShot::CopyObject(const idSnapShot& older, int objectNum, bool forceStale) {
  const objectState_t* source = older.FindObjectByID(objectNum);
  if (source == nullptr) return false;
  objectState_t* destination = FindOrCreateObjectByID(objectNum);
  *destination = *source;
  if (forceStale) destination->visMask = 0;
  return true;
}

int idSnapShot::CompareObject(const idSnapShot* older, int objectNum, int start, int end,
                              int oldStart) {
  if (older == nullptr) return 0;
  objectState_t* current = FindOrCreateObjectByID(objectNum);
  const objectState_t* previous = older->FindObjectByID(objectNum);
  if (previous == nullptr) return end != 0 ? end - start : current->buffer.size;
  const int currentSize = static_cast<int>(current->buffer.size);
  const int oldSize = static_cast<int>(previous->buffer.size);
  int compareEnd = std::min(currentSize, oldSize - (oldStart - start));
  int differences = end == 0 && currentSize > oldSize ? currentSize - oldSize : 0;
  if (end != 0) compareEnd = std::min(compareEnd, end);
  for (int i = start, oldIndex = oldStart; i < compareEnd; ++i, ++oldIndex) {
    if (i >= 0 && i < currentSize && oldIndex >= 0 && oldIndex < oldSize)
      differences += current->buffer.data[i] != previous->buffer.data[oldIndex];
  }
  return differences;
}

void idSnapShot::ApplyToExistingState(int objectID, idBitMsg& message) {
  objectState_t* state = FindObjectByID(objectID);
  if (state == nullptr || !state->createdFromTemplate) return;
  const unsigned int count = std::min<unsigned int>(state->buffer.size, message.GetSize());
  for (unsigned int i = 0; i < count; ++i) state->buffer.data[i] += message.readData[i];
}

idSnapShot::objectState_t* idSnapShot::GetTemplateState(int objectNum,
                                                        idSnapShot* templates,
                                                        objectState_t* newState) {
  if (templates == nullptr) return nullptr;
  objectState_t* state = templates->FindObjectByID(objectNum);
  if (state == nullptr && newState != nullptr) {
    templates->CopyObject(*this, objectNum);
    state = templates->FindObjectByID(objectNum);
    if (state != nullptr) state->createdFromTemplate = true;
  }
  return state;
}

void idSnapShot::SubmitLZWJob(const submitDeltaJobsInfo_t& info, objParms_t*& base,
                              objParms_t*& current, lzwParm_t*& lzw, bool saveDictionary) {
  if (current == base) return;
  lzw->numObjects = static_cast<int>(current - base);
  lzw->headers = base->destHeader;
  lzw->curTime = time;
  lzw->baseTime = info.oldSnap != nullptr ? info.oldSnap->time : 0;
  lzw->baseSequence = info.baseSequence;
  lzw->saveDictionary = saveDictionary;
  lzw->fragmented = base != info.objParms;
  lzw->ioData = info.lzwInOutData;
  LZWJob(lzw);
  ++lzw;
  base = current;
}

void idSnapShot::SubmitWriteDeltaToJobs(const submitDeltaJobsInfo_t& info) {
  objParms_t* current = info.objParms;
  objParms_t* base = current;
  objHeader_t* header = info.headers;
  std::uint8_t* destination = info.objMemory;
  lzwParm_t* lzw = info.lzwParms;
  for (int i = 0; i < objectStates.Num() && current - info.objParms < info.maxObjParms &&
                  header - info.headers < info.maxHeaders; ++i) {
    objectState_t* newer = objectStates[i];
    objectState_t* older = info.oldSnap != nullptr ? info.oldSnap->FindObjectByID(newer->objectNum) : nullptr;
    current->visIndex = static_cast<std::uint8_t>(info.visIndex);
    current->newState = {1, newer->buffer.data, static_cast<std::uint16_t>(newer->buffer.size),
                         newer->objectNum, newer->visMask};
    if (older != nullptr)
      current->oldState = {1, older->buffer.data, static_cast<std::uint16_t>(older->buffer.size),
                           older->objectNum, older->visMask};
    current->destHeader = header;
    current->dest = destination;
    SnapshotObjectJob(current);
    destination += ObjDestinationSize(newer->buffer.size);
    ++current;
    ++header;
  }
  SubmitLZWJob(info, base, current, lzw, false);
}

bool idSnapShot::ReadDeltaForJob(char* memory, int size, int /*visIndex*/,
                                 idSnapShot* templates) {
  lzwCompressionData_t lzwState{};
  idLZWCompressor compressor(&lzwState);
  compressor.Start(reinterpret_cast<std::uint8_t*>(memory), size);
  int sequence = 0;
  int baseSequence = 0;
  compressor.ReadAgnostic(sequence);
  compressor.ReadAgnostic(baseSequence);
  compressor.ReadAgnostic(time);
  std::uint16_t lastObject = 0;
  while (!compressor.IsOverflowed()) {
    std::uint16_t objectDelta = 0;
    if (compressor.ReadAgnostic(objectDelta) != sizeof(objectDelta)) return false;
    if (objectDelta == static_cast<std::uint16_t>(0xFFFFu - lastObject)) break;
    const int objectID = lastObject + objectDelta;
    lastObject = static_cast<std::uint16_t>(objectID);
    objectSize_t objectSize = 0;
    compressor.ReadAgnostic(objectSize);
    objectState_t* state = FindObjectByID(objectID);
    if (objectSize == SIZE_STALE) {
      if (state != nullptr) state->stale = true;
      continue;
    }
    if (objectSize == SIZE_NOT_STALE) {
      if (state != nullptr) state->stale = false;
      compressor.ReadAgnostic(objectSize);
    }
    if (objectSize == 0) {
      const int index = FindObjectIndexByID(objectID);
      if (index >= 0) { FreeObjectState(index); objectStates.RemoveIndex(index); }
      continue;
    }
    objectBuffer_t decoded(static_cast<unsigned int>(objectSize));
    idZeroRunLengthCompressor rle;
    rle.Start(nullptr, &compressor, objectSize + 16);
    rle.ReadBytes(decoded.data, static_cast<unsigned int>(objectSize));
    if (state == nullptr) {
      state = FindOrCreateObjectByID(objectID);
      objectState_t* templateState = templates != nullptr ? templates->FindObjectByID(objectID) : nullptr;
      if (templateState != nullptr) {
        state->buffer = templateState->buffer;
        state->createdFromTemplate = true;
      }
    }
    if (state->buffer.size == decoded.size) {
      for (unsigned int i = 0; i < decoded.size; ++i) decoded.data[i] += state->buffer.data[i];
    }
    state->buffer = decoded;
    ++state->changedCount;
    state->expectedSequence = sequence;
  }
  return !compressor.IsOverflowed();
}
