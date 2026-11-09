#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDBOX_CLASS_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#include "engine/gamelib/aas2/obstacleavoidancemanagerlocal.h"

#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/framework/common.h"
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/text/str.h"

#undef WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDBOX_CLASS_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <cstring>
#include <xmmintrin.h>

class idEntity;

idRenderDebug* GetGameLibRenderDebug();

idCVar aas_showPathTree(
    "aas_showPathTree", "0", CVAR_BOOL,
    "1 = show the path tree when drawing obstacle avoidance",
    static_cast<argCompletionRef_t>(nullptr));
idCVar aas_showObstacleAvoidance(
    "aas_showObstacleAvoidance", "0", CVAR_INTEGER,
    "shows obstacles along paths",
    static_cast<argCompletionRef_t>(nullptr));
idCVar aas_showObstacleScale(
    "aas_showObstacleScale", "1", CVAR_FLOAT,
    "scale used when rendering the obstacles top down", 1.0f, 100.0f,
    static_cast<argCompletionRef_t>(nullptr));
idCVar aas_skipObstacleAvoidance(
    "aas_skipObstacleAvoidance", "0", CVAR_BOOL,
    "ignore all dynamic obstacles along paths",
    static_cast<argCompletionRef_t>(nullptr));
idCVar aas_debugSaveQuery("aas_debugSaveQuery", "", 0, "",
                          static_cast<argCompletionRef_t>(nullptr));
idCVar aas_useObstacleAvoidanceJobs(
    "aas_useObstacleAvoidanceJobs", "1", CVAR_INTEGER,
    "0 = off, 1 = threaded", 0.0f, 1.0f,
    static_cast<argCompletionRef_t>(nullptr));

__forceinline testQuery_t::testQuery_t() noexcept {
  bbox = bounds_zero;
  bbox.b[0] = {idMath::INFINITY, idMath::INFINITY, idMath::INFINITY};
  bbox.b[1] = {-idMath::INFINITY, -idMath::INFINITY,
               -idMath::INFINITY};
}

testQuery_t testQuery;

