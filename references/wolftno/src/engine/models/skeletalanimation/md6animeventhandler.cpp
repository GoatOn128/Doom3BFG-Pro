#include "engine/models/skeletalanimation/md6animeventhandler.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <new>

extern idAnimEventFindCallback md6AnimEventFindCallback;
extern idAnimEventCommandCallback md6AnimEventCommandCallback;
extern idAnimEventClientCallback md6AnimEventClientCallback;

idCVar anim_debugEvents(
    "anim_debugEvents", "0", CVAR_INTEGER,
    "1 = show triggered events; 2 = also show skipped events; "
    "3 = also show tracking information; 4 = verbose spew");
idCVar anim_debugEvents_Name(
    "anim_debugEvents_Name", "", CVAR_ALL,
    "optional animation to debug animevents for");

namespace {

int LeafFrame(const idMD6LeafPlay* play, int gameTime, int ticksPerSec) {
  if (play == nullptr || play->anim == nullptr || ticksPerSec == 0)
    return 0;
  const idMD6AnimData* data = play->anim->animData;
  const unsigned numFrames = data != nullptr ? data->numFrames : 0;
  const int frameRate = data != nullptr ? data->frameRate : 30;
  const int frameTime = gameTime >= play->startTime
                            ? static_cast<int>(
                                  static_cast<float>(gameTime - play->startTime) *
                                  play->rateScale)
                            : 0;
  unsigned frame = static_cast<unsigned>(frameTime * frameRate / ticksPerSec);
  if (play->wrapMode != 0) {
    if (numFrames > 1)
      frame %= numFrames - 1;
  } else if (frame >= numFrames) {
    frame = numFrames > 0 ? numFrames - 1 : 0;
  }
  return static_cast<int>(frame);
}

int LeafLoopCount(const idMD6LeafPlay* play, int gameTime, int ticksPerSec) {
  if (play == nullptr || play->anim == nullptr || play->wrapMode == 0 ||
      ticksPerSec == 0)
    return 0;
  const idMD6AnimData* data = play->anim->animData;
  const unsigned numFrames = data != nullptr ? data->numFrames : 0;
  if (numFrames <= 1)
    return 0;
  const int frameRate = data != nullptr ? data->frameRate : 30;
  const int frameTime = gameTime >= play->startTime
                            ? static_cast<int>(
                                  static_cast<float>(gameTime - play->startTime) *
                                  play->rateScale)
                            : 0;
  return frameTime * frameRate / ticksPerSec /
         static_cast<int>(numFrames - 1);
}

bool IsClient() {
  return md6AnimEventClientCallback != nullptr &&
         md6AnimEventClientCallback();
}

const idAnimEventCommandInfo* EventCommand(int eventNumber) {
  return md6AnimEventCommandCallback != nullptr
             ? md6AnimEventCommandCallback(eventNumber)
             : nullptr;
}

bool DebugMatches(const idMD6Anim* anim) {
  const char* filter = anim_debugEvents_Name.GetString();
  if (filter == nullptr || *filter == '\0')
    return true;
  const char* name = anim != nullptr ? anim->name.str : "";
  return idStr::Find(name, filter, true, 0, -1) != -1;
}

} // namespace

template <typename T>
__forceinline void* idListArrayNew(int num, memTag_t tag, bool zeroBuffer) {
  if (num <= 0)
    return nullptr;
  auto* values = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(num), tag, zeroBuffer, ALIGN_16,
      HEAP_DEFAULTHEAP));
  if (values != nullptr) {
    for (int index = 0; index < num; ++index)
      ::new (static_cast<void*>(&values[index])) T;
  }
  return values;
}

template <typename T>
__forceinline void* idListArrayResize(void* oldPointer, int oldNum, int newNum,
                                     memTag_t tag, bool zeroBuffer) {
  auto* replacement = static_cast<T*>(
      newNum > 0 ? idListArrayNew<T>(newNum, tag, zeroBuffer) : nullptr);
  auto* oldValues = static_cast<T*>(oldPointer);
  if (replacement != nullptr) {
    const int copyCount = std::min(oldNum, newNum);
    for (int index = 0; index < copyCount; ++index)
      replacement[index] = oldValues[index];
  }
  idMem::Singleton()->Free(oldPointer, ALIGN_16);
  return replacement;
}

idAnimEventHandler::idAnimEventHandler() {
  trackedEvents.granularity = MAX_TRACKED_EVENTS;
  triggeredEvents.granularity = MAX_TRACKED_EVENTS;
  nextEventToFetch = 0;
}

