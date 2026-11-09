#pragma once

#include "engine/gamelib/aas2/aas2callback_avoidlocation.h"
#include "shared/idlib/containers/index.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

enum invalidReachability_t { INVALID_REACHABILITY = -1 };
enum invalidAASTree_t { INVALID_AAS_TREE = -1 };

using idAAS2ReachabilityIndex = idIndex<short, invalidReachability_t>;
using idAAS2TreeIndex = idIndex<int, invalidAASTree_t>;

struct idQuantizedVec3 {
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

struct aas2Reachability_t {
  int travelFlags;
  std::uint16_t travelTime;
  std::uint16_t fromAreaNum;
  std::uint16_t toAreaNum;
  std::uint16_t padding;
  idQuantizedVec3 start;
  idQuantizedVec3 end;
  int areaTTOfsAndNumber;
  idAAS2ReachabilityIndex next;
  idAAS2ReachabilityIndex rev_next;
};

struct aas2Area_t {
  int travelFlags;
  std::uint16_t flags;
  std::int16_t numEdges;
  int firstEdge;
  std::int16_t cluster;
  std::int16_t clusterAreaNum;
  int obstaclePVSOffset;
  idAAS2ReachabilityIndex reach;
  idAAS2ReachabilityIndex rev_reach;
  std::uint16_t firstChokePoint;
  std::uint16_t numChokePoints;
  std::uint16_t firstCover;
  std::uint16_t numCover;
  std::uint16_t firstTraversal;
  std::uint16_t numTraversals;
  std::uint16_t firstHintNode;
  std::uint16_t numHintNodes;
};

struct aas2Node_t {
  unsigned int planeNum;
  unsigned int flags;
  int children[2];
};

struct aas2Portal_t {
  std::uint16_t areaNum;
  std::int16_t clusters[2];
  std::int16_t clusterAreaNum[2];
  std::uint16_t maxAreaTravelTime;
};

struct aas2Cluster_t {
  int numAreas;
  int numReachableAreas;
  int numPortals;
  int firstPortal;
};

class idAAS2Settings {
public:
  std::uint8_t recoveredPrefix[0x80];
  idBounds boundingBox;
  std::uint8_t recoveredBeforeSpeeds[0x34];
  float groundSpeed;
  float waterSpeed;
  std::uint8_t recoveredTail[0x5c];
};

class idAAS2File {
public:
  idAAS2ReachabilityIndex FindReachabilityByName(const char* name) const;
  int BoundsAreaNums(idAAS2TreeIndex tree, const idBounds& bounds,
                    int* areas, int maxAreas) const;
  int GetTraversalsForReachability(
      idAAS2ReachabilityIndex reachIndex,
      idList<int, 5>& traversalList) const;

  std::uint8_t recoveredResource[0x30];
  unsigned int crc;
  unsigned int timestamp;
  idList<int, 37> visitedAreas;
  idAAS2Settings settings;
  int majorVersion;
  int minorVersion;
  int firstFakeVertex;
  int firstFakeEdge;
  int firstFakeEdgeIndex;
  int firstFakeArea;
  bool needsReload;
  std::uint8_t recoveredBeforeTrees[7];
  std::uint8_t trees[0x18];
  idList<idPlane, 37> planes;
  idList<idVec3, 37> vertices;
  std::uint8_t edges[0x18];
  idList<int, 37> edgeIndex;
  idList<aas2Reachability_t, 37> reachabilities;
  idList<aas2Area_t, 37> areas;
  idList<aas2Node_t, 37> nodes;
  idList<aas2Portal_t, 37> portals;
  idList<int, 37> portalIndex;
  idList<aas2Cluster_t, 37> clusters;
  std::uint8_t recoveredTail[0x138];
};

struct idAAS2Goal {
  int areaNum;
  idVec3 origin;
};

struct idAAS2NearestReachable {
  int nearestDestArea;
  idVec3 nearestDestPos;
  int nearestTravelTime;
  int quickestDestArea;
  idVec3 quickestDestPos;
  int quickestTravelTime;
};

class idAAS2TravelSpeeds {
public:
  float defaultSpeed;
  float swimSpeed;
  float flySpeed;
  float crouchSpeed;
};

class idAASRoutingCache {
public:
  explicit idAASRoutingCache(int cacheSize);