namespace {

template <typename Function, typename Type>
__forceinline
Function VirtualFunction(Type* object, std::size_t byteOffset) {
  const auto table = *reinterpret_cast<const void* const* const*>(object);
  return reinterpret_cast<Function>(
      const_cast<void*>(table[byteOffset / sizeof(void*)]));
}

__forceinline idEntity* GameGetEntity(idGame* game, int entityNumber) {
  using Function = idEntity* (*)(idGame*, int);
  return VirtualFunction<Function>(game, 0x450)(game, entityNumber);
}

__forceinline void GameEntityGetViewAngles(idGame* game, idEntity* entity,
                                           idAngles& angles) {
  using Function = void (*)(idGame*, idEntity*, idAngles&);
  VirtualFunction<Function>(game, 0x4A0)(game, entity, angles);
}

__forceinline void GameEntityGetEyePosition(idGame* game, idEntity* entity,
                                            idVec3& position) {
  using Function = void (*)(idGame*, idEntity*, idVec3&);
  VirtualFunction<Function>(game, 0x4A8)(game, entity, position);
}

__forceinline const idVec4& AsVec4(const idColor& color) {
  return reinterpret_cast<const idVec4&>(color);
}

__forceinline const idVec4& DebugColor(
    idObstacleAvoidanceCore::avoidColor_t color) {
  switch (color) {
    case idObstacleAvoidanceCore::COLOR_WHITE:
      return AsVec4(idColor::colorWhite);
    case idObstacleAvoidanceCore::COLOR_LIGHT_GREY:
      return AsVec4(idColor::colorLtGrey);
    case idObstacleAvoidanceCore::COLOR_BLUE:
      return AsVec4(idColor::colorBlue);
    case idObstacleAvoidanceCore::COLOR_GREEN:
      return AsVec4(idColor::colorGreen);
    case idObstacleAvoidanceCore::COLOR_YELLOW:
      return AsVec4(idColor::colorYellow);
    case idObstacleAvoidanceCore::COLOR_ORANGE:
      return AsVec4(idColor::colorOrange);
    case idObstacleAvoidanceCore::COLOR_PURPLE:
      return AsVec4(idColor::colorPurple);
    case idObstacleAvoidanceCore::COLOR_CYAN:
      return AsVec4(idColor::colorCyan);
    case idObstacleAvoidanceCore::COLOR_MAGENTA:
      return AsVec4(idColor::colorMagenta);
    case idObstacleAvoidanceCore::COLOR_RED:
    case idObstacleAvoidanceCore::COLOR_INVALID:
    default:
      return AsVec4(idColor::colorRed);
  }
}

__forceinline void InitializePathForQuery(obstaclePath_t& path,
                                          const obstacleRoute_t& route) {
  const obstacleRoute_t::routeArea_t& firstArea = route.areas[0];
  path.seekPos[0] = firstArea.end;
  path.seekPos[1] = firstArea.end;
  path.numSeekPos = 1;
  path.seekPosPlane = {0.0f, 0.0f, 0.0f, 0.0f};
  path.firstObstaclePosition = {0.0f, 0.0f, 0.0f};
  path.firstObstacle = -1;
  path.startPosOutsideObstacles = firstArea.start;
  path.startPosObstacle = -1;
  path.seekPosOutsideObstacles = firstArea.end;
  path.seekPosObstacle = -1;
  path.targetDist = 1.0e30f;
  path.pathLength = 1.0e30f;
  path.nextCorner = {0.0f, 0.0f, 0.0f, 0.0f};
  path.nextCornerAngle = 0.0f;
  path.wallCorners[0] = firstArea.end;
  path.wallCorners[1] = firstArea.end;
  const std::uint32_t validAndDone = 0x01010101u;
  std::memcpy(&path.hasValidPath, &validAndDone, sizeof(validAndDone));
  for (obstaclePath_t::radar_t& radar : path.radar) {
    radar.direction = {0.0f, 0.0f, 0.0f};
    radar.obstacle = -1;
  }
}

} // namespace

void ObstacleAvoidanceJob(obstacleParms_t* parms) {
  idObstacleAvoidanceCore core;
  std::uint8_t tempBuffer[0x10000];
  core.FindPathAroundObstacles(
      parms->path, parms->debugLines, parms->maxDebugLines,
      parms->debugText, parms->maxDebugText, tempBuffer,
      static_cast<int>(sizeof(tempBuffer)), parms->bbox, parms->gravity,
      parms->lastDir, parms->lastCorner, parms->obstacleRadius,
      parms->frameMoveDist, parms->cornerCircleRadius, parms->flags,
      parms->route, parms->obstacles, parms->numObstacles, parms->vertices,
      parms->numVertices, parms->edges, parms->numEdges, parms->corners,
      parms->numCorners, parms->traces, parms->numTraces,
      parms->traceFractions);
  parms->path->done = true;
}

idParallelJobRegistration register_ObstacleAvoidanceJob(
    reinterpret_cast<jobRun_t>(ObstacleAvoidanceJob),
    "ObstacleAvoidanceJob");

idObstacleAvoidance::idObstacleAvoidance()
    : buffers(nullptr),
      lastDir{0.0f, 0.0f, 0.0f},
      lastCorner{0.0f, 0.0f, 0.0f, 0.0f},
      lastUpdateTime(-1),
      firstUpdateTime(-1) {}

idObstacleAvoidance::~idObstacleAvoidance() {
  obstacleAvoidanceManager->FreeObstacleBuffers(this);
}

void idObstacleAvoidance::AllocBuffers() {
  obstacleAvoidanceManager->AllocObstacleBuffers(this);
}

obstacleVertex_t* idObstacleAvoidance::GetVertexArray() {
  return buffers != nullptr ? buffers->vertices : nullptr;
}

int idObstacleAvoidance::GetMaxVertices() {
  return buffers != nullptr ? 0x300 : 0;
}

