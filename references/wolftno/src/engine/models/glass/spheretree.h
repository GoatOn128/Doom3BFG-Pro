#pragma once

#include "engine/renderer/render.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idRenderWorld;

enum sphereNodeFlag_t : int {
  SPHERE_NODE_NONE = 0,
  SPHERE_NODE_SUPER = 1,
  SPHERE_NODE_ROOT_TREE = 2,
  SPHERE_NODE_ROOT_NODE = 4,
  SPHERE_NODE_LINKED = 8,
  SPHERE_NODE_RECOMPUTE = 16,
};

struct sphereNode_t {
  sphereNode_t* parent;
  sphereNode_t* child;
  sphereNode_t* prev;
  sphereNode_t* next;
  idVec3 origin;
  float radius;
  float slop;
  std::uint32_t flags;
  int link;
  int id;
};

class idSphereTree {
public:
  idSphereTree();
  ~idSphereTree();

  void Alloc(int maxSpheres, float rootSize, float leafSize, float balance);
  sphereNode_t& AddSphere(const idVec3& origin, float radius,
                          sphereNodeFlag_t flags, int id);
  void RemoveSphere(int sphereIndex);
  void Update();
  void DebugRenderTree(idRenderWorld* renderWorld);
  int CollectSpheresFromView(const idRenderView* view, int* ids,
                             int maxIds);
  bool FirstCollision(const idVec3& origin, float radius, int* id);
  bool NextCollision(const idVec3& origin, float radius, int* id);
  bool LeafCollision(const idVec3& origin, float radius, int* id);

  int maxSpheres{};
  float rootSize{};
  float leafSize{};
  float balance{};
  idList<sphereNode_t, 5> spheres;
  idList<sphereNode_t*, 5> integrate;
  idList<sphereNode_t*, 5> recompute;
  idList<int, 5> heapId;
  idList<const sphereNode_t*, 5> work;
  sphereNode_t* root{};
  sphereNode_t* leaf{};

private:
  void Unlink(sphereNode_t& node);
  bool Recompute(sphereNode_t& node);
  void PrintNode(idRenderWorld* renderWorld, const sphereNode_t* node);
  sphereNode_t& AllocNode();
  void FreeNode(sphereNode_t& node);
  void QueueIntegrate(sphereNode_t& node);
  void SetOrigin(sphereNode_t& node, const idVec3& origin);
  void SetRadius(sphereNode_t& node, float radius);
  void Integrate(sphereNode_t& node, sphereNode_t& super, float size);

  friend struct SphereTreeValidationAccess;
};

extern idCVar g_debugSphereTree;

static_assert(sizeof(sphereNode_t) == 0x40, "sphere-node ABI drift");
static_assert(offsetof(sphereNode_t, parent) == 0x00,
              "sphere-node parent offset drift");
static_assert(offsetof(sphereNode_t, child) == 0x08,
              "sphere-node child offset drift");
static_assert(offsetof(sphereNode_t, prev) == 0x10,
              "sphere-node previous offset drift");
static_assert(offsetof(sphereNode_t, next) == 0x18,
              "sphere-node next offset drift");
static_assert(offsetof(sphereNode_t, origin) == 0x20,
              "sphere-node origin offset drift");
static_assert(offsetof(sphereNode_t, radius) == 0x2c,
              "sphere-node radius offset drift");
static_assert(offsetof(sphereNode_t, slop) == 0x30,
              "sphere-node slop offset drift");
static_assert(offsetof(sphereNode_t, flags) == 0x34,
              "sphere-node flags offset drift");
static_assert(offsetof(sphereNode_t, link) == 0x38,
              "sphere-node link offset drift");
static_assert(offsetof(sphereNode_t, id) == 0x3c,
              "sphere-node ID offset drift");
static_assert(sizeof(idSphereTree) == 0x98, "sphere-tree ABI drift");
static_assert(offsetof(idSphereTree, spheres) == 0x10,
              "sphere-tree sphere-list offset drift");
static_assert(offsetof(idSphereTree, integrate) == 0x28,
              "sphere-tree integrate-list offset drift");
static_assert(offsetof(idSphereTree, recompute) == 0x40,
              "sphere-tree recompute-list offset drift");
static_assert(offsetof(idSphereTree, heapId) == 0x58,
              "sphere-tree heap-list offset drift");
static_assert(offsetof(idSphereTree, work) == 0x70,
              "sphere-tree work-list offset drift");
static_assert(offsetof(idSphereTree, root) == 0x88,
              "sphere-tree root offset drift");
static_assert(offsetof(idSphereTree, leaf) == 0x90,
              "sphere-tree leaf offset drift");
