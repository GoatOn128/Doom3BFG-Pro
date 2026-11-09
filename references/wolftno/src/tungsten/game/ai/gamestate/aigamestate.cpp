#include "tungsten/game/ai/gamestate/aigamestate.h"

#include <algorithm>
#include <unordered_map>

template <typename T, int BlockSize, int Tag>
void idBlockAlloc<T, BlockSize, Tag>::AllocNewBlock() {
  capacity += BlockSize;
  active.reserve(static_cast<std::size_t>(capacity));
}

template <typename T, int BlockSize, int Tag>
T* idBlockAlloc<T, BlockSize, Tag>::Alloc() {
  if (static_cast<int>(active.size()) >= capacity) AllocNewBlock();
  active.push_back(std::make_unique<T>());
  return active.back().get();
}

template <typename T, int BlockSize, int Tag>
void idBlockAlloc<T, BlockSize, Tag>::Shutdown() {
  active.clear();
  active.shrink_to_fit();
  capacity = 0;
}

template <typename T, int BlockSize, int Tag>
idBlockAlloc<T, BlockSize, Tag>::~idBlockAlloc() {
  Shutdown();
}

struct idAIGameState::storage_t {
  idBlockAlloc<aiLatchedState_t, 256, 63> stateAllocator;
  idBlockAlloc<aiLatchedEvent_t, 64, 63> eventAllocator;
  std::unordered_map<int, aiLatchedState_t*> states;
  std::vector<aiLatchedEvent_t*> events;
  std::vector<idSpawnId> forcedLatches;
};

// DIA: idBlockAlloc<aiLatchedState_t,256,63>::AllocNewBlock
template void idBlockAlloc<aiLatchedState_t, 256, 63>::AllocNewBlock();

// DIA: idBlockAlloc<aiLatchedEvent_t,64,63>::AllocNewBlock
template void idBlockAlloc<aiLatchedEvent_t, 64, 63>::AllocNewBlock();

// DIA: idAIGameState::GetEntityType
int idAIGameState::GetEntityType(const idEntity* entity) {
  if (entity == nullptr) return 8;
  const std::string definition = entity->GetName();
  if (definition.find("player") != std::string::npos) return 1;
  if (definition.find("projectile") != std::string::npos) return 3;
  if (definition.find("turret") != std::string::npos) return 5;
  if (definition.find("vehicle") != std::string::npos) return 2;
  if (definition.find("robot") != std::string::npos) return 7;
  if (definition.find("ai_") != std::string::npos) return 0;
  return 8;
}

// DIA: idAIGameState::GetState
const aiLatchedState_t* idAIGameState::GetState(const idSpawnId* spawnId) const {
  if (spawnId == nullptr || !spawnId->IsValid() || storage == nullptr)
    return nullptr;
  const auto found = storage->states.find(spawnId->value);
  return found != storage->states.end() ? found->second : nullptr;
}

// DIA: idAIGameState::GetStateForEntityNum
const aiLatchedState_t* idAIGameState::GetStateForEntityNum(
    int entityNumber) const {
  if (entityNumber == 0x1fff || storage == nullptr) return nullptr;
  for (const auto& entry : storage->states)
    if ((entry.first & 0x1fff) == entityNumber) return entry.second;
  return nullptr;
}

// DIA: idAIGameState::GetEvent
const aiLatchedEvent_t* idAIGameState::GetEvent(int eventId) const {
  if (eventId < 0 || storage == nullptr) return nullptr;
  for (const aiLatchedEvent_t* event : storage->events)
    if (event != nullptr && event->eventId == eventId) return event;
  return nullptr;
}

// DIA: idBlockAlloc<aiLatchedState_t,256,63>::Shutdown
template void idBlockAlloc<aiLatchedState_t, 256, 63>::Shutdown();

// DIA: idBlockAlloc<aiLatchedState_t,256,63>::Alloc
template aiLatchedState_t* idBlockAlloc<aiLatchedState_t, 256, 63>::Alloc();

// DIA: idBlockAlloc<aiLatchedEvent_t,64,63>::Shutdown
template void idBlockAlloc<aiLatchedEvent_t, 64, 63>::Shutdown();

// DIA: idAIGameState::Shutdown
void idAIGameState::Shutdown() {
  masterIds.clear();
  entities.clear();
  if (storage != nullptr) {
    storage->states.clear();
    storage->events.clear();
    storage->forcedLatches.clear();
    storage->stateAllocator.Shutdown();
    storage->eventAllocator.Shutdown();
  }
  gameTime = -1;
  gameFrame = -1;
}

