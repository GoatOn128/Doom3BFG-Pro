#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

class idSerializerLog {
public:
  struct logEntry_t {
    idStr typeName;
    idStr instanceName;
    int objID{};
    int parent{};
    int parentObjID{};
    int start{};
    int end{};
    int startWriteBit{};
    int endWriteBit{};
  };
  struct compressionEntry_t { int objID{}; int csize{}; };

  idSerializerLog();
  ~idSerializerLog();
  void AddRef();
  void Release();
  int Find(int parentObjectID, const char* typeName, const char* instanceName);
  void AddCompressionLog(int objectID, int compressedSize);

  idList<logEntry_t, 44> logEntries;
  idList<compressionEntry_t, 44> compressionEntries;
  int finalCompressedSize{};
  int finalUncompressedSize{};
  bool fragmented{};
  int lastEntry{};
  int lastObjID{};
  int refCount{};
};

static_assert(sizeof(idSerializerLog) == 72, "Wolf retail serializer log ABI drift");
