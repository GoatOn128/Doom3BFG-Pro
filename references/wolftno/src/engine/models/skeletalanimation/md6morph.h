#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

enum morphDataType_t : std::int32_t {
  MORPHDATA_NONE = 0,
  MORPHDATA_SPHERE = 1,
  MORPHDATA_BOX = 2,
  MORPHDATA_MORPH = 3,
  MORPHDATA_CLIPSPHERE = 4,
  MORPHDATA_CLIPELLIPSOID = 5
};

class idMD6MorphSet {
public:
  int meshIndex;
  int groupIndex;
  morphDataType_t type;
  idVec3 pos;
  idVec3 size;
  idMat3 axis;
  idList<std::uint16_t, 115> indices;
  idList<std::uint8_t, 115> direction;

  idMD6MorphSet()
      : meshIndex(-1), groupIndex(-1), type(MORPHDATA_NONE), pos{}, size{},
        axis(mat3_identity) {}
};

class idMD6Morph {
public:
  ~idMD6Morph();
  void Write_Binary(idFile* file) const;
  void Load_Binary(idFile* file, unsigned int magic);
  idMD6MorphSet& AllocSet();
  int NumSets() const { return sets.Num(); }
  const idMD6MorphSet& GetSet(int index) const { return sets[index]; }

  idList<idMD6MorphSet, 5> sets;
};

static_assert(sizeof(morphDataType_t) == 4, "morphDataType_t ABI drift");
static_assert(sizeof(idMD6MorphSet) == 120, "idMD6MorphSet ABI drift");
static_assert(offsetof(idMD6MorphSet, meshIndex) == 0,
              "idMD6MorphSet::meshIndex ABI drift");
static_assert(offsetof(idMD6MorphSet, groupIndex) == 4,
              "idMD6MorphSet::groupIndex ABI drift");
static_assert(offsetof(idMD6MorphSet, type) == 8,
              "idMD6MorphSet::type ABI drift");
static_assert(offsetof(idMD6MorphSet, pos) == 12,
              "idMD6MorphSet::pos ABI drift");
static_assert(offsetof(idMD6MorphSet, size) == 24,
              "idMD6MorphSet::size ABI drift");
static_assert(offsetof(idMD6MorphSet, axis) == 36,
              "idMD6MorphSet::axis ABI drift");
static_assert(offsetof(idMD6MorphSet, indices) == 72,
              "idMD6MorphSet::indices ABI drift");
static_assert(offsetof(idMD6MorphSet, direction) == 96,
              "idMD6MorphSet::direction ABI drift");
static_assert(sizeof(idMD6Morph) == 24, "idMD6Morph ABI drift");