void idObstacleAvoidance::SetNumVertices(int num) {
  if (buffers != nullptr) {
    buffers->numVertices = num;
  }
}

obstacleEdge_t* idObstacleAvoidance::GetEdgeArray() {
  return buffers != nullptr ? buffers->edges : nullptr;
}

int idObstacleAvoidance::GetMaxEdges() {
  return buffers != nullptr ? 0x200 : 0;
}

void idObstacleAvoidance::SetNumEdges(int num) {
  if (buffers != nullptr) {
    buffers->numEdges = num;
  }
}

void idObstacleAvoidance::AddObstacle(const idBox& box, idSpawnId id,
                                      bool soft) {
  if (buffers == nullptr || buffers->numObstacles >= 128) {
    return;
  }

  obstacleBox_t& obstacle = buffers->obstacles[buffers->numObstacles++];
  obstacle.axis = box.axis;
  obstacle.center = box.center;
  obstacle.extents = box.extents;
  obstacle.soft = soft;
  obstacle.id = id;
}

void idObstacleAvoidance::AddObstacle(const idBounds& bounds,
                                      const idVec3& origin,
                                      const idMat3& axis, idSpawnId id,
                                      bool soft) {
  if (buffers == nullptr || buffers->numObstacles >= 128) {
    return;
  }

  obstacleBox_t& obstacle = buffers->obstacles[buffers->numObstacles++];
  obstacle.axis = axis;
  const idVec3 localCenter{
      (bounds.b[0].x + bounds.b[1].x) * 0.5f,
      (bounds.b[0].y + bounds.b[1].y) * 0.5f,
      (bounds.b[0].z + bounds.b[1].z) * 0.5f,
  };
  obstacle.center = localCenter;
  obstacle.extents = {bounds.b[1].x - localCenter.x,
                      bounds.b[1].y - localCenter.y,
                      bounds.b[1].z - localCenter.z};
  obstacle.center = {
      ((localCenter.x * axis.mat[0].x +
        localCenter.y * axis.mat[1].x) +
       localCenter.z * axis.mat[2].x) +
          origin.x,
      ((localCenter.y * axis.mat[1].y +
        localCenter.x * axis.mat[0].y) +
       localCenter.z * axis.mat[2].y) +
          origin.y,
      ((localCenter.y * axis.mat[1].z +
        localCenter.x * axis.mat[0].z) +
       localCenter.z * axis.mat[2].z) +
          origin.z,
  };
  obstacle.id = id;
  obstacle.soft = soft;
}

void idObstacleAvoidance::Restart(const idVec3& start,
                                  const idVec3& goal) {
  if (buffers == nullptr || buffers->path == nullptr) {
    return;
  }

  obstaclePath_t& path = *buffers->path;
  path.seekPos[0] = goal;
  path.seekPos[1] = goal;
  const idVec3 delta{goal.x - start.x, goal.y - start.y,
                     goal.z - start.z};
  const float lengthSquared =
      (delta.y * delta.y + delta.x * delta.x) + delta.z * delta.z;
  const float inverseLength =
      1.0f /
      _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
          _mm_set_ss(lengthSquared), idMath::vector_float_smallestNonDenorm)));
  path.seekPosPlane.a = delta.x * inverseLength;
  path.seekPosPlane.b = delta.y * inverseLength;
  path.seekPosPlane.c = delta.z * inverseLength;
  path.seekPosPlane.d =
      -((path.seekPosPlane.b * goal.y +
         path.seekPosPlane.a * goal.x) +
        path.seekPosPlane.c * goal.z);

  path.numSeekPos = 0;
  path.seekPosPlane = {0.0f, 0.0f, 0.0f, 0.0f};
  path.firstObstaclePosition = {0.0f, 0.0f, 0.0f};
  path.firstObstacle = -1;
  path.startPosOutsideObstacles = start;
  path.startPosObstacle = -1;
  path.seekPosOutsideObstacles = start;
  path.seekPosObstacle = -1;
  path.targetDist = 0.0f;
  path.pathLength = 0.0f;
  path.nextCorner = {0.0f, 0.0f, 0.0f, 0.0f};
  path.nextCornerAngle = 0.0f;
  path.wallCorners[0] = goal;
  path.wallCorners[1] = goal;
  path.useRadarForward = false;
  path.radarForward = {0.0f, 0.0f, 0.0f};
  path.hasValidPath = true;
  path.startPosValid = true;
  path.seekPosValid = true;
  path.done = false;
  for (obstaclePath_t::radar_t& radar : path.radar) {
    radar.direction = {0.0f, 0.0f, 0.0f};
  }
}

