#include "engine/sys/sys_voicechat.h"

#include "shared/idlib/sys/sys_time.h"

#include <algorithm>

int idVoiceChatMgr::FindTalkerIndex(const lobbyUser_t* user, int lobbyType) const {
  for (int i = 0; i < static_cast<int>(talkers.size()); ++i) {
    const talker_t& talker = talkers[static_cast<std::size_t>(i)];
    if (talker.user == user && talker.lobbyType == lobbyType) return i;
  }
  return -1;
}

bool idVoiceChatMgr::IsRestrictedByPrivleges() const {
  return (disableVoiceReasons & REASON_PRIVILEGES) != 0;
}

int idVoiceChatMgr::FindTalkerByUserId(lobbyUserID_t userId, int lobbyType) const {
  for (int i = 0; i < static_cast<int>(talkers.size()); ++i) {
    const talker_t& talker = talkers[static_cast<std::size_t>(i)];
    if (talker.user != nullptr && talker.user->lobbyUserID == userId && talker.lobbyType == lobbyType)
      return i;
  }
  return -1;
}

voiceState_t idVoiceChatMgr::GetVoiceState(const lobbyUser_t* user) {
  if (user == nullptr) return VOICECHAT_STATE_NO_MIC;
  const int index = FindTalkerByUserId(user->lobbyUserID, activeLobbyType);
  if (index < 0) return VOICECHAT_STATE_NO_MIC;
  talker_t& talker = talkers[static_cast<std::size_t>(index)];
  if (!talker.hasHeadset) return VOICECHAT_STATE_NO_MIC;
  if (talker.isMuted) return VOICECHAT_STATE_MUTED_LOCAL;
  if (talker.talking && Sys_Milliseconds() - talker.talkingTime > 200) talker.talking = false;
  if (!talker.talking) return VOICECHAT_STATE_NOT_TALKING;
  return talker.talkingGlobal ? VOICECHAT_STATE_TALKING_GLOBAL : VOICECHAT_STATE_TALKING;
}

void idVoiceChatMgr::ToggleMuteLocal(const lobbyUser_t* source, const lobbyUser_t* target) {
  if (source == nullptr || target == nullptr ||
      FindTalkerByUserId(source->lobbyUserID, activeLobbyType) < 0) return;
  const int targetIndex = FindTalkerByUserId(target->lobbyUserID, activeLobbyType);
  if (targetIndex >= 0) {
    talker_t& talker = talkers[static_cast<std::size_t>(targetIndex)];
    talker.isMuted = !talker.isMuted;
  }
}

int idVoiceChatMgr::FindMachine(const lobbyAddress_t& address, int lobbyType) const {
  for (int i = 0; i < static_cast<int>(remoteMachines.size()); ++i) {
    const remoteMachine_t& machine = remoteMachines[static_cast<std::size_t>(i)];
    if (machine.refCount > 0 && machine.lobbyType == lobbyType && machine.address.Compare(address))
      return i;
  }
  return -1;
}

int idVoiceChatMgr::AddMachine(const lobbyAddress_t& address, int lobbyType) {
  int index = FindMachine(address, lobbyType);
  if (index >= 0) {
    ++remoteMachines[static_cast<std::size_t>(index)].refCount;
    return index;
  }
  remoteMachine_t machine;
  machine.address = address;
  machine.lobbyType = lobbyType;
  machine.refCount = 1;
  auto freeSlot = std::find_if(remoteMachines.begin(), remoteMachines.end(),
                               [](const remoteMachine_t& item) { return item.refCount == 0; });
  if (freeSlot != remoteMachines.end()) {
    *freeSlot = machine;
    return static_cast<int>(freeSlot - remoteMachines.begin());
  }
  remoteMachines.push_back(machine);
  return static_cast<int>(remoteMachines.size() - 1);
}

void idVoiceChatMgr::RemoveMachine(int machineIndex, int lobbyType) {
  if (machineIndex < 0 || machineIndex >= static_cast<int>(remoteMachines.size())) return;
  remoteMachine_t& machine = remoteMachines[static_cast<std::size_t>(machineIndex)];
  if (machine.lobbyType == lobbyType && machine.refCount > 0) --machine.refCount;
}

void idVoiceChatMgr::UpdateRegisteredTalkers() {
  for (int pass = 0; pass < 2; ++pass) {
    for (int i = 0; i < static_cast<int>(talkers.size()); ++i) {
      talker_t& talker = talkers[static_cast<std::size_t>(i)];
      const bool shouldRegister = talker.lobbyType != -1 && disableVoiceReasons == 0 &&
                                  talker.lobbyType == activeLobbyType;
      if (shouldRegister && pass == 0) continue;
      if (talker.registered == shouldRegister) continue;
      if (!talker.registered) {
        talker.registeredSuccess = RegisterTalkerInternal(i);
      } else if (talker.registeredSuccess) {
        UnregisterTalkerInternal(i);
        talker.registeredSuccess = false;
      }
      talker.registered = shouldRegister;
    }
  }
}

void idVoiceChatMgr::RegisterTalker(lobbyUser_t* user, int lobbyType, bool isLocal) {
  if (user == nullptr || FindTalkerIndex(user, lobbyType) >= 0) return;
  talker_t talker;
  talker.user = user;
  talker.isLocal = isLocal;
  talker.lobbyType = lobbyType;
  talker.groupIndex = 0;
  if (!isLocal) talker.machineIndex = AddMachine(user->address, lobbyType);
  talkers.push_back(talker);
  UpdateRegisteredTalkers();
}

