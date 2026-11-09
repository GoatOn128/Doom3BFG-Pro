#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <array>
#include <cstdint>

enum voiceController_t : int {
  VC_SELF = 0,
  VC_LOCAL = VC_SELF,
  VC_GROUP = 1,
  VC_GLOBAL = 2,
  VC_NONE = 3,
};

enum aiVoicePriority_t {
  AIVOICEPRIORITY_NONE = 0,
  AIVOICEPRIORITY_AMBIENT = 1,
  AIVOICEPRIORITY_DIALOGUE = 2,
  AIVOICEPRIORITY_COMBAT = 3,
  AIVOICEPRIORITY_PAIN = 4,
};

enum voiceMsg_t : int {
  AIVOICE_IDLE_ALONE,
  AIVOICE_IDLE_BUDDY,
  AIVOICE_IDLE_BUDDY_RESPONSE,
  AIVOICE_IDLE_PRESPOT,
  AIVOICE_IDLE_FIRST_SPOT_CORPSE,
  AIVOICE_IDLE_ALARM,
  AIVOICE_SEARCH_NOTICE,
  AIVOICE_SEARCH_CONTINUE,
  AIVOICE_SEARCH_BUDDY,
  AIVOICE_SEARCH_BUDDY_RESPONSE,
  AIVOICE_SEARCH_SPOT_CORPSE,
  AIVOICE_COMBAT_MELEE_TAUNT,
  AIVOICE_COMBAT_RANGED_TAUNT,
  AIVOICE_COMBAT_FRIENDLY_BLOCKED,
  AIVOICE_COMBAT_CHANGE_POSITION,
  AIVOICE_COMBAT_FIRST_SPOT,
  AIVOICE_COMBAT_RESPOT,
  AIVOICE_COMBAT_FLANK_SPOT,
  AIVOICE_COMBAT_DOWNED,
  AIVOICE_COMBAT_GRENADE,
  AIVOICE_COMBAT_RELOAD,
  AIVOICE_COMBAT_DEATH_TAUNT,
  AIVOICE_COMBAT_FIGHT_RESPONSE,
  AIVOICE_COMBAT_HURT,
  AIVOICE_COMBAT_CLOSE_PERIODIC,
  AIVOICE_COMBAT_ALARM2,
  AIVOICE_COMBAT_WINNING,
  AIVOICE_COMBAT_LOSING,
  AIVOICE_COMBAT_REINFORCE,
  AIVOICE_COMBAT_MEINLEBEN,
  AIVOICE_MAX,
};

struct idSoundShader {
  idStr name;
  int durationMs{0};
};

struct idVoiceInfo {
  const idSoundShader* shader{nullptr};
  const idDeclVoiceOver* voiceOver{nullptr};
  voiceController_t controller{VC_LOCAL};
  aiVoicePriority_t priority{AIVOICEPRIORITY_AMBIENT};
  int interval{0};
  int intervalMax{0};
  int playChance{100};
  bool buddyOnly{false};
  bool playOnce{false};
};

struct idAIVoiceProfile {
  std::array<idVoiceInfo, AIVOICE_MAX> voices{};
  std::array<bool, AIVOICE_MAX> configured{};
  bool muteIdle{false};
  bool muteSearch{false};
  bool muteCombat{false};
  bool blocked{false};
  int livingGroupMembers{1};

  void Set(voiceMsg_t message, const idVoiceInfo& info) {
    voices[message] = info;
    configured[message] = true;
  }
};

class idAIVoiceController {
public:
  struct logEntry_t {
    int entityNumber{-1};
    int gameTime{0};
    voiceMsg_t message{AIVOICE_IDLE_ALONE};
  };

  static void PrintLog();
  explicit idAIVoiceController(voiceController_t controllerTypeValue);
  void InternalAdvanceNextPlayTime(voiceMsg_t message,
                                   const idVoiceInfo* voiceInfo);
  void FadeVoiceOver(idAI2* ai);
  void StopVoiceOver(idAI2* ai);
  static const idVoiceInfo* VoiceInfoForMessage(idAI2* ai,
                                                 voiceMsg_t message);
  idAIVoiceController* GetControllerForVoice(idAI2* ai,
                                              voiceMsg_t message);
  voiceController_t StartVoice(idAI2* ai, voiceMsg_t message,
                               const idVoiceInfo* voiceInfo);
  voiceController_t PlayVoiceOver(idAI2* ai, const idDeclVoiceOver* voiceOver,
                                  aiVoicePriority_t priority,
                                  bool samePriorityInterrupts);
  void ResetTiming(idAI2* ai);
  voiceController_t PlayVoice(idAI2* ai, voiceMsg_t message);

  static void SetGlobalController(idAIVoiceController* value) {
    globalController = value;
  }
  static idAIVoiceController* GetGlobalController() {
    return globalController;
  }
  static void ClearLog();
  static int GetLogCount() { return logCount; }
  static const logEntry_t& GetLogEntry(int chronologicalIndex);
  void SetRandomSeed(std::uint32_t value) { randomSeed = value; }
  int GetNextVoiceTime() const { return nextVoiceTime; }
  int GetLastVoiceTime() const { return lastVoiceTime; }
  int GetNextPlayTime(voiceMsg_t message) const {
    return nextPlayTime[message];
  }
  aiVoicePriority_t GetLastPriority() const { return lastVoicePriority; }
  bool WasFaded() const { return faded; }
  int GetStopVoiceOverCountForTest() const { return stopVoiceOverCount; }

private:
  int CurrentTime() const;
  int RandomInterval(int minimum, int maximum);
  int RandomPercent();
  static void AppendLog(idAI2* ai, voiceMsg_t message, int gameTime);

  voiceController_t controllerType;
  int nextVoiceTime{0};
  int lastVoiceTime{0};
  aiVoicePriority_t lastVoicePriority{AIVOICEPRIORITY_NONE};
  std::array<int, AIVOICE_MAX> nextPlayTime{};
  std::uint32_t randomSeed{0x12345678u};
  bool faded{false};
  int stopVoiceOverCount{0};

  inline static idAIVoiceController* globalController{nullptr};
  inline static std::array<logEntry_t, 50> logEntries{};
  inline static int logCount{0};
  inline static int printCount{0};
};

extern idCVar ai_voFadeTime;
