#pragma once

#include "engine/animation/md6.h"

class idMD6AnimProps {
public:
  enum wrapMode_t { WRAP_CLAMP, WRAP_LOOP };
  idMD6AnimProps() : anim(nullptr), aliasHandle(0xffff), wrap(WRAP_LOOP), rate(1.0f), tags(0) {}
  void SetAliasHandle(std::uint16_t handle);
  void SetAnim(const idMD6Anim* value);
  int NumAnims(const idDeclMD6* declMD6) const;
  const idMD6Anim* SelectAnim(const idDeclMD6* declMD6, int index) const;
  const idMD6Anim* SelectAndLoadAnim(const idDeclMD6* declMD6, int index) const;
  const idMD6Anim* anim;
  std::uint16_t aliasHandle;
  wrapMode_t wrap;
  float rate;
  unsigned tags;
};
static_assert(sizeof(idMD6AnimProps)==24,"MD6 animation properties ABI drift");
