#include "engine/models/glass/spheretree.h"

#include "engine/models/renderworld.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

extern idCVar r_zfar;
extern idCVar r_znear;

idCVar g_debugSphereTree(
    "g_debugSphereTree", "0", CVAR_INTEGER,
    "0 dont render, 1 render text on nodes and spheres on leaf nodes, 2 "
    "spheres on all nodes, 3 show links");

template <typename T>
void* idListArrayNew(int count, memTag_t, bool) {
  T* values = new (std::nothrow) T[count];
  if (values != nullptr) {
    std::memset(values, 0, static_cast<std::size_t>(count) * sizeof(T));
    for (int index = 0; index < count; ++index)
      values[index].link = -1;
  }
  return values;
}

template <typename T>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer) {
  T* replacement = static_cast<T*>(
      idListArrayNew<T>(newCount, tag, zeroBuffer));
  if (replacement == nullptr)
    return nullptr;
  T* oldValues = static_cast<T*>(oldPointer);
  const int copyCount = std::min(oldCount, newCount);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = oldValues[index];
  delete[] oldValues;
  return replacement;
}

template <>
bool idList<sphereNode_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;
  auto* replacement = static_cast<sphereNode_t*>(idListArrayResize<sphereNode_t>(
      list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (replacement == nullptr)
    return false;
  list = replacement;
  size = newSize;
  num = std::min(num, newSize);
  listStatic = 0;
  return true;
}

namespace {

constexpr std::uint32_t kNodeIndexMask = 0xffffff00u;
constexpr std::uint32_t kNodeFlagMask = 0x000000ffu;
constexpr std::uint32_t kIntegrateFlag = 0x20u;

int NodeIndex(const sphereNode_t& node) {
  return static_cast<int>(node.flags >> 8);
}

float DistanceSquared(const idVec3& left, const idVec3& right) {
  return (left - right).LengthSqr();
}

void LinkFirst(sphereNode_t& parent, sphereNode_t& node) {
  sphereNode_t* first = parent.child;
  if (first != nullptr)
    first->prev = &node;
  parent.child = &node;
  node.flags |= SPHERE_NODE_LINKED;
  node.next = first;
  node.prev = nullptr;
  node.parent = &parent;
}

void RemovePointer(idList<sphereNode_t*, 5>& list, sphereNode_t* node) {
  for (int index = 0; index < list.num; ++index) {
    if (list[index] == node) {
      list.RemoveIndex(index);
      return;
    }
  }
}

} // namespace

void idSphereTree::Unlink(sphereNode_t& node) {
  if ((node.flags & SPHERE_NODE_LINKED) == 0)
    return;
  if (node.parent != nullptr && node.parent->child == &node)
    node.parent->child = node.next;
  if (node.prev != nullptr)
    node.prev->next = node.next;
  if (node.next != nullptr)
    node.next->prev = node.prev;
  node.parent = nullptr;
  node.next = nullptr;
  node.prev = nullptr;
  node.flags &= ~static_cast<std::uint32_t>(SPHERE_NODE_LINKED);
}

bool idSphereTree::Recompute(sphereNode_t& node) {
  node.flags &= ~static_cast<std::uint32_t>(SPHERE_NODE_RECOMPUTE);
  sphereNode_t* child = node.child;
  if (child == nullptr)
    return true;
  if ((node.flags & SPHERE_NODE_ROOT_NODE) != 0)
    return false;

  idVec3 center = vec3_origin;
  int count = 0;
  for (sphereNode_t* current = child; current != nullptr;
       current = current->next) {
    center = center + current->origin;
    ++count;
  }
  center = center * (1.0f / static_cast<float>(count));

  float maximumRadius = -1.0e30f;
  for (sphereNode_t* current = child; current != nullptr;
       current = current->next) {
    const float allowed = current->radius - node.radius + balance;
    if (allowed * allowed > DistanceSquared(current->origin, center))
      break;
    maximumRadius = std::fmax(maximumRadius, current->radius);
    if (current->next == nullptr) {
      node.origin = center;
      node.radius = maximumRadius + balance;
      for (sphereNode_t* update = node.child; update != nullptr;
           update = update->next) {
        const float slop = std::fmax(update->parent->radius - update->radius,
                                    0.0f);
        update->slop = slop * slop;
      }
    }
  }
  return false;
}

void idSphereTree::PrintNode(idRenderWorld* renderWorld,
                             const sphereNode_t* node) {
  idSphere sphere{node->origin, node->radius};
  idVec4 textColor{1.0f, 0.5f, 0.0f, 1.0f};
  float textHeight = 0.0f;
  idStr flags("");
  if (g_debugSphereTree.valueInteger > 0) {
    if ((node->flags & SPHERE_NODE_SUPER) != 0) {
      textHeight = 10.0f;
      flags += "SUPER,";
      textColor = idColor::colorCyan;
    }
    if ((node->flags & SPHERE_NODE_ROOT_TREE) != 0) {
      textHeight = 20.0f;
      textColor = idColor::colorGreen;
      flags += "ROOTTREE,";
    }
    if ((node->flags & SPHERE_NODE_ROOT_NODE) != 0) {
      textHeight = 0.0f;
      textColor = idColor::colorRed;
      flags += "ROOTNODE";
    }
    idRenderDebug* debug = renderWorld->GetRenderDebug();
    if (g_debugSphereTree.valueInteger != 1 || flags.Length() == 0)
      debug->DebugSphere(idColor::colorBlue, sphere, 12, 0, false);
    if (node->parent != nullptr) {
      flags.Format("%s' \n NodeID: '%d' \n LinkID: '%d' \n ParID: '%d' \n",
                   flags.data, NodeIndex(*node), node->link,
                   NodeIndex(*node->parent));
    } else {
      flags.Format("%s' \n NodeID: '%d' \n LinkID: '%d' \n", flags.data,
                   NodeIndex(*node), node->link);
    }
    const idVec3 textOrigin = node->origin + idVec3{0.0f, 0.0f, textHeight};
    debug->DebugText(flags.data, textOrigin, 1.0f, textColor, 1, 0, false,
                     false);
  }
  if (g_debugSphereTree.valueInteger == 3 && node->link != -1) {
    idRenderDebug* debug = renderWorld->GetRenderDebug();
    const sphereNode_t& linked = spheres[node->link];
    debug->DebugLine(idColor::colorYellow, node->origin, linked.origin, 0,
                     false);
    flags.Format("Start: '%d' \n", NodeIndex(*node));
    debug->DebugText(flags.data, node->origin, 1.0f, textColor, 1, 0, false,
                     false);
    flags.Format("End: '%d' \n", node->link);
    debug->DebugText(flags.data, linked.origin, 1.0f, textColor, 1, 0, false,
                     false);
  }
}

void idSphereTree::DebugRenderTree(idRenderWorld* renderWorld) {
  if (g_debugSphereTree.valueInteger == 0)
    return;
  const sphereNode_t* stack[2049];
  int count = 1;
  stack[0] = root;
  do {
    const sphereNode_t* node = stack[--count];
    PrintNode(renderWorld, node);
    if ((node->flags & SPHERE_NODE_SUPER) != 0) {
      for (const sphereNode_t* child = node->child; child != nullptr;
           child = child->next)
        stack[count++] = child;
    } else if ((node->flags & SPHERE_NODE_ROOT_TREE) != 0 &&
               node->link != -1) {
      stack[count++] = &spheres[node->link];
    }
  } while (count > 0);
}

sphereNode_t& idSphereTree::AllocNode() {
  const int index = heapId[heapId.num - 1];
  --heapId.num;
  sphereNode_t& node = spheres[index];
  node.parent = nullptr;
  node.child = nullptr;
  node.prev = nullptr;
  node.next = nullptr;
  node.origin = vec3_origin;
  node.radius = 0.0f;
  node.flags &= kNodeIndexMask;
  node.link = -1;
  node.id = 0;
  return node;
}

idSphereTree::idSphereTree() {
  spheres.granularity = 5;
  integrate.granularity = 5;
  recompute.granularity = 5;
  heapId.granularity = 5;
  work.granularity = 5;
}

void idSphereTree::FreeNode(sphereNode_t& node) {
  if ((node.flags & SPHERE_NODE_ROOT_NODE) != 0)
    return;
  if ((node.flags & (SPHERE_NODE_SUPER | SPHERE_NODE_ROOT_TREE)) ==
      SPHERE_NODE_SUPER) {
    FreeNode(spheres[node.link]);
  }
  node.link = -1;
  if ((node.flags & kIntegrateFlag) != 0)
    RemovePointer(integrate, &node);
  if ((node.flags & SPHERE_NODE_RECOMPUTE) != 0)
    RemovePointer(recompute, &node);
  sphereNode_t* parent = node.parent;
  Unlink(node);
  if (parent != nullptr && parent->child == nullptr)
    FreeNode(*parent);
  node.flags &= kNodeIndexMask;
  heapId.Append(NodeIndex(node));
}

void idSphereTree::RemoveSphere(int sphereIndex) {
  if (sphereIndex >= 0)
    FreeNode(spheres[sphereIndex]);
}

void idSphereTree::Alloc(int maximumSpheres, float rootSizeValue,
                         float leafSizeValue, float balanceValue) {
  maxSpheres = 4 * maximumSpheres + 4;
  rootSize = rootSizeValue;
  leafSize = leafSizeValue;
  balance = balanceValue;
  if (maxSpheres <= spheres.size || spheres.ResizeForPort(maxSpheres))
    spheres.num = std::min(maxSpheres, spheres.size);
  std::memset(spheres.list, 0,
              static_cast<std::size_t>(spheres.num) * sizeof(sphereNode_t));

  if (maxSpheres <= heapId.size || heapId.ResizeForPort(maxSpheres))
    heapId.num = std::min(maxSpheres, heapId.size);
  for (int index = 0; index < maxSpheres; ++index) {
    heapId[index] = index;
    spheres[index].flags = static_cast<std::uint32_t>(index << 8);
  }

  root = &AllocNode();
  root->origin = vec3_origin;
  root->radius = 65536.0f;
  root->flags = (root->flags & kNodeIndexMask) | SPHERE_NODE_SUPER |
                SPHERE_NODE_ROOT_TREE | SPHERE_NODE_ROOT_NODE;
  leaf = &AllocNode();
  leaf->origin = vec3_origin;
  leaf->radius = 16384.0f;
  leaf->flags = (leaf->flags & kNodeIndexMask) | SPHERE_NODE_SUPER |
                SPHERE_NODE_ROOT_NODE;
  if (work.size < 2048)
    work.ResizeForPort(2048);
}

int idSphereTree::CollectSpheresFromView(const idRenderView* view, int* ids,
                                         int maxIds) {
  if (view == nullptr || maxIds <= 0)
    return 0;
  const float tanX = std::tan(view->r.fov_x * 0.017453292f * 0.5f);
  const float tanY = std::tan(view->r.fov_y * 0.017453292f * 0.5f);
  const float nearDistance = r_znear.valueFloat;
  const float farDistance = r_zfar.valueFloat <= 0.00000011920928955078125f
                                ? 1.0e30f
                                : r_zfar.valueFloat;
  work.Clear();
  work.Append(root);
  int count = 0;
  while (work.num > 0) {
    const sphereNode_t* node = work[work.num - 1];
    --work.num;
    const float paddedRadius = node->radius * 1.125f;
    const idVec3 delta = node->origin - view->r.vieworg;
    const float depth = view->r.viewaxis[0] * delta;
    if (nearDistance > depth + paddedRadius ||
        depth - paddedRadius > farDistance)
      continue;
    const idVec3 residual = delta - view->r.viewaxis[0] * depth;
    const float horizontal = view->r.viewaxis[1] * residual;
    const float horizontalLimit = depth * tanX;
    if (-horizontalLimit > horizontal + paddedRadius ||
        horizontal - paddedRadius > horizontalLimit)
      continue;
    const float vertical = view->r.viewaxis[2] * residual;
    const float verticalLimit = depth * tanY;
    if (-verticalLimit > vertical + paddedRadius ||
        vertical - paddedRadius > verticalLimit)
      continue;

    if ((node->flags & SPHERE_NODE_SUPER) != 0) {
      for (const sphereNode_t* child = node->child; child != nullptr;
           child = child->next)
        work.Append(child);
    } else if ((node->flags & SPHERE_NODE_ROOT_TREE) != 0) {
      if (node->link != -1)
        work.Append(&spheres[node->link]);
    } else {
      ids[count++] = node->id;
      if (count >= maxIds)
        return count;
    }
  }
  return count;
}

bool idSphereTree::NextCollision(const idVec3& origin, float radius, int* id) {
  while (work.num > 0) {
    const sphereNode_t* node = work[work.num - 1];
    --work.num;
    const float combinedRadius = radius + node->radius;
    if (DistanceSquared(node->origin, origin) >
        combinedRadius * combinedRadius)
      continue;
    if ((node->flags & SPHERE_NODE_SUPER) != 0) {
      for (const sphereNode_t* child = node->child; child != nullptr;
           child = child->next)
        work.Append(child);
    } else if ((node->flags & SPHERE_NODE_ROOT_TREE) != 0) {
      if (node->link != -1)
        work.Append(&spheres[node->link]);
    } else {
      if (id != nullptr)
        *id = node->id;
      return true;
    }
  }
  return false;
}

void idSphereTree::QueueIntegrate(sphereNode_t& node) {
  if ((node.flags & kIntegrateFlag) != 0)
    return;
  Unlink(node);
  sphereNode_t& parent =
      (node.flags & SPHERE_NODE_ROOT_TREE) != 0 ? *root : *leaf;
  LinkFirst(parent, node);
  node.flags |= kIntegrateFlag;
  const float available = std::fmax(parent.radius - node.radius, 0.0f);
  node.slop = available * available;
  integrate.Append(&node);
}

void idSphereTree::SetOrigin(sphereNode_t& node, const idVec3& origin) {
  node.origin = origin;
  sphereNode_t* parent = node.parent;
  if (parent == nullptr || (node.flags & kIntegrateFlag) != 0 ||
      DistanceSquared(parent->origin, node.origin) < node.slop)
    return;
  if ((parent->flags & SPHERE_NODE_RECOMPUTE) == 0) {
    if (parent->child != nullptr) {
      parent->flags |= SPHERE_NODE_RECOMPUTE;
      recompute.Append(parent);
    } else {
      FreeNode(*parent);
    }
  }
  QueueIntegrate(node);
}

void idSphereTree::SetRadius(sphereNode_t& node, float radius) {
  if (std::fabs(radius - node.radius) <= 0.00000011920928955078125f)
    return;
  node.radius = radius;
  sphereNode_t* parent = node.parent;
  if (parent == nullptr)
    return;
  const float available = std::fmax(parent->radius - radius, 0.0f);
  node.slop = available * available;
  if ((node.flags & kIntegrateFlag) != 0)
    return;
  if ((parent->flags & SPHERE_NODE_RECOMPUTE) == 0) {
    if (parent->child != nullptr) {
      parent->flags |= SPHERE_NODE_RECOMPUTE;
      recompute.Append(parent);
    } else {
      FreeNode(*parent);
    }
  }
  if (DistanceSquared(node.parent->origin, node.origin) >= node.slop)
    QueueIntegrate(node);
}

void idSphereTree::Integrate(sphereNode_t& node, sphereNode_t& super,
                             float size) {
  float containingDistance = 1.0e30f;
  float nearestDistance = 1.0e30f;
  sphereNode_t* containing = nullptr;
  sphereNode_t* nearest = nullptr;
  for (sphereNode_t* child = super.child; child != nullptr;
       child = child->next) {
    if ((child->flags &
         (SPHERE_NODE_SUPER | SPHERE_NODE_ROOT_NODE)) != SPHERE_NODE_SUPER ||
        child->child == nullptr)
      continue;
    const float distance = DistanceSquared(node.origin, child->origin);
    const float radiusDifference = child->radius - node.radius;
    if (radiusDifference * radiusDifference < distance) {
      if (containing == nullptr && nearestDistance > distance) {
        nearestDistance = distance;
        nearest = child;
      }
    } else if (containingDistance > distance) {
      containingDistance = distance;
      containing = child;
    }
  }

  Unlink(node);
  sphereNode_t* parent = containing;
  if (parent == nullptr && nearest != nullptr) {
    const float required =
        std::sqrt(std::fmax(nearestDistance, 1.17549435e-38f)) +
        nearest->radius + balance;
    if (size > required) {
      nearest->radius = required;
      parent = nearest;
    }
  }
  if (parent != nullptr) {
    LinkFirst(*parent, node);
    const float available = std::fmax(parent->radius - node.radius, 0.0f);
    node.slop = available * available;
    Recompute(*parent);
    if ((parent->flags & SPHERE_NODE_ROOT_TREE) == 0) {
      sphereNode_t& proxy = spheres[parent->link];
      SetOrigin(proxy, parent->origin);
      SetRadius(proxy, parent->radius);
    }
  }

  if ((node.flags & SPHERE_NODE_LINKED) == 0) {
    sphereNode_t& cluster = AllocNode();
    cluster.origin = node.origin;
    cluster.radius = balance + node.radius;
    cluster.flags = (cluster.flags & kNodeIndexMask) |
                    (super.flags & SPHERE_NODE_ROOT_TREE) |
                    SPHERE_NODE_SUPER;
    LinkFirst(cluster, node);
    float available = std::fmax(cluster.radius - node.radius, 0.0f);
    node.slop = available * available;
    LinkFirst(super, cluster);
    available = std::fmax(super.radius - cluster.radius, 0.0f);
    cluster.slop = available * available;
    Recompute(cluster);
    if ((cluster.flags & SPHERE_NODE_ROOT_TREE) == 0) {
      sphereNode_t& proxy = AllocNode();
      proxy.origin = cluster.origin;
      proxy.radius = cluster.radius;
      proxy.flags =
          (proxy.flags & kNodeIndexMask) | SPHERE_NODE_ROOT_TREE;
      proxy.link = NodeIndex(cluster);
      cluster.link = NodeIndex(proxy);
      QueueIntegrate(proxy);
    }
  }
  node.flags &= ~kIntegrateFlag;
}

bool idSphereTree::FirstCollision(const idVec3& origin, float radius,
                                  int* id) {
  work.Clear();
  work.Append(root);
  return NextCollision(origin, radius, id);
}

bool idSphereTree::LeafCollision(const idVec3& origin, float radius, int* id) {
  return FirstCollision(origin, radius, id);
}

void idSphereTree::Update() {
  while (recompute.num > 0) {
    sphereNode_t* node = recompute[recompute.num - 1];
    --recompute.num;
    if (Recompute(*node))
      FreeNode(*node);
  }
  while (integrate.num > 0) {
    sphereNode_t* node = integrate[integrate.num - 1];
    --integrate.num;
    if ((node->flags & SPHERE_NODE_ROOT_TREE) != 0)
      Integrate(*node, *root, rootSize);
    else
      Integrate(*node, *leaf, leafSize);
  }
}

sphereNode_t& idSphereTree::AddSphere(const idVec3& origin, float radius,
                                      sphereNodeFlag_t flags, int id) {
  sphereNode_t& node = AllocNode();
  node.origin = origin;
  node.flags = (node.flags & kNodeIndexMask) |
               static_cast<std::uint32_t>(flags);
  node.radius = radius;
  node.id = id;
  QueueIntegrate(node);
  return node;
}

template void* idListArrayNew<sphereNode_t>(int, memTag_t, bool);
template void* idListArrayResize<sphereNode_t>(void*, int, int, memTag_t,
                                               bool);
