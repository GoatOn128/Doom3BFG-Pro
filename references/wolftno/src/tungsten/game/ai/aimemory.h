#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <array>

enum aiVarOp_t { AIVAR_SET, AIVAR_ADD, AIVAR_SUBTRACT };
enum aiMelee_t : int {
  AIMELEE_CLOSE = 0,
  AIMELEE_MEDIUM = 0x80,
  AIMELEE_LONG = 0x100,
  AIMELEE_MOVING_FORWARD = 0x2000,
};

enum aiItemSelect_t : int {
  AIITEMSELECT_NONE = 0,
  AIITEMSELECT_GRENADE = 1,
  AIITEMSELECT_DUAL_MUTANT = 2,
  AIITEMSELECT_CLUB_MUTANT = 3,
  AIITEMSELECT_SCOOP_MUTANT = 4,
  AIITEMSELECT_LARGE_RANGED_MUTANT = 5,
  AIITEMSELECT_HEAVY_BANDIT = 6,
  AIITEMSELECT_PIPE_BANDIT = 7,
  AIITEMSELECT_IMP = 8,
  AIITEMSELECT_MAX = 9,
};

struct idDeclAIBehavior {
  struct dodging_t {
    float sidestepDistance{0.0F};
    float diveDistance{0.0F};
  } dodging;
  std::array<aiItemSelect_t, 2> itemSelect{};
  float muzzleTraceClearOffset{0.0F};
  struct damageBehaviors_t {
    float movingPainSpeed{0.0F};
    float movingDeathSpeed{0.0F};
    float slowMovingDeathSpeed{0.0F};
    unsigned int longPainDamageFilter{0};
    float longPainHealthLimit{0.0F};
    float injuredHealthLimit{0.0F};
    int injuredChance{0};
    float injuredBleedOutTime{0.0F};
    float injuredStandUpTime{0.0F};
    std::array<float, 5> difficultyDamageScale{};
    bool canTakeDamage{false};
    aiInjuredRunMode_t injuredRunMode{AIINJUREDRUNMODE_MOSTDAMAGED};
  } damageBehaviors;
  struct movementBehaviors_t {
    float crouchMoveMax{0.0F};
    float injuredCrouchMoveMax{0.0F};
    float runMoveMin{0.0F};
    float sprintMoveMin{0.0F};
    int coverCrouchPathTime{0};
    int coverRunPathTime{0};
    int coverSprintPathTime{0};
    int fireRunningToCoverChance{0};
    float cornerCircleRadius{0.0F};
    float alignToPointDistance{0.0F};
    bool runsIfShotAt{false};
    bool crouchWhenStill{false};
    bool allowTransitionOvershoot{false};
    bool useOldAlignToPoint{false};
    bool runFacingForward{false};
    bool strafeMoveToEntity{false};
    bool enemyCloseObscurityTest{false};
    bool allowEnemySightedCrouchIdle{false};
    bool obstacleAvoidIgnorePlayer{false};
    bool aasPush{false};
    bool allowElevators{false};
  } movementBehaviors;
  struct runTypeBehaviors_t {
    int scrambleRunIndex{0};
    int farRunIndex{0};
    int gunDownIndex{0};
    int narrowIndex{0};
    int upStairsIndex{0};
    int downStairsIndex{0};
    int duckIndex{0};
  } runTypeBehaviors;
  struct idleBehaviors_t {
    int gunDownIndex{0};
    int narrowIndex{0};
    int duckIndex{0};
  } idleBehaviors;
  int minAimDodgeInterval{0};
  int maxAimDodgeInterval{0};
  bool disablePainAfterActionScript{false};
  int minCloseMeleeInterval{0};
  int maxCloseMeleeInterval{0};
  int minMediumMeleeInterval{0};
  int maxMediumMeleeInterval{0};
  int minLongMeleeInterval{0};
  int maxLongMeleeInterval{0};
  bool meleeEnabled{false};
  float closeMeleeDist{0.0F};
  float mediumMeleeDist{0.0F};
  float longMeleeDist{0.0F};
  float movingMeleeDist{0.0F};
  float meleeHeight{0.0F};
  float meleeExtrapolate{0.0F};
  float aimOffset{0.0F};
  bool canGreetHumans{false};
  bool canGreetPlayers{false};
  float grappleMaxDistance{0.0F};
  float grappleHeight{0.0F};
  std::uint8_t traversalFlagsByte0{1};
  std::uint8_t traversalFlagsByte1{0};
  int traversalAimEvasionDuration{0};
  int traversalGunfireEvasionDuration{0};
  float traversalEvasionAimDist{400.0F};
  float traversalEvasionGunfireDist{1024.0F};
};

