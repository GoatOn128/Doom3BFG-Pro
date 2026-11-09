#pragma once

#include "engine/framework/file_savegame.h"
#include "shared/idlib/dict.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idSaveGameProcessor;

class idSaveGameDetails {
public:
  idSaveGameDetails();

  idDict descriptors;
  bool damaged;
  std::byte paddingToDate[7];
  std::int64_t date;
  idStrStatic<64> slotName;
};

class idSaveGameManager {
public:
  int ExecuteProcessor(idSaveGameProcessor* processor);
};

static_assert(sizeof(idSaveGameDetails)==184,
              "save-game details ABI drift");
static_assert(offsetof(idSaveGameDetails,date)==72,
              "save-game details date offset drift");
static_assert(offsetof(idSaveGameDetails,slotName)==80,
              "save-game details slot offset drift");
