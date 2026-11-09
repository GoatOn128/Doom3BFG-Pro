#pragma once

#include "engine/models/skeletalanimation/md6animevent.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

#ifndef ID_GAME_TIME_UNIQUE_T_DEFINED
#define ID_GAME_TIME_UNIQUE_T_DEFINED
enum gameTimeUnique_t {};
#endif

#ifndef ID_TYPESAFE_NUMBER_DEFINED
#define ID_TYPESAFE_NUMBER_DEFINED
template <typename T, typename UniqueTag> class idTypesafeNumber {
public:
  constexpr idTypesafeNumber() = default;
  constexpr idTypesafeNumber(T inputValue) : value(inputValue) {}
  constexpr idTypesafeNumber(const idTypesafeNumber& other)
      : value(other.value) {}
  constexpr T GetValue() const { return value; }

  T value{};
};
#endif

using idAnimEventFindCallback = bool (*)(
    const idDeclMD6* decl, const idMD6Anim* anim, int lastFrame,
    int currentFrame, idStaticList<const idMD6AnimEvent*, 16>& events);
using idAnimEventCommandCallback = const idAnimEventCommandInfo* (*)(
    int eventNumber);
using idAnimEventClientCallback = bool (*)();

void SetAnimEventHandlerCallbacks(idAnimEventFindCallback findEvents,
                                  idAnimEventCommandCallback findCommand,
                                  idAnimEventClientCallback isClient);

class idAnimEventHandler {
public:
  static constexpr int MAX_TRACKED_ANIMS = 32;
  static constexpr int MAX_TRACKED_EVENTS = 32;
  static constexpr int MAX_FRAME_INFOS = 64;

  struct trackedEvent_t {
    int startTime{};
    std::int16_t eventNum{-1};
    std::uint8_t initCounter{};
    std::uint8_t pad{};
    idAnimEventId eventId{};
    std::uint32_t eventIdPad{};
    const idMD6Anim* anim{};
    std::int16_t eventFrame{};
    std::int16_t loopCount{-1};
    std::uint32_t tailPad{};
  };

  struct triggeredEvent_t : trackedEvent_t {
    const idMD6AnimEvent* event{};
  };

  struct eventInfo_t {
    const idMD6Anim* anim{};
    const idMD6Leaf* leaf{};
    int startTime{-1};
    std::uint8_t initCounter{};
    std::uint8_t pad{};
    std::uint16_t lastFrame{};
    std::uint16_t curFrame{};
    std::int16_t loopCount{-1};
    bool canSkip{};
    std::uint8_t tailPad[3]{};
  };

  idAnimEventHandler();
  bool FetchNextEvent(const idMD6Anim*& animHandle,
                      const idMD6AnimEvent*& event);
  void OffsetTimes(idTypesafeNumber<int, gameTimeUnique_t> timeOffset);
  void ClearTrackedEvents();
  void BuildAnimEventList(const idDeclMD6* decl, const idMD6Node* tree,
                          int curTime, int previousTime, int ticksPerSec,
                          const int* const onlyEvents, int onlyNumEvents);

  idList<trackedEvent_t, 5> trackedEvents;
  idList<triggeredEvent_t, 5> triggeredEvents;
  int nextEventToFetch{};

private:
  void AddPlayLeafToFrameList(idStaticList<eventInfo_t, 64>& frameList,
                              const idMD6LeafPlay* play, int curTime,
                              int previousTime, int ticksPerSec,
                              bool canSkip);
  void WalkTree(idStaticList<eventInfo_t, 64>& frameList,
                const idMD6Node* node, int curTime, int previousTime,
                int ticksPerSec);
  void BuildEventList(idList<triggeredEvent_t, 5>& eventList,
                      const idStaticList<eventInfo_t, 64>& frameList,
                      const idDeclMD6* decl, int curTime, int previousTime,
                      int ticksPerSec, const int* const onlyEvents,
                      int onlyNumEvents);
};

static_assert(sizeof(idTypesafeNumber<int, gameTimeUnique_t>) == 4);
static_assert(sizeof(idAnimEventHandler::trackedEvent_t) == 0x20);
static_assert(offsetof(idAnimEventHandler::trackedEvent_t, eventId) == 0x08);
static_assert(offsetof(idAnimEventHandler::trackedEvent_t, anim) == 0x10);
static_assert(sizeof(idAnimEventHandler::triggeredEvent_t) == 0x28);
static_assert(sizeof(idAnimEventHandler::eventInfo_t) == 0x20);
static_assert(offsetof(idAnimEventHandler::eventInfo_t, startTime) == 0x10);
static_assert(offsetof(idAnimEventHandler::eventInfo_t, lastFrame) == 0x16);
static_assert(sizeof(idAnimEventHandler) == 0x38);
static_assert(offsetof(idAnimEventHandler, triggeredEvents) == 0x18);
static_assert(offsetof(idAnimEventHandler, nextEventToFetch) == 0x30);