class idInfoPath : public idEntity {
public:
  void SetNextPathPoint(idInfoPath* value) { nextPathPoint = value; }
  idInfoPath* GetNextPathPoint() const { return nextPathPoint; }
private:
  idInfoPath* nextPathPoint{nullptr};
};

class idScenePoint_UserGroup : public idEntity {};

class idVolume_Restriction : public idEntity {
public:
  idVolume_Restriction() {
    bounds[0] = {-100000.0F, -100000.0F, -100000.0F};
    bounds[1] = {100000.0F, 100000.0F, 100000.0F};
  }
  void SetActive(bool value) { active = value; }
  bool IsActive() const { return active; }
  void SetBounds(const idBounds& value) { bounds = value; }
  bool ContainsPoint(const idVec3& point) const {
    return point.x >= bounds[0].x && point.x <= bounds[1].x &&
           point.y >= bounds[0].y && point.y <= bounds[1].y &&
           point.z >= bounds[0].z && point.z <= bounds[1].z;
  }
private:
  bool active{true};
  idBounds bounds{};
};

class idVolume_ScenePoints : public idEntity {
public:
  void AddScenePoint(idScenePoint* value) {
    if (value != nullptr) scenePoints.Append(value);
  }
  void GetScenePoints(idList<idEntityPtr<idScenePoint>, 5>* output,
                      scenePointType_t type,
                      idVolume_Restriction* restriction,
                      idScenePoint_UserGroup* userGroup) const;
private:
  idList<idScenePoint*, 5> scenePoints;
};

class idVolume_LocationVoice : public idEntity {
public:
  bool CheckActivation(idAI2* ai, int currentTime,
                       const idVec3* suspectedPosition) {
    (void)ai;
    (void)suspectedPosition;
    if (!enabled || currentTime < nextActivationTime) return false;
    ++activationCount;
    nextActivationTime = reuseDelay < 0.0F
                             ? currentTime
                             : currentTime + static_cast<int>(reuseDelay * 1000.0F);
    return true;
  }
  void SetReuseDelay(float value) { reuseDelay = value; }
  int GetActivationCount() const { return activationCount; }
  int nextActivationTime{0};
  float reuseDelay{0.0F};
  bool enabled{true};
private:
  int activationCount{0};
};

class idAIMemory {
public:
  enum aiMemoryAnimAfterOverride : int {
    AIMEMORY_ANIMAFTER_OVERRIDE_DEFAULT = 0,
    AIMEMORY_ANIMAFTER_OVERRIDE_IDLE = 1,
    AIMEMORY_ANIMAFTER_OVERRIDE_RUN = 2,
    AIMEMORY_ANIMAFTER_OVERRIDE_CURRENT = 3,
  };

  struct idScenePointList {
    void Clear();
    idList<idEntityPtr<idScenePoint>, 5> scenePointList;
    bool hasSearchedForScenePoints{false};
  };

  idAIMemory();
  ~idAIMemory();

