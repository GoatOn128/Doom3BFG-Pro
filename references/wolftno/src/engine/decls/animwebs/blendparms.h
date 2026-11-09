#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/text/parser.h"

#include <cstdint>

class blendParms_t {
public:
  struct parms_t {
    const idMD6Anim* srcAnim;
    const idMD6Anim* destAnim;
    std::int16_t sourceStartFrame;
    std::int16_t sourceDuration;
    std::int16_t destStartFrame;
    std::int16_t destDuration;
    std::uint8_t flags;
    std::uint8_t originBlend;
    std::uint8_t blendType;
  } parms;
  blendParms_t();
  explicit blendParms_t(int durationMS);
  int GetDuration() const;
#if defined(WOLFTNO_RETAIL_CHANNEL_BLENDPARMS_ABI)
  std::int16_t GetDurationMS() const;
#else
  int GetDurationMS() const;
#endif
#if defined(WOLFTNO_RETAIL_CHANNEL_BLENDPARMS_ABI)
  bool GetSourceFrames(const idMD6Anim* anim, std::int16_t& first,
                       std::int16_t& last) const;
  bool GetDestFrames(const idMD6Anim* anim, std::int16_t& first,
                     std::int16_t& duration) const;
#else
  bool GetSourceFrames(const idMD6Anim* anim, std::int16_t* first,
                       std::int16_t* last) const;
  bool GetDestFrames(const idMD6Anim* anim, std::int16_t* first, std::int16_t* duration) const;
#endif
  void SetDestStartFrame(int frame);
  void SetDuration(int frames);
  void SetDurationMS(int milliseconds);
  void Parse(const char* fromSubWebName,const char* fromStateName,const char* toSubWebName,const char* toStateName,idParser& source);
};
static_assert(sizeof(blendParms_t)==32,"blend parameters ABI drift");
