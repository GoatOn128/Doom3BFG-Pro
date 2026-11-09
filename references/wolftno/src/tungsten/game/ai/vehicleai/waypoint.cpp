#include "tungsten/game/ai/vehicleai/waypoint.h"

// DIA: idWaypoint::~idWaypoint
idWaypoint::~idWaypoint() {
  if (gameLocal == nullptr) return;

  for (int index = 0; index < gameLocal->waypointEntities.Num(); ++index) {
    if (gameLocal->waypointEntities[index] == this) {
      gameLocal->waypointEntities.RemoveIndex(index);
      break;
    }
  }
}

// DIA: idWaypoint::Spawn
void idWaypoint::Spawn() {
  if (gameLocal != nullptr) gameLocal->waypointEntities.Append(this);
}
