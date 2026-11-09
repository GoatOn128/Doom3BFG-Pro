#include "engine/gamelib/animstack/animweb/animwebeventhandler.h"
#include "engine/gamelib/animstack/animweb/animator_animweb_abi.h"

#include "engine/framework/common.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/typeinfo/typeinfo.h"

#include <cstddef>

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:?GetStateName@idDeclAnimWeb@@QEBAPEBDV?$idIndex@"         \
    "FW4invalidStateIndex_t@idDeclAnimWeb@@@@@Z="                             \
    "?GetStateName@idDeclAnimWeb@@QEBAPEBDU?$animWebIndex_t@"                 \
    "UanimWebStateTag@@@@@Z")
#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:?GetSubWebName@idDeclAnimWeb@@QEBAPEBDV?$idIndex@"        \
    "FW4invalidSubWebIndex_t@idDeclAnimWeb@@@@@Z="                            \
    "?GetSubWebName@idDeclAnimWeb@@QEBAPEBDU?$animWebIndex_t@"                \
    "UanimWebSubWebTag@@@@@Z")

idCVar animweb_debugEventHandler(
    "animweb_debugEventHandler", "0", CVAR_BOOL,
    "1 = show debug info for he animweb event handler",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

template <typename Function, typename Object>
Function VirtualFunction(Object* object, std::size_t byteOffset) {
  auto** const vtable = *reinterpret_cast<void***>(object);
  return reinterpret_cast<Function>(vtable[byteOffset / sizeof(void*)]);
}

idGame* CommonGame() {
  using Function = idGame* (*)(idCommon*);
  return VirtualFunction<Function>(common, 0x1F8)(common);
}

idTypeInfoTools* GameTypeInfoTools(idGame* game) {
  using Function = idTypeInfoTools* (*)(idGame*);
  return VirtualFunction<Function>(game, 0x2E8)(game);
}

} // namespace

idAnimWebEventHandler_Zion::idAnimWebEventHandler_Zion() {
  for (int index = 0; index < 9; ++index) {
    events[index].swi.value = -1;
    events[index].si.value = -1;
    events[index].eventNum = -1;
    events[index].priority = idAnimWebEvent::PRIORITY_LOW;
  }
}

void idAnimWebEventHandler_Zion::ClearEvent(animWebEvent_t eventType) {
  events[eventType].eventNum = -1;
}

void idAnimWebEventHandler_Zion::ClearEvents() {
  for (int index = 0; index < 9; ++index) {
    events[index].eventNum = -1;
  }
}

void idAnimWebEventHandler_Zion::SetEvent(
    const idAnimator_AnimWeb& web, idAnimWebSubWebIndex subWebIndex,
    idAnimWebStateIndex stateIndex, animWebEvent_t eventType, int eventNum,
    idAnimWebEvent::priority_t priority) {
  idAnimWebEvent& event = events[eventType];
  if (event.priority > priority) {
    if (animweb_debugEventHandler.valueInteger != 0) {
      idGame* const priorityGame = CommonGame();
      idGame* const eventGame = CommonGame();
      const idDeclAnimWeb* const declaration = web.decl;
      const char* const priorityName =
          GameTypeInfoTools(priorityGame)->GetEnumName(
              "idAnimWebEvent::priority_t", priority, "<unknown>");
      const char* const eventName =
          GameTypeInfoTools(eventGame)->GetEnumName(
              "animWebEvent_t", eventType, "<unknown>");
      const char* const stateName = declaration->GetStateName(stateIndex);
      const char* const subWebName = declaration->GetSubWebName(subWebIndex);
      idLib::Printf(
          "Ignored anim web web event '%s/%s', type %s, priority %s.\n",
          subWebName, stateName, eventName, priorityName);
    }
    return;
  }

  event.swi = subWebIndex;
  event.si = stateIndex;
  event.eventNum = eventNum;
  event.priority = priority;
  if (animweb_debugEventHandler.valueInteger != 0) {
    idGame* const priorityGame = CommonGame();
    idGame* const eventGame = CommonGame();
    const idDeclAnimWeb* const declaration = web.decl;
    const char* const priorityName =
        GameTypeInfoTools(priorityGame)->GetEnumName(
            "idAnimWebEvent::priority_t", priority, "<unknown>");
    const char* const eventName =
        GameTypeInfoTools(eventGame)->GetEnumName(
            "animWebEvent_t", eventType, "<unknown>");
    const char* const stateName = declaration->GetStateName(stateIndex);
    const char* const subWebName = declaration->GetSubWebName(subWebIndex);
    idLib::Printf(
        "Added anim web web event '%s/%s', type %s, priority %s.\n",
        subWebName, stateName, eventName, priorityName);
  }
}

void idAnimWebEventHandler_Zion::TriggerEventType(
    idAnimator_AnimWeb& web, const idDeclAnimWebNode* node,
    animWebEvent_t eventType, int eventParm) {
  if (node == nullptr) {
    return;
  }

  idAnimWebEvent& event = events[eventType];
  if (event.eventNum >= 0 &&
      node->subWebIndex.value == event.swi.value &&
      node->states.stateIndex[0].value == event.si.value) {
    web.TriggerEvent(node, eventType, event, eventParm);
    if (animweb_debugEventHandler.valueInteger != 0) {
      idGame* const game = CommonGame();
      const idAnimWebStateIndex stateIndex{event.si.value};
      const idDeclAnimWeb* const declaration = web.decl;
      const idAnimWebSubWebIndex subWebIndex{event.swi.value};
      const char* const eventName =
          GameTypeInfoTools(game)->GetEnumName(
              "animWebEvent_t", eventType, "<unknown>");
      const char* const stateName = declaration->GetStateName(stateIndex);
      const char* const subWebName = declaration->GetSubWebName(subWebIndex);
      idLib::Printf(
          "Triggered anim web web event '%s/%s', type %s.\n",
          subWebName, stateName, eventName);
    }
    event.eventNum = -1;
  }
}

bool idAnimWebEventHandler_Zion::IsEventPending(
    animWebEvent_t eventType) const {
  return events[eventType].eventNum >= 0;
}

bool idAnimWebEventHandler_Zion::IsAnyEventPending() const {
  for (int index = 0; index < 9; ++index) {
    if (events[index].eventNum >= 0) {
      return true;
    }
  }
  return false;
}
