#include "tungsten/game/ai/vehicleai/splinepathmgr.h"

#include <algorithm>
#include <cmath>
#include <limits>

idNavSpline::idNavSpline(std::vector<idVec3> controlPoints)
    : points(std::move(controlPoints)) {
  cumulativeDistance.push_back(0.0F);
  for (std::size_t index = 1; index < points.size(); ++index) {
    length += (points[index] - points[index - 1]).Length();
    cumulativeDistance.push_back(length);
  }
}

idVec3 idNavSpline::Sample(float requestedDistance) const {
  if (points.empty()) return {};
  if (points.size() == 1 || requestedDistance <= 0.0F) return points.front();
  if (requestedDistance >= length) return points.back();
  for (std::size_t index = 1; index < points.size(); ++index) {
    if (requestedDistance <= cumulativeDistance[index]) {
      const float segmentLength = cumulativeDistance[index] -
                                  cumulativeDistance[index - 1];
      const float fraction = segmentLength > 0.0F
          ? (requestedDistance - cumulativeDistance[index - 1]) /
                segmentLength : 0.0F;
      return points[index - 1] + (points[index] - points[index - 1]) * fraction;
    }
  }
  return points.back();
}

float idNavSpline::ClosestDistance(const idVec3& point) const {
  if (points.size() < 2) return 0.0F;
  float bestDistanceSquared = std::numeric_limits<float>::max();
  float bestPathDistance = 0.0F;
  for (std::size_t index = 1; index < points.size(); ++index) {
    const idVec3 segment = points[index] - points[index - 1];
    const float segmentLengthSquared = segment.LengthSqr();
    const float fraction = segmentLengthSquared > 0.0F
        ? std::clamp(((point - points[index - 1]) * segment) /
                         segmentLengthSquared, 0.0F, 1.0F)
        : 0.0F;
    const idVec3 closest = points[index - 1] + segment * fraction;
    const float distanceSquared = (point - closest).LengthSqr();
    if (distanceSquared < bestDistanceSquared) {
      bestDistanceSquared = distanceSquared;
      bestPathDistance = cumulativeDistance[index - 1] +
                         std::sqrt(segmentLengthSquared) * fraction;
    }
  }
  return bestPathDistance;
}

void idNavPathPosTracker::SetWorldPosition(
    const idVec3& worldPosition, const idNavSplinePosition& previousPosition) {
  pathPosition = previousPosition;
  if (pathPosition.navSpline != nullptr)
    pathPosition.distance = pathPosition.navSpline->ClosestDistance(worldPosition);
}

void idNavPathPosTracker::Update(bool accurate) {
  ++updateCount;
  lastAccurate = accurate;
}

// DIA: idAISplinePathMgr::idAISplinePathMgr
idAISplinePathMgr::idAISplinePathMgr() = default;

// DIA: `idAISplinePathMgr::idAISplinePathMgr'::`1'::dtor$0
// DIA: `idAISplinePathMgr::idAISplinePathMgr'::`1'::dtor$1

// DIA: idAISplinePathMgr::~idAISplinePathMgr
idAISplinePathMgr::~idAISplinePathMgr() = default;

// DIA: idAISplinePathMgr::`scalar deleting destructor'

// DIA: idAISplinePathMgr::UpdateTrackersPeriodic
void idAISplinePathMgr::UpdateTrackersPeriodic(
    const idList<int, 5>* indexList, int frequencyInFrames,
    bool fastUpdateOthers) {
  if (indexList == nullptr || indexList->Num() == 0 || trackers.empty()) return;
  const int frequency = std::clamp(frequencyInFrames, 1, indexList->Num());
  const int batchSize = std::max(1, indexList->Num() / frequency);
  const int batch = (currentFrame % frequency) * batchSize;
  for (int listIndex = 0; listIndex < indexList->Num(); ++listIndex) {
    const int trackerIndex = (*indexList)[listIndex];
    if (trackerIndex < 0 || trackerIndex >= static_cast<int>(trackers.size()))
      continue;
    const bool selected = listIndex >= batch &&
                          listIndex < std::min(indexList->Num(), batch + batchSize);
    if (!selected && !fastUpdateOthers) continue;
    idNavPathPosTracker& tracker = trackers[static_cast<std::size_t>(trackerIndex)];
    idEntity* owner = tracker.GetOwner();
    if (owner == nullptr || owner->GetPhysics() == nullptr ||
        tracker.GetPathPosition().navSpline == nullptr) continue;
    tracker.SetWorldPosition(owner->GetPhysics()->GetOrigin(),
                             tracker.GetPathPosition());
    tracker.Update(selected);
  }
}