  const idAIPos* DebugGetEnemyTrailPos(int index) const;
  bool CheckSoundAlarm(int currentTime);
  void SetAIVarExpression(const char* name, aiVarOp_t operation,
                          const char* value);
  float GetScriptedAggro(int currentTime) const;
  void SetNextMeleeTime(aiMelee_t meleeType, int currentTime);
  int GetNextMeleeTime(aiMelee_t meleeType) const;
  void ClearLastHeldScenePoint();
  idScenePoint* GetLastHeldScenePoint() const;
  void ActivateCurScenePoint(idAI2* user, int currentTime);
  void SetCurLookScenePoint(idAI2* user, int currentTime,
                            idScenePoint* scenePoint);
  void ActivateCurLookScenePoint(idAI2* user, int currentTime);
  void SetTakeScenePointTimeStamp(int currentTime);
  void SetActivateScenePointTimeStamp(int currentTime);
  void ClearScenePointTimeStamps();
  void FirstThink(const idDeclAIBehavior* behavior);
  void ClearOldestTrailPos();
  void ClearAllTrailPositions(int currentTime);
  void CheckVoiceVolumes(idAI2* ai, int currentTime,
                         const idVec3* suspectedPosition);
  void ReportInjured();
  void TriggerPulled();
  bool HasOverrideScriptScenePointOfType(scenePointType_t type) const;
  bool HasOverrideScriptScenePointNotOfType(scenePointType_t type) const;
  void UpdateScenePointHistory(idScenePoint* scenePoint);
  float GetScenePointHistoryScore(idScenePoint* scenePoint) const;
  void UpdateLookScenePointHistory(idScenePoint* scenePoint);
  bool IsLastHeldLookScenePoint(idScenePoint* scenePoint) const;
  float GetLookScenePointHistoryScore(idScenePoint* scenePoint) const;
  void SetOverrideScenePoint(idScenePoint* scenePoint);
  void AddEnemyTrailPos(int currentTime, const idVec3& enemyPosition);
  bool GetOldestTrailPos(idAIPos* trail) const;
  void SetLastDamage(idEntity* attacker, idEntity* inflictor);
  void SetCurrentPath(const idInfoPath* path);
  void ClearScenePointLists();
  void SetRestrictionVolume(idVolume_Restriction* volume);
  bool HasRestrictionVolume();
  void SetRestrictionPos(const idVec3& position);
  void RemoveScenePointVolume(idVolume_ScenePoints* volume);
  void SetCurScenePoint(idAI2* user, int currentTime,
                        idScenePoint* scenePoint);
  void SetScenePointUserGroup(idScenePoint_UserGroup* group);
  bool CheckIfDirtyScenePoint(idScenePoint* dirtyScenePoint);
  bool CheckIfDirtyLookScenePoint(idScenePoint* dirtyLookScenePoint);
  void GetScenePoints(idList<idEntityPtr<idScenePoint>, 5>* output,
                      scenePointType_t type);
  void AddScenePointVolume(idVolume_ScenePoints* volume);
  idEntity* GetActionEntity() const;
  void SetActionEntity(idEntity* value) { actionEntity = value; }

  void SetScriptedAggro(const idEntity* entity, float aggression, int endTime);
  void SetProjectileInfo(const idAIProjectileInfo* info);
  const idEntity* GetEnemy() const;
  const idEntity* GetFriendly() const;
  const idEntity* GetFriendlyBlocker() const;
  const idEntity* GetCurrentNeutral() const { return currentNeutral; }
  idEntity* GetLastAttacker() const;
  bool ShouldAvoid(int currentTime);
  void SetAvoidEntity(int currentTime, const idEntity* entity, float radius,
                      float durationSeconds);
  void ClearAvoid() {
    avoidEntity = nullptr;
    avoidRadius = 0.0F;
    avoidFinishedTime = -1;
  }
  void SetSpottedEnemyTrigger(const idEntity* entity);
  const idEntity* GetSpottedEnemyTrigger() const;
  void SetDetectedEnemyTrigger(const idEntity* entity);
  const idEntity* GetDetectedEnemyTrigger() const;
  void SetTriggeredInteraction(idPlayer* entity);
  void SetInteractionUsableForTest(bool value) { interactionIsUsable = value; }
  bool IsInteractionUsable() const { return interactionIsUsable; }

