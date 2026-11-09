#include "tungsten/game/ai/aasobstacle.h"

// DIA: idAASObstacle::idAASObstacle
idAASObstacle::idAASObstacle()
    : startOn(false),
      dynamicObstacle(true),
      travelFlags(AAS_TFL_INVALID),
      enabled(false) {}

// DIA: idAASObstacle::ChangeAreaState
void idAASObstacle::ChangeAreaState() {
  idPhysics* obstaclePhysics = GetPhysics();
  gameLocal->ChangeAreaTravelFlags(obstaclePhysics->GetAbsBounds(),
                                   AAS_AREA_CONTENTS_OBSTACLE,
                                   travelFlags, enabled, GetName());

  if (!dynamicObstacle) {
    return;
  }

  std::uint32_t contents = obstaclePhysics->GetContents();
  if (enabled) {
    obstaclePhysics->SetContents(contents | CONTENTS_DYNAMIC_OBSTACLE);
    obstaclePhysics->EnableClip();
  } else {
    obstaclePhysics->SetContents(contents & ~CONTENTS_DYNAMIC_OBSTACLE);
    obstaclePhysics->DisableClip();
  }
}

// DIA: idAASObstacle::PostSpawn
void idAASObstacle::PostSpawn() {
  idEntity::PostSpawn();
  enabled = startOn;
  ChangeAreaState();
}

// DIA: idAASObstacle::Remove
void idAASObstacle::Remove() {
  idEntity::Remove();
  enabled = false;
  ChangeAreaState();
}

// DIA: idAASObstacle::OnHide
void idAASObstacle::OnHide(bool stopFx) {
  idEntity::OnHide(stopFx);
  enabled = false;
  ChangeAreaState();
}

// DIA: idAASObstacle::OnShow
void idAASObstacle::OnShow() {
  idEntity::OnShow();
  enabled = true;
  ChangeAreaState();
}

// DIA: idAASObstacle::OnActivate
void idAASObstacle::OnActivate(idEntity*) {
  enabled = !enabled;
  ChangeAreaState();
}
