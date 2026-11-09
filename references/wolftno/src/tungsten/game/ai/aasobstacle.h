#pragma once

#include "tungsten/game/game_runtime.h"

class idAASObstacle final : public idEntity {
public:
  idAASObstacle();

  void ChangeAreaState();
  void PostSpawn() override;
  void Remove() override;
  void OnHide(bool stopFx) override;
  void OnShow() override;
  void OnActivate(idEntity* activator) override;

  bool IsEnabled() const { return enabled; }
  bool StartsOn() const { return startOn; }
  bool IsDynamicObstacle() const { return dynamicObstacle; }
  std::uint32_t GetTravelFlags() const { return travelFlags; }

  void SetStartOn(bool value) { startOn = value; }
  void SetDynamicObstacle(bool value) { dynamicObstacle = value; }
  void SetTravelFlags(std::uint32_t value) { travelFlags = value; }

private:
  // The retail constructor stores 0x0100 across these adjacent flags:
  // start_on defaults off while dynamic_obstacle defaults on.
  bool startOn{false};
  bool dynamicObstacle{true};
  std::uint32_t travelFlags{AAS_TFL_INVALID};
  bool enabled{false};
};