void idObstacleAvoidance::ProjectTopDown(idVec3& point,
                                         const idVec3& startPos,
                                         float scale) const {
  idGame* const game = common->Game();
  idEntity* const player = GameGetEntity(game, 0);
  if (player == nullptr) {
    return;
  }

  idVec3 viewOrigin;
  idAngles viewAngles;
  GameEntityGetEyePosition(game, player, viewOrigin);
  GameEntityGetViewAngles(game, player, viewAngles);
  const idMat3 viewAxis = viewAngles.ToMat3();
  const idAngles playerAngles{0.0f, -viewAngles.yaw, 0.0f};
  const idMat3 playerAxis = playerAngles.ToMat3();
  const idVec3 delta{point.x - startPos.x, point.y - startPos.y,
                     point.z - startPos.z};
  const float horizontal =
      (playerAxis.mat[1].x * delta.y +
       playerAxis.mat[0].x * delta.x) +
      playerAxis.mat[2].x * delta.z;
  const float vertical =
      (playerAxis.mat[1].y * delta.y +
       playerAxis.mat[0].y * delta.x) +
      playerAxis.mat[2].y * delta.z;
  point = {
      (viewOrigin.x + viewAxis.mat[0].x * scale +
       viewAxis.mat[1].x * vertical) +
          viewAxis.mat[2].x * horizontal,
      (viewOrigin.y + viewAxis.mat[0].y * scale +
       viewAxis.mat[1].y * vertical) +
          viewAxis.mat[2].y * horizontal,
      (viewOrigin.z + viewAxis.mat[0].z * scale +
       viewAxis.mat[1].z * vertical) +
          viewAxis.mat[2].z * horizontal,
  };
}

void idObstacleAvoidance::DrawBox(const idVec3& startPos,
                                  float radius) const {
  const float arrowHalfWidth = radius * 0.1f;
  const float arrowLength = radius - arrowHalfWidth;
  idVec3 points[7] = {
      {startPos.x + radius, startPos.y + radius, startPos.z},
      {startPos.x + radius, startPos.y - radius, startPos.z},
      {startPos.x - radius, startPos.y - radius, startPos.z},
      {startPos.x - radius, startPos.y + radius, startPos.z},
      {startPos.x, startPos.y + radius, startPos.z},
      {startPos.x + arrowHalfWidth, startPos.y + arrowLength, startPos.z},
      {startPos.x - arrowHalfWidth, startPos.y + arrowLength, startPos.z},
  };
  const float scale = (radius * 2.5f) / aas_showObstacleScale.valueFloat;
  for (idVec3& point : points) {
    ProjectTopDown(point, startPos, scale);
  }
  for (int index = 0; index < 4; ++index) {
    GetGameLibRenderDebug()->DebugLine(
        AsVec4(idColor::colorCyan), points[index], points[(index + 1) & 3], 0,
        false);
  }
  GetGameLibRenderDebug()->DebugLine(AsVec4(idColor::colorCyan), points[4],
                                     points[5], 0, false);
  GetGameLibRenderDebug()->DebugLine(AsVec4(idColor::colorCyan), points[4],
                                     points[6], 0, false);
}

obstaclePath_t& obstaclePath_t::operator=(const obstaclePath_t& other) {
  std::memmove(this, &other, sizeof(*this));
  return *this;
}

obstacleRoute_t& obstacleRoute_t::operator=(const obstacleRoute_t& other) {
  std::memmove(this, &other, sizeof(*this));
  return *this;
}

