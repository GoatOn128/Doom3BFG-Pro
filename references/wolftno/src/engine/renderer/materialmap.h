#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/str.h"

struct idVirtualImageFile {
  idAtomicString fileName;
  unsigned timeStamp{};

  bool NeedsReload();
};

struct idMaterialMapping {
  idAtomicString materialName;
  unsigned materialChecksum{};
  unsigned materialTimestamp{};
  int x{};
  int y{};
  int width{};
  int height{};
  idVec4 scaleBias{};
  idVirtualImageFile specularFile;
  idVirtualImageFile coverFile;
  idVirtualImageFile diffuseFile;
  idVirtualImageFile bumpFile;
  idVirtualImageFile powerFile;
};

class idMaterialMap {
public:
  idMaterialMap();
  bool GetImageDimensions(int& imageWidth, int& imageHeight) const;

  idStr name;
  int width{};
  int height{};
  unsigned skinFileTimeStamp{};
  idList<idMaterialMapping, 46> mappings;
};

static_assert(sizeof(idVirtualImageFile) == 16, "idVirtualImageFile ABI drift");
static_assert(sizeof(idMaterialMapping) == 128, "idMaterialMapping ABI drift");
static_assert(offsetof(idMaterialMap, width) == 40, "idMaterialMap width offset drift");
static_assert(sizeof(idMaterialMap) == 80, "idMaterialMap ABI drift");
