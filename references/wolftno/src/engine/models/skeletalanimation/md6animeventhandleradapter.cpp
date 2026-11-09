#include "engine/models/skeletalanimation/md6animeventhandler.h"

idAnimEventFindCallback md6AnimEventFindCallback = nullptr;
idAnimEventCommandCallback md6AnimEventCommandCallback = nullptr;
idAnimEventClientCallback md6AnimEventClientCallback = nullptr;

void SetAnimEventHandlerCallbacks(idAnimEventFindCallback findEvents,
                                  idAnimEventCommandCallback findCommand,
                                  idAnimEventClientCallback isClient) {
  md6AnimEventFindCallback = findEvents;
  md6AnimEventCommandCallback = findCommand;
  md6AnimEventClientCallback = isClient;
}

