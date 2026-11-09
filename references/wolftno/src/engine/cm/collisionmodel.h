#pragma once

#include "engine/common/contents.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/geometry/tracemodel.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

enum cmType_t { CM_TYPE_POLYGON, CM_TYPE_SPHERE, CM_TYPE_GRID };

struct cmEdge_t {
  std::uint32_t vertex[2]{};
  idVec3 normal{};
};

struct cmPolygon_t {
  idPlane plane{};
  idBounds bounds{};
  std::vector<std::uint32_t> vertices;
  std::uint32_t contents{CONTENTS_SOLID};
  int material{};
};

struct cmPolytope_t {
  std::vector<idPlane> planes;
  idBounds bounds{};
  std::uint32_t contents{CONTENTS_SOLID};
};

struct cmModelStats_t {
  std::size_t vertices{};
  std::size_t edges{};
  std::size_t polygons{};
  std::size_t polytopes{};
  std::size_t bytes{};
};

struct contactInfo_t {
  enum contactType_t {
    CONTACT_NONE = 0,
    CONTACT_EDGE = 1,
    CONTACT_TRMVERTEX = 2,
    CONTACT_SPHERE = 3,
  };

  contactType_t type{CONTACT_NONE};
  idVec3 point{};
  idVec3 normal{};
  union {
    float dist{};
    float distance;
  };
  float separation{};
  union {
    std::uint32_t contentFlags{};
    std::uint32_t contents;
  };
  union {
    int surfaceFlags{};
    int material;
  };
  int surfaceType{};
  int modelFeature{-1};
  union {
    int trmFeature{-1};
    int traceFeature;
  };
  int entityNum{-1};
  int physicsId{};
  int bodyId{};
  int selfId{};
  std::uint8_t flags{};
  std::uint8_t surfaceColor[3]{};
};

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)
#endif
struct trace_t {
  float fraction{1.0f};
  idVec3 endpos{};
  idMat3 endAxis{mat3_identity};
  union {
    contactInfo_t c{};
    contactInfo_t contact;
    struct {
      std::uint8_t recoveredContactPrefix[74];
      bool startSolid;
      bool allSolid;
    };
  };
#if defined(WOLFTNO_RETAIL_TRACE_ASSIGN_ABI)
  trace_t& operator=(const trace_t& other);
#endif
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

static_assert(sizeof(contactInfo_t) == 0x4C,
              "contactInfo_t ABI drift");
static_assert(offsetof(contactInfo_t, point) == 0x04,
              "contact point offset drift");
static_assert(offsetof(contactInfo_t, normal) == 0x10,
              "contact normal offset drift");
static_assert(offsetof(contactInfo_t, contentFlags) == 0x24,
              "contact contents offset drift");
static_assert(offsetof(contactInfo_t, entityNum) == 0x38,
              "contact entity offset drift");
static_assert(sizeof(trace_t) == 0x80, "trace_t ABI drift");
static_assert(offsetof(trace_t, endpos) == 0x04,
              "trace end-position offset drift");
static_assert(offsetof(trace_t, endAxis) == 0x10,
              "trace end-axis offset drift");
static_assert(offsetof(trace_t, c) == 0x34,
              "trace contact offset drift");

class idCollisionModel {
public:
  virtual ~idCollisionModel();
  virtual const char* GetName() const = 0;
  virtual cmType_t GetType() const = 0;
  virtual const idBounds& GetBounds() const = 0;
  virtual std::uint32_t GetContents() const = 0;
  virtual cmModelStats_t GetPolytopeStats() const = 0;
};

// Narrow model-loading boundary for modules that cannot include the current
// collision builder's temporary material declaration.
idCollisionModel* LoadCollisionModelByName(const char* path);

class idCollisionModelLocal final : public idCollisionModel {
public:
  idCollisionModelLocal();

  const char* GetName() const override { return name.data; }
  cmType_t GetType() const override { return type; }
  const idBounds& GetBounds() const override;
  std::uint32_t GetContents() const override;
  cmModelStats_t GetPolytopeStats() const override;

  const idVec3* GetVertex(std::size_t index) const;
  const cmEdge_t* GetEdge(std::size_t index) const;
  const cmPolygon_t* GetPolygon(std::size_t index) const;
  const cmPolytope_t* GetPolytope(std::size_t index) const;
  std::size_t GetPolytopes(const idBounds& query, const cmPolytope_t** output,
                           std::size_t capacity) const;
  std::size_t GetTotalMemory() const;
  std::size_t GetLoadedMemory() const;
  std::size_t GetMaxResidentMemory() const;
  idBounds GetBox() const;
  int GetJoint(std::size_t index) const;
  static const std::vector<idCollisionModelLocal*>& GetResourceList();
  static bool ValidateStreamFileHeader(const void* data, std::size_t size);

  void MakeDefault();
  void FreeData();
  bool Write_Binary(const char* path) const;
  bool Load_Binary(const char* path);
  bool ReloadIfStale();
  bool LoadResource(const char* path);

  idStr name{"default"};
  cmType_t type{CM_TYPE_POLYGON};
  idBounds bounds{};
  std::vector<idVec3> vertices;
  std::vector<cmEdge_t> edges;
  std::vector<cmPolygon_t> polygons;
  std::vector<cmPolytope_t> polytopes;
  float sphereRadius{};
};

bool CM_BoundsIntersect(const idBounds& first, const idBounds& second);
idBounds CM_TranslatedBounds(const idBounds& bounds, const idVec3& origin);
idPlane CM_GetPolygonPlane(const cmPolygon_t& polygon);

struct cmStreamArea_t {
  const std::uint8_t* data{};
  std::size_t size{};
};

bool SetupStreamAreaPtrs(const void* data, std::size_t size, std::vector<cmStreamArea_t>& areas);
std::shared_ptr<std::vector<std::uint8_t>> AcquireSubModelData(const void* data, std::size_t size);
void ReleaseSubModelData(std::shared_ptr<std::vector<std::uint8_t>>& data);
void SwapModelTreeNodes(std::vector<std::uint32_t>& nodes);
void SwapSubModelHeader(cmModelStats_t& header);
void SwapSubModelData(idCollisionModelLocal& model);
void SwapStreamAreas(std::vector<cmStreamArea_t>& areas);
void SwapBigSphereModelStruct(idCollisionModelLocal& model);
void SwapBigSphereModelData(std::vector<float>& sphereData);
