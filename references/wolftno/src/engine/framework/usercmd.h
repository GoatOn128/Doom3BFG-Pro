#pragma once

#include "shared/idlib/math/vector.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

class usercmd_t {
public:
  struct touch_t {
    std::int16_t pos[2];
    bool active;
    std::uint8_t alignment;
  };
  usercmd_t() { std::memset(this,0,sizeof(*this)); }
  void ClearMoveCmds();
  void ClearButtons() { buttons=0;buttons2=0; }
  void Print() const;

  std::uint32_t buttons;
  std::uint16_t buttons2;
  std::int8_t forwardmove;
  std::int8_t rightmove;
  std::int8_t upmove;
  std::int8_t forwardmoveSec;
  std::int8_t rightmoveSec;
  std::uint8_t inhibited:1;
  std::uint8_t fromBot:1;
  std::uint8_t reservedFlags:6;
  std::int16_t angles[3];
  int clientGameFrame;
  idVec3 pos;
  std::int16_t pitch;
  std::int16_t yaw;
  std::int16_t roll;
  std::uint8_t predictionStateBits;
  int serverGameTime;
  std::uint16_t fireCount;
  std::int16_t fireAngles[2];
  std::int16_t firePos[3];
  touch_t touch[2];
  std::uint8_t vehicleThrottle;
  std::uint8_t speed;
};

static_assert(sizeof(usercmd_t)==76,"usercmd_t ABI drift");
static_assert(offsetof(usercmd_t,clientGameFrame)==20,"usercmd frame ABI drift");
static_assert(offsetof(usercmd_t,pos)==24,"usercmd position ABI drift");
static_assert(offsetof(usercmd_t,serverGameTime)==44,"usercmd server time ABI drift");
static_assert(offsetof(usercmd_t,touch)==60,"usercmd touch ABI drift");
static_assert(sizeof(usercmd_t::touch_t)==6,"usercmd touch record ABI drift");
