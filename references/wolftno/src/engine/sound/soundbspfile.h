#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/plane.h"

#include <cstdint>

struct sbspFace_t {
  std::int16_t center[3];
  std::uint16_t planeNum;
  std::uint16_t numVerts;
  std::uint16_t firstVert;
  std::int16_t faceAreas[2];
};

struct sbspArea_t {
  std::uint16_t numFaces;
  std::uint16_t firstFace;
  std::int16_t environment;
  std::int16_t door;
};

struct sbspNode_t {
  std::uint32_t planeNum;
  std::int16_t children[2];
};

struct alignas(8) flatSoundBSP_t {
  idPlane* planes;
  idVec3* vertices;
  int* vertIndex;
  sbspFace_t* faces;
  int* faceIndex;
  sbspArea_t* areas;
  sbspNode_t* nodes;
  int numPlanes;
  int numVertices;
  int numVertIndex;
  int numFaces;
  int numFaceIndex;
  int numAreas;
  int numNodes;
};

class idSoundBSPFile {
public:
  idSoundBSPFile();
  ~idSoundBSPFile();

  int PointAreaNum(const idVec3* point) const;
  void Clear();
  bool Read(const char* filename);

  idList<idPlane, 39> planes;
  idList<idVec3, 39> vertices;
  idList<int, 39> vertIndex;
  idList<sbspFace_t, 39> faces;
  idList<int, 39> faceIndex;
  idList<sbspArea_t, 39> areas;
  idList<sbspNode_t, 39> nodes;
  int numEnvironments{0};
  int numDoors{0};
  flatSoundBSP_t flatBSP{};

private:
  void UpdateFlatBSP();
};

static_assert(sizeof(sbspFace_t) == 16);
static_assert(sizeof(sbspArea_t) == 8);
static_assert(sizeof(sbspNode_t) == 8);
static_assert(sizeof(flatSoundBSP_t) == 88);
static_assert(sizeof(idSoundBSPFile) == 264);
