#include "engine/sound/soundbspfile.h"

#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <cstring>
#include <memory>
#include <string>

namespace {

constexpr std::uint32_t kSoundBspMagic = 0x534E4409U;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00FF0000U) |
         ((value >> 8) & 0x0000FF00U) | (value >> 24);
}

bool ReadU16(idFile& file, std::uint16_t& value) {
  if (file.Read(&value, sizeof(value)) != sizeof(value)) {
    return false;
  }
  value = ByteSwap16(value);
  return true;
}

bool ReadU32(idFile& file, std::uint32_t& value) {
  if (file.Read(&value, sizeof(value)) != sizeof(value)) {
    return false;
  }
  value = ByteSwap32(value);
  return true;
}

bool ReadS16(idFile& file, std::int16_t& value) {
  std::uint16_t bits = 0;
  if (!ReadU16(file, bits)) {
    return false;
  }
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

bool ReadS32(idFile& file, int& value) {
  std::uint32_t bits = 0;
  if (!ReadU32(file, bits)) {
    return false;
  }
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

bool ReadFloat(idFile& file, float& value) {
  std::uint32_t bits = 0;
  if (!ReadU32(file, bits)) {
    return false;
  }
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

template <typename T>
bool ReadCount(idFile& file, idList<T, 39>& list) {
  int count = 0;
  if (!ReadS32(file, count) || count < 0) {
    return false;
  }
  list.SetNum(count);
  return true;
}

std::unique_ptr<idFile> OpenSoundBsp(const char* filename) {
  std::unique_ptr<idFile> file(fileSystem->OpenFileRead(filename));
  if (file && file->Length() > 0) {
    return file;
  }

  std::string alternate = filename ? filename : "";
  const std::size_t extension = alternate.find_last_of('.');
  if (extension != std::string::npos) {
    alternate.erase(extension);
  }
  alternate += "/soundworld.sbsp";
  file.reset(fileSystem->OpenFileRead(alternate.c_str()));
  return file;
}

}  // namespace

int idSoundBSPFile::PointAreaNum(const idVec3* point) const {
  if (nodes.Num() == 1) {
    return 0;
  }

  int nodeNumber = 1;
  for (;;) {
    const sbspNode_t& node = nodes[nodeNumber];
    const idPlane& plane = planes[node.planeNum];
    const float distance = point->x * plane.a + point->y * plane.b +
                           point->z * plane.c + plane.d;
    nodeNumber = node.children[distance >= 0.0F ? 0 : 1];
    if (nodeNumber < 0) {
      return -nodeNumber;
    }
    if (nodeNumber == 0) {
      return 0;
    }
  }
}

void idSoundBSPFile::UpdateFlatBSP() {
  flatBSP.planes = planes.list;
  flatBSP.vertices = vertices.list;
  flatBSP.vertIndex = vertIndex.list;
  flatBSP.faces = faces.list;
  flatBSP.faceIndex = faceIndex.list;
  flatBSP.areas = areas.list;
  flatBSP.nodes = nodes.list;
  flatBSP.numPlanes = planes.Num();
  flatBSP.numVertices = vertices.Num();
  flatBSP.numVertIndex = vertIndex.Num();
  flatBSP.numFaces = faces.Num();
  flatBSP.numFaceIndex = faceIndex.Num();
  flatBSP.numAreas = areas.Num();
  flatBSP.numNodes = nodes.Num();
}

void idSoundBSPFile::Clear() {
  vertices.SetNum(1);
  vertices[0] = {};
  faces.SetNum(1);
  faces[0] = {};
  areas.SetNum(1);
  areas[0] = {};
  areas[0].door = -1;
  nodes.SetNum(1);
  nodes[0] = {};
  UpdateFlatBSP();
}

bool idSoundBSPFile::Read(const char* filename) {
  std::unique_ptr<idFile> file = OpenSoundBsp(filename);
  if (!file || file->Length() <= 0) {
    return false;
  }

  std::uint32_t magic = 0;
  if (!ReadU32(*file, magic) || magic != kSoundBspMagic ||
      !ReadCount(*file, planes)) {
    return false;
  }
  for (int index = 0; index < planes.Num(); ++index) {
    if (!ReadFloat(*file, planes[index].a) || !ReadFloat(*file, planes[index].b) ||
        !ReadFloat(*file, planes[index].c) || !ReadFloat(*file, planes[index].d)) {
      return false;
    }
  }

  if (!ReadCount(*file, vertices)) {
    return false;
  }
  for (int index = 0; index < vertices.Num(); ++index) {
    if (!ReadFloat(*file, vertices[index].x) || !ReadFloat(*file, vertices[index].y) ||
        !ReadFloat(*file, vertices[index].z)) {
      return false;
    }
  }

  if (!ReadCount(*file, vertIndex)) {
    return false;
  }
  for (int index = 0; index < vertIndex.Num(); ++index) {
    if (!ReadS32(*file, vertIndex[index])) {
      return false;
    }
  }

  if (!ReadCount(*file, faces)) {
    return false;
  }
  for (int index = 0; index < faces.Num(); ++index) {
    sbspFace_t& face = faces[index];
    if (!ReadU16(*file, face.planeNum) || !ReadU16(*file, face.numVerts) ||
        !ReadU16(*file, face.firstVert) || !ReadS16(*file, face.faceAreas[0]) ||
        !ReadS16(*file, face.faceAreas[1]) || !ReadS16(*file, face.center[0]) ||
        !ReadS16(*file, face.center[1]) || !ReadS16(*file, face.center[2])) {
      return false;
    }
  }

  if (!ReadCount(*file, faceIndex)) {
    return false;
  }
  for (int index = 0; index < faceIndex.Num(); ++index) {
    if (!ReadS32(*file, faceIndex[index])) {
      return false;
    }
  }

  if (!ReadCount(*file, areas)) {
    return false;
  }
  for (int index = 0; index < areas.Num(); ++index) {
    sbspArea_t& area = areas[index];
    if (!ReadU16(*file, area.numFaces) || !ReadU16(*file, area.firstFace) ||
        !ReadS16(*file, area.environment) || !ReadS16(*file, area.door)) {
      return false;
    }
  }

  if (!ReadCount(*file, nodes)) {
    return false;
  }
  for (int index = 0; index < nodes.Num(); ++index) {
    sbspNode_t& node = nodes[index];
    if (!ReadU32(*file, node.planeNum) || !ReadS16(*file, node.children[0]) ||
        !ReadS16(*file, node.children[1])) {
      return false;
    }
  }

  if (!ReadS32(*file, numEnvironments) || !ReadS32(*file, numDoors) ||
      !ReadU32(*file, magic) || magic != kSoundBspMagic) {
    return false;
  }

  UpdateFlatBSP();
  if (flatBSP.numAreas > 4096) {
    idLib::Error("RouteToGoal: areas > MAX_SBSP_AREAS! Please increase it.");
    return false;
  }
  return true;
}
