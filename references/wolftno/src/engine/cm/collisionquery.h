#pragma once

#include "engine/cm/collisionmodel.h"

#include <functional>
#include <mutex>

struct queryParms_t {
  idVec3 start{};
  idVec3 end{};
  idMat3 axis{mat3_identity};
  const idTraceModel* traceModel{};
  std::uint32_t contentsMask{CONTENTS_ALL};
  queryParms_t& operator=(const queryParms_t& other);
};

struct idTraceWork {
  queryParms_t parms;
  idBounds sweptBounds{};
  std::vector<contactInfo_t> contacts;
  void Init(const queryParms_t& source);
};

struct idModelCheckCounts {
  std::size_t vertices{};
  std::size_t edges{};
  std::size_t polygons{};
  void SetupForSubModel(const idCollisionModelLocal& model);
};

struct slideMoveState_t {
  idVec3 position{};
  idVec3 velocity{};
  std::vector<idVec3> clipPlanes;
  bool blocked{};
};

struct contactsResult_t {
  std::vector<contactInfo_t> contacts;
  void CopyFrom(const contactsResult_t& other);
};

struct clipResult_t {
  std::vector<idVec3> vertices;
  std::vector<std::uint32_t> indices;
  clipResult_t& operator=(const clipResult_t& other);
};