  int type;
  int size;
  int cluster;
  int areaNum;
  int travelFlags;
  std::uint32_t recoveredPadding;
  idAASRoutingCache* next;
  idAASRoutingCache* prev;
  idAASRoutingCache* time_next;
  idAASRoutingCache* time_prev;
  std::uint16_t startTravelTime;
  std::uint8_t recoveredBeforeArrays[6];
  unsigned char* reachabilities;
  unsigned short* travelTimes;
};

class idRoutingUpdate {
public:
  int cluster;
  int areaNum;
  std::uint16_t tmpTravelTime;
  std::uint8_t recoveredBeforeTravelTimes[6];
  unsigned short* areaTravelTimes;
  idVec3 start;
  std::uint32_t recoveredBeforeLinks;
  idRoutingUpdate* next;
  idRoutingUpdate* prev;
  bool isInList;
  std::uint8_t recoveredTail[7];
};

class idAAS2 {
public:
#define ID_AAS2_ROUTING_SLOT(slot)                                            \
  virtual std::intptr_t RoutingSlot##slot() const { return 0; }
  ID_AAS2_ROUTING_SLOT(00);
  ID_AAS2_ROUTING_SLOT(01);
  ID_AAS2_ROUTING_SLOT(02);
  ID_AAS2_ROUTING_SLOT(03);
  ID_AAS2_ROUTING_SLOT(04);
  ID_AAS2_ROUTING_SLOT(05);
  ID_AAS2_ROUTING_SLOT(06);
  ID_AAS2_ROUTING_SLOT(07);
  ID_AAS2_ROUTING_SLOT(08);
  ID_AAS2_ROUTING_SLOT(09);
  ID_AAS2_ROUTING_SLOT(10);
  ID_AAS2_ROUTING_SLOT(11);
  ID_AAS2_ROUTING_SLOT(12);
  virtual bool NearestPointReachable(
      idAAS2TreeIndex, const idVec3&, int, const idVec3&, const idBounds&,
      int, idAAS2NearestReachable&) const {
    return false;
  }
  virtual bool PushPointIntoAreaNum(int, idVec3&) const { return false; }
  ID_AAS2_ROUTING_SLOT(15);
  virtual idVec3 AreaCenter(int) const { return {0.0f, 0.0f, 0.0f}; }
  ID_AAS2_ROUTING_SLOT(17);
  ID_AAS2_ROUTING_SLOT(18);
  ID_AAS2_ROUTING_SLOT(19);
  ID_AAS2_ROUTING_SLOT(20);
  ID_AAS2_ROUTING_SLOT(21);
  ID_AAS2_ROUTING_SLOT(22);
  ID_AAS2_ROUTING_SLOT(23);
  virtual bool ChangeAreaTravelFlags(const idBounds&, int, int, bool,
                                     const char*) {
    return false;
  }
  virtual bool ChangeReachabilityTravelFlags(const char*, int, bool) {
    return false;
  }
  virtual bool ChangeReachabilityTravelFlags(idAAS2ReachabilityIndex, int,
                                             bool) {
    return false;
  }
  virtual int TravelTimeToGoalArea(int, const idVec3&, int, const idVec3&,
                                   int, const idAAS2TravelSpeeds&) const {
    return 0;
  }
  virtual bool RouteToGoalArea(int, const idVec3&, int, const idVec3&, int,
                               int&, idAAS2ReachabilityIndex&) const {
    return false;
  }
  virtual bool FindNearestGoal(idAAS2Goal&, int, const idVec3&, int,
                               idAAS2Callback&) const {
    return false;
  }
#undef ID_AAS2_ROUTING_SLOT
};

class idAAS2Local : public idAAS2 {
public:
  bool NearestPointReachable(
      idAAS2TreeIndex tree, const idVec3& source, int sourceAreaNum,
      const idVec3& destination, const idBounds& destinationBounds,
      int travelFlags, idAAS2NearestReachable& nearestInfo) const override;
  bool ChangeAreaTravelFlags(const idBounds& bounds, int areaFlags,
                             int travelFlags, bool set,
                             const char* debugName) override;
  bool ChangeReachabilityTravelFlags(const char* name, int travelFlag,
                                     bool set) override;
  bool ChangeReachabilityTravelFlags(idAAS2ReachabilityIndex index,
                                     int travelFlag, bool set) override;
  int TravelTimeToGoalArea(int startAreaNum, const idVec3& startOrigin,
                           int goalAreaNum, const idVec3& goalOrigin,
                           int travelFlags,
                           const idAAS2TravelSpeeds& speeds) const override;
  bool RouteToGoalArea(int startAreaNum, const idVec3& startOrigin,
                       int goalAreaNum, const idVec3& goalOrigin,
                       int travelFlags, int& travelTime,
                       idAAS2ReachabilityIndex& reachIndex) const override;
  bool FindNearestGoal(idAAS2Goal& goal, int startAreaNum,
                       const idVec3& startOrigin, int travelFlags,
                       idAAS2Callback& callback) const override;

