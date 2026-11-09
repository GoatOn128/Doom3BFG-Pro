#include "tungsten/game/ai/encountergroups.h"
#include "tungsten/game/ai/targetting/targetlist.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

idList<idEncounterDebugPrimitive, 5> debugPrimitives;

int GameTime() {
  return gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
}

idVec3 EntityOrigin(const idEntity* entity) {
  if (entity == nullptr || entity->GetPhysics() == nullptr) return {};
  return entity->GetPhysics()->GetOrigin();
}

float HealthRatio(float health, float maxHealth) {
  return maxHealth > 0.0F ? std::clamp(health / maxHealth, 0.0F, 1.0F) : 0.0F;
}

float NormalizeSafe(idVec3& value) {
  const float length = value.Length();
  if (length > 0.0F) value *= 1.0F / length;
  return length;
}

int NextRandomIndex(int& seed, int count) {
  if (count <= 0) return -1;
  seed = seed * 1103515245 + 12345;
  return static_cast<int>((static_cast<unsigned int>(seed) >> 16U) %
                          static_cast<unsigned int>(count));
}

}  // namespace

idCVar ai_debugGrenadeHints("ai_debugGrenadeHints", "0", 0,
                            "draw encounter grenade hints");
idCVar ai_debugGoodWill("ai_debugGoodWill", "0", 0,
                        "draw encounter role goodwill decisions");

// DIA: idEncounterGroup::SetAnyMemberHasSeenDeadBody
void idEncounterGroup::SetAnyMemberHasSeenDeadBody(bool seen) {
  anyMemberHasSeenDeadBody = seen;
}

// DIA: idEncounterGroup::GetAnyMemberHasSeenDeadBody
bool idEncounterGroup::GetAnyMemberHasSeenDeadBody() const {
  return anyMemberHasSeenDeadBody;
}

// DIA: idEncounterGroup::CanAddFirer
bool idEncounterGroup::CanAddFirer() const {
  return maxSimultaneousFirers < 0 ||
         numSimultaneousFiring < maxSimultaneousFirers;
}

// DIA: idEncounterGroup::AddFirer
void idEncounterGroup::AddFirer() {
  if (CanAddFirer()) ++numSimultaneousFiring;
}

// DIA: idEncounterGroup::RemoveFirer
void idEncounterGroup::RemoveFirer() {
  if (numSimultaneousFiring > 0) --numSimultaneousFiring;
}

// DIA: idCombatStage::GetNodesListByHintType
const idList<int, 5>* idCombatStage::GetNodesListByHintType(
    idAICombatHint::combatHintType_t hintType) const {
  switch (hintType) {
    case idAICombatHint::COMBAT_HINT_DEFENSE_POINT: return &defenseNodes;
    case idAICombatHint::COMBAT_HINT_ASSAULT_POINT: return &assaultNodes;
    case idAICombatHint::COMBAT_HINT_FLANK_POINT: return &flankNodes;
    case idAICombatHint::COMBAT_HINT_FLANK_TARGET_POINT: return &flankTargetNodes;
    case idAICombatHint::COMBAT_HINT_FLANK_PATH_POINT: return &flankPathNodes;
    case idAICombatHint::COMBAT_HINT_GRENADE_POINT: return &grenadeNodes;
    case idAICombatHint::COMBAT_HINT_RECHARGE_POINT: return &rechargeNodes;
    case idAICombatHint::COMBAT_HINT_SUPPRESSION_FIRE_POINT:
      return &suppressionFireNodes;
    default: return nullptr;
  }
}

// DIA: idList<idStr,5>::RemoveIndex
template bool idList<idStr, 5>::RemoveIndex(int index);

// DIA: idEncounterGroup::NumLivingMembers
int idEncounterGroup::NumLivingMembers() const {
  int count = numLivingMembers;
  for (int i = 0; i < childGroups.Num(); ++i) {
    if (childGroups[i] != nullptr) count += childGroups[i]->NumLivingMembers();
  }
  return count;
}

// DIA: idEncounterGroup::NumDeadMembers
int idEncounterGroup::NumDeadMembers() const {
  int count = numDeadMembers;
  for (int i = 0; i < childGroups.Num(); ++i) {
    if (childGroups[i] != nullptr) count += childGroups[i]->NumDeadMembers();
  }
  return count;
}

// DIA: idEncounterGroup::GetNumTargets
int idEncounterGroup::GetNumTargets() const { return targetList.Num(); }