// DIA: idAISplinePathMgr::DrawWaypoints
void idAISplinePathMgr::DrawWaypoints() {
  drawnWaypointSegments = waypointPositions.size() > 1
      ? static_cast<int>(waypointPositions.size() - 1) : 0;
}

// DIA: idAISplinePathMgr::SetupWaypoints
void idAISplinePathMgr::SetupWaypoints() {
  splines.clear();
  if (waypointPositions.size() >= 2)
    splines.push_back(std::make_unique<idNavSpline>(waypointPositions));
  for (idNavPathPosTracker& tracker : trackers)
    tracker.SetSpline(splines.empty() ? nullptr : splines.front().get());
  SpecialSetup();
}

// DIA: idAISplinePathMgr::SpecialSetup
void idAISplinePathMgr::SpecialSetup() { ++specialSetupCount; }

int idAISplinePathMgr::AddTracker(idEntity* owner, bool vehicleTracker) {
  trackers.emplace_back();
  trackers.back().SetOwner(owner);
  trackers.back().SetSpline(splines.empty() ? nullptr : splines.front().get());
  trackerIsVehicle.push_back(vehicleTracker);
  return static_cast<int>(trackers.size() - 1);
}

idNavPathPosTracker* idAISplinePathMgr::GetTracker(int index) {
  return index >= 0 && index < static_cast<int>(trackers.size())
             ? &trackers[static_cast<std::size_t>(index)] : nullptr;
}

// DIA: idAISplinePathMgr::Update
void idAISplinePathMgr::Update() {
  idList<int, 5> vehicleTrackers;
  idList<int, 5> otherTrackers;
  for (int index = 0; index < static_cast<int>(trackers.size()); ++index) {
    if (trackers[static_cast<std::size_t>(index)].GetOwner() == nullptr) continue;
    (trackerIsVehicle[static_cast<std::size_t>(index)]
         ? vehicleTrackers : otherTrackers).Append(index);
  }
  UpdateTrackersPeriodic(&vehicleTrackers, 4, true);
  UpdateTrackersPeriodic(&otherTrackers, 10, false);
  if (spl_waypoints.GetInteger() != 0) DrawWaypoints();
  updateTime = gameLocal != nullptr
                   ? static_cast<float>(gameLocal->GetGameTimeMs())
                   : static_cast<float>(currentFrame * 16);
  nextRecalcTime = std::max(nextRecalcTime, static_cast<int>(updateTime));
}

// DIA: `idAISplinePathMgr::Update'::`1'::dtor$0
// DIA: `idAISplinePathMgr::Update'::`1'::dtor$1

// DIA: `dynamic initializer for 'spl_debug''
// DIA: `dynamic atexit destructor for 'spl_debug''
idCVar spl_debug("spl_debug", "0", 1, "show debug path");
// DIA: `dynamic initializer for 'spl_pos''
// DIA: `dynamic atexit destructor for 'spl_pos''
idCVar spl_pos("spl_pos", "0", 1, "set debug spline position");
// DIA: `dynamic initializer for 'spl_graph''
// DIA: `dynamic atexit destructor for 'spl_graph''
idCVar spl_graph("spl_graph", "0", 1, "draw spline graph");
// DIA: `dynamic initializer for 'spl_timer''
// DIA: `dynamic atexit destructor for 'spl_timer''
idCVar spl_timer("spl_timer", "0", 1, "show spline timing");
// DIA: `dynamic initializer for 'spl_showAllPaths''
// DIA: `dynamic atexit destructor for 'spl_showAllPaths''
idCVar spl_showAllPaths("spl_showAllPaths", "0", 4,
                        "display all vehicle AI paths");
// DIA: `dynamic initializer for 'spl_pathStepSize''
// DIA: `dynamic atexit destructor for 'spl_pathStepSize''
idCVar spl_pathStepSize("spl_pathStepSize", "300", 4,
                        "spline point step size");
// DIA: `dynamic initializer for 'spl_showPathBounds''
// DIA: `dynamic atexit destructor for 'spl_showPathBounds''
idCVar spl_showPathBounds("spl_showPathBounds", "0", 1,
                          "show spline bounds");
// DIA: `dynamic initializer for 'spl_waypoints''
// DIA: `dynamic atexit destructor for 'spl_waypoints''
idCVar spl_waypoints("spl_waypoints", "0", 1, "show spline waypoints");
