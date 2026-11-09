#include "engine/models/skeletalanimation/md6alias.h"

idMD6AliasAnimLoad g_md6AliasAnimLoad = nullptr;
idMD6AliasAnimCompatible g_md6AliasAnimCompatible = nullptr;
bool forceIdListsToReallocateEveryAppend = false;

void SetMD6AliasAnimLoad(idMD6AliasAnimLoad load) {
  g_md6AliasAnimLoad = load;
}

void SetMD6AliasAnimCompatible(idMD6AliasAnimCompatible compatible) {
  g_md6AliasAnimCompatible = compatible;
}
