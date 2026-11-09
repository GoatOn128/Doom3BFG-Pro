#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"
#include "tungsten/game/game_runtime.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

struct AFBodyPart_t {
  idVec3 origin{};
  idQuat orientation{0.0F, 0.0F, 0.0F, 1.0F};
};

class idCorpseFile {
public:
  template <typename T>
  void WriteValue(const T& value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&value);
    data.insert(data.end(), bytes, bytes + sizeof(T));
  }
  template <typename T>
  bool ReadValue(T& value) {
    if (cursor + sizeof(T) > data.size()) return false;
    std::memcpy(&value, data.data() + cursor, sizeof(T));
    cursor += sizeof(T);
    return true;
  }
  void WriteString(const idStr& value);
  bool ReadString(idStr& value);
  void Rewind() { cursor = 0; }
  const std::vector<std::uint8_t>& Data() const { return data; }

private:
  std::vector<std::uint8_t> data;
  std::size_t cursor{0};
};

class idCorpseManager {
public:
  struct corpseSave_t {
    corpseSave_t();
    idList<AFBodyPart_t, 5> bodies;
    idStr def;
    idStr subWeb;
    idStr curState;
    idQuat orientation{0.0F, 0.0F, 0.0F, 1.0F};
    idVec3 position{};
    int checkpointKilled{0};
    int deathAnimIndex{0};
    bool ragdoll{false};
  };

  struct deadBody_t {
    idEntity* entity{nullptr};
    corpseSave_t save;
    int deathTime{0};
    int removeTime{0};
    bool visible{false};
    bool looted{false};
  };

  class spawner_t {
  public:
    virtual ~spawner_t() = default;
    virtual idEntity* SpawnCorpse(const corpseSave_t& savedCorpse) = 0;
  };

  idCorpseManager();
  void SpawnCorpses();
  bool RemoveOldestNonVisibleDeadBody();
  void CheckDeadBodyRemoval();
  void Write(idCorpseFile* file, int savegameVersionMajor,
             std::int64_t savegameVersionMinor);
  void Clear();
  void Read(idCorpseFile* file, int savegameVersionMajor,
            std::int64_t savegameVersionMinor);

  void AddDeadBody(const deadBody_t& body) { deadAI.Append(body); }
  void AddCheckpointCorpse(const corpseSave_t& corpse) {
    checkpointAI.Append(corpse);
  }
  void SetSpawner(spawner_t* value) { spawner = value; }
  void SetCurrentTime(int value) { currentTime = value; }
  void SetMultiplayer(bool value) { multiplayer = value; }
  int GetDeadBodyCount() const { return deadAI.Num(); }
  int GetCheckpointCorpseCount() const { return checkpointAI.Num(); }
  int GetSpawnedCount() const { return spawnedCount; }
  int GetCurrentCheckpoint() const { return currentCheckpoint; }
  const corpseSave_t& GetCheckpointCorpse(int index) const {
    return checkpointAI[index];
  }

private:
  idList<corpseSave_t, 5> checkpointAI;
  idList<deadBody_t, 5> deadAI;
  spawner_t* spawner{nullptr};
  int currentTime{0};
  int currentCheckpoint{0};
  int spawnedCount{0};
  bool multiplayer{false};
};

extern idCVar g_skipCorpseManager;
extern idCVar g_maxNumDeadAIBodies;
