#pragma once

#include "tungsten/game/ai/aaspositionmgr.h"
#include "tungsten/game/ai/aitypes.h"
#include "tungsten/game/ai/aivisibility.h"

#include <array>
#include <algorithm>

class idAI2;
class idAIWorldState;
class idEncounterGroup;
class idFaction;

enum aiSense_t : int {
  AISENSE_HEARING = 0,
  AISENSE_SIGHT = 1,
  AISENSE_MAX = 2,
  AISENSE_NONE = AISENSE_MAX
};
enum aiStimulus_t {
  AISTIM_NONE = 0,
  AISTIM_HEARING = 1,
  AISTIM_SOUND = AISTIM_HEARING,
  AISTIM_TOUCH = 2,
  AISTIM_DAMAGE = 3,
  AISTIM_GUNFIRE = 4,
  AISTIM_GROUP_SUSPECTED = 5,
  AISTIM_SHARED = AISTIM_GROUP_SUSPECTED,
  AISTIM_GROUP_CONFIRMED = 6,
  AISTIM_TRIGGER = 7,
  AISTIM_SIGHT = 8,
  AISTIM_DIRECT = AISTIM_SIGHT,
  AISTIM_SIGHT_COVER = 9,
  AISTIM_DIRECT_VISIBLE = AISTIM_SIGHT_COVER,
  AISTIM_MAX = 10
};
enum visibleType_t {
  VISIBLE_OUTSIDE_FOV = 0,
  VISIBLE_OUTSIDE_FOV_CLEAR = 1,
  VISIBLE_NOT_VISIBLE = 2,
  VISIBLE_IN_COVER = 3,
  VISIBLE_VISIBLE = 4,
  VISIBLE_INVALID = 5
};

template <int Minimum, int Maximum>
class idLeakyIntegrator {
public:
  bool AddStimulus(int curTimeMS, float stimulus);
  void Update(int curTimeMS) {
    if (lastUpdateMS >= 0 && curTimeMS > lastUpdateMS)
      value = std::max(static_cast<float>(Minimum),
                       value - decayRate * (curTimeMS - lastUpdateMS) * 0.001F);
    lastUpdateMS = curTimeMS;
  }

  float value{static_cast<float>(Minimum)};
  float decayRate{0.0F};
  int lastUpdateMS{-1};
  bool triggered{false};
};

struct aiPhysicalState_t {
  aiPhysicalState_t();
  idVec3 origin{};
  idVec3 velocity{};
  idQuat rotation{0.0F, 0.0F, 0.0F, 1.0F};
  idVec3 viewfwd{1.0F, 0.0F, 0.0F};
  idVec3 eyePos{};
  int crouchChangeDelta{0};
  std::uint8_t stimScale{0};
  std::uint8_t lightScale{255};
  std::uint16_t entFlags{0};
  idBounds bounds{};
};

struct aiLatchedState_t {
  aiLatchedState_t();
  aiPhysicalState_t physical{};
  idFaction* faction{nullptr};
  idSpawnId spawnId{};
  idSpawnId masterId{};
  int frameNum{-1};
  int groupHandle{-1};
  std::uint8_t horzFOV{0};
  std::uint8_t vertFOV{0};
  std::uint8_t entityType{8};
};

struct aiLatchedEvent_t {
  aiLatchedEvent_t();
  int eventTime{-1};
  int eventId{-1};
  idVec3 origin{};
  idSpawnId creatorId{};
  idSpawnId relevantId{};
  std::uint8_t priority{0};
  std::uint8_t flags{0};
  int eventClass{0};
};

struct aiSpringVec3_t {
  float k{1.0F};
  float m{1.0F};
  float c{2.0F};
  idVec3 p0{};
  idVec3 p1{};
  idVec3 velocity{};
  float maxSpeed{0.0F};
  float maxLength{0.0F};
};

class aiSenseState_t {
public:
  aiSenseState_t();
  aiSenseState_t(const aiSenseState_t& other);
  virtual ~aiSenseState_t() = default;
  virtual void InitSpring(const aiLatchedState_t*, const class idAILogic*) {}
  virtual void Update(const aiLatchedState_t*) {}
  virtual void UpdateSpring(const trackingParms_t*) {}
  virtual const aiSpringVec3_t* GetSpring() const { return nullptr; }
  virtual bool HasSpring() const { return false; }

  const aiPhysicalState_t* GetConfirmedPhysicalSafe() const;
  int GetConfirmedStimTimeSafe() const;
  void ExtrapolatePosition(float seconds, idVec3* futurePos) const;
  void SetUnconfirmedPhysical(const aiPhysicalState_t* physical);
  void SetConfirmedPhysical(const aiPhysicalState_t* physical);

  aiPhysicalState_t unConfirmedPhysical{};
  int lastUnconfirmedStimTime{-1};
  aiPhysicalState_t confirmedPhysical{};
  int lastConfirmedStimTime{-1};
  idLeakyIntegrator<0, 1> integrator{};
  idVec3 aiOrigin{};
  aiSense_t senseType{AISENSE_NONE};
  aiStimulus_t stimulusType{AISTIM_NONE};
  std::uint8_t senseFlags{0};
};

class idAISenseState_Hearing final : public aiSenseState_t {
public:
  idAISenseState_Hearing() { senseType = AISENSE_HEARING; }
};

class idAISenseState_Sight final : public aiSenseState_t {
public:
  idAISenseState_Sight() { senseType = AISENSE_SIGHT; }
  void InitSpring(const aiLatchedState_t* state, const idAILogic*) override;
  void InitSpring(const aiLatchedState_t* state) { InitSpring(state, nullptr); }
  void Update(const aiLatchedState_t* state) override;
  void UpdateSpring(const trackingParms_t* parms) override;
  const aiSpringVec3_t* GetSpring() const override { return &confirmedOriginSpring; }
  bool HasSpring() const override { return springInitialized; }