// DIA: idBlockAlloc<aiLatchedState_t,256,63>::~idBlockAlloc<aiLatchedState_t,256,63>
template idBlockAlloc<aiLatchedState_t, 256, 63>::~idBlockAlloc();

// DIA: idBlockAlloc<aiLatchedEvent_t,64,63>::~idBlockAlloc<aiLatchedEvent_t,64,63>
template idBlockAlloc<aiLatchedEvent_t, 64, 63>::~idBlockAlloc();

// DIA: idAIGameState::idAIGameState
idAIGameState::idAIGameState() : storage(new storage_t) {}

// DIA: idAIGameState::~idAIGameState
idAIGameState::~idAIGameState() {
  Shutdown();
  delete storage;
  storage = nullptr;
}

// DIA: `idAIGameState::~idAIGameState'::`1'::dtor$0
// DIA: `idAIGameState::~idAIGameState'::`1'::dtor$1
// DIA: `idAIGameState::~idAIGameState'::`1'::dtor$2

// DIA: idAIGameState::GetPhysicalState
bool idAIGameState::GetPhysicalState(const idGameLocal*,
                                     const idEntity* entity,
                                     aiPhysicalState_t* physicalState) const {
  if (entity == nullptr || physicalState == nullptr) return false;
  const idPhysics* physics = entity->GetPhysics();
  if (physics == nullptr) return false;
  physicalState->origin = physics->GetOrigin();
  physicalState->velocity = entity->GetLinearVelocity();
  physicalState->bounds = physics->GetAbsBounds();
  physicalState->viewfwd = entity->GetPresentableAxis()->mat[0];
  physicalState->eyePos = physicalState->origin;
  physicalState->eyePos.z = physicalState->bounds[1].z;
  return true;
}

// DIA: idAIGameState::LatchEntityState
void idAIGameState::LatchEntityState(const idGameLocal* game,
                                     idEntity* entity, bool force) {
  if (entity == nullptr || storage == nullptr) return;
  int entityNumber = entity->GetEntityNumber();
  if (entityNumber < 0 || entityNumber >= 0x1fff) return;
  idSpawnId spawnId{entityNumber};
  const auto existing = storage->states.find(spawnId.value);
  aiLatchedState_t* state = existing != storage->states.end()
                                ? existing->second : nullptr;
  if (state != nullptr && state->frameNum == gameFrame && !force) return;
  if (state == nullptr) {
    state = storage->stateAllocator.Alloc();
    storage->states[spawnId.value] = state;
  }
  state->spawnId = spawnId;
  state->masterId = GetMaster(spawnId);
  state->frameNum = gameFrame;
  state->entityType = static_cast<std::uint8_t>(GetEntityType(entity));
  GetPhysicalState(game, entity, &state->physical);
  entities[spawnId.value] = entity;
}

// DIA: idAIGameState::UpdateFromGame
void idAIGameState::UpdateFromGame(const idGameLocal* game) {
  if (game == nullptr) return;
  gameTime = game->GetGameTimeMs();
  gameFrame = game->GetFrameNum();
  for (const auto& entry : entities)
    LatchEntityState(game, entry.second, false);
  UpdateForcedLatches(game);
}

// DIA: `idAIGameState::UpdateFromGame'::`1'::dtor$1

// DIA: idAIGameState::UpdateForcedLatches
void idAIGameState::UpdateForcedLatches(const idGameLocal* game) {
  if (storage == nullptr) return;
  for (const idSpawnId spawnId : storage->forcedLatches) {
    const auto found = entities.find(spawnId.value);
    if (found != entities.end()) LatchEntityState(game, found->second, true);
  }
  storage->forcedLatches.clear();
}

// DIA: idAIGameState::ForceLatch
void idAIGameState::ForceLatch(idSpawnId spawnId) {
  if (!spawnId.IsValid() || storage == nullptr) return;
  const auto duplicate = std::find_if(storage->forcedLatches.begin(),
      storage->forcedLatches.end(), [spawnId](idSpawnId existing) {
        return existing.value == spawnId.value;
      });
  if (duplicate == storage->forcedLatches.end())
    storage->forcedLatches.push_back(spawnId);
}

void idAIGameState::LatchEvent(const aiLatchedEvent_t& source) {
  if (storage == nullptr || storage->events.size() >= 256) return;
  aiLatchedEvent_t* event = storage->eventAllocator.Alloc();
  *event = source;
  storage->events.push_back(event);
}

// DIA: `dynamic initializer for 'ai_debugGameState''
// DIA: `dynamic atexit destructor for 'ai_debugGameState''
idCVar ai_debugGameState("ai_debugGameState", "0", 1,
                         "output AI game state debug information");