// DIA: idEncounterGroup::LowestPlayerHealth
float idEncounterGroup::LowestPlayerHealth() const {
  float result = 1.0F;
  bool found = false;
  for (int i = 0; i < targetList.Num(); ++i) {
    if (targetList[i].player == nullptr) continue;
    result = std::min(result, HealthRatio(targetList[i].health,
                                          targetList[i].maxHealth));
    found = true;
  }
  return found ? result : 0.0F;
}

// DIA: idEncounterGroup::HighestPlayerHealth
float idEncounterGroup::HighestPlayerHealth() const {
  float result = 0.0F;
  for (int i = 0; i < targetList.Num(); ++i) {
    if (targetList[i].player != nullptr)
      result = std::max(result, HealthRatio(targetList[i].health,
                                            targetList[i].maxHealth));
  }
  return result;
}

// DIA: idEncounterGroup::AnyMemberHighestAlertCycle
int idEncounterGroup::AnyMemberHighestAlertCycle() const {
  ++highestAlertCycleQueryCount;
  return anyMemberHighestAlertCycle;
}

// DIA: idEncounterGroup::SetGroupEnemyUpdateTimeStamp
void idEncounterGroup::SetGroupEnemyUpdateTimeStamp(const idEntity* enemy,
                                                     int time,
                                                     const idVec3* enemyPosition) {
  groupEnemy = enemy;
  groupEnemyUpdateTimeStamp = time;
  (void)enemyPosition;
}

// DIA: idEncounterGroup::IsRecentlyGroupSpotted
bool idEncounterGroup::IsRecentlyGroupSpotted(const idEntity* enemy,
                                               float seconds) const {
  if (enemy == nullptr || enemy != groupEnemy || groupEnemyUpdateTimeStamp < 0)
    return false;
  return GameTime() - groupEnemyUpdateTimeStamp <=
         static_cast<int>(std::max(0.0F, seconds) * 1000.0F);
}

// DIA: idEncounterGroup::GetPlayerWithLowestHealth
const idPlayer* idEncounterGroup::GetPlayerWithLowestHealth() const {
  const target_t* best = nullptr;
  for (int i = 0; i < targetList.Num(); ++i) {
    if (targetList[i].player == nullptr) continue;
    if (best == nullptr || HealthRatio(targetList[i].health, targetList[i].maxHealth) <
                               HealthRatio(best->health, best->maxHealth))
      best = &targetList[i];
  }
  return best != nullptr ? best->player : nullptr;
}

// DIA: idEncounterGroup::GetPlayerWithHighestHealth
const idPlayer* idEncounterGroup::GetPlayerWithHighestHealth() const {
  const target_t* best = nullptr;
  for (int i = 0; i < targetList.Num(); ++i) {
    if (targetList[i].player == nullptr) continue;
    if (best == nullptr || HealthRatio(targetList[i].health, targetList[i].maxHealth) >
                               HealthRatio(best->health, best->maxHealth))
      best = &targetList[i];
  }
  return best != nullptr ? best->player : nullptr;
}

// DIA: idEncounterGroupMgr::Spawn
void idEncounterGroupMgr::Spawn() {
  spawnedGroupCount = 0;
  if (manager == nullptr) return;
  for (int i = 0; i < spawnGroups.Num(); ++i) {
    if (manager->AddGroup(spawnGroups[i].data).value >= 0) ++spawnedGroupCount;
  }
}

// DIA: idCombatStage::GetNumHintNodes
const idList<int, 5>* idCombatStage::GetNumHintNodes(
    idAICombatHint::combatHintType_t hintType) const {
  return GetNodesListByHintType(hintType);
}

// DIA: DrawPrimitive
void DrawPrimitive(const idAICombatHint* hint, const char* name, int duration) {
  if (hint == nullptr) return;
  idEncounterDebugPrimitive primitive;
  primitive.origin = hint->origin;
  primitive.name = name != nullptr ? name : "hint";
  primitive.duration = std::max(0, duration);
  debugPrimitives.Append(primitive);
}

const idList<idEncounterDebugPrimitive, 5>& GetEncounterDebugPrimitives() {
  return debugPrimitives;
}

void ClearEncounterDebugPrimitives() { debugPrimitives.Clear(); }

// DIA: idEncounterState::idEncounterState
idEncounterState::idEncounterState() = default;

// DIA: idEncounterGroup::FindIndex
int idEncounterGroup::FindIndex(idAI2* ai) const {
  for (int i = 0; i < members.Num(); ++i)
    if (members[i].ai == ai) return i;
  return -1;
}