  void SetCurrentTime(int value) { currentTime = value; }
  int GetSpawnTimeForTest() const { return spawnTime; }
  int GetNextDodgeTimeForTest() const { return nextDodgeTime; }
  idScenePoint* GetOverrideScenePoint() const {
    return overrideScenePoint;
  }
  void SetEnemy(idEntity* value) { currentEnemy = value; }
  void SetFriendly(idEntity* value) { currentFriendly = value; }
  void SetFriendlyBlocker(idEntity* value) { friendlyBlocker = value; }
  void SetTurretBurstState(int nextBurst, int endBurst, int lastFired) {
    nextBurstTime = nextBurst;
    endBurstTime = endBurst;
    lastFiredAtTime = lastFired;
  }
  int GetNextBurstTimeForTest() const { return nextBurstTime; }
  int GetEndBurstTimeForTest() const { return endBurstTime; }
  int GetLastFiredAtTimeForTest() const { return lastFiredAtTime; }
  void SetLastFiredAtTime(int value) { lastFiredAtTime = value; }
  void SetTraversalThreatTimesForTest(int lastAimedAt, int lastFiredAt,
                                      int lastAvoid) {
    lastAimedAtTime = lastAimedAt;
    lastFiredAtTime = lastFiredAt;
    lastAvoidTime = lastAvoid;
  }
  int GetLastAimedAtTimeForTest() const { return lastAimedAtTime; }
  void SetLastAimedAtTimeForTest(int value) { lastAimedAtTime = value; }
  int GetLastAvoidTimeForTest() const { return lastAvoidTime; }
  void SetCurrentNeutral(idEntity* value) { currentNeutral = value; }
  void SetLastAttacker(idEntity* value) { lastAttacker = value; }
  void SetSoundAlarmTime(int value) { timeToSoundAlarm = value; }
  int GetSoundAlarmTimeForTest() const { return timeToSoundAlarm; }
  void SetEnemyTrailUpdatePeriod(int value) { enemyTrailUpdatePeriod = value; }
  int GetEnemyTrailUpdatePeriodForTest() const {
    return enemyTrailUpdatePeriod;
  }
  void AddVoiceVolume(idVolume_LocationVoice* value) {
    voiceVolumes.Append(value);
  }
  void SetCurScenePointDirty(bool value) { curScenePointDirty = value; }
  void SetCurLookScenePointDirty(bool value) {
    curLookScenePointDirty = value;
  }
  const idStr& GetAIVarName() const { return varName; }
  const idStr& GetAIVarValue() const { return varValue; }
  aiVarOp_t GetAIVarOperation() const { return varOp; }
  int GetInjuredTime() const { return injuredTime; }
  int GetLastDamageTime() const { return lastDamageTime; }
  idEntity* GetLastAttackerEntity() const { return lastAttacker; }
  idEntity* GetLastInflictorEntity() const { return lastInflictor; }
  const idInfoPath* GetCurrentPath() const;
  const idInfoPath* GetNextPath() const { return nextPath; }
  const idAIPos& GetRestrictionPos() const { return restrictionPos; }
  int GetCurScenePointTakeTime() const { return curScenePointTakeTime; }
  int GetCurScenePointActivateTime() const {
    return curScenePointActivateTime;
  }
  int GetBlockedShots() const { return numShotsThisPullBlocked; }
  void SetBlockedShots(int total, int consecutive) {
    numShotsThisPullBlocked = total;
    numShotsThisPullBlockedConsecutive = consecutive;
  }
  const idAIPos& GetLastSafeEnemyPos() const { return lastSafeEnemyPos; }
  const idAIPos& GetCurrentEnemySuspectedPos() const {
    return currentEnemySuspectedPos;
  }
  void SetCurrentEnemySuspectedPos(const idVec3& value, bool valid) {
    currentEnemySuspectedPos.pos = value;
    currentEnemySuspectedPos.valid = valid;
  }
  void SetLastSafeEnemyPos(const idVec3& value) {
    lastSafeEnemyPos.pos = value;
    lastSafeEnemyPos.valid = true;
  }
  void SetLastSafeEnemyPosValidForTest(bool value) {
    lastSafeEnemyPos.valid = value;
  }
  void SetEnemyPosInsideAASForTest(bool value) {
    enemyPosInsideAAS = value;
  }
  bool IsEnemyPosInsideAASForTest() const { return enemyPosInsideAAS; }
  void SetBoundsReachableAreaNumCallTimeForTest(int value) {
    boundsReachableAreaNumCallTime = value;
  }
  int GetBoundsReachableAreaNumCallTimeForTest() const {
    return boundsReachableAreaNumCallTime;
  }
  void SetCurrentFriendlyBlockerTimeForTest(int value) {
    currentFriendlyBlockerTime = value;
  }
  int GetCurrentFriendlyBlockerTimeForTest() const {
    return currentFriendlyBlockerTime;
  }
  void SetCurrentFriendlyLOSBlockerForTest(bool value) {
    currentFriendlyLOSBlocker = value;
  }
  bool HasCurrentFriendlyLOSBlockerForTest() const {
    return currentFriendlyLOSBlocker;
  }
  void SetLastSafePositionForTest(const idVec3& value, bool valid) {
    lastSafePos.pos = value;
    lastSafePos.valid = valid;
  }
  const idAIPos& GetLastSafePositionForTest() const { return lastSafePos; }
  void SetCurrentEnemyDirectionForTest(const idVec3& value) {
    currentEnemyDir = value;
  }
  const idVec3& GetCurrentEnemyDirectionForTest() const {
    return currentEnemyDir;
  }
  void SetCurrentEnemyAimDirectionForTest(const idVec3& value) {
    currentEnemyAimDir = value;
  }
  const idVec3& GetCurrentEnemyAimDirectionForTest() const {
    return currentEnemyAimDir;
  }
  void SetCurrentEnemyRangeForTest(float value) {
    currentEnemyRange = value;
  }
  float GetCurrentEnemyRangeForTest() const { return currentEnemyRange; }
  void SetFriendlyLOSBlockerDirectionForTest(const idVec3& value) {
    currentFriendlyLOSBlockerDir = value;
  }
  const idVec3& GetFriendlyLOSBlockerDirectionForTest() const {
    return currentFriendlyLOSBlockerDir;
  }
  void SetFriendlyFireCosAngleOffForTest(float value) {
    friendlyFireCosAngleOff = value;
  }
  float GetFriendlyFireCosAngleOffForTest() const {
    return friendlyFireCosAngleOff;
  }
  void SetPlayerViewDataForTest(float dot, float range, int spottedTime) {
    playerDot = dot;
    playerRange = range;
    playerSpottedTime = spottedTime;
  }
  float GetPlayerDotForTest() const { return playerDot; }
  float GetPlayerRangeForTest() const { return playerRange; }
  int GetPlayerSpottedTimeForTest() const { return playerSpottedTime; }
  void SetEventInstigatorForTest(idEntity* value) {
    eventInstigator = value;
  }
  idEntity* GetEventInstigatorForTest() const { return eventInstigator; }
  void SetLastIdleTalkTimeForTest(int value) { lastIdleTalkTime = value; }
  int GetLastIdleTalkTimeForTest() const { return lastIdleTalkTime; }
  void SetLookScenePointAllowancesForTest(bool combat, bool search,
                                          bool roam) {
    allowCombatLookSPs = combat;
    allowSearchLookSPs = search;
    allowRoamLookSPs = roam;
  }
  bool GetAllowCombatLookScenePointsForTest() const {
    return allowCombatLookSPs;
  }
  bool GetAllowSearchLookScenePointsForTest() const {
    return allowSearchLookSPs;
  }
  bool GetAllowRoamLookScenePointsForTest() const {
    return allowRoamLookSPs;
  }
  void SetEnemyTrailPosForTest(int index, const idVec3& value, bool valid) {
    if (index < 0 || index >= static_cast<int>(enemyTrailPositions.size())) {
      return;
    }
    const int wrapped = (index + enemyTrailFinger) % 5;
    enemyTrailPositions[wrapped] = {value, valid};
  }
  int GetEnemyTrailFingerForTest() const { return enemyTrailFinger; }
  int GetNextEnemyTrailTimeForTest() const { return nextEnemyTrailTime; }
  idScenePoint* GetCurScenePointForTest() const { return curScenePoint; }
  idScenePoint* GetCurLookScenePointForTest() const {
    return curLookScenePoint;
  }
  void SetEmergencyCoverExit(bool value) { emergencyCoverExit = value; }
  bool HasEmergencyCoverExit() const { return emergencyCoverExit; }
  void SetAnimAfterOverride(aiMemoryAnimAfterOverride value) {
    animAfterOverride = value;
  }
  aiMemoryAnimAfterOverride GetAnimAfterOverride() const {
    return animAfterOverride;
  }
  void SetGangstaPoseEnabled(bool value) { enableGangstaPose = value; }
  bool IsGangstaPoseEnabled() const { return enableGangstaPose; }
  const idEntity* GetAvoidEntityForTest() const { return avoidEntity; }
  float GetAvoidRadiusForTest() const { return avoidRadius; }
  int GetAvoidFinishedTimeForTest() const { return avoidFinishedTime; }
  void SetAvoidPoint(const idVec3& value) { avoidPoint = value; }
  const idVec3& GetAvoidPointForTest() const { return avoidPoint; }
  idPlayer* GetTriggeredInteractionForTest() const;

