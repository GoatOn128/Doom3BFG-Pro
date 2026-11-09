#pragma once

#include "shared/idlib/containers/list.h"
#include "tungsten/game/game_runtime.h"

#include <memory>
#include <string>
#include <vector>

class idNavSpline {
public:
  explicit idNavSpline(std::vector<idVec3> controlPoints = {});
  idVec3 Sample(float distance) const;
  float ClosestDistance(const idVec3& point) const;
  float GetLength() const { return length; }
  int NumPoints() const { return static_cast<int>(points.size()); }

private:
  std::vector<idVec3> points;
  std::vector<float> cumulativeDistance;
  float length{0.0F};
};

struct idNavSplinePosition {
  idNavSpline* navSpline{nullptr};
  float distance{0.0F};
  idVec3 GetWorldPos() const {
    return navSpline != nullptr ? navSpline->Sample(distance) : idVec3{};
  }
};

class idNavPathPosTracker {
public:
  void SetOwner(idEntity* value) { owner = value; }
  idEntity* GetOwner() const { return owner; }
  void SetSpline(idNavSpline* spline) { pathPosition.navSpline = spline; }
  void SetWorldPosition(const idVec3& worldPosition,
                        const idNavSplinePosition& previousPosition);
  void Update(bool accurate);
  const idNavSplinePosition& GetPathPosition() const { return pathPosition; }
  int GetUpdateCount() const { return updateCount; }
  bool WasLastUpdateAccurate() const { return lastAccurate; }

private:
  idEntity* owner{nullptr};
  idNavSplinePosition pathPosition{};
  int updateCount{0};
  bool lastAccurate{false};
};

class idAISplinePathMgr {
public:
  idAISplinePathMgr();
  virtual ~idAISplinePathMgr();
  void UpdateTrackersPeriodic(const idList<int, 5>* indexList,
                              int frequencyInFrames,
                              bool fastUpdateOthers);
  void DrawWaypoints();
  void SetupWaypoints();
  virtual void SpecialSetup();
  void Update();

  void AddWaypoint(const idVec3& point) { waypointPositions.push_back(point); }
  int AddTracker(idEntity* owner, bool vehicleTracker);
  idNavPathPosTracker* GetTracker(int index);
  void SetFrame(int value) { currentFrame = value; }
  int NumSplines() const { return static_cast<int>(splines.size()); }
  int NumTrackers() const { return static_cast<int>(trackers.size()); }
  int GetDrawnWaypointSegments() const { return drawnWaypointSegments; }
  int GetSpecialSetupCount() const { return specialSetupCount; }
  const char* GetTemporaryPathName() const { return temporaryPathName.c_str(); }

private:
  std::vector<idVec3> waypointPositions;
  std::vector<std::unique_ptr<idNavSpline>> splines;
  std::vector<idNavPathPosTracker> trackers;
  std::vector<bool> trackerIsVehicle;
  int currentFrame{0};
  int drawnWaypointSegments{0};
  int specialSetupCount{0};
  int nextRecalcTime{0};
  float updateTime{0.0F};
  std::string temporaryPathName{"tempVehAISplinePath"};
  bool trackPickups{false};
};

extern idCVar spl_debug;
extern idCVar spl_pos;
extern idCVar spl_graph;
extern idCVar spl_timer;
extern idCVar spl_showAllPaths;
extern idCVar spl_pathStepSize;
extern idCVar spl_showPathBounds;
extern idCVar spl_waypoints;
