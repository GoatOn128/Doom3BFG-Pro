#pragma once

#include "engine/animation/md6.h"

using idMD6AliasAnimLoad = const idMD6Anim* (*)(const char* name,
                                                bool makeDefault,
                                                bool skipStaleCheck);
using idMD6AliasAnimCompatible = bool (*)(const idMD6Anim* animation,
                                          const idMD6Skel* skeleton);

void SetMD6AliasAnimLoad(idMD6AliasAnimLoad load);
void SetMD6AliasAnimCompatible(idMD6AliasAnimCompatible compatible);
extern bool forceIdListsToReallocateEveryAppend;

template <>
bool idList<idMD6AliasEntry, 19>::Resize(int newSize);
template <>
int idList<idMD6AliasEntry, 19>::Append(const idMD6AliasEntry& object);