  const idEntity* scriptedAggroEntity{nullptr};
  float scriptedAggression{0.0F};
  int scriptedAggroEndTime{0};
  idAIProjectileInfo projectileInfo;

private:
  idScenePointList& ListForType(scenePointType_t type);
  const idScenePointList& ListForType(scenePointType_t type) const;
  static bool Contains(const idList<idEntityPtr<idScenePoint>, 5>& list,
                       idScenePoint* point);

  std::array<idAIPos, 5> enemyTrailPositions{};
  int enemyTrailFinger{0};
  int enemyTrailUpdatePeriod{500};
  int nextEnemyTrailTime{0};
  int timeToSoundAlarm{-1};
  idStr varName;
  aiVarOp_t varOp{AIVAR_SET};
  idStr varValue;
  int nextCloseMeleeTime{0};
  int nextMediumMeleeTime{0};
  int nextLongMeleeTime{0};
  std::array<idScenePoint*, 5> scenePointHistory{};
  std::array<idScenePoint*, 5> lookScenePointHistory{};
  idScenePoint* curScenePoint{nullptr};
  idScenePoint* curLookScenePoint{nullptr};
  bool curScenePointActivated{false};
  bool curLookScenePointActivated{false};
  int curScenePointTakeTime{-1};
  int curScenePointActivateTime{-1};
  int spawnTime{0};
  int nextDodgeTime{0};
  idList<idVolume_LocationVoice*, 5> voiceVolumes;
  int nextVoiceVolumeCheckTime{0};
  int injuredTime{-1};
  int numShotsThisPullBlocked{0};
  int numShotsThisPullBlockedConsecutive{0};
  idScenePoint* overrideScenePoint{nullptr};
  idScenePoint* keptOverrideScenePoint{nullptr};
  idEntity* lastAttacker{nullptr};
  idEntity* lastInflictor{nullptr};
  idEntity* currentEnemy{nullptr};
  idEntity* currentFriendly{nullptr};
  idEntity* friendlyBlocker{nullptr};
  int nextBurstTime{0};
  int endBurstTime{0};
  int lastFiredAtTime{-1};
  int lastAimedAtTime{-1};
  int lastAvoidTime{-1};
  idEntity* currentNeutral{nullptr};
  int lastDamageTime{-1};
  const idInfoPath* currentPath{nullptr};
  const idInfoPath* nextPath{nullptr};
  idScenePointList combatScenePoints;
  idScenePointList searchScenePoints;
  idScenePointList roamScenePoints;
  idScenePointList lookScenePoints;
  idVolume_Restriction* restrictionVolume{nullptr};
  bool restrictionVolumeActive{false};
  idAIPos restrictionPos{};
  idList<idVolume_ScenePoints*, 5> scenePointVolumes;
  idScenePoint_UserGroup* scenePointUserGroup{nullptr};
  bool curScenePointDirty{false};
  bool curLookScenePointDirty{false};
  int currentTime{0};
  idEntity* actionEntity{nullptr};
  idAIPos lastSafeEnemyPos{};
  bool emergencyCoverExit{false};
  aiMemoryAnimAfterOverride animAfterOverride{
      AIMEMORY_ANIMAFTER_OVERRIDE_DEFAULT};
  bool enableGangstaPose{false};
  const idEntity* avoidEntity{nullptr};
  float avoidRadius{0.0F};
  int avoidFinishedTime{0};
  idVec3 avoidPoint{};
  const idEntity* spottedEnemyTrigger{nullptr};
  int spottedEnemyTriggerNumber{-1};
  const idEntity* detectedEnemyTrigger{nullptr};
  int detectedEnemyTriggerNumber{-1};
  idPlayer* triggeredInteraction{nullptr};
  int triggeredInteractionNumber{-1};
  bool interactionIsUsable{false};
  bool enemyPosInsideAAS{false};
  int boundsReachableAreaNumCallTime{0};
  int currentFriendlyBlockerTime{-1};
  bool currentFriendlyLOSBlocker{false};
  idAIPos currentEnemySuspectedPos{};
  idAIPos lastSafePos{};
  idVec3 currentEnemyDir{0.0F, 0.0F, -1.0F};
  idVec3 currentEnemyAimDir{0.0F, 0.0F, -1.0F};
  float currentEnemyRange{-1.0F};
  idVec3 currentFriendlyLOSBlockerDir{0.0F, 0.0F, 1.0F};
  float friendlyFireCosAngleOff{0.96600002F};
  float playerDot{0.0F};
  float playerRange{16000.0F};
  int playerSpottedTime{0};
  idEntity* eventInstigator{nullptr};
  int lastIdleTalkTime{-1};
  bool allowCombatLookSPs{false};
  bool allowSearchLookSPs{false};
  bool allowRoamLookSPs{false};
};
