#pragma once

#include "tungsten/game/game_runtime.h"

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

struct sysEvent_t {
  enum class Type { none, key, character } type{Type::none};
  int value{0};
  bool pressed{false};
};

struct idReliableGuiMsg {
  std::string text;
  bool team{false};
};

class idPlayerGuis_Render {
public:
  idPlayerGuis_Render();
  ~idPlayerGuis_Render();
  void Cleanup();
  void NoteSwfForBuildGame(const std::string& swfName);
  bool HandleGuiEvent(const sysEvent_t& event);
  bool IsPlayerControlInhibited() const;
  void SetPlayerNum(int playerNumber);
  void RenderGame(int currentTime);
  void HandleReliableGuiMsg(int type, const idReliableGuiMsg& message);
  void SetupBindings();
  void Render_TextChat(int currentTime);
  void Init(const std::string& mapName);
  void Render(int currentTime);
  void DisplayTextChatEntry();
  void DisplayTeamTextChatEntry();
  void DisplayTextChatMessage(const std::string& message);
  void SendChatText(const std::string& text, bool team);
  void CancelChatting();

  bool IsInitialized() const { return initialized; }
  bool IsChatting() const { return chatting; }
  int GetPlayerNum() const { return playerNum; }
  int GetRenderCount() const { return renderCount; }
  const std::string& GetChatEntryMode() const { return chatEntryMode; }
  const std::deque<std::string>& GetVisibleMessages() const { return visibleMessages; }
  const std::vector<std::string>& GetSentMessages() const { return sentMessages; }
  const std::vector<std::string>& GetBuildSwfs() const { return buildSwfs; }
  const std::unordered_map<std::string, std::string>& GetBindings() const { return bindings; }

private:
  struct ChatMessage { std::string text; int expiresAt{0}; };
  std::string map;
  std::string chatEntryMode;
  std::string inputBuffer;
  std::vector<std::string> buildSwfs;
  std::vector<std::string> sentMessages;
  std::deque<ChatMessage> messages;
  std::deque<std::string> visibleMessages;
  std::unordered_map<std::string, std::string> bindings;
  int playerNum{-1};
  int renderCount{0};
  bool initialized{false};
  bool chatting{false};
};

class idSWFScriptFunction_CancelChatting {
public:
  explicit idSWFScriptFunction_CancelChatting(idPlayerGuis_Render& owner)
      : gui(owner) {}
  void Call();
private:
  idPlayerGuis_Render& gui;
};
