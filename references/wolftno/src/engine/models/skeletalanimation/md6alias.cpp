#include "engine/models/skeletalanimation/md6alias.h"

#include "engine/decls/decl.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <new>

extern idMD6AliasAnimLoad g_md6AliasAnimLoad;
extern idMD6AliasAnimCompatible g_md6AliasAnimCompatible;

namespace {
const idMD6Anim* LoadAnim(const char* name, bool makeDefault,
                          bool skipStaleCheck) {
  return g_md6AliasAnimLoad != nullptr
             ? g_md6AliasAnimLoad(name, makeDefault, skipStaleCheck)
             : nullptr;
}

} // namespace

bool idMD6Alias::IsInherited(const idMD6Alias* other) const {
  if (mappings.num != other->mappings.num)
    return false;
  for (int index = 0; index < mappings.num; ++index) {
    if (idStr::Icmp(mappings.list[index].animRef.str,
                    other->mappings.list[index].animRef.str) != 0)
      return false;
  }
  return idStr::Icmp(other->name.str, name.str) == 0;
}

void idMD6Alias::LoadAnims(const idDeclMD6* decl) {
  if (decl == nullptr || decl->model == nullptr)
    return;
  flags |= 2u;
  for (int index = 0; index < mappings.num; ++index) {
    idMD6AliasEntry& mapping = mappings.list[index];
    mapping.anim = LoadAnim(mapping.animRef.str, false, false);
    if (mapping.anim == nullptr) {
      idLib::Warning("Failed to load anim '%s' for alias '%s', md6Def '%s'",
                     mapping.animRef.str, name.str, decl->model->name.str);
      continue;
    }
    const idMD6Skel* skeleton = decl->model->skeleton;
    if (g_md6AliasAnimCompatible != nullptr &&
        !g_md6AliasAnimCompatible(mapping.anim, skeleton)) {
      idLib::Warning(
          "Animation '%s' was loaded for invalid skeleton '%s', md6Def '%s'",
          mapping.anim->name.str,
          skeleton->name.str,
          decl->name.str);
      mapping.anim = nullptr;
    }
  }
}

void idMD6Alias::LoadAnimation(int index) {
  if (index >= 0 && index < mappings.num) {
    mappings.list[index].anim =
        LoadAnim(mappings.list[index].animRef.str, false, false);
  }
}

idMD6Alias::idMD6Alias()
    : name(), aliasHandle(0xffffu), mappings() {
  mappings.ClearFree();
}

template <>
bool idList<idMD6AliasEntry, 19>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      delete[] list;
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;
  idMD6AliasEntry* replacement = new (std::nothrow) idMD6AliasEntry[newSize];
  if (replacement == nullptr)
    return false;
  const int copyCount = std::min(num, newSize);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = list[index];
  delete[] list;
  list = replacement;
  size = newSize;
  if (num > newSize)
    num = newSize;
  return true;
}

template <>
int idList<idMD6AliasEntry, 19>::Append(const idMD6AliasEntry& object) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize))
      return -1;
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity - (size + granularity) % granularity;
    } else if (size <= 0x3fffffff) {
      newSize = size * 2;
      if (newSize == 0)
        newSize = 1;
    } else {
      newSize = 0x7fffffff;
    }
    if (!Resize(newSize))
      return -1;
  }
  if (num >= size)
    return num - 1;
  list[num] = object;
  return num++;
}

void idMD6Alias::Parse(const idDecl* decl, idParser* source,
                       int* loadErrors) {
  idToken token;
  source->ExpectTokenType(TT_PUNCTUATION, 46, token);
  source->ExpectTokenString("name");
  source->ExpectTokenType(TT_STRING, 0, token);
  name.Set(token.data);
  flags = 0;
  mappings.Clear();

  while (source->ReadToken(token)) {
    if (token.type == TT_PUNCTUATION && token.subtype == 47)
      break;
    if (token == "anim") {
      source->ExpectTokenType(TT_STRING, 0, token);
      idMD6AliasEntry entry;
      entry.animRef.Set(token.data);
      entry.anim = nullptr;
      mappings.Append(entry);
    } else if (token == "flags") {
      source->ExpectTokenType(TT_PUNCTUATION, 46, token);
      while (source->ReadToken(token)) {
        if (token.type == TT_PUNCTUATION && token.subtype == 47)
          break;
        if (idStr::Icmp(token.data, "forceLoad") == 0)
          flags |= 1u;
      }
    } else {
      source->Error("Unknown token '%s'", token.data);
    }
  }

  if ((flags & 1u) != 0) {
    for (int index = 0; index < mappings.num; ++index) {
      if (LoadAnim(mappings.list[index].animRef.str, true, false) == nullptr) {
        ++*loadErrors;
        idLib::Warning(
            "Failed to load animation '%s' for alias '%s' while parsing "
            "aliases for md6Def '%s'.",
            mappings.list[index].animRef.str, name.str,
            decl->name.str);
      }
    }
  }
}
