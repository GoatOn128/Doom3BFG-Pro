#pragma once

#include "engine/aas2file/aas2file.h"
#include "tungsten/game/ai/aaspositionmgr.h"
#include "tungsten/game/ai/aibehaviors.h"

class idDynamicCoverProvider {
public:
  virtual ~idDynamicCoverProvider() = default;
  virtual aas2Cover_t* GetDynamicCoverPoint(const idAAS2* aas,
                                             std::int16_t index) const = 0;
};

class idAICover {
public:
  idAICover();
  idAICover(const idAAS2* aas, std::int16_t index);
  idAICover& operator=(const idAICover& other);

  void Init(const idEntity* entity, const idAAS2* aas, std::int16_t index);
  aas2Cover_t* GetCoverPoint() const;
  void Clear();
  bool IsValid() const;
  int GetCoverPosture() const;
  const idVec3& GetOrigin() const;
  void GetAxis(idMat3& axis) const;
  void Unreserve();
  void GetCoverActions(idCoverActions* actions) const;
  bool IsPointInArc(const idVec3& point, float arcCosine) const;

  const idAAS2* GetAAS() const { return aas; }
  std::int16_t GetCoverIndex() const { return coverIndex; }
  int GetSpawnId() const { return spawnId; }

private:
  const idAAS2* aas{nullptr};
  const idEntity* ownerEntity{nullptr};
  int spawnId{0x1FFF};
  std::int16_t coverIndex{-1};
};

class idCoverBlocker : public idEntity {
public:
  ~idCoverBlocker() override;
  void Spawn();
  static int GetBlockerCount() { return blockers.Num(); }

private:
  inline static idList<idCoverBlocker*, 5> blockers;
};