bool idAnimEventHandler::FetchNextEvent(const idMD6Anim*& animHandle,
                                        const idMD6AnimEvent*& event) {
  if (nextEventToFetch < triggeredEvents.num) {
    animHandle = triggeredEvents.list[nextEventToFetch].anim;
    event = triggeredEvents.list[nextEventToFetch++].event;
    return true;
  }
  event = nullptr;
  return false;
}

void idAnimEventHandler::OffsetTimes(
    idTypesafeNumber<int, gameTimeUnique_t> timeOffset) {
  for (int index = 0; index < trackedEvents.num; ++index)
    trackedEvents.list[index].startTime += timeOffset.GetValue();
}

void idAnimEventHandler::ClearTrackedEvents() { trackedEvents.num = 0; }

void idAnimEventHandler::AddPlayLeafToFrameList(
    idStaticList<eventInfo_t, 64>& frameList, const idMD6LeafPlay* play,
    int curTime, int previousTime, int ticksPerSec, bool canSkip) {
  eventInfo_t* info = frameList.Alloc();
  if (info == nullptr)
    return;
  info->anim = play->anim;
  info->leaf = play;
  info->startTime = play->startTime;
  info->initCounter = play->initCounter;
  info->lastFrame = static_cast<std::uint16_t>(
      LeafFrame(play, previousTime, ticksPerSec));
  info->curFrame =
      static_cast<std::uint16_t>(LeafFrame(play, curTime, ticksPerSec));
  info->loopCount = static_cast<std::int16_t>(
      LeafLoopCount(play, curTime, ticksPerSec));
  info->canSkip = canSkip;
}

void idAnimEventHandler::WalkTree(idStaticList<eventInfo_t, 64>& frameList,
                                  const idMD6Node* node, int curTime,
                                  int previousTime, int ticksPerSec) {
  if (frameList.num >= MAX_FRAME_INFOS || node == nullptr)
    return;
  idArray<const idMD6Node*, 32> stack{};
  idArray<bool, 32> canSkipStack{};
  int stackIndex = 0;
  stack.ptr[0] = node;
  canSkipStack.ptr[0] = false;
  while (stackIndex >= 0) {
    const idMD6Node* current = stack.ptr[stackIndex];
    const bool canSkip = canSkipStack.ptr[stackIndex--];
    switch (current->type) {
      case idMD6Node::NODE_LEAF_PLAY:
        if (frameList.num >= frameList.size) {
          idLib::Error("more than %d anim events", frameList.size);
        }
        AddPlayLeafToFrameList(frameList,
                               static_cast<const idMD6LeafPlay*>(current),
                               curTime, previousTime, ticksPerSec, canSkip);
        break;
      case idMD6Node::NODE_BEST_LEAF: {
        const auto* best = static_cast<const idMD6BestLeaf*>(current);
        if (best->leafList.num > 0) {
          const int selected = best->bestLeafIndex < best->leafList.num
                                   ? best->bestLeafIndex
                                   : 0;
          const idMD6Node* child = best->leafList.list[selected];
          if (child != nullptr && stackIndex + 1 < 32) {
            stack.ptr[++stackIndex] = child;
            canSkipStack.ptr[stackIndex] = canSkip;
          }
        }
        break;
      }
      case idMD6Node::NODE_BRANCH:
      case idMD6Node::NODE_BLEND_BRANCH:
      case idMD6Node::NODE_BLENDA_BRANCH:
      case idMD6Node::NODE_FUSION_BRANCH: {
        const auto* branch = static_cast<const idMD6Branch*>(current);
        bool leftCanSkip = canSkip;
        bool rightCanSkip = canSkip;
        if (branch->filterGroup == 0 &&
            (branch->op < 3 || branch->op > 6)) {
          if (branch->currentAlpha > 0.5f)
            leftCanSkip = canSkip || branch->right != nullptr;
          else
            rightCanSkip = canSkip || branch->left != nullptr;
        }
        if (branch->left != nullptr && stackIndex + 1 < 32) {
          stack.ptr[++stackIndex] = branch->left;
          canSkipStack.ptr[stackIndex] = leftCanSkip;
        }
        if (branch->right != nullptr && stackIndex + 1 < 32) {
          stack.ptr[++stackIndex] = branch->right;
          canSkipStack.ptr[stackIndex] = rightCanSkip;
        }
        break;
      }
      default:
        break;
    }
  }
}

