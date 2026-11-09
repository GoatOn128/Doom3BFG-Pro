#include "engine/models/skeletalanimation/md6animevent.h"

idMD6AnimEventLookupCallback md6AnimEventLookupCallback = nullptr;
idMD6AnimAliasCallback md6AnimAliasCallback = nullptr;

void SetMD6AnimEventParseCallbacks(
    idMD6AnimEventLookupCallback findEvent,
    idMD6AnimAliasCallback addStrongAliasRef) {
  md6AnimEventLookupCallback = findEvent;
  md6AnimAliasCallback = addStrongAliasRef;
}
