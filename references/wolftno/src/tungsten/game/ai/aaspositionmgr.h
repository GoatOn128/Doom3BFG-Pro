#pragma once

#include "tungsten/game/game_runtime.h"

#include <array>
#include <cstdint>

struct aas2Cover_t;
struct aas2TraceFloor_t;
class idAAS2Settings;

class idAAS2 {
public:
  virtual ~idAAS2() = default;

  virtual std::uint16_t PointAreaNum(int tree,
                                     const idVec3& origin) const = 0;
  virtual idBounds DefaultSearchBounds() const = 0;
  virtual std::uint16_t PointReachableAreaNum(
      int tree, const idVec3& origin, const idBounds& searchBounds,
      int areaFlags, int travelFlags) const = 0;
  virtual void PushPointIntoAreaNum(std::uint16_t areaNum,
                                    idVec3& origin) const = 0;
  virtual aas2Cover_t* GetCoverPoint(std::int16_t index) const {
    (void)index;
    return nullptr;
  }
  virtual idVec3 GetGravityDirection() const { return {0.0F, 0.0F, -1.0F}; }
  virtual int PointAreaNumMaxheight(int tree, const idVec3& origin,
                                    float maxHeight) const {
    (void)maxHeight;
    return PointAreaNum(tree, origin);
  }
  virtual bool TraceFloor(aas2TraceFloor_t* trace, const idVec3& start,
                          int startAreaNum, const idVec3& end,
                          int travelFlags, bool allowFloorNormalChange,
                          bool ignoreGravityDirectionDistance,
                          bool ignoreSameArea) const;
  virtual bool PushPointIntoNearestAreaNum(int* areaNum,
                                           idVec3& point) const;
  virtual const idAAS2Settings* GetSettings() const { return nullptr; }
  virtual bool IsDefault() const { return false; }
  virtual const char* GetName() const { return ""; }
};

class idAASPosition {
public:
  int GetAreaNum() const;
  void Update(const idAAS2* newAAS, const idVec3& newOrigin);

  bool IsValid() const { return valid; }
  const idVec3& GetOrigin() const { return origin; }
  const idVec3& GetValidOrigin() const { return validOrigin; }
  const idAAS2* GetAAS() const { return aas; }

private:
  friend class idAASPositionMgr;

  bool valid{false};
  std::uint16_t areaNum{0};
  idVec3 origin{};
  idVec3 validOrigin{};
  const idAAS2* aas{nullptr};
};

class idAASPositionMgr {
public:
  idAASPositionMgr();

  const idAASPosition& GetAASPosition(const idAAS2* aas,
                                      const idEntity* entity);
  int GetNumAASPositions() const { return numAASPositions; }

private:
  static constexpr int MAX_AAS_POSITIONS = 2;

  std::array<idAASPosition, MAX_AAS_POSITIONS> aasPosition{};
  int numAASPositions{0};
  std::array<int, MAX_AAS_POSITIONS> lastUpdateFrame{{-1, -1}};
};

extern idCVar ai_debugAASPosition;
extern idCVar ai_debugLevel;
