#include "engine/aas2file/aas2file.h"

#include <algorithm>

const std::vector<idAAS2DebugAreaModel*>& idAAS2DebugAreaModel::GetResourceList() {
  static std::vector<idAAS2DebugAreaModel*> resources;
  return resources;
}

bool idAAS2DebugAreaModel::LoadResource() {
  return !resourcePath.IsEmpty() && LoadResource(resourcePath.data);
}

bool idAAS2DebugAreaModel::LoadResource(const char* path) {
  if (!path) return false;
  resourcePath = path;
  idStr error;
  valid = data.LoadBinary(path, &error, &data.sourceTimestamp);
  if (valid) {
    auto& resources = const_cast<std::vector<idAAS2DebugAreaModel*>&>(GetResourceList());
    if (std::find(resources.begin(), resources.end(), this) == resources.end()) resources.push_back(this);
  }
  return valid;
}

bool idAAS2DebugAreaModel::ReloadIfStale() { return LoadResource(); }

bool idAAS2DebugAreaModel::WriteResourceFile() {
  return !resourcePath.IsEmpty() && idAAS2DebugAreaModelGenerator::WriteToBinary(
                                        data.sourceName.data, resourcePath.data,
                                        data.sourceTimestamp, &data);
}

bool idAAS2DebugAreaModel::IsValidForAASFile(const idAAS2File* file) const {
  return valid && file && file->valid &&
         (data.sourceTimestamp == 0 || file->timestamp == 0 || data.sourceTimestamp == file->timestamp);
}

