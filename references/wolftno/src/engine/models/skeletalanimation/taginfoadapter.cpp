#include "engine/models/skeletalanimation/taginfo.h"

idTagJointLookupCallback tagJointLookupCallback = nullptr;

void SetTagJointLookupCallback(idTagJointLookupCallback callback) {
  tagJointLookupCallback = callback;
}
