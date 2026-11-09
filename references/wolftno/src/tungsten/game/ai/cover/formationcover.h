#pragma once

#include "tungsten/game/ai/cover/dynamiccovermgr.h"

class idFormationCover : public idDynamicCover {
public:
  idFormationCover();
  void UpdateUsable(const idEntity* owner, const idAAS2* aas,
                    unsigned int currentTime) override;
  void InternalUpdateCurrent(const idEntity* owner, const idAAS2* aas,
                             int currentTime);
  void InternalUpdateUsable(const idEntity* owner, const idAAS2* aas,
                            int currentTime);
};

class idFormationCoverMgr : public idDynamicCoverMgr {
protected:
  idDynamicCover* AllocCover(const idAAS2* aas) override;
};
