#pragma once

#include "shared/idlib/text/str.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>

struct idMatchParameters;

class idGameSpawnInfo {
public:
  idGameSpawnInfo(const idGameSpawnInfo& other);
  idGameSpawnInfo(const char* mapName,int gameFlags);
  idGameSpawnInfo(const idMatchParameters* matchParms,int gameFlags,
                  const char* spawnSpotOverride,const char* checkpointName,
                  int numPlayers,int gameSpawnFlags);
  ~idGameSpawnInfo();
  void MakeNamesCanonical();
  void FixupFeatureLayers();
  void ReadLayersFromCommaSeparatedStrings(const char* activeString,
                                            const char* deactiveString);
  void WriteLayersToCommaSeparatedStrings(idStr& activeString,
                                           idStr& deactiveString) const;
  const idStrStatic<1024>& GetSpawnSpot() const;
  void SetSpawnSpot(const char* spawnSpot);
  void SetSpawnSpotOverride(const char* spawnSpot);

  int gameFlags;
  std::byte padding0[4];
  idStrStatic<1024> mapName;
  idStrStatic<1024> spawnSpot;
  idStrStatic<1024> spawnSpotOverride;
  idStrStatic<64> checkpointName;
  idStrStatic<1024> loadingScreen;
  idStaticList<idStrStatic<128>,256> activeLayers;
  idStaticList<idStrStatic<128>,256> deactiveLayers;
  std::uint8_t comboMap : 1;
  std::uint8_t devmap : 1;
  std::uint8_t forceHeapReset : 1;
  std::uint8_t reservedFlags : 5;
  std::byte padding1[3];
  int gameSpawnFlags;
  int numPlayers;
  int currentCheckpointSlot;
};

static_assert(sizeof(idGameSpawnInfo)==0x16150,"idGameSpawnInfo ABI drift");
static_assert(offsetof(idGameSpawnInfo,mapName)==0x8,"spawn map ABI drift");
static_assert(offsetof(idGameSpawnInfo,loadingScreen)==0xCE8,"spawn load screen ABI drift");
static_assert(offsetof(idGameSpawnInfo,activeLayers)==0x1110,"spawn active layers ABI drift");
static_assert(offsetof(idGameSpawnInfo,currentCheckpointSlot)==0x1614C,
              "spawn checkpoint slot ABI drift");
