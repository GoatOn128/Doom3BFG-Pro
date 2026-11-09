#pragma once

#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idCmdArgs;

class idDiscSwapManager {
public:
  enum command_t : int {
    DSC_NONE=0,
    DSC_MAP=1,
    DSC_MULTIPLAYER_INVITE=2,
    DSC_LOAD=3
  };
  enum error_t : int { ERROR_NONE=0 };

  void Reset();
  error_t Swap(command_t command,int discNumber,bool relaunch);
  bool RebuildStateAfterSwap(command_t command,const idCmdArgs& args);

  idStrStatic<256> map;
  idStrStatic<64> savegameFile;
  idStrStatic<1000> additionalArguments;
  idStrStatic<500> layerData;
  idStrStatic<124> spawnSpot;
  void* user;
  bool isSwapping;
  std::byte padding[7];
  void* inviteInfo;
};

static_assert(sizeof(idDiscSwapManager)==2176,"idDiscSwapManager ABI drift");
static_assert(offsetof(idDiscSwapManager,layerData)==1440,
              "disc swap layer ABI drift");
static_assert(offsetof(idDiscSwapManager,inviteInfo)==2168,
              "disc swap invite ABI drift");

extern idDiscSwapManager* discSwapMgr;
