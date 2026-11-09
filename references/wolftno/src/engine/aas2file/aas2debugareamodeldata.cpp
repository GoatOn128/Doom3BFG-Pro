#include "engine/aas2file/aas2file.h"

#include <array>
#include <fstream>
#include <string>

namespace {
constexpr std::array<char, 8> kDebugMagic{'W', 'T', 'N', 'D', 'A', 'A', 'S', '\0'};
template <typename T> bool ReadValue(std::istream& stream, T& value) {
  return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
}
}

idAAS2DebugAreaModelData::idAAS2DebugAreaModelData() = default;
idAAS2DebugAreaModelData::~idAAS2DebugAreaModelData() { Free(); }

void idAAS2DebugAreaModelData::BuildResourceNameFromAASName(const char* aasName,
                                                            idStr* resourceName) {
  if (!resourceName) return;
  std::string value = aasName ? aasName : "unnamed";
  const std::size_t extension = value.find_last_of('.');
  if (extension != std::string::npos) value.resize(extension);
  value += ".debugareas";
  *resourceName = value.c_str();
}

void idAAS2DebugAreaModelData::Draw(const std::vector<int>* visibleAreas) const {
  // Rendering is owned by the renderer module. Validate and touch only the requested ranges here.
  if (!visibleAreas) {
    for (const auto& area : areas) {
      const volatile int count = std::max(0, area.vertexCount);
      (void)count;
    }
    return;
  }
  for (int areaNum : *visibleAreas) {
    if (areaNum >= 0 && static_cast<std::size_t>(areaNum) < areas.size()) {
      const volatile int count = std::max(0, areas[areaNum].vertexCount);
      (void)count;
    }
  }
}

void idAAS2DebugAreaModelData::Free() {
  sourceName.Clear();
  sourceTimestamp = 0;
  vertices.clear();
  areas.clear();
}

bool idAAS2DebugAreaModelData::LoadBinary(const char* binaryFileName, idStr* errorMsg,
                                          unsigned* outputTimestamp) {
  if (!binaryFileName) {
    if (errorMsg) *errorMsg = "missing debug-area filename";
    return false;
  }
  std::ifstream stream(binaryFileName, std::ios::binary);
  std::array<char, 8> magic{};
  std::uint32_t nameLength{}, vertexCount{}, areaCount{};
  if (!stream.read(magic.data(), magic.size()) || magic != kDebugMagic ||
      !ReadValue(stream, sourceTimestamp) || !ReadValue(stream, nameLength) ||
      nameLength > 1'048'576u) {
    if (errorMsg) *errorMsg = "invalid debug-area header";
    return false;
  }
  std::string loadedName(nameLength, '\0');
  if ((nameLength && !stream.read(loadedName.data(), nameLength)) ||
      !ReadValue(stream, vertexCount) || vertexCount > 16'000'000u) {
    if (errorMsg) *errorMsg = "invalid debug-area name or vertex count";
    return false;
  }
  vertices.resize(vertexCount);
  if ((vertexCount && !stream.read(reinterpret_cast<char*>(vertices.data()), sizeof(idVec3) * vertexCount)) ||
      !ReadValue(stream, areaCount) || areaCount > 16'000'000u) {
    if (errorMsg) *errorMsg = "truncated debug-area vertices";
    return false;
  }
  areas.resize(areaCount);
  if (areaCount && !stream.read(reinterpret_cast<char*>(areas.data()), sizeof(debugAreaInfo_t) * areaCount)) {
    if (errorMsg) *errorMsg = "truncated debug-area records";
    return false;
  }
  sourceName = loadedName.c_str();
  if (outputTimestamp) *outputTimestamp = sourceTimestamp;
  if (errorMsg) errorMsg->Clear();
  return true;
}

