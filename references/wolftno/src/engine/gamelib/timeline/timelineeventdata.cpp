#include "engine/gamelib/timeline/timelineeventdata.h"

tlEventData_t::tlEventData_t()
    : time(0.0f), duration(0.0f), type(TL_EVENT_NONE), anim(),
      useAnimPos(false), useAnimScale(false), sound(nullptr),
      worldPos(vec3_origin), worldRot(ang_zero), keyName(TL_KEY_INVALID),
      keyValue(), keyAnimData(), activateStart(true), activateEnd(false),
      env(nullptr), lightEntDefText() {}