bool idObstacleAvoidance::SaveQuery(const char* fileName) {
  idFile* const file =
      fileSystem->OpenFileWriteRetail(fileName, FSPATH_SAVE);
  if (file == nullptr) {
    return false;
  }

  constexpr char queryId[] = "ObstacleAvoidanceQuery";
  int queryIdLength = idStr::Length(queryId);
  file->Write(&queryIdLength, sizeof(queryIdLength));
  file->Write(queryId, static_cast<std::size_t>(queryIdLength));
  file->Write(&testQuery.time, sizeof(testQuery.time));
  file->Write(&testQuery.gameMsPerFrame, sizeof(testQuery.gameMsPerFrame));
  file->Write(&testQuery.bbox, sizeof(testQuery.bbox));
  file->Write(&testQuery.gravity, sizeof(testQuery.gravity));
  file->Write(&testQuery.obstacleRadius, sizeof(testQuery.obstacleRadius));
  file->Write(&testQuery.frameMoveDist, sizeof(testQuery.frameMoveDist));
  file->Write(&testQuery.flags, sizeof(testQuery.flags));
  file->Write(&buffers->numObstacles, sizeof(buffers->numObstacles));
  file->Write(&buffers->numVertices, sizeof(buffers->numVertices));
  file->Write(&buffers->numEdges, sizeof(buffers->numEdges));
  file->Write(&buffers->numCorners, sizeof(buffers->numCorners));
  file->Write(buffers->route, sizeof(*buffers->route));
  file->Write(buffers->obstacles,
              sizeof(*buffers->obstacles) * buffers->numObstacles);
  file->Write(buffers->vertices,
              sizeof(*buffers->vertices) * buffers->numVertices);
  file->Write(buffers->edges,
              sizeof(*buffers->edges) * buffers->numEdges);
  file->Write(buffers->corners,
              sizeof(*buffers->corners) * buffers->numCorners);
  delete file;
  return true;
}

