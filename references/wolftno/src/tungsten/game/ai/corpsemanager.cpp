#include "tungsten/game/ai/corpsemanager.h"

#include <algorithm>
#include <cstring>

idCVar g_skipCorpseManager(
    "g_skipCorpseManager", "0", 1,
    "If true, don't read/write or spawn corpses during checkpoint load/save.");
idCVar g_maxNumDeadAIBodies(
    "g_maxNumDeadAIBodies", "12", 2,
    "All bodies above this will get faded out and removed, if not visible by player then just remove.");

void idCorpseFile::WriteString(const idStr& value) {
  const int length = value.Length();
  WriteValue(length);
  data.insert(data.end(), value.data, value.data + length);
}

bool idCorpseFile::ReadString(idStr& value) {
  int length = 0;
  if (!ReadValue(length) || length < 0 || cursor + length > data.size()) {
    return false;
  }
  std::string text(reinterpret_cast<const char*>(data.data() + cursor),
                   static_cast<std::size_t>(length));
  cursor += static_cast<std::size_t>(length);
  value = text.c_str();
  return true;
}

// DIA: idCorpseManager::SpawnCorpses
void idCorpseManager::SpawnCorpses() {
  if (g_skipCorpseManager.GetInteger() != 0 || spawner == nullptr) return;
  for (int index = 0; index < checkpointAI.Num(); ++index) {
    if (!checkpointAI[index].def.IsEmpty() &&
        spawner->SpawnCorpse(checkpointAI[index]) != nullptr) {
      ++spawnedCount;
      currentCheckpoint =
          std::max(currentCheckpoint, checkpointAI[index].checkpointKilled + 1);
    }
  }
}

// DIA: idCorpseManager::RemoveOldestNonVisibleDeadBody
bool idCorpseManager::RemoveOldestNonVisibleDeadBody() {
  if (deadAI.Num() <= 0) return false;
  int selected = -1;
  int selectedScore = -1;
  for (int index = 0; index < deadAI.Num(); ++index) {
    const deadBody_t& body = deadAI[index];
    if (body.entity == nullptr || body.removeTime != 0) {
      selected = index;
      break;
    }
    const int age = std::max(currentTime - body.deathTime, 0);
    int score = age;
    if (!body.visible) score *= body.looted ? 1000 : 100;
    if (body.looted) score += 1000000;
    if (score > selectedScore) {
      selectedScore = score;
      selected = index;
    }
  }
  if (selected < 0) return false;
  if (deadAI[selected].entity != nullptr) deadAI[selected].entity->Remove();
  deadAI.RemoveIndexFast(selected);
  return true;
}

// DIA: idCorpseManager::corpseSave_t::corpseSave_t
idCorpseManager::corpseSave_t::corpseSave_t() = default;

// DIA: idCorpseManager::CheckDeadBodyRemoval
void idCorpseManager::CheckDeadBodyRemoval() {
  const int maximum = multiplayer ? 2 : g_maxNumDeadAIBodies.GetInteger();
  if (maximum < 0) return;
  for (int attempts = 0; deadAI.Num() > maximum && attempts < 5;
       ++attempts) {
    if (!RemoveOldestNonVisibleDeadBody()) break;
  }
}

// DIA: idList<AFBodyPart_t,5>::Resize
template bool idList<AFBodyPart_t, 5>::Resize(int newSize);

// DIA: idCorpseManager::Write
void idCorpseManager::Write(idCorpseFile* file, int savegameVersionMajor,
                            std::int64_t savegameVersionMinor) {
  (void)savegameVersionMajor;
  (void)savegameVersionMinor;
  if (file == nullptr) return;
  const int count = g_skipCorpseManager.GetInteger() != 0 ? 0 : deadAI.Num();
  file->WriteValue(count);
  for (int index = 0; index < count; ++index) {
    const corpseSave_t& corpse = deadAI[index].save;
    file->WriteString(corpse.def);
    file->WriteString(corpse.subWeb);
    file->WriteString(corpse.curState);
    file->WriteValue(corpse.orientation);
    file->WriteValue(corpse.position);
    file->WriteValue(corpse.checkpointKilled);
    file->WriteValue(corpse.deathAnimIndex);
    file->WriteValue(corpse.ragdoll);
    file->WriteValue(corpse.bodies.num);
    for (int body = 0; body < corpse.bodies.Num(); ++body) {
      file->WriteValue(corpse.bodies[body]);
    }
  }
}

// DIA: `idCorpseManager::Write'::`1'::dtor$0

// DIA: idList<idCorpseManager::corpseSave_t,5>::Resize
template bool idList<idCorpseManager::corpseSave_t, 5>::Resize(int newSize);

// DIA: idCorpseManager::Clear
void idCorpseManager::Clear() {
  checkpointAI.ClearFree();
  deadAI.ClearFree();
  spawnedCount = 0;
}

// DIA: idCorpseManager::idCorpseManager
idCorpseManager::idCorpseManager() = default;

// DIA: `idCorpseManager::idCorpseManager'::`1'::dtor$0
// DIA: `idCorpseManager::idCorpseManager'::`1'::dtor$1

// DIA: idCorpseManager::Read
void idCorpseManager::Read(idCorpseFile* file, int savegameVersionMajor,
                           std::int64_t savegameVersionMinor) {
  (void)savegameVersionMajor;
  (void)savegameVersionMinor;
  checkpointAI.ClearFree();
  if (file == nullptr) return;
  int count = 0;
  if (!file->ReadValue(count) || count < 0) return;
  for (int index = 0; index < count; ++index) {
    corpseSave_t corpse;
    if (!file->ReadString(corpse.def) || !file->ReadString(corpse.subWeb) ||
        !file->ReadString(corpse.curState) ||
        !file->ReadValue(corpse.orientation) ||
        !file->ReadValue(corpse.position) ||
        !file->ReadValue(corpse.checkpointKilled) ||
        !file->ReadValue(corpse.deathAnimIndex) ||
        !file->ReadValue(corpse.ragdoll)) {
      checkpointAI.Clear();
      return;
    }
    int bodyCount = 0;
    if (!file->ReadValue(bodyCount) || bodyCount < 0) {
      checkpointAI.Clear();
      return;
    }
    corpse.bodies.SetNum(bodyCount);
    for (int body = 0; body < bodyCount; ++body) {
      if (!file->ReadValue(corpse.bodies[body])) {
        checkpointAI.Clear();
        return;
      }
    }
    checkpointAI.Append(corpse);
  }
}

// DIA: `dynamic initializer for 'g_skipCorpseManager''
// DIA: `dynamic initializer for 'g_maxNumDeadAIBodies''
// DIA: `dynamic atexit destructor for 'g_skipCorpseManager''
// DIA: `dynamic atexit destructor for 'g_maxNumDeadAIBodies''