  aiSpringVec3_t confirmedOriginSpring{};
  bool springInitialized{false};
};

class idAILogic {
public:
  idAILogic(const idSpawnId& spawnId, const idSpawnId& curEnemyId,
            idEncounterGroup* encounterGroup, idFaction* faction,
            float combatTimeoutInterval, float searchTimeoutInterval,
            bool isRelaxed, bool isSearching);
  explicit idAILogic(const idSpawnId& spawnId);
  explicit idAILogic(idAI2* ai);
  const aiLatchedState_t* GetState() const;
  void SetGameState(const idAIGameState* value) { stateSource = value; }

  idSpawnId spawnId{};
  idSpawnId curEnemyId{};
  idEncounterGroup* encounterGroup{nullptr};
  idFaction* faction{nullptr};
  float combatTimeoutInterval{-1.0F};
  float searchTimeoutInterval{-1.0F};
  bool isRelaxed{true};
  bool isSearching{false};
  bool slowIdleSpot{false};
  bool senseUpdatesOnNonEnemies{false};
  bool pausePerception{false};
  const idAIGameState* stateSource{nullptr};
};

class idAIEntityState {
public:
  struct aiDefVisInfo_t {
    idClipQuery eyeQuery{};
    idClipQuery boundsQuery{};
    idClipQuery lastSeenPosQuery{};
    float fovWeight{0.0F};
    idVec3 delta{};
    bool closeEnough{false};
    bool eyeVisible{true};
    bool boundsVisible{true};
    bool queryReady{true};
    int entityNum{-1};
  };

  idAIEntityState();
  virtual ~idAIEntityState();
  idEntity* GetEntity() const;
  void Init(idAIGameState* gameState, const idAILogic* owner,
            const idSpawnId& entId, const aiLatchedState_t* state = nullptr);
  void UpdateSprings(const trackingParms_t* parms);
  static bool WithinFOV(const idAILogic* owner, const aiLatchedState_t* target,
                        float* fovWeight);
  static void AICanSee_Submit(const idAILogic* owner,
                             const aiLatchedState_t* target,
                             bool seeThroughBreakables,
                             aiDefVisInfo_t* info);
  static visibleType_t AICanSee_Resolve(const idAILogic* owner,
                                       aiDefVisInfo_t* info,
                                       const aiLatchedState_t* target);
  bool IsDead() const;
  bool IsDying() const;
  void DecreaseAwareness(const idAILogic* owner, int newAwareness);
  void IncreaseAwareness(const idAILogic* owner, int newAwareness);
  void SetSpot(int curTime, const idVec3& aimDirection);
  void Refresh_Submit(idAIGameState* gameState,
                      const idAIWorldState* worldState,
                      const idAILogic* owner, aiDefVisInfo_t* info);
  void Refresh_Resolve(idAIGameState* gameState, idAIWorldState* worldState,
                       const idAILogic* owner, aiDefVisInfo_t* info);
  void GetAimPoint(aiSense_t sense, aimPoint_t aimPoint, idVec3* point) const;
  bool IsVisible() const;
  void TestSurprise(const idAIGameState* gameState, const idAILogic* owner,
                    const aiLatchedState_t* state);
  bool UpdateSense(idAIGameState* gameState, const idAILogic* owner,
                   const idSpawnId& entId, const idSpawnId& sourceEntId,
                   int sourceEventId, aiSenseState_t* sense,
                   const aiLatchedState_t* state);
  void SetSenseState(idAIGameState* gameState, const idAILogic* owner,
                     aiSense_t sense, const aiPhysicalState_t* physical,
                     aiStimulus_t stimulusType, const idSpawnId& sourceId);
  void UpdateSeen(idAIGameState* gameState, const idAILogic* owner);
  void AddStimulus(idAIGameState* gameState, const idAILogic* owner,
                   aiStimulus_t stimulusType, float amount,
                   const idSpawnId& sourceEntityId, int sourceEventId,
                   const aiLatchedState_t* state = nullptr);

  idEntity* entity{nullptr};
  bool visible{true};
  bool hasFocus{true};
  bool aimAligned{true};
  bool muzzleInSolid{false};
  bool robotPowerAvailable{true};
  int confirmedStimTime{0};
  bool dead{false};
  bool dying{false};
  idSpawnId spawnId{};
  int initTime{-1};
  int curAwareness{0};
  int highestAwareness{0};
  int targetAttitude{1};
  std::uint8_t targetHealthRatio{255};
  int firstSpot{-3600000};
  int spotDuration{0};
  int spotLostDuration{-1};
  int currentTime{0};
  visibleType_t lastVisible{VISIBLE_OUTSIDE_FOV};
  int visChangeTime{-1};
  aiSense_t lastSense{AISENSE_NONE};
  float lastVisualStimulus{0.0F};
  idVec3 aimDir{0.0F, 0.0F, -1.0F};
  std::uint8_t entityType{8};
  bool active{false};
  bool awarenessChanged{false};
  idAASPosition aasPosition{};
  idAISenseState_Hearing hearingSense{};
  idAISenseState_Sight sightSense{};
  std::array<aiSenseState_t*, 2> senses{{&hearingSense, &sightSense}};
};

extern idCVar ai_useOriginSprings;
extern idCVar ai_originSpringExtrapolate;
extern idCVar ai_debugOriginSprings;
extern idCVar ai_debugFOV;
extern idCVar debugDeferredWorldRefresh;