// DIA: idEncounterGroup::Remove
void idEncounterGroup::Remove(idAI2* ai) {
  const int index = FindIndex(ai);
  if (index < 0) return;
  if (members[index].firing) RemoveFirer();
  if (ai != nullptr && ai->GetAIGroupIdentity() == this)
    ai->SetAIEventEncounterGroup(nullptr);
  members.RemoveIndex(index);
  UpdateLivingMemberCount();
}

// DIA: idEncounterGroup::PercentMembersAlive
float idEncounterGroup::PercentMembersAlive() const {
  const int total = NumLivingMembers() + NumDeadMembers();
  return total > 0 ? static_cast<float>(NumLivingMembers()) /
                         static_cast<float>(total)
                   : 0.0F;
}

// DIA: idEncounterGroup::AverageHealth
float idEncounterGroup::AverageHealth() const {
  float total = 0.0F;
  int count = 0;
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai == nullptr || members[i].ai->IsDeadState()) continue;
    total += HealthRatio(members[i].ai->GetBaseHealth(), members[i].maxHealth);
    ++count;
  }
  return count > 0 ? total / static_cast<float>(count) : 0.0F;
}

// DIA: idEncounterGroup::AverageEnemyHealthRatio
float idEncounterGroup::AverageEnemyHealthRatio() const {
  float total = 0.0F;
  int count = 0;
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai == nullptr || members[i].ai->IsDeadState()) continue;
    total += std::clamp(members[i].enemyHealthRatio, 0.0F, 1.0F);
    ++count;
  }
  return count > 0 ? total / static_cast<float>(count) : 0.0F;
}

// DIA: idEncounterGroup::LowestTargetPlayerHealth
float idEncounterGroup::LowestTargetPlayerHealth() const {
  return LowestPlayerHealth();
}

// DIA: idEncounterGroup::HighestTargetPlayerHealth
float idEncounterGroup::HighestTargetPlayerHealth() const {
  return HighestPlayerHealth();
}

// DIA: idEncounterGroup::ActiveMemberHighestAlertCycle
int idEncounterGroup::ActiveMemberHighestAlertCycle() const {
  int highest = 0;
  for (int i = 0; i < members.Num(); ++i)
    if (members[i].active && members[i].ai != nullptr &&
        !members[i].ai->IsDeadState())
      highest = std::max(highest, members[i].alertCycle);
  return highest;
}

// DIA: idEncounterGroup::GetRandomMember
idAI2* idEncounterGroup::GetRandomMember() {
  const int index = NextRandomIndex(randomSeed, members.Num());
  return index >= 0 ? members[index].ai : nullptr;
}

// DIA: idEncounterGroup::GetRandomDeadMember
idAI2* idEncounterGroup::GetRandomDeadMember() {
  idStaticList<idAI2*, 64> candidates;
  for (int i = 0; i < members.Num(); ++i)
    if (members[i].ai != nullptr && members[i].ai->IsDeadState())
      candidates.Append(members[i].ai);
  const int index = NextRandomIndex(randomSeed, candidates.Num());
  return index >= 0 ? candidates[index] : nullptr;
}

// DIA: idEncounterGroup::GetRandomEnemy
const idEntity* idEncounterGroup::GetRandomEnemy() {
  const int index = NextRandomIndex(randomSeed, targetList.Num());
  return index >= 0 ? targetList[index].target : nullptr;
}

// DIA: idEncounterGroup::GetTargetedPlayerWithLowestHealth
const idPlayer* idEncounterGroup::GetTargetedPlayerWithLowestHealth() const {
  return GetPlayerWithLowestHealth();
}

// DIA: idEncounterGroup::GetTargetedPlayerWithHighestHealth
const idPlayer* idEncounterGroup::GetTargetedPlayerWithHighestHealth() const {
  return GetPlayerWithHighestHealth();
}

// DIA: idEncounterGroup::SetPendingCombatStage
bool idEncounterGroup::SetPendingCombatStage(int index, int delay) {
  if (index < 0 || index >= combatStages.Num()) return false;
  pendingCombatStage = index;
  pendingCombatStageTime = GameTime() + std::max(0, delay);
  return true;
}

// DIA: idEncounterGroup::UpdateAnyMemberHighestAlertCycle
void idEncounterGroup::UpdateAnyMemberHighestAlertCycle() {
  anyMemberHighestAlertCycle = 0;
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai != nullptr)
      members[i].alertCycle = members[i].ai->GetEncounterAlertCycle();
    anyMemberHighestAlertCycle =
        std::max(anyMemberHighestAlertCycle, members[i].alertCycle);
  }
}

