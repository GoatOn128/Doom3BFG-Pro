#pragma once

#include "engine/framework/game_network.h"

class idFile_SaveGame;
class idSaveGameDetails;

class idGameLocal : public idGame {
public:
  void GetSaveGameDetails(idSaveGameDetails& details,
                          idFile_SaveGame* saveFile);
};

extern idGameLocal* gameLocal;

static_assert(sizeof(idGameLocal) == 8, "game-local interface ABI drift");