bool idObstacleAvoidance::FindPathAroundObstacles(
    obstaclePath_t& path, int time, int gameMsPerFrame,
    const idBounds& bbox, const idVec3& gravity, float obstacleRadius,
    float frameMoveDist, float cornerCircleRadius, int flags,
    const obstacleRoute_t& route, obstacleTraceSet_t* traceSet) {
  InitializePathForQuery(path, route);
  if (buffers == nullptr) {
    return true;
  }

  const bool sameTime = time == lastUpdateTime;
  const int elapsedTime = time - lastUpdateTime;
  if (aas_skipObstacleAvoidance.valueInteger != 0 ||
      (!sameTime && (flags & 1) != 0 && buffers->numObstacles == 0)) {
    obstacleAvoidanceManager->FreeObstacleBuffers(this);
    return true;
  }

  testQuery.time = time;
  testQuery.gameMsPerFrame = gameMsPerFrame;
  testQuery.bbox = bbox;
  testQuery.gravity = gravity;
  testQuery.obstacleRadius = obstacleRadius;
  testQuery.flags = flags;
  testQuery.frameMoveDist = frameMoveDist;
  const char* const saveQueryName = aas_debugSaveQuery.GetString();
  if (saveQueryName[0] != '\0') {
    SaveQuery(saveQueryName);
    aas_debugSaveQuery.SetString("", true);
  }

  const bool useRadarForward = path.useRadarForward;
  const idVec3 radarForward = path.radarForward;
  bool copiedTraceSet = false;
  if (firstUpdateTime == -1 || firstUpdateTime == time ||
      elapsedTime >= 3 * gameMsPerFrame) {
    lastDir = {0.0f, 0.0f, 0.0f};
    lastCorner = {0.0f, 0.0f, 0.0f, 0.0f};
  } else {
    path = *buffers->path;
    lastDir = {path.seekPos[0].x - path.startPosOutsideObstacles.x,
               path.seekPos[0].y - path.startPosOutsideObstacles.y,
               path.seekPos[0].z - path.startPosOutsideObstacles.z};
    lastCorner = path.nextCorner;
    if (traceSet != nullptr) {
      std::memmove(traceSet->traceFractions, buffers->traceFractions,
                   sizeof(float) * traceSet->prevNumTraces);
      std::memmove(buffers->traces, traceSet->traces,
                   sizeof(obstacleTrace_t) * traceSet->numTraces);
      buffers->numTraces = traceSet->numTraces;
      copiedTraceSet = true;
    }
  }

  lastUpdateTime = time;
  if (firstUpdateTime == -1) {
    firstUpdateTime = time;
  }
  *buffers->route = route;
  if (!copiedTraceSet && traceSet != nullptr) {
    std::memmove(buffers->traces, traceSet->traces,
                 sizeof(obstacleTrace_t) * traceSet->numTraces);
    buffers->numTraces = traceSet->numTraces;
  }

  avoidDebugLine_t debugLines[2048];
  avoidDebugText_t debugText[256];
  debugLines[0].color = idObstacleAvoidanceCore::COLOR_INVALID;
  debugText[0].color = idObstacleAvoidanceCore::COLOR_INVALID;

  obstacleParms_t& parms = *buffers->parms;
  parms.path = buffers->path;
  parms.path->done = false;
  parms.path->useRadarForward = useRadarForward;
  parms.path->radarForward = radarForward;
  parms.bbox = bbox;
  parms.gravity = gravity;
  parms.lastDir = lastDir;
  parms.lastCorner = lastCorner;
  parms.obstacleRadius = obstacleRadius;
  parms.frameMoveDist = frameMoveDist;
  parms.cornerCircleRadius = cornerCircleRadius;
  parms.flags = flags;
  parms.numObstacles = buffers->numObstacles;
  parms.numVertices = buffers->numVertices;
  parms.numEdges = buffers->numEdges;
  parms.numCorners = buffers->numCorners;
  parms.numTraces = buffers->numTraces;
  parms.maxDebugLines = 0;
  parms.maxDebugText = 0;
  parms.route = buffers->route;
  parms.obstacles = buffers->obstacles;
  parms.vertices = buffers->vertices;
  parms.edges = buffers->edges;
  parms.corners = buffers->corners;
  parms.traces = buffers->traces;
  parms.traceFractions = buffers->traceFractions;
  parms.debugLines = nullptr;
  parms.debugText = nullptr;
  if (aas_showObstacleAvoidance.valueInteger > 2 ||
      aas_showPathTree.valueInteger != 0) {
    parms.flags |= 0x20;
  }

  if (aas_useObstacleAvoidanceJobs.valueInteger == 1) {
    if (!sameTime) {
      obstacleAvoidanceManager->GetJobList()->AddJob(
          reinterpret_cast<jobRun_t>(ObstacleAvoidanceJob), &parms);
    }
  } else {
    parms.debugLines = debugLines;
    parms.maxDebugLines = 2048;
    parms.debugText = debugText;
    parms.maxDebugText = 256;
    ObstacleAvoidanceJob(&parms);
    parms.debugLines = nullptr;
    parms.debugText = nullptr;
  }

  const idVec3& startPos = route.areas[0].start;
  if (aas_showObstacleAvoidance.valueInteger > 0) {
    for (int index = 0;
         debugLines[index].color != idObstacleAvoidanceCore::COLOR_INVALID;
         ++index) {
      idVec3 start = debugLines[index].start;
      idVec3 end = debugLines[index].end;
      if (aas_showObstacleAvoidance.valueInteger > 1) {
        const float scale =
            (obstacleRadius * 2.5f) / aas_showObstacleScale.valueFloat;
        ProjectTopDown(start, startPos, scale);
        ProjectTopDown(end, startPos, scale);
      }
      GetGameLibRenderDebug()->DebugLine(DebugColor(debugLines[index].color),
                                         start, end, 0, false);
    }

    for (int index = 0;
         debugText[index].color != idObstacleAvoidanceCore::COLOR_INVALID;
         ++index) {
      idVec3 position = debugText[index].pos;
      if (aas_showObstacleAvoidance.valueInteger > 1) {
        ProjectTopDown(
            position, startPos,
            (obstacleRadius * 2.5f) / aas_showObstacleScale.valueFloat);
      }
      GetGameLibRenderDebug()->DebugText(
          debugText[index].text, position, 0.1f,
          DebugColor(debugText[index].color), 1, 0, false, false);
    }
  }
  if (aas_showObstacleAvoidance.valueInteger > 1) {
    DrawBox(startPos, obstacleRadius);
  }
  if (!path.startPosValid) {
    idLib::Warning(
        "GetPointOutsideObstacles: no valid start point found outside "
        "obstacles startPos (%f %f %f)",
        startPos.x, startPos.y, startPos.z);
  }
  if (!path.seekPosValid) {
    idLib::Warning(
        "GetPointOutsideObstacles: no valid seek point found outside "
        "obstacles seekPos (%f %f %f) and (%f %f %f)",
        path.seekPos[0].x, path.seekPos[0].y, path.seekPos[0].z,
        path.seekPos[1].x, path.seekPos[1].y, path.seekPos[1].z);
  }
  return path.hasValidPath;
}

