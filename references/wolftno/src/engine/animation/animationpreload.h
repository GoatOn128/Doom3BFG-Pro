#pragma once

#include "shared/idlib/text/str.h"

// NEEDS_WORK(animation): implemented by the animation module when that folder is recovered.
class idMD6Anim;
class idDeclMD6;
class idMD6Model;
class idAnimationPreload {
public:
  virtual ~idAnimationPreload() = default;
  virtual void AddStrongAliasRef(const char* alias) = 0;
  virtual void LoadUserChannelAnim(const char* resourceName) = 0;
  virtual const idMD6Anim* ResolveAnim(const char* resourceName) = 0;
  virtual const idMD6Anim* ResolveAliasAnim(const idDeclMD6* model, const char* alias) {
    (void)model;
    (void)alias;
    return nullptr;
  }
  virtual const idDeclMD6* ResolveModel(const char* resourceName) {
    (void)resourceName;
    return nullptr;
  }
  virtual const char* FindAliasName(unsigned short handle) {
    (void)handle;
    return nullptr;
  }
  // NEEDS_WORK(animation): the animation folder owns model-scoped alias
  // registration and user-channel lookup.  AnimWeb uses this ABI boundary
  // until that implementation is recovered.
  virtual unsigned short ResolveAliasHandle(const idDeclMD6*, const char* alias) {
    return static_cast<unsigned short>(idStr::IHash(alias));
  }
  virtual const idMD6Anim* ResolveDefaultAnim(const idMD6Model*) { return nullptr; }
  virtual short ResolveUserChannelIndex(const idMD6Model*, const char*) { return -1; }
};

inline idAnimationPreload*& AnimationPreloadService() {
  static idAnimationPreload* service = nullptr;
  return service;
}
