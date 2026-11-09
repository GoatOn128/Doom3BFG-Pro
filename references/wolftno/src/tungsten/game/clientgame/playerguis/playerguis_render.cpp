#include "tungsten/game/clientgame/playerguis/playerguis_render.h"

#include <algorithm>

// DIA: idPlayerGuis_Render::Cleanup
void idPlayerGuis_Render::Cleanup() {
  initialized = false; chatting = false; map.clear(); inputBuffer.clear();
  messages.clear(); visibleMessages.clear(); bindings.clear();
}

// DIA: idPlayerGuis_Render::NoteSwfForBuildGame
void idPlayerGuis_Render::NoteSwfForBuildGame(const std::string& swfName) {
  if (!swfName.empty() && std::find(buildSwfs.begin(), buildSwfs.end(), swfName) == buildSwfs.end())
    buildSwfs.push_back(swfName);
}

// DIA: idPlayerGuis_Render::HandleGuiEvent
bool idPlayerGuis_Render::HandleGuiEvent(const sysEvent_t& event) {
  if (!initialized) return false;
  if (event.type == sysEvent_t::Type::key && event.pressed && event.value == 13) {
    if (chatting) SendChatText(inputBuffer, chatEntryMode == "team");
    return true;
  }
  if (event.type == sysEvent_t::Type::key && event.pressed && event.value == 27) {
    CancelChatting(); return true;
  }
  if (chatting && event.type == sysEvent_t::Type::character && event.value >= 32) {
    inputBuffer.push_back(static_cast<char>(event.value)); return true;
  }
  return false;
}

// DIA: idPlayerGuis_Render::IsPlayerControlInhibited
bool idPlayerGuis_Render::IsPlayerControlInhibited() const { return chatting; }
// DIA: idPlayerGuis_Render::SetPlayerNum
void idPlayerGuis_Render::SetPlayerNum(int playerNumber) { playerNum = playerNumber; }

// DIA: `idPlayerGuis_Render::Init'::`15'::idSWFScriptFunction_SendChatText::Call
void idPlayerGuis_Render::SendChatText(const std::string& text, bool team) {
  if (!text.empty()) sentMessages.push_back((team ? "team:" : "all:") + text);
  chatting = false; inputBuffer.clear(); chatEntryMode.clear();
}

// DIA: idPlayerGuis_Render::RenderGame
void idPlayerGuis_Render::RenderGame(int currentTime) {
  (void)currentTime; if (initialized) ++renderCount;
}

// DIA: idPlayerGuis_Render::HandleReliableGuiMsg
void idPlayerGuis_Render::HandleReliableGuiMsg(int type, const idReliableGuiMsg& message) {
  (void)type; DisplayTextChatMessage((message.team ? "[TEAM] " : "") + message.text);
}

// DIA: idPlayerGuis_Render::SetupBindings
void idPlayerGuis_Render::SetupBindings() {
  bindings = {{"cancelChat", "ESCAPE"}, {"sendChat", "ENTER"},
              {"chat", "T"}, {"teamChat", "Y"}, {"scoreboard", "TAB"}};
}
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$0
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$1
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$2
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$3
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$4
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$5
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$6
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$7
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$8
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$9
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$10
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$11
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$12
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$13
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$14
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$15
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$16
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$17
// DIA: `idPlayerGuis_Render::SetupBindings'::`1'::dtor$18

// DIA: idPlayerGuis_Render::~idPlayerGuis_Render
idPlayerGuis_Render::~idPlayerGuis_Render() { Cleanup(); }
// DIA: `idPlayerGuis_Render::~idPlayerGuis_Render'::`1'::dtor$0

// DIA: idPlayerGuis_Render::Render_TextChat
void idPlayerGuis_Render::Render_TextChat(int currentTime) {
  while (!messages.empty() && messages.front().expiresAt <= currentTime) messages.pop_front();
  visibleMessages.clear(); for (const ChatMessage& message : messages) visibleMessages.push_back(message.text);
}

// DIA: idPlayerGuis_Render::idPlayerGuis_Render
idPlayerGuis_Render::idPlayerGuis_Render() = default;

// DIA: idPlayerGuis_Render::Init
void idPlayerGuis_Render::Init(const std::string& mapName) {
  Cleanup(); map = mapName; initialized = true; SetupBindings();
  NoteSwfForBuildGame("swf/playerhud.swf"); NoteSwfForBuildGame("swf/textchat.swf");
}
// DIA: `idPlayerGuis_Render::Init'::`1'::dtor$0
// DIA: `idPlayerGuis_Render::Init'::`1'::dtor$1
// DIA: `idPlayerGuis_Render::Init'::`1'::dtor$2

// DIA: idPlayerGuis_Render::Render
void idPlayerGuis_Render::Render(int currentTime) { RenderGame(currentTime); Render_TextChat(currentTime); }

// DIA: idSWFScriptFunction_CancelChatting::Call
void idSWFScriptFunction_CancelChatting::Call() { gui.CancelChatting(); }
// DIA: `idSWFScriptFunction_CancelChatting::Call'::`1'::dtor$0
// DIA: `idSWFScriptFunction_CancelChatting::Call'::`1'::dtor$5
// DIA: `idSWFScriptFunction_CancelChatting::Call'::`1'::dtor$1
// DIA: `idSWFScriptFunction_CancelChatting::Call'::`1'::dtor$7
// DIA: `idSWFScriptFunction_CancelChatting::Call'::`1'::dtor$9
// DIA: `idSWFScriptFunction_CancelChatting::Call'::`1'::dtor$3

// DIA: idPlayerGuis_Render::DisplayTextChatEntry
void idPlayerGuis_Render::DisplayTextChatEntry() { chatting = true; chatEntryMode = "all"; inputBuffer.clear(); }
// DIA: `idPlayerGuis_Render::DisplayTextChatEntry'::`1'::dtor$5
// DIA: `idPlayerGuis_Render::DisplayTextChatEntry'::`1'::dtor$0
// DIA: `idPlayerGuis_Render::DisplayTextChatEntry'::`1'::dtor$8
// DIA: `idPlayerGuis_Render::DisplayTextChatEntry'::`1'::dtor$1
// DIA: `idPlayerGuis_Render::DisplayTextChatEntry'::`1'::dtor$2
// DIA: `idPlayerGuis_Render::DisplayTextChatEntry'::`1'::dtor$3

// DIA: idPlayerGuis_Render::DisplayTeamTextChatEntry
void idPlayerGuis_Render::DisplayTeamTextChatEntry() { chatting = true; chatEntryMode = "team"; inputBuffer.clear(); }

// DIA: idPlayerGuis_Render::DisplayTextChatMessage
void idPlayerGuis_Render::DisplayTextChatMessage(const std::string& message) {
  const int now = gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
  messages.push_back({message, now + 8000}); while (messages.size() > 8) messages.pop_front();
}
// DIA: `idPlayerGuis_Render::DisplayTextChatMessage'::`1'::dtor$3
// DIA: `idPlayerGuis_Render::DisplayTextChatMessage'::`1'::dtor$0
// DIA: `idPlayerGuis_Render::DisplayTextChatMessage'::`1'::dtor$5
// DIA: `idPlayerGuis_Render::DisplayTextChatMessage'::`1'::dtor$6
// DIA: `idPlayerGuis_Render::DisplayTextChatMessage'::`1'::dtor$8

void idPlayerGuis_Render::CancelChatting() { chatting = false; inputBuffer.clear(); chatEntryMode.clear(); }