void idAnimEventHandler::BuildEventList(
    idList<triggeredEvent_t, 5>& eventList,
    const idStaticList<eventInfo_t, 64>& frameList, const idDeclMD6* decl,
    int curTime, int previousTime, int ticksPerSec,
    const int* const onlyEvents, int onlyNumEvents) {
  (void)previousTime;
  eventList.num = 0;
  const int debug = anim_debugEvents.GetInteger();
  for (int frameIndex = 0; frameIndex < frameList.num; ++frameIndex) {
    const eventInfo_t& info = frameList.list[frameIndex];
    idStaticList<const idMD6AnimEvent*, 16> validEvents;
    const bool found = md6AnimEventFindCallback != nullptr &&
                       md6AnimEventFindCallback(
                           decl, info.anim, info.lastFrame, info.curFrame,
                           validEvents);
    if (!found && debug > 3 && DebugMatches(info.anim)) {
      idLib::Printf(
          "AnimEvent(%d)(no anim events found): curFrame %d, lastFrame %d, "
          "loop %d, leaf %p, anim '%s'\n",
          curTime, info.curFrame, info.lastFrame, info.loopCount, info.leaf,
          info.anim != nullptr ? info.anim->name.str : "NULL");
    }
    for (int eventIndex = 0; eventIndex < validEvents.num; ++eventIndex) {
      const idMD6AnimEvent* event = validEvents.list[eventIndex];
      if (event == nullptr)
        continue;
      const int loopCount =
          info.lastFrame <= info.curFrame || event->frameNum <= info.curFrame
              ? info.loopCount
              : info.loopCount - 1;
      bool skip = onlyNumEvents > 0;
      for (int allowed = 0; skip && allowed < onlyNumEvents; ++allowed)
        skip = event->eventNum != onlyEvents[allowed];
      const idAnimEventCommandInfo* command = EventCommand(event->eventNum);
      if ((command == nullptr || !command->isClientSafe) && IsClient())
        skip = true;
      if ((command != nullptr && command->canSkip && info.canSkip) || skip)
        continue;
      bool alreadyTracked = false;
      for (int trackedIndex = trackedEvents.num - 1; trackedIndex >= 0;
           --trackedIndex) {
        const trackedEvent_t& tracked = trackedEvents.list[trackedIndex];
        if (tracked.eventId.value == event->eventId.value &&
            tracked.loopCount == loopCount &&
            tracked.initCounter == info.initCounter) {
          alreadyTracked = true;
          break;
        }
      }
      if (alreadyTracked) {
        if (debug > 1 && DebugMatches(info.anim)) {
          idLib::Printf(
              "AnimEvent(%d)(skipping): '%s', curFrame %d, lastFrame %d, "
              "loop %d, leaf %p, anim '%s'\n",
              curTime,
              command != nullptr && command->name != nullptr
                  ? command->name
                  : "<NULL>",
              info.curFrame, info.lastFrame, loopCount, info.leaf,
              info.anim != nullptr ? info.anim->name.str : "NULL");
        }
        continue;
      }
      if (eventList.num >= eventList.granularity) {
        idLib::Error("more than %d triggered events",
                     eventList.granularity);
      }
      triggeredEvent_t* triggered = eventList.Alloc();
      if (triggered == nullptr)
        break;
      triggered->startTime = info.startTime;
      triggered->eventNum = event->eventNum;
      triggered->initCounter = info.initCounter;
      triggered->eventId = event->eventId;
      triggered->anim = info.anim;
      triggered->eventFrame = event->frameNum;
      triggered->loopCount = static_cast<std::int16_t>(loopCount);
      triggered->event = event;
      if (debug > 0 && DebugMatches(info.anim)) {
        idLib::Printf("AnimEvent(%d)(triggering): '%s', anim '%s'\n",
                      curTime,
                      command != nullptr && command->name != nullptr
                          ? command->name
                          : "<NULL>",
                      info.anim != nullptr ? info.anim->name.str : "NULL");
      }
    }
  }

  for (int trackedIndex = trackedEvents.num - 1; trackedIndex >= 0;
       --trackedIndex) {
    const trackedEvent_t& tracked = trackedEvents.list[trackedIndex];
    bool keep = false;
    for (int frameIndex = 0; frameIndex < frameList.num; ++frameIndex) {
      const eventInfo_t& info = frameList.list[frameIndex];
      if (tracked.anim == info.anim && tracked.loopCount == info.loopCount &&
          tracked.initCounter == info.initCounter) {
        int frame = -1;
        if (info.leaf != nullptr &&
            info.leaf->type == idMD6Node::NODE_LEAF_PLAY) {
          frame = LeafFrame(static_cast<const idMD6LeafPlay*>(info.leaf),
                            curTime, ticksPerSec);
        }
        if (frame >= tracked.eventFrame) {
          keep = true;
          break;
        }
      }
    }
    if (!keep) {
      if (debug > 2 && DebugMatches(tracked.anim)) {
        const idAnimEventCommandInfo* command = EventCommand(tracked.eventNum);
        idLib::Printf(
            "AnimEvent(%d)(untracking): '%s', eventFrame %d, loop %d, "
            "anim = '%s'\n",
            curTime,
            command != nullptr && command->name != nullptr ? command->name
                                                            : "<NULL>",
            tracked.eventFrame, tracked.loopCount,
            tracked.anim != nullptr ? tracked.anim->name.str : "NULL");
      }
      trackedEvents.RemoveIndexFast(trackedIndex);
    }
  }

  if (eventList.num + trackedEvents.num >= trackedEvents.granularity) {
    for (int eventIndex = 0; eventIndex < eventList.num; ++eventIndex) {
      int staleIndex = -1;
      for (int trackedIndex = 0; trackedIndex < trackedEvents.num;
           ++trackedIndex) {
        const trackedEvent_t& tracked = trackedEvents.list[trackedIndex];
        if (tracked.anim == nullptr) {
          staleIndex = trackedIndex;
          break;
        }
        const idMD6AnimData* data = tracked.anim->animData;
        const int numFrames = data != nullptr ? data->numFrames : 0;
        const int frameRate = data != nullptr ? data->frameRate : 30;
        int loopTime = 0;
        if (tracked.loopCount > 0 && frameRate != 0)
          loopTime = ticksPerSec * tracked.loopCount * (numFrames - 1) /
                     frameRate;
        const int elapsed = curTime - tracked.startTime - loopTime;
        const int frame = ticksPerSec != 0
                              ? elapsed * frameRate / ticksPerSec
                              : 0;
        if (tracked.eventFrame < frame - 1) {
          staleIndex = trackedIndex;
          break;
        }
      }
      if (staleIndex >= 0)
        trackedEvents.RemoveIndexFast(staleIndex);
    }
  }

  for (int eventIndex = 0; eventIndex < eventList.num; ++eventIndex) {
    if (trackedEvents.num >= trackedEvents.granularity) {
      idLib::Warning("Tracked anim events overflow for md6Def '%s'.",
                     decl != nullptr ? decl->name.str : "<NULL>");
      break;
    }
    trackedEvents.Append(eventList.list[eventIndex]);
    if (debug > 2) {
      const trackedEvent_t& tracked =
          trackedEvents.list[trackedEvents.num - 1];
      if (DebugMatches(tracked.anim)) {
        const idAnimEventCommandInfo* command = EventCommand(tracked.eventNum);
        idLib::Printf(
            "AnimEvent(%d)(tracking): '%s', eventFrame %d, loop %d, "
            "anim = '%s'\n",
            curTime,
            command != nullptr && command->name != nullptr ? command->name
                                                            : "<NULL>",
            tracked.eventFrame, tracked.loopCount,
            tracked.anim != nullptr ? tracked.anim->name.str : "NULL");
      }
    }
  }
}