// DIA: idEncounterGroup::GetLeader
idAI2* idEncounterGroup::GetLeader() const {
  for (int i = 0; i < members.Num(); ++i)
    if (members[i].ai != nullptr && !members[i].ai->IsDeadState() &&
        members[i].active)
      return members[i].ai;
  return nullptr;
}

// DIA: idAI2::UseTauntChatterVO
bool idAI2::UseTauntChatterVO() {
  if (!GetAIEventState().tauntsEnabled || IsDeadState() || IsDyingState() ||
      !encounterActive)
    return false;
  ++tauntUseCount;
  return true;
}

// DIA: idEncounterGroup::MemberKilled
void idEncounterGroup::MemberKilled(idAI2* ai, const idEntity* attacker,
                                    const idEntity* inflictor) {
  ++memberKilledCallCount;
  lastKilledMember = ai;
  lastKilledAttacker = attacker;
  lastKilledInflictor = inflictor;
  const int index = FindIndex(ai);
  if (index < 0 || ai == nullptr) return;
  ai->SetBaseHealth(0.0F);
  if (members[index].firing) {
    members[index].firing = false;
    RemoveFirer();
  }
  ++recentlyKilledMembers;
  SetGroupEnemyUpdateTimeStamp(attacker, GameTime());
  UpdateLivingMemberCount();
}

// DIA: idEncounterGroup::UpdateGroupAmbush
void idEncounterGroup::UpdateGroupAmbush(idAI2* member,
                                         const idVec3& enemyPos) {
  if (!shouldAmbush || member == nullptr) return;
  const idVec3 memberOrigin = EntityOrigin(member);
  const idEntity* alertTarget = ownGroupAlertState.target != nullptr
                                    ? ownGroupAlertState.target
                                    : otherGroupAlertState.target;
  if (alertTarget == nullptr) return;
  if ((memberOrigin - EntityOrigin(alertTarget)).LengthSqr() >
      (memberOrigin - enemyPos).LengthSqr())
    shouldAmbush = false;
}

// DIA: idEncounterGroup::FindSpeaker
idAI2* idEncounterGroup::FindSpeaker(const idEntity* enemy,
                                     bool mustBeNearLastVisPos,
                                     bool mustBeVisible,
                                     const idAI2* excludeAI) const {
  idAI2* best = nullptr;
  float bestDistance = std::numeric_limits<float>::max();
  const int now = GameTime();
  for (int i = 0; i < members.Num(); ++i) {
    const member_t& member = members[i];
    if (member.ai == nullptr || member.ai == excludeAI ||
        member.ai->IsDeadState() || member.ai->IsDyingState() ||
        !member.active || member.ai->GetEncounterEnemy() != enemy)
      continue;
    if (mustBeVisible && now - member.lastSeenEnemyTime > 5000) continue;
    const float distance =
        (EntityOrigin(member.ai) - ownGroupAlertState.lastKnownPosition)
            .LengthSqr();
    if (mustBeNearLastVisPos && distance > 3600.0F * 3600.0F) continue;
    if (distance < bestDistance) {
      best = member.ai;
      bestDistance = distance;
    }
  }
  return best;
}

// DIA: idEncounterGroup::SetPendingCombatStageByName
bool idEncounterGroup::SetPendingCombatStageByName(const idStr& name,
                                                   int delay) {
  for (int i = 0; i < combatStages.Num(); ++i)
    if (combatStages[i].name.Icmp(name.data) == 0)
      return SetPendingCombatStage(i, delay);
  return false;
}

// DIA: idEncounterGroup::SuggestEncounterGroupRole
bool idEncounterGroup::SuggestEncounterGroupRole(
    idAI2* ai, encounterGroupRole_t role,
    idCombatStage::combatStage_t combatStageChange,
    bool previouslyPlayedOrderVo) {
  if (ai == nullptr || FindIndex(ai) < 0 || ai->IsDeadState()) return false;
  if (combatStageChange != idCombatStage::COMBAT_STAGE_MAX &&
      !SetCurrentCombatStage(static_cast<int>(combatStageChange)))
    return false;
  ai->SetEncounterRole(role);
  members[FindIndex(ai)].role = role;
  if (!previouslyPlayedOrderVo) ai->UseTauntChatterVO();
  return true;
}