bool idObstacleAvoidance::TestQuery(const char* fileName) {
  idFile* const file =
      fileSystem->OpenFileReadRetail(fileName, true, false);
  if (file == nullptr) {
    return false;
  }

  idStr queryId;
  int queryIdLength = 0;
  file->Read(&queryIdLength, sizeof(queryIdLength));
  if (queryIdLength <= 0) {
    queryId.Empty();
  } else {
    queryId.Fill(' ', queryIdLength);
    file->Read(&queryId[0], static_cast<std::size_t>(queryIdLength));
  }
  if (queryId != "ObstacleAvoidanceQuery") {
    delete file;
    return false;
  }

  file->Read(&testQuery.time, sizeof(testQuery.time));
  file->Read(&testQuery.gameMsPerFrame, sizeof(testQuery.gameMsPerFrame));
  file->Read(&testQuery.bbox, sizeof(testQuery.bbox));
  file->Read(&testQuery.gravity, sizeof(testQuery.gravity));
  file->Read(&testQuery.obstacleRadius, sizeof(testQuery.obstacleRadius));
  file->Read(&testQuery.frameMoveDist, sizeof(testQuery.frameMoveDist));
  file->Read(&testQuery.circleCornerRadius,
             sizeof(testQuery.circleCornerRadius));
  file->Read(&testQuery.flags, sizeof(testQuery.flags));
  file->Read(&buffers->numObstacles, sizeof(buffers->numObstacles));
  file->Read(&buffers->numVertices, sizeof(buffers->numVertices));
  file->Read(&buffers->numEdges, sizeof(buffers->numEdges));
  file->Read(&buffers->numCorners, sizeof(buffers->numCorners));
  file->Read(buffers->route, sizeof(*buffers->route));
  file->Read(buffers->obstacles,
             sizeof(*buffers->obstacles) * buffers->numObstacles);
  file->Read(buffers->vertices,
             sizeof(*buffers->vertices) * buffers->numVertices);
  file->Read(buffers->edges,
             sizeof(*buffers->edges) * buffers->numEdges);
  file->Read(buffers->corners,
             sizeof(*buffers->corners) * buffers->numCorners);
  delete file;

  obstaclePath_t path;
  const bool result = FindPathAroundObstacles(
      path, testQuery.time, testQuery.gameMsPerFrame, testQuery.bbox,
      testQuery.gravity, testQuery.obstacleRadius, testQuery.frameMoveDist,
      testQuery.circleCornerRadius, testQuery.flags, *buffers->route,
      nullptr);
  GetGameLibRenderDebug()->DebugBounds(
      AsVec4(idColor::colorOrange), testQuery.bbox,
      buffers->route->areas[0].start,
      0, false);
  return result;
}