void idVoiceChatMgr::UnregisterTalker(lobbyUser_t* user, int lobbyType, bool isLocal) {
  const int index = FindTalkerIndex(user, lobbyType);
  if (index < 0) return;
  talker_t& talker = talkers[static_cast<std::size_t>(index)];
  if (talker.isLocal != isLocal) return;
  talker.lobbyType = -1;
  UpdateRegisteredTalkers();
  if (talker.machineIndex >= 0) RemoveMachine(talker.machineIndex, lobbyType);
  talkers.erase(talkers.begin() + index);
}

void idVoiceChatMgr::SetActiveLobby(int lobbyType) {
  if (activeLobbyType != lobbyType) {
    activeLobbyType = lobbyType;
    UpdateRegisteredTalkers();
  }
}

bool idVoiceChatMgr::GetLocalChatData(int talkerIndex, std::uint8_t* data, int& dataSize) {
  if (talkerIndex < 0 || talkerIndex >= static_cast<int>(talkers.size()) || data == nullptr ||
      dataSize <= 5) return false;
  talker_t& talker = talkers[static_cast<std::size_t>(talkerIndex)];
  if (!talker.isLocal || !talker.registeredSuccess || talker.user == nullptr) return false;
  idBitMsg header;
  header.InitWrite(data, dataSize);
  talker.user->lobbyUserID.WriteToMsg(header);
  const int headerSize = header.GetSize();
  int payloadSize = dataSize - headerSize;
  if (!GetLocalChatDataInternal(talkerIndex, data + headerSize, payloadSize)) {
    dataSize = 0;
    return false;
  }
  dataSize = headerSize + payloadSize;
  talker.talking = true;
  talker.talkingTime = static_cast<int>(Sys_Milliseconds());
  return dataSize > 0;
}

void idVoiceChatMgr::SubmitIncomingChatData(const std::uint8_t* data, int dataSize) {
  if (data == nullptr || dataSize <= 5) return;
  idBitMsg header;
  header.InitRead(data, dataSize);
  lobbyUserID_t userId;
  userId.ReadFromMsg(header);
  const int index = FindTalkerByUserId(userId, activeLobbyType);
  if (index < 0) return;
  talker_t& talker = talkers[static_cast<std::size_t>(index)];
  if (talker.registeredSuccess && !talker.isMuted) {
    talker.talking = true;
    talker.talkingTime = static_cast<int>(Sys_Milliseconds());
    SubmitIncomingChatDataInternal(index, data + 5, dataSize - 5);
  }
}

void idVoiceChatMgr::GetActiveLocalTalkers(std::vector<int>& localTalkers) {
  localTalkers.clear();
  for (int i = 0; i < static_cast<int>(talkers.size()); ++i) {
    const talker_t& talker = talkers[static_cast<std::size_t>(i)];
    if (talker.isLocal && talker.registeredSuccess && TalkerHasData(i)) localTalkers.push_back(i);
  }
}

bool idVoiceChatMgr::CanSendVoiceTo(int fromIndex, int toIndex) const {
  return fromIndex >= 0 && fromIndex < static_cast<int>(talkers.size()) && toIndex >= 0 &&
         toIndex < static_cast<int>(talkers.size()) &&
         talkers[static_cast<std::size_t>(fromIndex)].isLocal &&
         !talkers[static_cast<std::size_t>(toIndex)].isMuted;
}

void idVoiceChatMgr::GetRecipientsForTalker(
    int talkerIndex, std::vector<const lobbyAddress_t*>& recipients) {
  recipients.clear();
  if (talkerIndex < 0 || talkerIndex >= static_cast<int>(talkers.size()) ||
      !talkers[static_cast<std::size_t>(talkerIndex)].isLocal) return;
  ++sendFrame;
  for (int i = 0; i < static_cast<int>(talkers.size()); ++i) {
    const talker_t& talker = talkers[static_cast<std::size_t>(i)];
    if (!talker.registeredSuccess || talker.isLocal || !CanSendVoiceTo(talkerIndex, i) ||
        (!sendGlobal && talker.groupIndex != activeGroupIndex) || talker.machineIndex < 0) continue;
    remoteMachine_t& machine = remoteMachines[static_cast<std::size_t>(talker.machineIndex)];
    if (machine.refCount <= 0 || machine.lobbyType != activeLobbyType || machine.sendFrame == sendFrame)
      continue;
    machine.sendFrame = sendFrame;
    recipients.push_back(&machine.address);
  }
}

void idVoiceChatMgr::SetHeadsetState(int talkerIndex, bool state) {
  if (talkerIndex < 0 || talkerIndex >= static_cast<int>(talkers.size())) return;
  talker_t& talker = talkers[static_cast<std::size_t>(talkerIndex)];
  if (talker.hasHeadset != state) {
    talker.hasHeadset = state;
    talker.hasHeadsetChanged = true;
  }
}

bool idVoiceChatMgr::HasHeadsetStateChanged(int talkerIndex) {
  if (talkerIndex < 0 || talkerIndex >= static_cast<int>(talkers.size())) return false;
  talker_t& talker = talkers[static_cast<std::size_t>(talkerIndex)];
  const bool changed = talker.hasHeadsetChanged;
  talker.hasHeadsetChanged = false;
  return changed;
}

void idVoiceChatMgr::SetDisableVoiceReason(disableVoiceReason_t reason) {
  if ((disableVoiceReasons & reason) == 0) {
    disableVoiceReasons |= reason;
    UpdateRegisteredTalkers();
  }
}

void idVoiceChatMgr::ClearDisableVoiceReason(disableVoiceReason_t reason) {
  if ((disableVoiceReasons & reason) != 0) {
    disableVoiceReasons &= ~reason;
    UpdateRegisteredTalkers();
  }
}

