#include "tungsten/game/ai/aivoicecontroller.h"

#include <algorithm>

idCVar ai_voFadeTime(
    "ai_voFadeTime", "1.5", 4,
    "time in seconds VO will fade when walking away from a conversation");

// DIA: idAIVoiceController::PrintLog
void idAIVoiceController::PrintLog() { printCount = std::min(logCount, 50); }

// DIA: idAIVoiceController::idAIVoiceController
idAIVoiceController::idAIVoiceController(
    voiceController_t controllerTypeValue)
    : controllerType(controllerTypeValue) {}

int idAIVoiceController::CurrentTime() const {
  return gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
}

int idAIVoiceController::RandomInterval(int minimum, int maximum) {
  if (maximum < minimum) std::swap(minimum, maximum);
  if (maximum == minimum) return minimum;
  randomSeed = 1664525u * randomSeed + 1013904223u;
  const unsigned range = static_cast<unsigned>(maximum - minimum + 1);
  return minimum + static_cast<int>(((randomSeed >> 10) & 0x7fffu) % range);
}

int idAIVoiceController::RandomPercent() { return RandomInterval(0, 99); }

// DIA: idAIVoiceController::InternalAdvanceNextPlayTime
void idAIVoiceController::InternalAdvanceNextPlayTime(
    voiceMsg_t message, const idVoiceInfo* voiceInfo) {
  if (message < 0 || message >= AIVOICE_MAX || voiceInfo == nullptr) return;
  nextPlayTime[message] =
      std::max(nextPlayTime[message],
               CurrentTime() +
                   RandomInterval(voiceInfo->interval, voiceInfo->intervalMax));
}

// DIA: idAIVoiceController::FadeVoiceOver
void idAIVoiceController::FadeVoiceOver(idAI2* ai) {
  if (ai != nullptr) ai->SetVoiceOverPlaying(false);
  nextVoiceTime = 0;
  lastVoicePriority = AIVOICEPRIORITY_NONE;
  faded = true;
}

// DIA: idAIVoiceController::StopVoiceOver
void idAIVoiceController::StopVoiceOver(idAI2* ai) {
  ++stopVoiceOverCount;
  if (ai != nullptr) ai->SetVoiceOverPlaying(false);
  nextVoiceTime = 0;
  lastVoicePriority = AIVOICEPRIORITY_NONE;
  faded = false;
}

// DIA: idAIVoiceController::VoiceInfoForMessage
const idVoiceInfo* idAIVoiceController::VoiceInfoForMessage(
    idAI2* ai, voiceMsg_t message) {
  if (ai == nullptr || message < 0 || message >= AIVOICE_MAX ||
      ai->GetVoiceProfile() == nullptr ||
      !ai->GetVoiceProfile()->configured[message]) {
    return nullptr;
  }
  return &ai->GetVoiceProfile()->voices[message];
}

// DIA: idAIVoiceController::GetControllerForVoice
idAIVoiceController* idAIVoiceController::GetControllerForVoice(
    idAI2* ai, voiceMsg_t message) {
  const idVoiceInfo* info = VoiceInfoForMessage(ai, message);
  if (info == nullptr) return nullptr;
  if (info->controller == VC_GROUP) {
    return ai != nullptr ? ai->GetVoiceGroupController() : nullptr;
  }
  if (info->controller == VC_GLOBAL) return globalController;
  return this;
}

void idAIVoiceController::AppendLog(idAI2* ai, voiceMsg_t message,
                                    int gameTime) {
  const int index = logCount % 50;
  logEntries[index] = {ai != nullptr ? ai->GetEntityNumber() : -1, gameTime,
                       message};
  ++logCount;
}

