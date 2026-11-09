#pragma once

#include "engine/sys/sys_lobby_types.h"

#include <cstdint>
#include <vector>

enum voiceState_t : std::int32_t {
  VOICECHAT_STATE_NO_MIC = 0,
  VOICECHAT_STATE_MUTED_LOCAL,
  VOICECHAT_STATE_MUTED_REMOTE,
  VOICECHAT_STATE_MUTED_ALL,
  VOICECHAT_STATE_NOT_TALKING,
  VOICECHAT_STATE_TALKING,
  VOICECHAT_STATE_TALKING_GLOBAL,
  NUM_VOICECHAT_STATE
};

class idVoiceChatMgr {
public:
  enum disableVoiceReason_t : std::uint32_t { REASON_GENERIC = 1, REASON_PRIVILEGES = 2 };

  idVoiceChatMgr() = default;
  virtual ~idVoiceChatMgr() = default;
  virtual bool RegisterTalkerInternal(int index) = 0;
  virtual void UnregisterTalkerInternal(int index) = 0;
  virtual bool GetLocalChatDataInternal(int talkerIndex, std::uint8_t* data, int& dataSize) = 0;
  virtual void SubmitIncomingChatDataInternal(int talkerIndex, const std::uint8_t* data,
                                              int dataSize) = 0;
  virtual bool TalkerHasData(int talkerIndex) = 0;

  void RegisterTalker(lobbyUser_t* user, int lobbyType, bool isLocal);
  void UnregisterTalker(lobbyUser_t* user, int lobbyType, bool isLocal);
  void GetActiveLocalTalkers(std::vector<int>& localTalkers);
  void GetRecipientsForTalker(int talkerIndex, std::vector<const lobbyAddress_t*>& recipients);
  void SetActiveLobby(int lobbyType);
  int FindTalkerByUserId(lobbyUserID_t userId, int lobbyType) const;
  bool GetLocalChatData(int talkerIndex, std::uint8_t* data, int& dataSize);
  void SubmitIncomingChatData(const std::uint8_t* data, int dataSize);
  voiceState_t GetVoiceState(const lobbyUser_t* user);
  bool IsRestrictedByPrivleges() const;
  void ToggleMuteLocal(const lobbyUser_t* source, const lobbyUser_t* target);
  void SetHeadsetState(int talkerIndex, bool state);
  bool HasHeadsetStateChanged(int talkerIndex);
  int FindMachine(const lobbyAddress_t& address, int lobbyType) const;
  int AddMachine(const lobbyAddress_t& address, int lobbyType);
  void UpdateRegisteredTalkers();
  void SetDisableVoiceReason(disableVoiceReason_t reason);
  void ClearDisableVoiceReason(disableVoiceReason_t reason);

protected:
  struct remoteMachine_t {
    int lobbyType{-1};
    lobbyAddress_t address{};
    int refCount{};
    int sendFrame{-1};
  };
  struct talker_t {
    lobbyUser_t* user{};
    bool isLocal{};
    int lobbyType{-1};
    int groupIndex{-1};
    bool registered{};
    bool registeredSuccess{};
    int machineIndex{-1};
    bool isMuted{};
    bool hasHeadset{true};
    bool hasHeadsetChanged{};
    bool talking{};
    bool talkingGlobal{};
    int talkingTime{};
  };

  int FindTalkerIndex(const lobbyUser_t* user, int lobbyType) const;
  void RemoveMachine(int machineIndex, int lobbyType);
  bool CanSendVoiceTo(int fromIndex, int toIndex) const;

  std::vector<talker_t> talkers;
  std::vector<remoteMachine_t> remoteMachines;
  int activeLobbyType{-1};
  int activeGroupIndex{};
  int sendFrame{};
  std::uint32_t disableVoiceReasons{};
  bool sendGlobal{};
};

