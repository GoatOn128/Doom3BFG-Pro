#include "engine/aas2file/aas2file.h"

#include <array>
#include <fstream>

namespace {
constexpr std::array<char, 8> kDebugMagic{'W', 'T', 'N', 'D', 'A', 'A', 'S', '\0'};
template <typename T> void WriteValue(std::ostream& stream, const T& value) {
  stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
}

bool idAAS2DebugAreaModelGenerator::WriteToBinary(
    const char* uniqueIdName, const char* fileName, unsigned sourceTimestamp,
    const idAAS2DebugAreaModelData* data) {
  if (!fileName || !data) return false;
  std::ofstream stream(fileName, std::ios::binary | std::ios::trunc);
  if (!stream) return false;
  stream.write(kDebugMagic.data(), kDebugMagic.size());
  WriteValue(stream, sourceTimestamp);
  const idStr source(uniqueIdName ? uniqueIdName : data->sourceName.data);
  const std::uint32_t nameLength = static_cast<std::uint32_t>(source.Length());
  WriteValue(stream, nameLength);
  if (nameLength) stream.write(source.data, nameLength);
  const std::uint32_t vertexCount = static_cast<std::uint32_t>(data->vertices.size());
  WriteValue(stream, vertexCount);
  if (vertexCount) stream.write(reinterpret_cast<const char*>(data->vertices.data()), sizeof(idVec3) * vertexCount);
  const std::uint32_t areaCount = static_cast<std::uint32_t>(data->areas.size());
  WriteValue(stream, areaCount);
  if (areaCount) stream.write(reinterpret_cast<const char*>(data->areas.data()), sizeof(idAAS2DebugAreaModelData::debugAreaInfo_t) * areaCount);
  return static_cast<bool>(stream);
}

