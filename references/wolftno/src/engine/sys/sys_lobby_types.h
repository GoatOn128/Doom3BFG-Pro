#pragma once

#include "shared/idlib/networking/bitmsg.h"
#include "shared/idlib/hashing/uuid.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/str.h"
#include "engine/sys/sys_localuser.h"

#include <cstdint>
#include <cstring>

class idCallback {
public:
  virtual ~idCallback() = default;
  virtual idCallback* Clone() const = 0;
  virtual void Call() = 0;
};

struct lobbyAddress_t {
  bool usingBackendPlatform{};
  std::uint64_t platformId{};
  netadr_t netAddr{};

  bool Compare(const lobbyAddress_t& other, bool ignoreSession = false) const;
  void InitFromNetadr(const netadr_t& address);
  const char* ToString() const;
};

struct lobbyConnectInfo_t {
  std::uint64_t platformId{};
  bool usingBackendPlatform{true};
  netadr_t netAddr{};
};

struct idMatchParameters {
  idMatchParameters();
  idMatchParameters(const idMatchParameters&);
  idMatchParameters(int gameMode,int searchMode,int gameMap,int gameType,
      int numSlots,std::uint8_t matchFlags,const idStr& mapName,
      const idStr& layersActive,const idStr& layersDeactive,
      const idStr& spawnSpot,const idStr& loadingScreen,
      float minRespawnDelay,float timeLimit,int scoreLimit,int teamCount,
      bool swapTeams,bool stopWatch,bool comboMap,bool devmap,
      bool transitional,bool forceHeapReset,int checkpointSlot,bool playVideo);
  ~idMatchParameters();
  idMatchParameters& operator=(const idMatchParameters&)=default;
  void SetMapName(const char* name);

  idUUID4 matchID;
  int gameMode{};
  int searchMode{};
  int gameMap{};
  int gameType{};
  int numSlots{};
  int startCheckSlot{};
  std::uint8_t matchFlags{};
  std::byte padding0[7]{};
  idStrStatic<128> mapName;
  idStrStatic<2048> layersActive;
  idStrStatic<2048> layersDeactive;
  idStrStatic<64> spawnSpot;
  idStrStatic<128> loadingScreen;
  int timeLimit{};
  int scoreLimit{};
  int numBots{};
  int botSkill{};
  float minRespawnDelay{};
  int teamCount{};
  bool swapTeams{};
  bool stopWatch{};
  bool comboMap{};
  bool devmap{};
  bool isTransitionalMapRequest{};
  bool forceHeapReset{};
  bool playVideo{};
  std::byte padding1[1]{};
  int nightmareCompletionStat{-1};
  idStaticList<int,4> completionStats;
  int collectionStat{-1};
  int specialCollectionStat{-1};
  int challengeLeaderboardId{-1};
  int skillLevel{};
  bool nextMapDiscSwap{};
  std::byte padding2[7]{};
};

static_assert(sizeof(idMatchParameters)==4896,"idMatchParameters ABI drift");

struct serverInfo_t {
  idStr name;
  int map{-1};
  int type{-1};
  int mode{-1};
  bool joinable{true};
  int numPlayers{-1};
  int maxPlayers{-1};

  void Read(idBitMsg& message);
  void Write(idBitMsg& message) const;
};

struct lobbyUserID_t {
  localUserHandle_t localUserHandle{};
  std::uint8_t lobbyType{};

  bool operator==(const lobbyUserID_t& other) const {
    return localUserHandle == other.localUserHandle && lobbyType == other.lobbyType;
  }
  void WriteToMsg(idBitMsg& message) const;
  void ReadFromMsg(idBitMsg& message);
};

struct lobbyUser_t {
  bool isBot{};
  bool isDemoPlayer{};
  bool isSpectator{};
  void* botData{};
  int peerIndex{-1};
  lobbyUserID_t lobbyUserID{};
  char gamertag[65]{};
  int pingMs{};
  bool disconnecting{};
  int emblemOrdinal{};
  int level{};
  int teamNumber{};
  unsigned partyToken{};
  bool arbitrationAcked{};
  lobbyAddress_t address{};
  int migrationGameData{};
  int voiceChatGroup{};
};
