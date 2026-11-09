#pragma once

#include "engine/decls/animwebs/declanimweb.h"

#include <cstddef>
#include <cstdint>

class idCVar;
class idAnimator_AnimWeb;

enum animWebEvent_t : int {
  ANIMWEB_EVENT_START_BLEND_IN = 0,
  ANIMWEB_EVENT_START_BLEND_OUT = 1,
  ANIMWEB_EVENT_END_BLEND_IN = 2,
  ANIMWEB_EVENT_END_BLEND_OUT = 3,
  ANIMWEB_EVENT_DEST_START_BLEND_IN = 4,
  ANIMWEB_EVENT_DEST_END_BLEND_IN = 5,
  ANIMWEB_EVENT_DEST_CLAMPED = 6,
  ANIMWEB_EVENT_7 = 7,
  ANIMWEB_EVENT_8 = 8,
  ANIMWEB_EVENT_NONE = -1,
};

using idAnimWebSubWebIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidSubWebIndex_t>;
using idAnimWebStateIndex =
    idIndex<std::int16_t, idDeclAnimWeb::invalidStateIndex_t>;

class idAnimWebEvent {
public:
  enum priority_t : int {
    PRIORITY_LOW = 0,
    PRIORITY_MISC = PRIORITY_LOW,
    PRIORITY_DEATH = 1,
    PRIORITY_MAX = 2,
    PRIORITY_NORMAL = PRIORITY_DEATH,
    PRIORITY_HIGH = PRIORITY_MAX,
  };

  idAnimWebSubWebIndex swi;
  idAnimWebStateIndex si;
  int eventNum;
  priority_t priority;
};

class idAnimWebEventHandler_Zion {
public:
  idAnimWebEventHandler_Zion();

  virtual void ClearEvent(animWebEvent_t eventType);
  virtual void ClearEvents();
  virtual void SetEvent(const idAnimator_AnimWeb& web,
                        idAnimWebSubWebIndex subWebIndex,
                        idAnimWebStateIndex stateIndex,
                        animWebEvent_t eventType, int eventNum,
                        idAnimWebEvent::priority_t priority);
  virtual void TriggerEventType(idAnimator_AnimWeb& web,
                                const idDeclAnimWebNode* node,
                                animWebEvent_t eventType, int eventParm);
  virtual bool IsEventPending(animWebEvent_t eventType) const;
  virtual bool IsAnyEventPending() const;

  idAnimWebEvent events[9];
};

extern idCVar animweb_debugEventHandler;

static_assert(sizeof(idAnimWebSubWebIndex) == 2,
              "AnimWeb subweb index ABI drift");
static_assert(sizeof(idAnimWebStateIndex) == 2,
              "AnimWeb state index ABI drift");
static_assert(sizeof(idAnimWebEvent) == 0x0C,
              "AnimWeb event ABI drift");
static_assert(offsetof(idAnimWebEvent, eventNum) == 0x04,
              "AnimWeb event number offset drift");
static_assert(offsetof(idAnimWebEvent, priority) == 0x08,
              "AnimWeb event priority offset drift");
static_assert(offsetof(idAnimWebEventHandler_Zion, events) == 0x08,
              "AnimWeb event array offset drift");
static_assert(sizeof(idAnimWebEventHandler_Zion) == 0x78,
              "AnimWeb event handler ABI drift");