// DIA: idCombatStage::GetCombatHintEntity
const idAICombatHint* idCombatStage::GetCombatHintEntity(int index) const {
  return index >= 0 && index < hints.Num() ? hints[index] : nullptr;
}

// DIA: idCombatStage::GetClosestHintNodePtr
const idAICombatHint* idCombatStage::GetClosestHintNodePtr(
    idAICombatHint::combatHintType_t hintType, const idVec3& currentPos,
    int hintClass) const {
  const idList<int, 5>* nodes = GetNodesListByHintType(hintType);
  const idAICombatHint* best = nullptr;
  float bestDistance = std::numeric_limits<float>::max();
  if (nodes == nullptr) return nullptr;
  for (int i = 0; i < nodes->Num(); ++i) {
    const idAICombatHint* hint = GetCombatHintEntity((*nodes)[i]);
    if (hint == nullptr || !hint->enabled || hint->dontUseUntil > GameTime() ||
        (hint->hintClass & hintClass) == 0)
      continue;
    const float distance = (hint->origin - currentPos).LengthSqr();
    if (distance < bestDistance) {
      best = hint;
      bestDistance = distance;
    }
  }
  return best;
}

// DIA: idCombatStage::GetClosestHintNodePtrByDot
const idAICombatHint* idCombatStage::GetClosestHintNodePtrByDot(
    idAICombatHint::combatHintType_t hintType, const idVec3& currentPos,
    const idVec3& direction, int hintClass) const {
  const idList<int, 5>* nodes = GetNodesListByHintType(hintType);
  const idAICombatHint* best = nullptr;
  float bestDot = -2.0F;
  if (nodes == nullptr) return nullptr;
  idVec3 desired = direction;
  NormalizeSafe(desired);
  for (int i = 0; i < nodes->Num(); ++i) {
    const idAICombatHint* hint = GetCombatHintEntity((*nodes)[i]);
    if (hint == nullptr || !hint->enabled || hint->dontUseUntil > GameTime() ||
        (hint->hintClass & hintClass) == 0)
      continue;
    idVec3 toHint = hint->origin - currentPos;
    if (NormalizeSafe(toHint) == 0.0F) return hint;
    const float dot = toHint * desired;
    if (dot > bestDot) {
      best = hint;
      bestDot = dot;
    }
  }
  return best;
}

// DIA: idCombatStage::CategorizeHintGroupCombatNodes
void idCombatStage::CategorizeHintGroupCombatNodes() {
  defenseNodes.Clear(); assaultNodes.Clear(); flankNodes.Clear();
  flankTargetNodes.Clear(); flankPathNodes.Clear(); grenadeNodes.Clear();
  rechargeNodes.Clear(); suppressionFireNodes.Clear();
  for (int i = 0; i < hints.Num(); ++i) {
    if (hints[i] == nullptr) continue;
    idList<int, 5>* list = const_cast<idList<int, 5>*>(
        GetNodesListByHintType(hints[i]->type));
    if (list != nullptr) list->Append(i);
  }
}

// DIA: idEncounterGroup::DebugDrawCombatStage
void idEncounterGroup::DebugDrawCombatStage(const idCombatStage* stage,
                                            int duration) {
  if (stage == nullptr) return;
  for (int i = 0; i < stage->hints.Num(); ++i) {
    const idAICombatHint* hint = stage->hints[i];
    if (hint == nullptr) continue;
    if (hint->type != idAICombatHint::COMBAT_HINT_GRENADE_POINT ||
        ai_debugGrenadeHints.GetInteger() != 0)
      DrawPrimitive(hint, stage->name.data, duration);
  }
}

// DIA: idEncounterGroup::OnGroupMessage
void idEncounterGroup::OnGroupMessage(aiGroupMessage_t message,
                                      idEntity* entity) {
  lastMessage = message;
  lastMessageEntity = entity;
  ++messageCount;
  if (message >= AIGROUPMSG_ALERT) {
    ownGroupAlertState.target = entity;
    ownGroupAlertState.alertCycle = message;
    ownGroupAlertState.lastSeenTime = GameTime();
    ownGroupAlertState.lastKnownPosition = EntityOrigin(entity);
  }
}

// DIA: idEncounterGroup::GetCurrentCombatStageCoverIndices
void idEncounterGroup::GetCurrentCombatStageCoverIndices(
    idList<int, 5>* output) const {
  if (output == nullptr) return;
  *output = activeCoverIndices;
  if (curCombatStage < 0 || curCombatStage >= combatStages.Num()) return;
  const idCombatStage& stage = combatStages[curCombatStage];
  for (int i = 0; i < stage.hints.Num(); ++i) {
    const idAICombatHint* hint = stage.hints[i];
    if (hint == nullptr || !hint->enabled || hint->coverIndex < 0) continue;
    bool duplicate = false;
    for (int j = 0; j < output->Num(); ++j)
      duplicate |= (*output)[j] == hint->coverIndex;
    if (!duplicate) output->Append(hint->coverIndex);
  }
}

