#include "engine/network/serializerlog.h"

#include "shared/idlib/text/str.h"

idSerializerLog::idSerializerLog() : lastEntry(-1), lastObjID(-1), refCount(1) {}

idSerializerLog::~idSerializerLog() = default;

void idSerializerLog::AddRef() { ++refCount; }

void idSerializerLog::Release() {
  if (--refCount <= 0) delete this;
}

int idSerializerLog::Find(int parentObjectID, const char* typeName,
                          const char* /*instanceName*/) {
  for (int i = 0; i < logEntries.Num(); ++i) {
    if (logEntries[i].parentObjID == parentObjectID &&
        idStr::Icmp(logEntries[i].typeName.data, typeName) == 0) {
      return i;
    }
  }
  return -1;
}

void idSerializerLog::AddCompressionLog(int objectID, int compressedSize) {
  if (objectID > 0 && compressedSize > 0) {
    compressionEntry_t* entry = compressionEntries.Alloc();
    entry->objID = objectID;
    entry->csize = compressedSize;
  }
}