// DIA: idAIVoiceController::StartVoice
voiceController_t idAIVoiceController::StartVoice(
    idAI2* ai, voiceMsg_t message, const idVoiceInfo* voiceInfo) {
  if (ai == nullptr || voiceInfo == nullptr || message < 0 ||
      message >= AIVOICE_MAX ||
      (voiceInfo->shader == nullptr && voiceInfo->voiceOver == nullptr)) {
    return VC_NONE;
  }
  const int now = CurrentTime();
  if (nextPlayTime[message] > now ||
      (voiceInfo->playOnce && nextPlayTime[message] != 0) ||
      (ai->IsDeadState() && voiceInfo->priority < AIVOICEPRIORITY_PAIN) ||
      (now < nextVoiceTime && voiceInfo->priority <= lastVoicePriority)) {
    return VC_NONE;
  }
  InternalAdvanceNextPlayTime(message, voiceInfo);
  if (RandomPercent() >= std::clamp(voiceInfo->playChance, 0, 100)) {
    return VC_NONE;
  }

  const int duration = voiceInfo->voiceOver != nullptr
                           ? voiceInfo->voiceOver->durationMs
                           : voiceInfo->shader->durationMs;
  lastVoicePriority = voiceInfo->priority;
  lastVoiceTime = now;
  nextVoiceTime = now + std::max(duration, 0);
  faded = false;
  ai->SetVoiceOverPlaying(true);
  AppendLog(ai, message, now);
  return controllerType;
}

// DIA: idAIVoiceController::PlayVoiceOver
voiceController_t idAIVoiceController::PlayVoiceOver(
    idAI2* ai, const idDeclVoiceOver* voiceOver,
    aiVoicePriority_t priority, bool samePriorityInterrupts) {
  if (ai == nullptr || voiceOver == nullptr) return VC_NONE;
  const int now = CurrentTime();
  if (now < nextVoiceTime &&
      (samePriorityInterrupts ? priority < lastVoicePriority
                              : priority <= lastVoicePriority)) {
    return VC_NONE;
  }
  if (ai->IsDeadState() && priority < AIVOICEPRIORITY_PAIN) return VC_NONE;
  lastVoicePriority = priority;
  lastVoiceTime = now;
  nextVoiceTime = now + std::max(voiceOver->durationMs, 0);
  faded = false;
  ai->SetVoiceOverPlaying(true);
  return controllerType;
}

// DIA: idAIVoiceController::ResetTiming
void idAIVoiceController::ResetTiming(idAI2* ai) {
  for (int index = 0; index < AIVOICE_MAX; ++index) {
    const voiceMsg_t message = static_cast<voiceMsg_t>(index);
    if (message == AIVOICE_IDLE_FIRST_SPOT_CORPSE ||
        message == AIVOICE_IDLE_ALARM || message == AIVOICE_SEARCH_NOTICE ||
        message == AIVOICE_COMBAT_FIRST_SPOT) {
      continue;
    }
    const idVoiceInfo* info = VoiceInfoForMessage(ai, message);
    idAIVoiceController* controller = GetControllerForVoice(ai, message);
    if (info != nullptr && controller != nullptr) {
      controller->InternalAdvanceNextPlayTime(message, info);
    }
  }
}

// DIA: idAIVoiceController::PlayVoice
voiceController_t idAIVoiceController::PlayVoice(idAI2* ai,
                                                  voiceMsg_t message) {
  const idVoiceInfo* info = VoiceInfoForMessage(ai, message);
  if (ai == nullptr || info == nullptr || ai->GetVoiceProfile()->blocked) {
    return VC_NONE;
  }
  const int alertCycle = ai->GetAIEventState().alertCycle;
  if ((alertCycle == 0 && ai->GetVoiceProfile()->muteIdle) ||
      (alertCycle == 1 && ai->GetVoiceProfile()->muteSearch) ||
      (alertCycle == 2 && ai->GetVoiceProfile()->muteCombat) ||
      (info->buddyOnly && ai->GetVoiceProfile()->livingGroupMembers < 2)) {
    return VC_NONE;
  }
  idAIVoiceController* controller = GetControllerForVoice(ai, message);
  if (controller == nullptr) return VC_NONE;
  const voiceController_t result = controller->StartVoice(ai, message, info);
  if (result == VC_NONE) return VC_NONE;
  return controller == this ? result : StartVoice(ai, message, info);
}

void idAIVoiceController::ClearLog() {
  logCount = 0;
  printCount = 0;
}

const idAIVoiceController::logEntry_t& idAIVoiceController::GetLogEntry(
    int chronologicalIndex) {
  const int count = std::min(logCount, 50);
  const int first = logCount > 50 ? logCount % 50 : 0;
  const int clamped = std::clamp(chronologicalIndex, 0, count - 1);
  return logEntries[(first + clamped) % 50];
}

// DIA: `dynamic initializer for 'ai_voFadeTime''

// DIA: `dynamic atexit destructor for 'ai_voFadeTime''