// DIA: idEncounterGroup::GetLivingMembers
void idEncounterGroup::GetLivingMembers(idStaticList<idAI2*, 64>* output) const {
  if (output == nullptr) return;
  for (int i = 0; i < members.Num() && output->Num() < 64; ++i)
    if (members[i].ai != nullptr && !members[i].ai->IsDeadState())
      output->Append(members[i].ai);
  for (int i = 0; i < childGroups.Num() && output->Num() < 64; ++i)
    if (childGroups[i] != nullptr) childGroups[i]->GetLivingMembers(output);
}

// DIA: idEncounterGroup::GetRandomLiveMember
idAI2* idEncounterGroup::GetRandomLiveMember() {
  idStaticList<idAI2*, 64> living;
  GetLivingMembers(&living);
  const int index = NextRandomIndex(randomSeed, living.Num());
  return index >= 0 ? living[index] : nullptr;
}

// DIA: idEncounterGroup::GetClosestActiveGroupMemberToPoint
idAI2* idEncounterGroup::GetClosestActiveGroupMemberToPoint(
    const idVec3& point) const {
  idAI2* best = nullptr;
  float bestDistance = std::numeric_limits<float>::max();
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai == nullptr || members[i].ai->IsDeadState() ||
        !members[i].active)
      continue;
    const float distance = (EntityOrigin(members[i].ai) - point).LengthSqr();
    if (distance < bestDistance) {
      best = members[i].ai;
      bestDistance = distance;
    }
  }
  return best;
}

// DIA: idEncounterGroup::ResolveChildReferences
void idEncounterGroup::ResolveChildReferences() {
  for (int i = childGroups.Num() - 1; i >= 0; --i)
    if (childGroups[i] == nullptr || childGroups[i] == this)
      childGroups.RemoveIndex(i);
  for (int i = childGroups.Num() - 1; i >= 0; --i)
    for (int j = 0; j < i; ++j)
      if (childGroups[i] == childGroups[j]) {
        childGroups.RemoveIndex(i);
        break;
      }
}

// DIA: idEncounterGroup::LastLivingMemberSeenEnemytime
int idEncounterGroup::LastLivingMemberSeenEnemytime() const {
  int latest = -1;
  for (int i = 0; i < members.Num(); ++i)
    if (members[i].ai != nullptr && !members[i].ai->IsDeadState())
      latest = std::max(latest, members[i].lastSeenEnemyTime);
  return latest;
}

// DIA: idEncounterGroup::UpdateFiringConditions
void idEncounterGroup::UpdateFiringConditions() {
  int allowed = maxSimultaneousFirers < 0 ? members.Num()
                                          : maxSimultaneousFirers;
  numSimultaneousFiring = 0;
  for (int i = 0; i < members.Num(); ++i) {
    const bool canFire = members[i].ai != nullptr &&
                         !members[i].ai->IsDeadState() && members[i].active &&
                         members[i].ai->GetEncounterEnemy() != nullptr &&
                         numSimultaneousFiring < allowed;
    members[i].firing = canFire;
    if (canFire) ++numSimultaneousFiring;
  }
}

// DIA: idEncounterGroup::UpdateGroupMemberRoles
bool idEncounterGroup::UpdateGroupMemberRoles(
    idCombatStage::combatStage_t combatStageChange) {
  if (combatStageChange != idCombatStage::COMBAT_STAGE_MAX &&
      !SetCurrentCombatStage(static_cast<int>(combatStageChange)))
    return false;
  if (curCombatStage < 0 || curCombatStage >= combatStages.Num()) return false;
  const idCombatStage& stage = combatStages[curCombatStage];
  if (stage.rolePriorityList.Num() == 0) return false;
  int roleIndex = 0;
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai == nullptr || members[i].ai->IsDeadState()) continue;
    const encounterGroupRole_t role =
        stage.rolePriorityList[roleIndex++ % stage.rolePriorityList.Num()];
    members[i].role = role;
    members[i].ai->SetEncounterRole(role);
  }
  return roleIndex > 0;
}