class idPolygonModelCollisionDetection {
public:
  static const idCollisionModelLocal* SetupSubModelPtrsFromData(const idCollisionModelLocal& model);
  static idCollisionModelLocal SetupSubModelForBounds(const idBounds& bounds,
                                                      std::uint32_t contents);
  static idBounds LocalExtentsFromUnTransformedBounds(const idBounds& bounds, const idVec3& origin);
  static bool TestStuckInSubModelBounds(const idCollisionModelLocal& model,
                                        const queryParms_t& parms);
  static bool TraceThroughSubModel(const idCollisionModelLocal& model, const queryParms_t& parms,
                                   trace_t& result);
  static bool TraceThroughSubModelTree(const idCollisionModelLocal& model,
                                       const queryParms_t& parms, trace_t& result);
  static std::vector<const idCollisionModelLocal*>
  GetSubModelsForTrace(const idCollisionModelLocal& model, const queryParms_t& parms);
  static trace_t StartTranslation(const idCollisionModelLocal& model, const queryParms_t& parms);
  static trace_t StartTranslationPoint(const idCollisionModelLocal& model,
                                       const queryParms_t& parms);
  static trace_t FinishTranslation(const trace_t& trace);
  static idTraceWork TranslationSetup(const queryParms_t& parms);
  static std::vector<std::size_t> TranslationUsedPrimitives(const idCollisionModelLocal& model,
                                                            const idBounds& sweptBounds);
  static std::vector<idPlane> TranslationHeartPlanes(const idCollisionModelLocal& model);
  static std::vector<idVec3> TranslationVerts(const idTraceModel& traceModel, const idVec3& origin);
  static std::vector<cmEdge_t> TranslationEdges(const idTraceModel& traceModel);
  static std::vector<std::size_t> TranslationPolys(const idTraceModel& traceModel);
  static idBounds TranslationBounds(const queryParms_t& parms);
  static void TranslationUpdateBounds(idBounds& bounds, const idVec3& point);
  static std::vector<float> TranslationPlueckerCache(const std::vector<idVec3>& vertices,
                                                     const idVec3& direction);
  static std::vector<float> TranslationEdgePlueckerCache(const std::vector<cmEdge_t>& edges,
                                                         const std::vector<idVec3>& vertices);
  static std::vector<int> TranslationSideCache(const std::vector<float>& values);
  static std::vector<int> TranslationPolygonSideCache(const idCollisionModelLocal& model,
                                                      const idVec3& point);
  static void AddContact(contactsResult_t& contacts, const contactInfo_t& contact,
                         std::size_t maximum = 64);
  static bool TranslateEdgeThroughEdge(const idVec3& movingStart, const idVec3& movingEnd,
                                       const idVec3& edgeStart, const idVec3& edgeEnd,
                                       float& fraction);
  static bool TranslatePointThroughPlane(const idVec3& start, const idVec3& end,
                                         const idPlane& plane, float& fraction);
  static bool TranslatePointThroughPolygon(const idVec3& start, const idVec3& end,
                                           const cmPolygon_t& polygon, float& fraction);
  static bool TranslateTrmEdgesThroughPolygon(const idTraceModel& traceModel, const idVec3& start,
                                              const idVec3& end, const cmPolygon_t& polygon,
                                              float& fraction);
  static bool TranslateTrmVertsThroughPolygon(const idTraceModel& traceModel, const idVec3& start,
                                              const idVec3& end, const cmPolygon_t& polygon,
                                              float& fraction);
  static bool TranslatePolygonVertsThroughTrm(const cmPolygon_t& polygon,
                                              const idCollisionModelLocal& model,
                                              const idTraceModel& traceModel, const idVec3& start,
                                              const idVec3& end, float& fraction);
  static bool TranslateTrmThroughPolygon(const idTraceModel& traceModel, const idVec3& start,
                                         const idVec3& end, const cmPolygon_t& polygon,
                                         const idCollisionModelLocal& model, float& fraction);
  static int StartContents(const idCollisionModelLocal& model, const queryParms_t& parms);
  static int StartContentsPoint(const idCollisionModelLocal& model, const idVec3& point,
                                std::uint32_t mask);
  static int FinishContents(int contents);
  static bool TestTrmVertsInPolytope(const idTraceModel& traceModel, const idVec3& origin,
                                     const cmPolytope_t& polytope);
  static bool TestTrmInPolygon(const idTraceModel& traceModel, const idVec3& origin,
                               const cmPolygon_t& polygon);
  static contactsResult_t StartContacts(const idCollisionModelLocal& model,
                                        const queryParms_t& parms, float distance);
  static contactsResult_t FinishContacts(const contactsResult_t& contacts);
  static bool TestTrmEdgeInContactWithPolygon(const idVec3& start, const idVec3& end,
                                              const cmPolygon_t& polygon, float epsilon,
                                              contactInfo_t* contact = nullptr);
  static bool TestTrmVertexInContactWithPolygon(const idVec3& vertex, const cmPolygon_t& polygon,
                                                float epsilon, contactInfo_t* contact = nullptr);
  static bool TestVertexInContactWithTrmPolygon(const idVec3& vertex,
                                                const idTraceModel& traceModel,
                                                const idVec3& origin, float epsilon);
  static bool TestTrmInContactWithPolygon(const idTraceModel& traceModel, const idVec3& origin,
                                          const cmPolygon_t& polygon, float epsilon,
                                          contactInfo_t* contact = nullptr);
  static clipResult_t StartClip(const idCollisionModelLocal& model, const queryParms_t& parms);
  static clipResult_t FinishClip(const clipResult_t& result);
  static bool ClipInPlace(std::vector<idVec3>& vertices, const idPlane& plane,
                          float epsilon = 0.001f);
  static clipResult_t ClipPolygonWithTrm(const cmPolygon_t& polygon,
                                         const idCollisionModelLocal& model,
                                         const idTraceModel& traceModel, const idVec3& origin,
                                         const idMat3& axis);
  static trace_t StartRotation(const idCollisionModelLocal& model, const queryParms_t& parms,
                               const idRotation& rotation);
  static trace_t StartRotationPoint(const idCollisionModelLocal& model, const queryParms_t& parms,
                                    const idRotation& rotation);
  static trace_t FinishRotation(const trace_t& trace);
  static queryParms_t RotationSetup(const queryParms_t& parms, const idRotation& rotation);
  static std::vector<idVec3> RotationVerts(const idTraceModel& traceModel,
                                           const idRotation& rotation);
  static std::vector<cmEdge_t> RotationEdges(const idTraceModel& traceModel);
  static std::vector<std::size_t> RotationPolys(const idTraceModel& traceModel);
  static idBounds RotationBounds(const queryParms_t& parms, const idRotation& rotation);
  static std::vector<float> RotationEdgePlueckerCache(const std::vector<cmEdge_t>& edges,
                                                      const std::vector<idVec3>& vertices);
  static std::vector<bool> RotationCullPolygonEdges(const cmPolygon_t& polygon, const idVec3& axis);
  static idVec3 TransformFromOriginAxisAngle(const idVec3& point, const idVec3& origin,
                                             const idVec3& axis, float angle);
  static idMat3 TransformAxisToZAxis(const idVec3& axis);
  static bool LineIntersectsBoundsShort(const idVec3& start, const idVec3& end,
                                        const idBounds& bounds);
  static bool CollisionBetweenEdgeBounds(const idVec3& firstStart, const idVec3& firstEnd,
                                         const idVec3& secondStart, const idVec3& secondEnd,
                                         float epsilon = 0.001f);
  static bool RotateEdgeThroughEdge(const idVec3& movingStart, const idVec3& movingEnd,
                                    const idVec3& edgeStart, const idVec3& edgeEnd,
                                    const idRotation& rotation, float& fraction);
  static idVec3 EdgeFurthestFromEdge(const idVec3& first, const idVec3& second,
                                     const idVec3& edgeStart, const idVec3& edgeEnd);
  static bool RotatePointThroughPlane(const idVec3& point, const idRotation& rotation,
                                      const idPlane& plane, float& fraction);
  static idVec3 PointFurthestFromPlane(const std::vector<idVec3>& points, const idPlane& plane);
  static bool RotatePointThroughEpsilonPlane(const idVec3& point, const idRotation& rotation,
                                             const idPlane& plane, float epsilon, float& fraction);
  static bool RotateTrmEdgesThroughPolygon(const idTraceModel& traceModel,
                                           const idRotation& rotation, const cmPolygon_t& polygon,
                                           float& fraction);
  static bool RotateTrmVertsThroughPolygon(const idTraceModel& traceModel,
                                           const idRotation& rotation, const cmPolygon_t& polygon,
                                           float& fraction);
  static bool RotatePolygonVertsThroughTrm(const cmPolygon_t& polygon,
                                           const idCollisionModelLocal& model,
                                           const idTraceModel& traceModel,
                                           const idRotation& rotation, float& fraction);
  static bool RotateTrmThroughPolygon(const idTraceModel& traceModel, const idRotation& rotation,
                                      const cmPolygon_t& polygon,
                                      const idCollisionModelLocal& model, float& fraction);
  static bool TraceThroughModel(const idCollisionModelLocal& model, const queryParms_t& parms,
                                trace_t& result);
  static std::unique_ptr<idTraceWork> AllocTraceWork(const queryParms_t& parms);
  static std::size_t GetTraceWorkSPUSize();
};

