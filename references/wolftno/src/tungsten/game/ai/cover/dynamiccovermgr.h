#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "tungsten/game/ai/cover/dynamiccover.h"

class idDynamicCoverMgr {
public:
  idDynamicCoverMgr();
  virtual ~idDynamicCoverMgr();

  void Shutdown();
  int NumCoverPoints(const idAAS2* aas) const;
  aas2Cover_t* GetCoverPoint(const idAAS2* aas, int index) const;
  const idEntity* GetReserver(const idAAS2* aas, int index) const;
  void Reserve(const idAAS2* aas, int index, const idEntity* entity);
  void Unreserve(const idAAS2* aas, int index);
  int GetUsableTime(const idAAS2* aas, int index) const;
  void SetUsableTime(const idAAS2* aas, int index, int time);
  void Init();
  void UpdateUsable(const idEntity* owner, unsigned int currentTime);
  static void ShowDynamicCover(const idVec3* point, float distance);

  bool IsInitialized() const { return initialized; }
  int GetCoverSetCount() const { return coverSets.Num(); }
  static int GetLastShownCount() { return lastShownCount; }

protected:
  virtual idDynamicCover* AllocCover(const idAAS2* aas);
  virtual void FreeCover(idDynamicCover* cover);
  int FindAAS(const idAAS2* aas) const;

private:
  idList<idDynamicCover*, 5> coverSets;
  idList<const idAAS2*, 5> aasList;
  bool initialized{false};
  inline static idList<idDynamicCoverMgr*, 5> managers;
  inline static int lastShownCount{0};
};

extern idCVar ai_showDynamicCover;