  idAAS2File* file;
  idStr name;
  bool defaulted;
  std::uint8_t recoveredBeforeVerify[7];
  struct verify_t {
    int currentIndex;
    int numInvalidVertices;
    int* invalidVertices;
    void* clipModel;
  } verify;
  mutable idAASRoutingCache*** areaCacheIndex;
  mutable int areaCacheIndexSize;
  std::uint32_t recoveredBeforePortalCache;
  mutable idAASRoutingCache** portalCacheIndex;
  mutable int portalCacheIndexSize;
  std::uint32_t recoveredBeforeAreaUpdate;
  mutable idRoutingUpdate* areaUpdate;
  mutable idRoutingUpdate* portalUpdate;
  mutable unsigned short* goalAreaTravelTimes;
  mutable unsigned short* areaTravelTimes;
  mutable int numAreaTravelTimes;
  std::uint32_t recoveredBeforeCacheList;
  mutable idAASRoutingCache* cacheListStart;
  mutable idAASRoutingCache* cacheListEnd;
  mutable int totalCacheMemory;
  float groundSpeedMultiplier;
  float waterSpeedMultiplier;
  std::uint8_t recoveredTail[0x6c];

private:
  bool SetupRouting();
  void ShutdownRouting();
  unsigned short CalcTravelTime(const idVec3& start, const idVec3& end,
                                int travelFlags) const;
  unsigned short AreaTravelTime(int areaNum, const idVec3& start,
                                const idVec3& end) const;
  void CalculateAreaTravelTimes();
  void SetupRoutingCache();
  void DeleteClusterCache(int clusterNum);
  void DeletePortalCache();
  void ShutdownRoutingCache();
  void RoutingStats() const;
  void UnlinkCache(idAASRoutingCache* cache) const;
  void DeleteOldestCache() const;
  int ClusterAreaNum(int clusterNum, int areaNum) const;
  void UpdateAreaRoutingCache(idAASRoutingCache* cache) const;
  idAASRoutingCache* GetAreaRoutingCache(int clusterNum, int areaNum,
                                        int travelFlags) const;
  void UpdatePortalRoutingCache(idAASRoutingCache* cache) const;
  idAASRoutingCache* GetPortalRoutingCache(int clusterNum, int areaNum,
                                          int travelFlags) const;
  bool GetClusterRoute(int startAreaNum, const idVec3& startOrigin,
                       int startClusterNum, int goalAreaNum, int travelFlags,
                       int& travelTime,
                       idAAS2ReachabilityIndex& bestReachIndex) const;
  void RemoveRoutingCacheUsingArea(int areaNum);
  bool ChangeAreaTravelFlags_r(int nodeNum, const idBounds& bounds,
                               int areaFlags, int travelFlags, bool set);