void idAnimEventHandler::BuildAnimEventList(
    const idDeclMD6* decl, const idMD6Node* tree, int curTime,
    int previousTime, int ticksPerSec, const int* const onlyEvents,
    int onlyNumEvents) {
  idStaticList<eventInfo_t, 64> frameList;
  WalkTree(frameList, tree, curTime, previousTime, ticksPerSec);
  BuildEventList(triggeredEvents, frameList, decl, curTime, previousTime,
                 ticksPerSec, onlyEvents, onlyNumEvents);
  nextEventToFetch = 0;
}

template void* idListArrayNew<idAnimEventHandler::eventInfo_t>(
    int, memTag_t, bool);
template void* idListArrayResize<idAnimEventHandler::eventInfo_t>(
    void*, int, int, memTag_t, bool);
template bool idList<idAnimEventHandler::eventInfo_t, 5>::Resize(int);
template bool idList<idAnimEventHandler::trackedEvent_t, 5>::Resize(int);
template bool idList<idAnimEventHandler::triggeredEvent_t, 5>::Resize(int);
template int idList<idAnimEventHandler::trackedEvent_t, 5>::Append(
    const idAnimEventHandler::trackedEvent_t&);
template idAnimEventHandler::eventInfo_t*
idList<idAnimEventHandler::eventInfo_t, 5>::Alloc();