class idSphereModelCollisionDetection {
public:
  static bool SetupCollisionSpherePtrs(const idCollisionModelLocal& model, idVec3& center,
                                       float& radius);
  static trace_t TraceThroughModel(const idCollisionModelLocal& model, const queryParms_t& parms);
};

class idCollisionQueryExecute {
public:
  static trace_t ExecutePolygonModelQuery(const idCollisionModelLocal& model,
                                          const queryParms_t& parms);
  static trace_t ExecuteSphereModelQuery(const idCollisionModelLocal& model,
                                         const queryParms_t& parms);
  static trace_t ExecuteSubModelQuery(const idCollisionModelLocal& model,
                                      const queryParms_t& parms);
  static bool TestPolygonModelQuery(const idCollisionModelLocal& model, const queryParms_t& parms);
  static bool TestSubModelQuery(const idCollisionModelLocal& model, const queryParms_t& parms);
  static queryParms_t SetupDependentParms(const queryParms_t& parent, const idVec3& modelOrigin,
                                          const idMat3& modelAxis = mat3_identity);
};

class idCollisionDetectionMerge {
public:
  static trace_t MergeTraceResults(const std::vector<trace_t>& traces);
  static int MergeContentsResults(const std::vector<int>& contents);
  static contactsResult_t MergeContactsResults(const std::vector<contactsResult_t>& contacts);
  static clipResult_t MergeClipResults(const std::vector<clipResult_t>& clips);
  static idVec3 ClipVelocity(const idVec3& input, const idVec3& normal, float overbounce = 1.001f);
  static void InitSlideMoveState(slideMoveState_t& state, const idVec3& position,
                                 const idVec3& velocity);
  static bool UpdateSlideMoveState(slideMoveState_t& state, const trace_t& trace);
  static trace_t FinishSlideMoveState(const slideMoveState_t& state, const trace_t& lastTrace);
  static trace_t MergeMotionResults(const trace_t& translation, const trace_t& rotation);
  static trace_t MergeStepMoveResults(const trace_t& forward, const trace_t& stepUp,
                                      const trace_t& stepDown);
  static std::vector<trace_t> MergeMultiTraceResults(const std::vector<trace_t>& traces,
                                                     std::size_t maxHits);
  static trace_t MergeQueryResults(const std::vector<trace_t>& traces);
};

idVec3 ClipVelocity(const idVec3& input, const idVec3& normal, float overbounce = 1.001f);
planeSide_t CM_BoundsPlaneSide(const idBounds& bounds, const idPlane& plane, float epsilon = 0.0f);
bool CM_BoundsShortPlaneCull(const idBounds& bounds, const idPlane& plane, float epsilon = 0.0f);
float CM_TanZeroHalfPI(float angleRadians);
float CM_ArcTanPositive(float value);
idBounds CM_PointRotationBounds(const idVec3& point, const idRotation& rotation);

trace_t CM_TraceBounds(const idBounds& target, std::uint32_t contents, const queryParms_t& parms);
bool CM_PointInsideBounds(const idVec3& point, const idBounds& bounds, float epsilon = 0.0f);