// DIA: `idEncounterGroup::UpdateGroupMemberRoles'::`1'::dtor$0
// DIA: `idEncounterGroup::UpdateGroupMemberRoles'::`1'::dtor$1
// DIA: `idEncounterGroup::UpdateGroupMemberRoles'::`1'::dtor$2

// DIA: idList<idEncounterGroup::target_t,5>::Resize
template bool idList<idEncounterGroup::target_t, 5>::Resize(int newSize);

// DIA: idEncounterGroupExpr::Value
float idEncounterGroupExpr::Value(idEncounterGroup* group) const {
  if (group == nullptr) return 0.0F;
  switch (value) {
    case VALUE_LIVING_MEMBERS: return static_cast<float>(group->NumLivingMembers());
    case VALUE_DEAD_MEMBERS: return static_cast<float>(group->NumDeadMembers());
    case VALUE_PERCENT_ALIVE: return group->PercentMembersAlive();
    case VALUE_AVERAGE_HEALTH: return group->AverageHealth();
    case VALUE_LOWEST_TARGET_HEALTH: return group->LowestTargetPlayerHealth();
    case VALUE_HIGHEST_ALERT_CYCLE:
      return static_cast<float>(group->AnyMemberHighestAlertCycle());
  }
  return 0.0F;
}

// DIA: `idEncounterGroupExpr::Value'::`1'::dtor$0
// DIA: `idEncounterGroupExpr::Value'::`1'::dtor$1
// DIA: `idEncounterGroupExpr::Value'::`1'::dtor$2
// DIA: `idEncounterGroupExpr::Value'::`1'::dtor$3
// DIA: `idEncounterGroupExpr::Value'::`1'::dtor$4
// DIA: `idEncounterGroupExpr::Value'::`1'::dtor$5

// DIA: idEncounterGroupExpr::TriggerTarget
void idEncounterGroupExpr::TriggerTarget(idEncounterGroup* group) {
  if (target != nullptr) target->OnActivate(group != nullptr ? group->GetLeader() : nullptr);
  if (callback && group != nullptr) callback(*group);
  triggered = true;
}

// DIA: idEncounterGroup::UpdateLivingMemberCount
void idEncounterGroup::UpdateLivingMemberCount() {
  numLivingMembers = 0;
  numDeadMembers = 0;
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai != nullptr && !members[i].ai->IsDeadState())
      ++numLivingMembers;
    else
      ++numDeadMembers;
  }
  livingMembers = numLivingMembers;
}

// DIA: idEncounterGroup::SetCurrentCombatStage
bool idEncounterGroup::SetCurrentCombatStage(int index) {
  if (index < 0 || index >= combatStages.Num()) return false;
  curCombatStage = index;
  pendingCombatStage = -1;
  pendingCombatStageTime = -1;
  activeCoverIndices.Clear();
  const idCombatStage& stage = combatStages[index];
  for (int i = 0; i < stage.hints.Num(); ++i)
    if (stage.hints[i] != nullptr && stage.hints[i]->coverIndex >= 0)
      activeCoverIndices.Append(stage.hints[i]->coverIndex);
  return true;
}

// DIA: idEncounterGroupExpr::Eval
bool idEncounterGroupExpr::Eval(idEncounterGroup* owner) const {
  const float current = Value(owner);
  switch (comparison) {
    case COMPARE_LESS: return current < threshold;
    case COMPARE_LESS_EQUAL: return current <= threshold;
    case COMPARE_EQUAL: return std::fabs(current - threshold) <= 0.0001F;
    case COMPARE_GREATER_EQUAL: return current >= threshold;
    case COMPARE_GREATER: return current > threshold;
  }
  return false;
}

// DIA: idEncounterGroupExpr::CheckTrigger
void idEncounterGroupExpr::CheckTrigger(idEncounterGroup* group) {
  if ((!triggered || repeat) && Eval(group)) TriggerTarget(group);
}

// DIA: idEncounterGroup::Init
void idEncounterGroup::Init(const char* nameValue, std::int16_t handleValue) {
  groupName = nameValue != nullptr ? nameValue : "";
  handle = handleValue;
  initialized = true;
  if (initialCombatStage >= 0 && initialCombatStage < combatStages.Num())
    SetCurrentCombatStage(initialCombatStage);
}

