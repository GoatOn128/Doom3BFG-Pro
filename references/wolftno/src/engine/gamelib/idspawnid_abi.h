#pragma once

class idSpawnId {
public:
  int value{0x1FFF};

  int GetEntityNumber() const { return value & 0x1FFF; }
  bool IsValid() const { return value != 0x1FFF; }
};

static_assert(sizeof(idSpawnId) == 0x04, "spawn-id ABI drift");