  friend struct AAS2RoutingValidationAccess;
};

static_assert(sizeof(idQuantizedVec3) == 0x06,
              "routing quantized vector ABI drift");
static_assert(sizeof(aas2Reachability_t) == 0x20,
              "routing reachability ABI drift");
static_assert(sizeof(aas2Area_t) == 0x28, "routing area ABI drift");
static_assert(sizeof(aas2Node_t) == 0x10, "routing node ABI drift");
static_assert(sizeof(aas2Portal_t) == 0x0c, "routing portal ABI drift");
static_assert(sizeof(aas2Cluster_t) == 0x10, "routing cluster ABI drift");
static_assert(sizeof(idAAS2Settings) == 0x130,
              "routing settings ABI drift");
static_assert(offsetof(idAAS2Settings, boundingBox) == 0x80,
              "routing bounds offset drift");
static_assert(offsetof(idAAS2Settings, groundSpeed) == 0xcc,
              "routing ground speed offset drift");
static_assert(sizeof(idAAS2File) == 0x3e0, "routing file ABI drift");
static_assert(offsetof(idAAS2File, settings) == 0x50,
              "routing settings offset drift");
static_assert(offsetof(idAAS2File, reachabilities) == 0x218,
              "routing reachability-list offset drift");
static_assert(offsetof(idAAS2File, areas) == 0x230,
              "routing area-list offset drift");
static_assert(offsetof(idAAS2File, nodes) == 0x248,
              "routing node-list offset drift");
static_assert(offsetof(idAAS2File, portals) == 0x260,
              "routing portal-list offset drift");
static_assert(offsetof(idAAS2File, portalIndex) == 0x278,
              "routing portal-index offset drift");
static_assert(offsetof(idAAS2File, clusters) == 0x290,
              "routing cluster-list offset drift");
static_assert(sizeof(idAAS2Goal) == 0x10, "routing goal ABI drift");
static_assert(sizeof(idAAS2NearestReachable) == 0x28,
              "routing nearest-result ABI drift");
static_assert(sizeof(idAAS2TravelSpeeds) == 0x10,
              "routing travel-speeds ABI drift");
static_assert(sizeof(idAASRoutingCache) == 0x50,
              "routing cache ABI drift");
static_assert(offsetof(idAASRoutingCache, reachabilities) == 0x40,
              "routing cache reachability offset drift");
static_assert(sizeof(idRoutingUpdate) == 0x40,
              "routing update ABI drift");
static_assert(offsetof(idRoutingUpdate, start) == 0x18,
              "routing update start offset drift");
static_assert(sizeof(idAAS2) == 0x08, "routing interface ABI drift");
static_assert(offsetof(idAAS2Local, file) == 0x08,
              "routing local file offset drift");
static_assert(offsetof(idAAS2Local, areaCacheIndex) == 0x58,
              "routing area-cache offset drift");
static_assert(offsetof(idAAS2Local, portalCacheIndex) == 0x68,
              "routing portal-cache offset drift");
static_assert(offsetof(idAAS2Local, areaUpdate) == 0x78,
              "routing area-update offset drift");
static_assert(offsetof(idAAS2Local, goalAreaTravelTimes) == 0x88,
              "routing goal-times offset drift");
static_assert(offsetof(idAAS2Local, cacheListStart) == 0xa0,
              "routing cache-list offset drift");
static_assert(offsetof(idAAS2Local, groundSpeedMultiplier) == 0xb4,
              "routing speed offset drift");
static_assert(sizeof(idAAS2Local) == 0x128,
              "routing local AAS ABI drift");