// DIA: idEncounterGroup::Add
void idEncounterGroup::Add(idAI2* ai) {
  if (ai == nullptr || FindIndex(ai) >= 0 || members.Num() >= 64) return;
  member_t member;
  member.ai = ai;
  member.maxHealth = std::max(1.0F, ai->GetBaseHealth());
  member.alertCycle = ai->GetEncounterAlertCycle();
  member.lastSeenEnemyTime = ai->GetEncounterLastSeenTime();
  member.active = ai->IsEncounterActive();
  members.Append(member);
  ai->SetAIEventEncounterGroup(this);
  UpdateLivingMemberCount();
}

// DIA: idEncounterGroup::CheckGroupTriggers
void idEncounterGroup::CheckGroupTriggers() {
  for (int i = 0; i < expressions.Num(); ++i) expressions[i].CheckTrigger(this);
}

// DIA: idEncounterGroup::Think
void idEncounterGroup::Think() {
  ++thinkCount;
  for (int i = 0; i < members.Num(); ++i) {
    if (members[i].ai == nullptr) continue;
    members[i].active = members[i].ai->IsEncounterActive();
    members[i].alertCycle = members[i].ai->GetEncounterAlertCycle();
    members[i].lastSeenEnemyTime = members[i].ai->GetEncounterLastSeenTime();
  }
  UpdateLivingMemberCount();
  UpdateAnyMemberHighestAlertCycle();
  if (pendingCombatStage >= 0 && GameTime() >= pendingCombatStageTime)
    SetCurrentCombatStage(pendingCombatStage);
  UpdateFiringConditions();
  CheckGroupTriggers();
}

// DIA: idList<idCombatStage,5>::~idList<idCombatStage,5>
template idList<idCombatStage, 5>::~idList();

// DIA: idEncounterGroup::idEncounterGroup
idEncounterGroup::idEncounterGroup() = default;

// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$0
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$1
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$2
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$3
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$4
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$5
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$6
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$7
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$8
// DIA: `idEncounterGroup::idEncounterGroup'::`1'::dtor$9

// DIA: idEncounterGroup::~idEncounterGroup
idEncounterGroup::~idEncounterGroup() = default;

// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$0
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$1
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$2
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$3
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$4
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$5
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$6
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$7
// DIA: `idEncounterGroup::~idEncounterGroup'::`1'::dtor$8

// DIA: idEncounterGroup::UpdateTargetList
void idEncounterGroup::UpdateTargetList(const idTargetList* targets) {
  if (targets == nullptr) return;
  const int frame = gameLocal != nullptr ? gameLocal->GetFrameNum() : 0;
  for (int i = 0; i < targets->NumViableTargets(); ++i) {
    const idTargetInfo* incoming = targets->GetViableTarget(i);
    if (incoming == nullptr || incoming->es == nullptr) continue;
    idEntity* entity = incoming->es->GetEntity();
    if (entity == nullptr) continue;
    int found = -1;
    for (int j = 0; j < targetList.Num(); ++j)
      if (targetList[j].target != nullptr &&
          targetList[j].target->GetEntityNumber() ==
              incoming->es->spawnId.value) {
        found = j;
        break;
      }
    if (found < 0) found = targetList.Append(target_t{});
    targetList[found].target = entity;
    targetList[found].player = dynamic_cast<idPlayer*>(entity);
    targetList[found].frame = frame;
  }
}

// DIA: idEncounterGroupMgr::idEncounterGroupMgr
idEncounterGroupMgr::idEncounterGroupMgr() = default;

// DIA: idEncounterGroupMgr::~idEncounterGroupMgr
idEncounterGroupMgr::~idEncounterGroupMgr() = default;

// DIA: `idEncounterGroupMgr::~idEncounterGroupMgr'::`1'::dtor$0
// DIA: `idEncounterGroupMgr::~idEncounterGroupMgr'::`1'::dtor$1

// DIA: idEncounterGroupMgr::`scalar deleting destructor'

// DIA: `dynamic initializer for 'ai_debugGrenadeHints''
// DIA: `dynamic initializer for 'ai_debugGoodWill''
// DIA: `dynamic atexit destructor for 'ai_debugGrenadeHints''
// DIA: `dynamic atexit destructor for 'ai_debugGoodWill''

// Header-emitted encounter-group accessors used by AI2.
idCombatStage* idEncounterGroup::GetCurrentCombatStage() {
  return curCombatStage < 0 || curCombatStage >= combatStages.Num()
             ? nullptr
             : &combatStages[curCombatStage];
}

const idCombatStage* idEncounterGroup::GetCurrentCombatStage() const {
  return curCombatStage < 0 || curCombatStage >= combatStages.Num()
             ? nullptr
             : &combatStages[curCombatStage];
}
