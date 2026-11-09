#include "engine/models/cuttable/cuttergraph.h"

#include "shared/idlib/containers/BitArray.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace {

constexpr memTag_t kGameTag = static_cast<memTag_t>(0x4D);

template <typename Type>
Type* AllocateRaw(const char* location, std::size_t count, memTag_t tag) {
  return static_cast<Type*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(Type) * count, tag, false, ALIGN_16,
      HEAP_DEFAULTHEAP));
}

template <typename Type, typename... Args>
Type* AllocateObject(const char* location, memTag_t tag, Args&&... args) {
  Type* storage = AllocateRaw<Type>(location, 1, tag);
  return storage != nullptr
             ? new (storage) Type(std::forward<Args>(args)...)
             : nullptr;
}

template <typename Type>
void FreeObject(Type*& object) {
  if (object == nullptr)
    return;
  object->~Type();
  idMem::Singleton()->Free(object, ALIGN_16);
  object = nullptr;
}

bool SamePoint(const idVec2i& left, const idVec2i& right) {
  return left.x == right.x && left.y == right.y;
}

int WrappedSquaredDistance(const idVec2i& left, const idVec2i& right) {
  const std::uint32_t dx = static_cast<std::uint32_t>(left.x) -
                           static_cast<std::uint32_t>(right.x);
  const std::uint32_t dy = static_cast<std::uint32_t>(left.y) -
                           static_cast<std::uint32_t>(right.y);
  return static_cast<std::int32_t>(dx * dx + dy * dy);
}

int WrappedSquare(int value) {
  const std::uint32_t unsignedValue = static_cast<std::uint32_t>(value);
  return static_cast<std::int32_t>(unsignedValue * unsignedValue);
}

int WrappedAdd(int left, int right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

int WrappedSubtract(int left, int right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) -
                                   static_cast<std::uint32_t>(right));
}

int WrappedMultiply(int left, int right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) *
                                   static_cast<std::uint32_t>(right));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return ((value & 0x000000ffU) << 24) | ((value & 0x0000ff00U) << 8) |
         ((value & 0x00ff0000U) >> 8) | ((value & 0xff000000U) >> 24);
}

void WriteBigInt(idFile* file, std::uint32_t value) {
  const std::uint32_t swapped = ByteSwap32(value);
  file->Write(&swapped, sizeof(swapped));
}

std::uint32_t ReadBigInt(idFile* file) {
  std::uint32_t value = 0;
  file->Read(&value, sizeof(value));
  return ByteSwap32(value);
}

void WriteBigShort(idFile* file, std::uint16_t value) {
  const std::uint16_t swapped = static_cast<std::uint16_t>((value << 8) | (value >> 8));
  file->Write(&swapped, sizeof(swapped));
}

std::uint16_t ReadBigShort(idFile* file) {
  std::uint16_t value = 0;
  file->Read(&value, sizeof(value));
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

void WriteBigFloat(idFile* file, float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  WriteBigInt(file, bits);
}

float ReadBigFloat(idFile* file) {
  const std::uint32_t bits = ReadBigInt(file);
  float value;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

template <typename T> int PointerIndex(const std::vector<T*>& values, const T* value) {
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (values[index] == value)
      return static_cast<int>(index);
  }
  return -1;
}

template <typename T> int AppendUnique(std::vector<T*>& values, T* value) {
  const int existing = PointerIndex(values, value);
  if (existing >= 0)
    return existing;
  values.push_back(value);
  return static_cast<int>(values.size() - 1);
}

void WriteIndex(idFile* file, int index) {
  WriteBigShort(file, static_cast<std::uint16_t>(index));
}

int ReadIndex(idFile* file) {
  const std::uint16_t index = ReadBigShort(file);
  return index == 0xffffU ? -1 : static_cast<int>(index);
}

template <typename T> T* PointerAt(const std::vector<T*>& values, int index) {
  return index >= 0 ? values[static_cast<std::size_t>(index)] : nullptr;
}

void NormalVectors(const idVec3& normal, idVec3& left, idVec3& down) {
  const float lengthSquared = normal.x * normal.x + normal.y * normal.y;
  if (lengthSquared == 0.0f) {
    left = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverseLength =
        1.0f / std::sqrt(std::max(lengthSquared, std::numeric_limits<float>::min()));
    left = {-normal.y * inverseLength, normal.x * inverseLength, 0.0f};
  }
  down = left.Cross(normal);
}

void FreeContourList(idContourManager* manager, Contour* contour) {
  while (manager != nullptr && contour != nullptr) {
    Contour* next = contour->next;
    manager->Free(contour);
    contour = next;
  }
}

bool HasGraphLink(const idCutterGraphManager::Node_t* source,
                  const idCutterGraphManager::Node_t* target) {
  for (const idCutterGraphManager::Link_t* link = source->link; link != nullptr;
       link = link->next) {
    if (link->node == target)
      return true;
  }
  return false;
}

} // namespace

idCutterGraphManager::IDHeap::IDHeap() : index(0), isStatic(0), numBits(0) {}

idCutterGraphManager::IDHeap::~IDHeap() { Clear(); }

int idCutterGraphManager::IDHeap::Alloc() {
  const std::uint32_t oldIndex = index;
  int byteIndex = static_cast<int>(oldIndex);

  if (buffer.list[byteIndex] == 0xffU) {
    do {
      ++byteIndex;
    } while (buffer.list[byteIndex] == 0xffU);
  }

  if (byteIndex >= (numBits >> 3) - 1) {
    if (oldIndex == 0)
      return -1;

    byteIndex = 0;
    while (byteIndex < static_cast<int>(oldIndex) &&
           buffer.list[byteIndex] == 0xffU) {
      ++byteIndex;
    }
    if (buffer.list[byteIndex] == 0xffU) {
      index = 0;
      return -1;
    }
  }

  int bitIndex = 0;
  while ((buffer.list[byteIndex] & static_cast<std::uint8_t>(1U << bitIndex)) != 0U &&
         bitIndex < 8) {
    ++bitIndex;
  }
  buffer.list[byteIndex] |= static_cast<std::uint8_t>(1U << bitIndex);
  index = static_cast<std::uint32_t>(byteIndex);
  return bitIndex + 8 * byteIndex;
}

void idCutterGraphManager::IDHeap::Clear() {
  buffer.ClearFree();
  index = 0;
  numBits = 0;
}

void idCutterGraphManager::IDHeap::Write(idFile* file) const {
  WriteBigInt(file, static_cast<std::uint32_t>(buffer.num));
  file->Write(buffer.list, static_cast<std::size_t>(buffer.num));
  WriteBigInt(file, index);
  WriteBigInt(file, isStatic);
  WriteBigInt(file, static_cast<std::uint32_t>(numBits));
}

void idCutterGraphManager::IDHeap::Read(idFile* file) {
  const int bufferSize = static_cast<int>(ReadBigInt(file));
  if (bufferSize > buffer.size)
    buffer.ResizeForPort(bufferSize);
  buffer.num = std::min(bufferSize, buffer.size);
  file->Read(buffer.list, static_cast<std::size_t>(bufferSize));

  index = ReadBigInt(file) & 0x7fffffffU;
  isStatic = ReadBigInt(file) != 0U;
  numBits = static_cast<int>(ReadBigInt(file));
}

idCutterGraphManager::Graph::Graph() = default;

idCutterGraphManager::Graph::~Graph() = default;

int idCutterGraphManager::Graph::FindNodeIndex(const idVec2i& pos, int threshold) const {
  const int distanceSquared = WrappedSquare(threshold);
  for (int index = 0; index < nodes.Num(); ++index) {
    if (WrappedSquaredDistance(nodes[index].data->pos, pos) <= distanceSquared)
      return index;
  }
  return -1;
}

idVec2i& idCutterGraphManager::Graph::GetLocal2D(idVec2i& result,
                                                  const idVec3& point) const {
  const idVec3 local = inverse * (point - position);
  result.x = static_cast<int>(std::floor(local * left + 0.5f));
  result.y = static_cast<int>(std::floor(local * down + 0.5f));
  return result;
}

idVec3& idCutterGraphManager::Graph::GetLocal3D(idVec3& result,
                                                const idVec2& point2d) const {
  const idVec3 base = left * point2d.x + down * point2d.y;
  const float distance = plane.a * base.x + plane.b * base.y + plane.c * base.z + plane.d;
  result = base - idVec3{plane.a, plane.b, plane.c} * distance;
  return result;
}

idVec3& idCutterGraphManager::Graph::GetPosition3D(idVec3& result,
                                                   const idVec2i& point2d) const {
  idVec3 local{};
  GetLocal3D(local, {static_cast<float>(point2d.x), static_cast<float>(point2d.y)});
  result = orientation * local + position;
  return result;
}

void idCutterGraphManager::Graph::SetPosition(const idVec3& position_,
                                               const idMat3& orientation_) {
  position = position_;
  orientation = orientation_;
  inverse = orientation_;
  inverse.InverseSelf();
}

void idCutterGraphManager::Graph::DebugDraw(idRenderWorld* renderWorld, int lifeTime) const {
  using GetRenderDebugFn = void* (*)(idRenderWorld*);
  using DebugLineFn = void (*)(void*, const idVec4*, const idVec3*, const idVec3*, int, bool);
  static const idVec4 colorOrange{1.0f, 0.5f, 0.0f, 1.0f};

  for (int index = 0; index < edges.Num(); ++index) {
    Edge_t* edge = edges[index].data;
    idVec3 first;
    idVec3 second;
    GetPosition3D(first, edge->node1->pos);
    GetPosition3D(second, edge->node2->pos);

    void** renderWorldVtable = *reinterpret_cast<void***>(renderWorld);
    void* renderDebug = reinterpret_cast<GetRenderDebugFn>(renderWorldVtable[0x128 / 8])(renderWorld);
    void** renderDebugVtable = *reinterpret_cast<void***>(renderDebug);
    reinterpret_cast<DebugLineFn>(renderDebugVtable[0x10 / 8])(
        renderDebug, &colorOrange, &first, &second, lifeTime, true);
  }
}

int idCutterGraphManager::Graph::FindClosestNode(const Edge_t* edge, const idVec2i& hitPos,
                                                 int threshold) const {
  int result = -1;
  int bestDistance = WrappedSquare(threshold);
  const int node1Distance = WrappedSquaredDistance(hitPos, edge->node1->pos);
  const int node2Distance = WrappedSquaredDistance(hitPos, edge->node2->pos);

  if (node1Distance < bestDistance) {
    for (int index = 0; index < nodes.Num(); ++index) {
      if (WrappedSquaredDistance(nodes[index].data->pos, edge->node1->pos) <= 0) {
        result = index;
        break;
      }
    }
    bestDistance = node1Distance;
  }

  if (node2Distance < bestDistance) {
    for (int index = 0; index < nodes.Num(); ++index) {
      if (WrappedSquaredDistance(nodes[index].data->pos, edge->node2->pos) <= 0)
        return index;
    }
    return -1;
  }
  return result;
}

idCutterGraphManager::Edge_t* idCutterGraphManager::Graph::FindSplitEdge(
    const Node_t* srcNode, const idVec2i& dst, idVec2i& pos) const {
  Edge_t* best = nullptr;
  float bestAlong = 1.0e30f;
  std::uint32_t alongBits = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&pos));
  float along;
  std::memcpy(&along, &alongBits, sizeof(along));
  idVec2i candidate{};
  idVec2i bestPosition{};
  for (int index = 0; index < edges.Num(); ++index) {
    Edge_t* edge = edges[index].data;
    if (edge->node1 == srcNode || edge->node2 == srcNode)
      continue;

    const float segmentX = static_cast<float>(dst.x - srcNode->pos.x);
    const float segmentY = static_cast<float>(dst.y - srcNode->pos.y);
    const float edgeX = static_cast<float>(edge->node2->pos.x - edge->node1->pos.x);
    const float edgeY = static_cast<float>(edge->node2->pos.y - edge->node1->pos.y);
    const float offsetX = static_cast<float>(srcNode->pos.x - edge->node1->pos.x);
    const float offsetY = static_cast<float>(srcNode->pos.y - edge->node1->pos.y);
    const float denominator = edgeY * segmentX - segmentY * edgeX;
    if (std::fabs(denominator) >= 1.1920929e-7f) {
      along = (offsetY * edgeX - offsetX * edgeY) / denominator;
      const float across = (offsetY * segmentX - offsetX * segmentY) / denominator;
      if (along < 0.0f || along > 1.0f || across < 0.0f || across > 1.0f)
        continue;
      candidate.x = srcNode->pos.x + static_cast<int>(along * segmentX);
      candidate.y = srcNode->pos.y + static_cast<int>(along * segmentY);
    } else {
      if (!SamePoint(edge->node1->pos, dst) && !SamePoint(edge->node2->pos, dst))
        continue;
    }

    if (along <= bestAlong) {
      bestAlong = along;
      bestPosition = candidate;
      best = edge;
    }
  }
  pos = bestPosition;
  return best;
}

void idCutterGraphManager::Graph::Init(const idStr& name_, Contour* contour) {
  name = name_;
  idheap.numBits = 1032;
  if (idheap.buffer.size < 129)
    idheap.buffer.ResizeForPort(129);
  idheap.buffer.num = std::min(129, idheap.buffer.size);
  idheap.index = 0;
  std::memset(idheap.buffer.list, 0, static_cast<std::size_t>(idheap.buffer.num));
  contoursOuter = contour;
  contoursInner = nullptr;
  contour->next = nullptr;
}

void idCutterGraphManager::Graph::AddNode(Node_t* node) {
  node->id = idheap.Alloc();
  nodes.Append(nodeHandle_t{node});
}

bool idCutterGraphManager::Graph::PathFind(idList<idVec2i>& points) {
  idList<Node_t*> pathNodes;
  pathNodes.ResizeForPort(1024);
  idBitArray visited;
  visited.bits = 1024;
  visited.memTag = TAG_BITARRAY;
  visited.free = true;
  visited.buffer = static_cast<unsigned char*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/BitArray.h(112) : static_cast< memTag_t >( memTag )",
          128, TAG_BITARRAY, false, ALIGN_16, HEAP_DEFAULTHEAP));
  std::memset(visited.buffer, 0, 128);
  idList<Link_t*> pathLinks;
  pathLinks.ResizeForPort(1024);

  for (int edgeIndex = 0; edgeIndex < edges.Num(); ++edgeIndex)
    edges[edgeIndex].data->visited = 0;

  pathNodes.Append(lastNode);
  pathLinks.Append(lastNode->link);
  const auto setVisited = [&visited](int id) {
    if (static_cast<unsigned int>(id) < 1024U)
      visited.buffer[static_cast<unsigned int>(id) >> 3] |=
          static_cast<std::uint8_t>(1U << (id & 7));
  };
  const auto clearVisited = [&visited](int id) {
    if (static_cast<unsigned int>(id) < 1024U)
      visited.buffer[static_cast<unsigned int>(id) >> 3] &=
          static_cast<std::uint8_t>(~(1U << (id & 7)));
  };
  const auto isVisited = [&visited](int id) {
    return static_cast<unsigned int>(id) < 1024U &&
           (visited.buffer[static_cast<unsigned int>(id) >> 3] &
            static_cast<std::uint8_t>(1U << (id & 7))) != 0U;
  };

  setVisited(lastNode->id);
  Node_t* previous = lastNode;
  Node_t* current = prevNode;
  Link_t* link = current->link;

  while (pathNodes.Num() > 0) {
    if (link != nullptr && !isVisited(current->id)) {
      // Continue walking this node's adjacency list.
    } else {
      if (link != nullptr && current->id == lastNode->id) {
        for (int index = 0; index < pathNodes.Num(); ++index)
          points.Append(pathNodes[index]->pos);
        return pathNodes.Num() > 2;
      }

      if (pathNodes.Num() < 2)
        break;
      current = pathNodes[pathNodes.Num() - 1];
      previous = pathNodes[pathNodes.Num() - 2];
      link = pathLinks[pathLinks.Num() - 1]->next;
      clearVisited(current->id);
      pathNodes.SetNum(pathNodes.Num() - 1);
      pathLinks.SetNum(pathLinks.Num() - 1);
    }

    while (link != nullptr) {
      if (!link->edge->visited && link->node != previous) {
        link->edge->visited = 1;
        setVisited(current->id);
        pathNodes.Append(current);
        pathLinks.Append(link);
        previous = current;
        current = link->node;
        link = current->link;
        break;
      }
      link = link->next;
    }

    if (pathNodes.Num() <= 0) {
      for (int index = 0; index < pathNodes.Num(); ++index)
        points.Append(pathNodes[index]->pos);
      return pathNodes.Num() > 2;
    }
  }

  return false;
}

void idCutterGraphManager::Graph::BuildGraphTree(graphTree_t* graphTree) const {
  for (int edgeIndex = 0; edgeIndex < edges.Num(); ++edgeIndex)
    edges[edgeIndex].data->visited = 0;

  for (int linkIndex = 0; linkIndex < links.Num(); ++linkIndex) {
    const int firstPosition = graphTree->positions.Num();
    Node_t* previousNode = nullptr;
    Link_t* link = links[linkIndex].data;
    while (link != nullptr) {
      if (link->edge->visited) {
        link = link->next;
        continue;
      }

      link->edge->visited = 1;
      Node_t* node = link->node;
      if (previousNode == nullptr) {
        previousNode = link->edge->node1;
        if (node == previousNode)
          previousNode = link->edge->node2;
        idVec3 position3d{};
        GetLocal3D(position3d, idVec2{static_cast<float>(previousNode->pos.x),
                                     static_cast<float>(previousNode->pos.y)});
        graphTree->positions.Append(position3d);
      }

      idVec3 position3d{};
      GetLocal3D(position3d,
                 idVec2{static_cast<float>(node->pos.x), static_cast<float>(node->pos.y)});
      graphTree->positions.Append(position3d);
      link = node->link;
    }

    if (graphTree->positions.Num() > firstPosition) {
      graphBranch_t branch;
      branch.index = static_cast<std::int16_t>(firstPosition);
      branch.num = static_cast<std::int16_t>(graphTree->positions.Num() - firstPosition);
      graphTree->branches.Append(branch);
    }
  }

  for (Contour* contour = contoursInner; contour != nullptr; contour = contour->next) {
    const int firstPosition = graphTree->positions.Num();
    for (ContourEdge_t* edge = contour->edges; edge != nullptr; edge = edge->next) {
      idVec3 position3d{};
      GetLocal3D(position3d,
                 idVec2{static_cast<float>(edge->node->pos.x),
                        static_cast<float>(edge->node->pos.y)});
      graphTree->positions.Append(position3d);
    }
    graphBranch_t branch;
    branch.index = static_cast<std::int16_t>(firstPosition);
    branch.num = static_cast<std::int16_t>(graphTree->positions.Num() - firstPosition);
    graphTree->contours.Append(branch);
  }

  for (int nodeIndex = 0; nodeIndex < nodes.Num(); ++nodeIndex) {
    Node_t* node = nodes[nodeIndex].data;
    if (node->link == nullptr)
      continue;

    idList<idVec3>* destination = nullptr;
    if (node->link->next == nullptr)
      destination = &graphTree->endpoints;
    else if (node->link->next->next != nullptr)
      destination = &graphTree->intersections;

    if (destination != nullptr) {
      idVec3 position3d{};
      GetLocal3D(position3d,
                 idVec2{static_cast<float>(node->pos.x), static_cast<float>(node->pos.y)});
      destination->Append(position3d);
    }
  }
}

idCutterGraphManager::idCutterGraphManager() : contourManager(nullptr), graph(nullptr) {
  linksPool.ResizeForPort(NUM_POOLLINKS);
  nodesPool.ResizeForPort(NUM_POOLNODES);
  edgesPool.ResizeForPort(NUM_POOLNODES);
  linksPoolBlock.ResizeForPort(4);
  nodesPoolBlock.ResizeForPort(4);
  edgesPoolBlock.ResizeForPort(4);
  for (int index = 0; index < linksPool.size; ++index)
    linksPool.list[index] = nullptr;
  for (int index = 0; index < nodesPool.size; ++index)
    nodesPool.list[index] = nullptr;
  for (int index = 0; index < edgesPool.size; ++index)
    edgesPool.list[index] = nullptr;
  for (int index = 0; index < linksPoolBlock.size; ++index)
    linksPoolBlock.list[index] = nullptr;
  for (int index = 0; index < nodesPoolBlock.size; ++index)
    nodesPoolBlock.list[index] = nullptr;
  for (int index = 0; index < edgesPoolBlock.size; ++index)
    edgesPoolBlock.list[index] = nullptr;
  contourManager = AllocateObject<idContourManager>(
      "w:\\build\\tech5\\engine\\models\\cuttable\\CutterGraph.cpp(968) : TAG_GAME",
      kGameTag);
}

idCutterGraphManager::~idCutterGraphManager() {
  for (int index = 0; index < graphs.Num(); ++index)
    FreeGraph(index);

  linksPool.ClearFree();
  nodesPool.ClearFree();
  edgesPool.ClearFree();
  for (int index = 0; index < linksPoolBlock.Num(); ++index) {
    idMem::Singleton()->Free(linksPoolBlock[index], ALIGN_16);
    linksPoolBlock[index] = nullptr;
  }
  for (int index = 0; index < nodesPoolBlock.Num(); ++index) {
    idMem::Singleton()->Free(nodesPoolBlock[index], ALIGN_16);
    nodesPoolBlock[index] = nullptr;
  }
  for (int index = 0; index < edgesPoolBlock.Num(); ++index) {
    idMem::Singleton()->Free(edgesPoolBlock[index], ALIGN_16);
    edgesPoolBlock[index] = nullptr;
  }
  linksPoolBlock.ClearFree();
  nodesPoolBlock.ClearFree();
  edgesPoolBlock.ClearFree();
  FreeObject(contourManager);
}

void idCutterGraphManager::MergeContours(Contour* contour) {
  contourManager->Merge(contour);
}

void idCutterGraphManager::FreeContour(Contour* contour) {
  contourManager->Free(contour);
}

void idCutterGraphManager::PreAcquire() {
  if (graph != nullptr)
    graph->lastNode = nullptr;
}

Contour* idCutterGraphManager::CreateContour(const idList<idVec2i>& points) {
  return contourManager->Create(points);
}

idVec2i& idCutterGraphManager::GetLocal2D(idVec2i& result, const idVec3& point) const {
  return graph->GetLocal2D(result, point);
}

idVec3& idCutterGraphManager::GetLocal3D(idVec3& result, const idVec2& point2d) const {
  return graph->GetLocal3D(result, point2d);
}

idVec3& idCutterGraphManager::GetPosition3D(idVec3& result, const idVec2i& point2d) const {
  return graph->GetPosition3D(result, point2d);
}

bool idCutterGraphManager::IsGraphValid(int id) const {
  return graphs[id].graph != nullptr;
}

void idCutterGraphManager::SetPosition(int id, const idVec3& position,
                                       const idMat3& orientation) {
  graphs[id].graph->SetPosition(position, orientation);
}

void idCutterGraphManager::GetNormalVectors(int id, idVec3& normal, idVec3& left,
                                            idVec3& up) const {
  const Graph* value = graphs[id].graph;
  normal = {value->plane.a, value->plane.b, value->plane.c};
  left = value->left;
  up = -value->down;
}

Contour* idCutterGraphManager::GetOuterContour(int id) const {
  return graphs[id].graph->contoursOuter;
}

Contour* idCutterGraphManager::GetInnerContour(int id) const {
  return graphs[id].graph->contoursInner;
}

void idCutterGraphManager::DebugDraw(int id, idRenderWorld* renderWorld, int lifeTime) const {
  if (graphs[id].graph != nullptr)
    graphs[id].graph->DebugDraw(renderWorld, lifeTime);
}

void idCutterGraphManager::PreAllocatePools() {
  if (linksPool.Num() == 0) {
    Link_t* block = AllocateRaw<Link_t>(
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterGraph.cpp(1399) : TAG_GAME",
        NUM_POOLLINKS, kGameTag);
    for (int index = 0; index < NUM_POOLLINKS; ++index) {
      block[index].allocated = 0;
      block[index].invalid = 0;
      linksPool.Append(&block[index]);
    }
    linksPoolBlock.Append(block);
  }
  if (nodesPool.Num() == 0) {
    Node_t* block = AllocateRaw<Node_t>(
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterGraph.cpp(1415) : TAG_GAME",
        NUM_POOLNODES, kGameTag);
    for (int index = 0; index < NUM_POOLNODES; ++index) {
      block[index].allocated = 0;
      nodesPool.Append(&block[index]);
    }
    nodesPoolBlock.Append(block);
  }
  if (edgesPool.Num() == 0) {
    Edge_t* block = AllocateRaw<Edge_t>(
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterGraph.cpp(1432) : TAG_GAME",
        NUM_POOLNODES, kGameTag);
    for (int index = 0; index < NUM_POOLNODES; ++index) {
      block[index].allocated = 0;
      edgesPool.Append(&block[index]);
    }
    edgesPoolBlock.Append(block);
  }
}

idCutterGraphManager::Node_t* idCutterGraphManager::AllocateNode() {
  if (nodesPool.Num() == 0)
    PreAllocatePools();
  Node_t* node = nodesPool[0];
  nodesPool.RemoveIndexFast(0);
  node->id = -1;
  node->next = nullptr;
  node->link = nullptr;
  node->pos = {};
  node->allocated = -1;
  return node;
}

idCutterGraphManager::Edge_t* idCutterGraphManager::AllocateEdge() {
  if (edgesPool.Num() == 0)
    PreAllocatePools();
  Edge_t* edge = edgesPool[0];
  edgesPool.RemoveIndexFast(0);
  edge->visited = 0;
  edge->allocated = 1;
  edge->next = nullptr;
  edge->node1 = nullptr;
  edge->node2 = nullptr;
  edge->contour = nullptr;
  edge->contourEdge = nullptr;
  return edge;
}

void idCutterGraphManager::FreeLink(Link_t* link) {
  while (link != nullptr) {
    Link_t* next = link->next;
    link->allocated = 0;
    link->invalid = 0;
    linksPool.Append(link);
    link = next;
  }
}

void idCutterGraphManager::FreeNode(Node_t* node) {
  while (node != nullptr) {
    Node_t* next = node->next;
    if (node->link != nullptr) {
      FreeLink(node->link);
      node->link = nullptr;
    }
    node->allocated = 0;
    nodesPool.Append(node);
    node = next;
  }
}

void idCutterGraphManager::FreeEdge(Edge_t* edge) {
  while (edge != nullptr) {
    Edge_t* next = edge->next;
    edge->allocated = 0;
    edgesPool.Append(edge);
    edge = next;
  }
}

void idCutterGraphManager::ProcessLinks(Contour* contour, Node_t* node) {
  int linkCount = 0;
  for (Link_t* link = node->link; link != nullptr; link = link->next)
    ++linkCount;
  if (linkCount < 3)
    return;

  for (Link_t* link = node->link; link != nullptr; link = link->next) {
    const idVec2 middle{
        static_cast<float>(node->pos.x) +
            (static_cast<float>(link->node->pos.x) - static_cast<float>(node->pos.x)) * 0.5f,
        static_cast<float>(node->pos.y) +
            (static_cast<float>(link->node->pos.y) - static_cast<float>(node->pos.y)) * 0.5f};
    if (!contour->bound.ContainsPoint(middle))
      continue;

    const int middleX = static_cast<int>(middle.x);
    const int middleY = static_cast<int>(middle.y);
    bool inside = false;
    for (ContourEdge_t* edge = contour->edges; edge != nullptr; edge = edge->next) {
      ContourNode_t* previous = edge->node;
      for (ContourNode_t* current = previous->next; current != nullptr;
           previous = current, current = current->next) {
        if (((previous->pos.y <= middleY && middleY < current->pos.y) ||
             (current->pos.y <= middleY && middleY < previous->pos.y)) &&
            WrappedSubtract(middleX, previous->pos.x) <
                WrappedMultiply(WrappedSubtract(current->pos.x, previous->pos.x),
                                WrappedSubtract(middleY, previous->pos.y)) /
                    WrappedSubtract(current->pos.y, previous->pos.y)) {
          inside = !inside;
        }
      }
    }
    if (!inside)
      continue;

    bool onEdge = false;
    for (ContourEdge_t* edge = contour->edges; edge != nullptr; edge = edge->next) {
      const idVec2 start{static_cast<float>(edge->node->pos.x),
                         static_cast<float>(edge->node->pos.y)};
      idVec2 end = start;
      for (ContourNode_t* current = edge->node->next; current != nullptr;
           current = current->next) {
        end = {static_cast<float>(current->pos.x), static_cast<float>(current->pos.y)};
      }
      if (((middle.y >= start.y && end.y >= middle.y) ||
           (middle.y >= end.y && start.y >= middle.y)) &&
          ((middle.x >= start.x && end.x >= middle.x) ||
           (middle.x >= end.x && start.x >= middle.x)) &&
          (end.x - start.x) * (middle.y - start.y) -
                  (end.y - start.y) * (middle.x - start.x) ==
              0.0f) {
        link->invalid = 2;
        onEdge = true;
        break;
      }
    }
    if (onEdge)
      continue;

    link->invalid |= 1;
    bool found = false;
    for (int index = 0; index < invalidNodes.Num(); ++index) {
      if (invalidNodes[index] == node) {
        found = true;
        break;
      }
    }
    if (!found)
      invalidNodes.Append(node);
  }
}

int idCutterGraphManager::AllocateGraph(const idStr& name, const idVec3& position,
                                        const idMat3& orientation, const idPlane& plane,
                                        const idList<idVec3>& points) {
  Graph* value = AllocateObject<Graph>(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW", TAG_NEW);
  value->SetPosition(position, orientation);
  value->plane = plane;
  NormalVectors({plane.a, plane.b, plane.c}, value->left, value->down);

  idStaticList<idVec2i, 1024> localPoints;
  localPoints.granularity = 1;
  for (int index = 0; index < points.Num(); ++index) {
    idVec2i local;
    local.x = static_cast<int>(std::floor(points[index] * value->left + 0.5f));
    local.y = static_cast<int>(std::floor(points[index] * value->down + 0.5f));
    localPoints.Append(local);
  }
  PreAllocatePools();
  Contour* contour = contourManager->Create(localPoints);
  value->Init(name, contour);
  value->lastNode = nullptr;
  value->prevNode = nullptr;
  graphs.Append(graph_t{value});
  return graphs.Num() - 1;
}

bool idCutterGraphManager::PathFind(idList<idVec2i>& points) {
  return graph->PathFind(points);
}

void idCutterGraphManager::ReleaseGraph(int /*id*/) {
  graph->nodes.ResizeForPort(graph->nodes.Num());
  graph->edges.ResizeForPort(graph->edges.Num());
  graph->links.ResizeForPort(graph->links.Num());
  graph->lastNode = nullptr;
  graph = nullptr;
}

void idCutterGraphManager::FreeGraph(int id) {
  Graph* value = graphs[id].graph;
  if (value == nullptr)
    return;
  if (value == graph)
    ReleaseGraph(id);

  FreeContourList(contourManager, value->contoursInner);
  FreeContourList(contourManager, value->contoursOuter);

  for (int index = 0; index < value->edges.Num(); ++index) {
    value->edges[index].data->next = nullptr;
    FreeEdge(value->edges[index].data);
    value->edges[index].data = nullptr;
  }
  for (int index = 0; index < value->links.Num(); ++index) {
    value->links[index].data->next = nullptr;
    FreeLink(value->links[index].data);
    value->links[index].data = nullptr;
  }
  for (int index = 0; index < value->nodes.Num(); ++index) {
    value->nodes[index].data->next = nullptr;
    value->nodes[index].data->link = nullptr;
    FreeNode(value->nodes[index].data);
    value->nodes[index].data = nullptr;
  }
  value->edges.ClearFree();
  value->nodes.ClearFree();
  value->links.ClearFree();
  FreeObject(value);
  graphs[id].graph = nullptr;
}

void idCutterGraphManager::BuildGraphTree(int id, graphTree_t* graphTree) const {
  if (graphs[id].graph != nullptr)
    graphs[id].graph->BuildGraphTree(graphTree);
}

void idCutterGraphManager::AddLink(Node_t* srcNode, Node_t* dstNode, Edge_t* edge) {
  if (linksPool.Num() == 0)
    PreAllocatePools();
  Link_t* forward = linksPool[0];
  linksPool.RemoveIndexFast(0);
  forward->allocated = 1;
  forward->next = nullptr;
  forward->edge = edge;
  forward->node = dstNode;
  forward->next = srcNode->link;
  srcNode->link = forward;

  if (linksPool.Num() == 0)
    PreAllocatePools();
  Link_t* reverse = linksPool[0];
  linksPool.RemoveIndexFast(0);
  reverse->allocated = 1;
  reverse->next = nullptr;
  reverse->edge = edge;
  reverse->node = srcNode;
  reverse->next = dstNode->link;
  dstNode->link = reverse;

  graph->links.Append({forward});
  graph->links.Append({reverse});
}

void idCutterGraphManager::RemoveLink(Node_t* srcNode, Node_t* dstNode) {
  auto removeOne = [this](Node_t* source, Node_t* target) {
    Link_t* previous = nullptr;
    for (Link_t* link = source->link; link != nullptr; link = link->next) {
      if (link->node == target) {
        if (previous != nullptr)
          previous->next = link->next;
        else
          source->link = link->next;

        for (int index = 0; index < graph->links.Num(); ++index) {
          if (graph->links[index].data == link) {
            graph->links.RemoveIndexFast(index);
            break;
          }
        }
        link->next = nullptr;
        FreeLink(link);
        return;
      }
      previous = link;
    }
  };
  removeOne(srcNode, dstNode);
  removeOne(dstNode, srcNode);
}

void idCutterGraphManager::Connect(Node_t* srcNode, Node_t* dstNode, Contour* contour) {
  Edge_t* edge = AllocateEdge();
  edge->node1 = srcNode;
  edge->node2 = dstNode;
  edge->contour = nullptr;
  edge->contourEdge = nullptr;
  graph->edges.Append({edge});
  edge->contour = contour;
  edge->contourEdge = nullptr;
  AddLink(srcNode, dstNode, edge);
}

bool idCutterGraphManager::BuildEdge(const idVec2i& dst, int maxConnectDistance) {
  if (graph->lastNode != nullptr &&
      WrappedSquaredDistance(graph->lastNode->pos, dst) > WrappedSquare(maxConnectDistance)) {
    graph->lastNode = nullptr;
  }

  if (graph->lastNode == nullptr) {
    int nodeIndex = graph->FindNodeIndex(dst, 6);
    if (nodeIndex < 0) {
      for (Contour* contour = graph->contoursInner; contour != nullptr; contour = contour->next) {
        if (contour->ContainsPoint(dst))
          return false;
      }
    }
    graph->prevNode = graph->lastNode;
    idVec3 unusedPosition;
    graph->GetPosition3D(unusedPosition, dst);
    Node_t* node = nodeIndex >= 0 ? graph->nodes[nodeIndex].data : nullptr;
    if (node == nullptr) {
      node = AllocateNode();
      node->pos = dst;
      graph->AddNode(node);
    }
    graph->lastNode = node;
    return false;
  }

  int nodeIndex = graph->FindNodeIndex(dst, 6);
  Node_t* lastNode = graph->lastNode;
  Node_t* target = nodeIndex >= 0 ? graph->nodes[nodeIndex].data : nullptr;
  if (lastNode == target)
    return false;

  if (target != nullptr && HasGraphLink(lastNode, target)) {
    graph->prevNode = lastNode;
    graph->lastNode = target;
    return false;
  }

  idVec2i testPos;
  Edge_t* splitEdge = graph->FindSplitEdge(lastNode, target != nullptr ? target->pos : dst,
                                            testPos);
  if (splitEdge != nullptr) {
    if (target == nullptr) {
      nodeIndex = graph->FindClosestNode(splitEdge, testPos, 6);
      if (nodeIndex >= 0) {
        target = graph->nodes[nodeIndex].data;
        if (target != nullptr && HasGraphLink(lastNode, target)) {
          graph->prevNode = lastNode;
          graph->lastNode = target;
          return false;
        }
      }
    }
    if (nodeIndex < 0)
      nodeIndex = graph->FindNodeIndex(testPos, 6);

    idVec2i middle;
    if (nodeIndex >= 0) {
      Node_t* splitNode = graph->nodes[nodeIndex].data;
      middle = {(splitNode->pos.x - lastNode->pos.x) / 2 + lastNode->pos.x,
                (splitNode->pos.y - lastNode->pos.y) / 2 + lastNode->pos.y};
    } else {
      middle = {(testPos.x - lastNode->pos.x) / 2 + lastNode->pos.x,
                (testPos.y - lastNode->pos.y) / 2 + lastNode->pos.y};
    }

    bool inside = false;
    for (Contour* contour = graph->contoursInner; contour != nullptr && !inside;
         contour = contour->next)
      inside = contour->ContainsPoint(middle);

    if (nodeIndex > 0)
      target = graph->nodes[nodeIndex].data;
    if (target == splitEdge->node1 || target == splitEdge->node2) {
      if (!inside) {
        if (HasGraphLink(lastNode, splitEdge->node2) ||
            HasGraphLink(lastNode, splitEdge->node1))
          return false;
        Connect(lastNode, target, splitEdge->contour);
      }
      graph->prevNode = graph->lastNode;
      graph->lastNode = target;
      return !inside;
    }
    if (target == lastNode)
      return false;
    graph->prevNode = graph->lastNode;
    graph->lastNode = target;
    return false;
  }

  const idVec2i middle = target != nullptr
                             ? idVec2i{lastNode->pos.x + (target->pos.x - lastNode->pos.x) / 2,
                                      lastNode->pos.y + (target->pos.y - lastNode->pos.y) / 2}
                             : dst;
  for (Contour* contour = graph->contoursInner; contour != nullptr; contour = contour->next) {
    if (contour->ContainsPoint(middle))
      return false;
  }

  if (target != nullptr) {
    if (!HasGraphLink(lastNode, target))
      Connect(lastNode, target, nullptr);
  } else if (WrappedSquaredDistance(lastNode->pos, dst) > 36) {
    target = AllocateNode();
    target->pos = dst;
    Connect(lastNode, target, nullptr);
    graph->AddNode(target);
  }
  graph->prevNode = graph->lastNode;
  graph->lastNode = target;
  return false;
}

void idCutterGraphManager::ApplyGraph(Graph* value) {
  if (value != nullptr) {
    for (int index = 0; index < graphs.Num(); ++index) {
      if (graphs[index].graph != nullptr && graphs[index].graph->name == value->name) {
        graph_t* slot = &graphs[index];
        FreeGraph(index);
        slot->graph = value;
        return;
      }
    }
  }
}

void idCutterGraphManager::Acquire(int id) {
  Graph* requested = graphs[id].graph;
  if (requested == graph)
    return;

  requested->lastNode = nullptr;
  if (graph != nullptr)
    ReleaseGraph(id);
  graph = requested;
  if (graph->nodes.size < 1024)
    graph->nodes.ResizeForPort(1024);
  if (graph->edges.size < 1024)
    graph->edges.ResizeForPort(1024);
  if (graph->links.size < 1024)
    graph->links.ResizeForPort(1024);
}

bool idCutterGraphManager::AddContour(Contour* contours, Contour* mergedContours) {
  for (Contour* contour = contours; contour != nullptr;) {
    Contour* nextContour = contour->next;
    for (ContourEdge_t* edge = contour->edges; edge != nullptr; edge = edge->next) {
      for (ContourNode_t* node = edge->node; node != nullptr && node->next != nullptr;
           node = node->next) {
        const idVec2i& pos0 = node->pos;
        const idVec2i& pos1 = node->next->pos;
        int node0Index = graph->FindNodeIndex(pos0, 2);
        int node1Index = graph->FindNodeIndex(pos1, 2);
        if (node0Index < 0)
          idLib::Printf("idCutterGraphManager::AddContour: Couldn't find node 0, attempting search nearby!\n");
        if (node0Index < 0)
          node0Index = graph->FindNodeIndex(pos0, 2);
        if (node0Index < 0) {
          idLib::Printf("idCutterGraphManager::AddContour: Couldn't find close node 0!\n");
          return false;
        }
        if (node1Index < 0)
          idLib::Printf("idCutterGraphManager::AddContour: Couldn't find node 1, attempting search nearby!\n");
        if (node1Index < 0)
          node1Index = graph->FindNodeIndex(pos1, 2);
        if (node1Index < 0) {
          idLib::Printf("idCutterGraphManager::AddContour: Couldn't find close node 1!\n");
          return false;
        }

        Edge_t* graphEdge = nullptr;
        Node_t* node0 = graph->nodes[node0Index].data;
        Node_t* node1 = graph->nodes[node1Index].data;
        for (Link_t* link = node0->link; link != nullptr; link = link->next) {
          if (link->node == node1) {
            graphEdge = link->edge;
            break;
          }
        }
        if (graphEdge != nullptr) {
          graphEdge->contour = contour;
          continue;
        }

        ContourEdge_t* mergedEdge = mergedContours->FindEdge(pos0, pos1);
        if (mergedEdge == nullptr) {
          idLib::Printf("idCutterGraphManager::AddContour: Couldn't find contour edge!\n");
          return false;
        }
        ContourNode_t* mergedNode = Contour::FindNode(mergedEdge, pos0, pos1);
        int currentIndex = graph->FindNodeIndex(mergedNode->pos, 2);
        while (std::abs(mergedNode->pos.x - pos1.x) > 2 ||
               std::abs(mergedNode->pos.y - pos1.y) > 2) {
          const int nextIndex = graph->FindNodeIndex(mergedNode->next->pos, 2);
          Node_t* currentNode = graph->nodes[currentIndex].data;
          Node_t* nextNode = graph->nodes[nextIndex].data;
          for (Link_t* link = currentNode->link; link != nullptr; link = link->next) {
            if (link->node == nextNode) {
              graphEdge = link->edge;
              break;
            }
          }
          if (graphEdge == nullptr) {
            idLib::Printf("idCutterGraphManager::AddContour: Couldn't find graph edge!\n");
            return false;
          }
          graphEdge->contour = contour;
          mergedNode = mergedNode->next;
          currentIndex = nextIndex;
          if (!SamePoint(nextNode->pos, pos1))
            contourManager->SplitEdge(edge, currentNode->pos, pos1, nextNode->pos);
        }
      }
    }
    contour->next = graph->contoursInner;
    graph->contoursInner = contour;
    contour = nextContour;
  }

  invalidNodes.ClearFree();

  const auto removeEdge = [this](Edge_t* edge) {
    Node_t* node1 = edge->node1;
    Node_t* node2 = edge->node2;
    if (!HasGraphLink(node1, node2) || !HasGraphLink(node2, node1))
      return;
    for (int index = 0; index < graph->edges.Num(); ++index) {
      if (graph->edges[index].data == edge) {
        graph->edges.RemoveIndexFast(index);
        break;
      }
    }
    edge->next = nullptr;
    FreeEdge(edge);
    RemoveLink(node1, node2);
  };

  const auto pointOnBoundary = [](const Contour* contour, const idVec2i& point) {
    for (ContourEdge_t* edge = contour->edges; edge != nullptr; edge = edge->next) {
      const idVec2i first = edge->node->pos;
      idVec2i last{};
      for (ContourNode_t* node = edge->node; node != nullptr; node = node->next) {
        if (SamePoint(node->pos, point))
          return true;
      }
      for (ContourNode_t* node = edge->node->next; node != nullptr; node = node->next)
        last = node->pos;
      const int dx = WrappedSubtract(last.x, first.x);
      const int dy = WrappedSubtract(last.y, first.y);
      const bool collinear =
          (first.y == point.y && last.y == point.y) ||
          (first.x == point.x && last.x == point.x) ||
          WrappedMultiply(WrappedSubtract(first.y, point.y),
                          WrappedSubtract(point.x, last.x)) ==
              WrappedMultiply(WrappedSubtract(first.x, point.x),
                              WrappedSubtract(point.y, last.y));
      if (collinear) {
        const int dot =
            WrappedAdd(WrappedMultiply(dy, WrappedSubtract(point.y, first.y)),
                       WrappedMultiply(dx, WrappedSubtract(point.x, first.x)));
        const int lengthSquared =
            WrappedAdd(WrappedMultiply(dx, dx), WrappedMultiply(dy, dy));
        if (dot >= 0 && dot <= lengthSquared)
          return true;
      }
    }
    return false;
  };

  for (int nodeIndex = graph->nodes.Num() - 1; nodeIndex >= 0; --nodeIndex) {
    Node_t* node = graph->nodes[nodeIndex].data;
    for (Link_t* link = node->link; link != nullptr; link = link->next)
      link->invalid = 0;

    bool removeNode = false;
    for (Contour* contour = graph->contoursInner; contour != nullptr; contour = contour->next) {
      const idVec2 point{static_cast<float>(node->pos.x), static_cast<float>(node->pos.y)};
      if (!contour->bound.ContainsPoint(point))
        continue;
      const bool strictlyInside = contour->ContainsPoint(node->pos) &&
                                  !pointOnBoundary(contour, node->pos);
      if (strictlyInside) {
        removeNode = true;
        break;
      }
      if (node->link != nullptr) {
        ProcessLinks(contour, node);
        break;
      }
    }

    if (removeNode) {
      for (Link_t* link = node->link; link != nullptr;) {
        Link_t* next = link->next;
        removeEdge(link->edge);
        link = next;
      }
      node->link = nullptr;
      node->next = nullptr;
      graph->nodes.RemoveIndexFast(nodeIndex);
      FreeNode(node);
    }
  }

  for (int nodeIndex = 0; nodeIndex < invalidNodes.Num(); ++nodeIndex) {
    for (Link_t* link = invalidNodes[nodeIndex]->link; link != nullptr;) {
      Link_t* next = link->next;
      if ((link->invalid & 3) == 1)
        removeEdge(link->edge);
      link = next;
    }
  }
  return true;
}

Contour* idCutterGraphManager::IntersectContours(Contour* contour) {
  idStaticList<idVec2i, 512> points;
  points.granularity = 1;
  for (ContourEdge_t* edge = contour->edges; edge != nullptr;
       edge = edge->next) {
    points.Append(edge->node->pos);
  }
  const int inputCount = points.Num();

  Contour* result = nullptr;
  Contour* previous = nullptr;
  Contour* current = graph->contoursInner;
  while (current != nullptr) {
    Contour* next = current->next;
    bool intersects = false;
    if (contour->bound.IntersectBounds(current->bound)) {
      points.SetNum(inputCount);
      for (ContourEdge_t* edge = contour->edges; edge != nullptr;
           edge = edge->next) {
        points.Append(edge->node->pos);
      }

      const int totalCount = points.Num();
      for (int pass = 0; pass < 2 && !intersects; ++pass) {
        const int candidateEnd = pass == 0 ? inputCount : totalCount;
        const int polygonEnd = pass == 1 ? inputCount : totalCount;
        const int candidateBegin = candidateEnd - inputCount;
        const int polygonBegin = polygonEnd - inputCount;
        for (int candidateIndex = candidateBegin;
             candidateIndex < candidateEnd && !intersects;
             ++candidateIndex) {
          const idVec2i candidate = points[candidateIndex];
          bool inside = false;
          int previousIndex = polygonEnd - 1;
          for (int polygonIndex = polygonBegin;
               polygonIndex < polygonEnd && !intersects;
               ++polygonIndex) {
            const idVec2i& previousPoint = points[previousIndex];
            const idVec2i& currentPoint = points[polygonIndex];
            if (SamePoint(previousPoint, candidate) ||
                SamePoint(currentPoint, candidate)) {
              intersects = true;
            } else if (((previousPoint.y < candidate.y &&
                         currentPoint.y >= candidate.y) ||
                        (currentPoint.y < candidate.y &&
                         previousPoint.y >= candidate.y)) &&
                       WrappedAdd(
                           previousPoint.x,
                           WrappedMultiply(
                               WrappedSubtract(currentPoint.x,
                                               previousPoint.x),
                               WrappedSubtract(candidate.y,
                                               previousPoint.y) /
                                   WrappedSubtract(currentPoint.y,
                                                   previousPoint.y))) <
                           candidate.x) {
              inside = !inside;
            }
            previousIndex = polygonIndex;
          }
          intersects = intersects || inside;
        }
      }
    }

    if (intersects) {
      current->next = result;
      result = current;
      if (previous != nullptr)
        previous->next = next;
      else
        graph->contoursInner = next;
    } else {
      previous = current;
    }
    current = next;
  }
  return result;
}

void idCutterGraphManager::Write(idFile* file, int graphHandle) const {
  const Graph* value = graphHandle >= 0 ? graphs[graphHandle].graph : nullptr;
  const std::uint8_t valid = value != nullptr ? 1U : 0U;
  file->Write(&valid, sizeof(valid));
  if (value == nullptr)
    return;

  std::vector<Node_t*> nodes;
  std::vector<std::uint16_t> nodesMapping;
  for (int index = 0; index < value->nodes.Num(); ++index) {
    nodesMapping.push_back(static_cast<std::uint16_t>(AppendUnique(nodes, value->nodes[index].data)));
  }

  std::vector<Edge_t*> edges;
  std::vector<std::uint16_t> edgesMapping;
  for (int index = 0; index < value->edges.Num(); ++index) {
    edgesMapping.push_back(static_cast<std::uint16_t>(AppendUnique(edges, value->edges[index].data)));
  }

  std::vector<Link_t*> links;
  std::vector<std::uint16_t> linksMapping;
  for (int index = 0; index < value->links.Num(); ++index) {
    linksMapping.push_back(static_cast<std::uint16_t>(AppendUnique(links, value->links[index].data)));
  }

  std::vector<Contour*> contours;
  for (Contour* contour = value->contoursOuter; contour != nullptr; contour = contour->next)
    AppendUnique(contours, contour);
  for (Contour* contour = value->contoursInner; contour != nullptr; contour = contour->next)
    AppendUnique(contours, contour);

  std::vector<ContourEdge_t*> contourEdges;
  for (Contour* contour : contours) {
    for (ContourEdge_t* edge = contour->edges; edge != nullptr; edge = edge->next)
      AppendUnique(contourEdges, edge);
  }
  std::vector<ContourNode_t*> contourNodes;
  for (ContourEdge_t* edge : contourEdges) {
    for (ContourNode_t* node = edge->node; node != nullptr; node = node->next)
      AppendUnique(contourNodes, node);
  }

  WriteBigShort(file, static_cast<std::uint16_t>(contourNodes.size()));
  for (ContourNode_t* node : contourNodes) {
    WriteIndex(file, PointerIndex(contourNodes, node->next));
    WriteBigInt(file, static_cast<std::uint32_t>(node->pos.x));
    WriteBigInt(file, static_cast<std::uint32_t>(node->pos.y));
  }

  WriteBigShort(file, static_cast<std::uint16_t>(contourEdges.size()));
  for (ContourEdge_t* edge : contourEdges) {
    WriteIndex(file, PointerIndex(contourEdges, edge->next));
    WriteIndex(file, PointerIndex(contourNodes, edge->node));
  }

  WriteBigShort(file, static_cast<std::uint16_t>(contours.size()));
  for (Contour* contour : contours) {
    WriteIndex(file, PointerIndex(contours, contour->next));
    WriteIndex(file, PointerIndex(contourEdges, contour->edges));
    WriteBigShort(file, static_cast<std::uint16_t>(contour->numEdges));
    WriteBigFloat(file, contour->bound.bounds[0].x);
    WriteBigFloat(file, contour->bound.bounds[0].y);
    WriteBigFloat(file, contour->bound.bounds[1].x);
    WriteBigFloat(file, contour->bound.bounds[1].y);
  }

  WriteBigShort(file, static_cast<std::uint16_t>(nodes.size()));
  for (Node_t* node : nodes) {
    WriteIndex(file, PointerIndex(nodes, node->next));
    WriteIndex(file, PointerIndex(links, node->link));
    WriteBigInt(file, static_cast<std::uint32_t>(node->pos.x));
    WriteBigInt(file, static_cast<std::uint32_t>(node->pos.y));
    WriteBigInt(file, static_cast<std::uint32_t>(node->id));
    const std::uint8_t allocated = static_cast<std::uint8_t>(node->allocated);
    file->Write(&allocated, sizeof(allocated));
  }

  WriteBigShort(file, static_cast<std::uint16_t>(edges.size()));
  for (Edge_t* edge : edges) {
    WriteIndex(file, PointerIndex(edges, edge->next));
    WriteIndex(file, PointerIndex(nodes, edge->node1));
    WriteIndex(file, PointerIndex(nodes, edge->node2));
    WriteIndex(file, PointerIndex(contours, edge->contour));
    WriteIndex(file, PointerIndex(contourEdges, edge->contourEdge));
    const std::uint8_t flags = static_cast<std::uint8_t>(edge->visited | (edge->allocated << 1));
    file->Write(&flags, sizeof(flags));
  }

  WriteBigShort(file, static_cast<std::uint16_t>(links.size()));
  for (Link_t* link : links) {
    WriteIndex(file, PointerIndex(links, link->next));
    WriteIndex(file, PointerIndex(edges, link->edge));
    WriteIndex(file, PointerIndex(nodes, link->node));
    const std::uint8_t allocated = static_cast<std::uint8_t>(link->allocated);
    file->Write(&allocated, sizeof(allocated));
  }

  WriteBigInt(file, static_cast<std::uint32_t>(nodesMapping.size()));
  for (std::uint16_t mapping : nodesMapping)
    WriteBigShort(file, mapping);
  WriteBigInt(file, static_cast<std::uint32_t>(edgesMapping.size()));
  for (std::uint16_t mapping : edgesMapping)
    WriteBigShort(file, mapping);
  WriteBigInt(file, static_cast<std::uint32_t>(linksMapping.size()));
  for (std::uint16_t mapping : linksMapping)
    WriteBigShort(file, mapping);

  WriteBigShort(file,
                static_cast<std::uint16_t>(PointerIndex(contours, value->contoursOuter) + 1));
  WriteBigShort(file,
                static_cast<std::uint16_t>(PointerIndex(contours, value->contoursInner) + 1));
  value->idheap.Write(file);

  const int nameLength = value->name.Length();
  file->Write(&nameLength, sizeof(nameLength));
  file->Write(value->name.data, static_cast<std::size_t>(nameLength));
  const float* inverse = &value->inverse.mat[0].x;
  const float* orientation = &value->orientation.mat[0].x;
  for (int index = 0; index < 9; ++index)
    WriteBigFloat(file, inverse[index]);
  for (int index = 0; index < 9; ++index)
    WriteBigFloat(file, orientation[index]);
  for (const float component : {value->position.x, value->position.y, value->position.z})
    WriteBigFloat(file, component);
  for (const float component : {value->plane.a, value->plane.b, value->plane.c, value->plane.d})
    WriteBigFloat(file, component);
  for (const float component : {value->left.x, value->left.y, value->left.z})
    WriteBigFloat(file, component);
  for (const float component : {value->down.x, value->down.y, value->down.z})
    WriteBigFloat(file, component);
}

void idCutterGraphManager::Read(idFile* file) {
  std::uint8_t valid = 0;
  file->Read(&valid, sizeof(valid));
  if (valid == 0)
    return;
  Graph* value = AllocateObject<Graph>(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW", TAG_NEW);
  value->lastNode = nullptr;
  value->prevNode = nullptr;

  struct ContourNodeRecord { int next; };
  struct ContourEdgeRecord { int next; int node; };
  struct ContourRecord { int next; int edge; };
  struct NodeRecord { int next; int link; };
  struct EdgeRecord { int next; int node1; int node2; int contour; int contourEdge; };
  struct LinkRecord { int next; int edge; int node; };

  std::vector<ContourNode_t*> contourNodes(ReadBigShort(file));
  std::vector<ContourNodeRecord> contourNodeRecords(contourNodes.size());
  for (std::size_t index = 0; index < contourNodes.size(); ++index) {
    ContourNode_t* node = contourManager->AllocNode();
    contourNodes[index] = node;
    contourNodeRecords[index].next = ReadIndex(file);
    node->pos.x = static_cast<int>(ReadBigInt(file));
    node->pos.y = static_cast<int>(ReadBigInt(file));
  }

  std::vector<ContourEdge_t*> contourEdges(ReadBigShort(file));
  std::vector<ContourEdgeRecord> contourEdgeRecords(contourEdges.size());
  for (std::size_t index = 0; index < contourEdges.size(); ++index) {
    contourEdges[index] = contourManager->AllocEdge();
    contourEdgeRecords[index].next = ReadIndex(file);
    contourEdgeRecords[index].node = ReadIndex(file);
  }

  std::vector<Contour*> contours(ReadBigShort(file));
  std::vector<ContourRecord> contourRecords(contours.size());
  for (std::size_t index = 0; index < contours.size(); ++index) {
    Contour* contour = contourManager->AllocContour();
    contours[index] = contour;
    contourRecords[index].next = ReadIndex(file);
    contourRecords[index].edge = ReadIndex(file);
    contour->numEdges = ReadBigShort(file);
    contour->bound.bounds[0] = {ReadBigFloat(file), ReadBigFloat(file)};
    contour->bound.bounds[1] = {ReadBigFloat(file), ReadBigFloat(file)};
  }

  std::vector<Node_t*> nodes(ReadBigShort(file));
  std::vector<NodeRecord> nodeRecords(nodes.size());
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    Node_t* node = AllocateNode();
    nodes[index] = node;
    nodeRecords[index].next = ReadIndex(file);
    nodeRecords[index].link = ReadIndex(file);
    node->pos.x = static_cast<int>(ReadBigInt(file));
    node->pos.y = static_cast<int>(ReadBigInt(file));
    node->id = static_cast<std::int32_t>(ReadBigInt(file));
    std::uint8_t allocated;
    file->Read(&allocated, sizeof(allocated));
    node->allocated = allocated;
  }

  std::vector<Edge_t*> edges(ReadBigShort(file));
  std::vector<EdgeRecord> edgeRecords(edges.size());
  for (std::size_t index = 0; index < edges.size(); ++index) {
    Edge_t* edge = AllocateEdge();
    edges[index] = edge;
    edgeRecords[index].next = ReadIndex(file);
    edgeRecords[index].node1 = ReadIndex(file);
    edgeRecords[index].node2 = ReadIndex(file);
    edgeRecords[index].contour = ReadIndex(file);
    edgeRecords[index].contourEdge = ReadIndex(file);
    std::uint8_t flags;
    file->Read(&flags, sizeof(flags));
    edge->visited = flags & 1U;
    edge->allocated = (flags >> 1) & 1U;
  }

  std::vector<Link_t*> links(ReadBigShort(file));
  std::vector<LinkRecord> linkRecords(links.size());
  for (std::size_t index = 0; index < links.size(); ++index) {
    if (linksPool.Num() == 0)
      PreAllocatePools();
    Link_t* link = linksPool[0];
    linksPool.RemoveIndexFast(0);
    links[index] = link;
    link->next = nullptr;
    link->edge = nullptr;
    link->node = nullptr;
    link->allocated = 1;
    linkRecords[index].next = ReadIndex(file);
    linkRecords[index].edge = ReadIndex(file);
    linkRecords[index].node = ReadIndex(file);
    std::uint8_t allocated;
    file->Read(&allocated, sizeof(allocated));
    link->allocated = allocated;
    link->invalid = 0;
  }

  for (std::size_t index = 0; index < contourNodes.size(); ++index)
    contourNodes[index]->next = PointerAt(contourNodes, contourNodeRecords[index].next);
  for (std::size_t index = 0; index < contourEdges.size(); ++index) {
    contourEdges[index]->next = PointerAt(contourEdges, contourEdgeRecords[index].next);
    contourEdges[index]->node = PointerAt(contourNodes, contourEdgeRecords[index].node);
  }
  for (std::size_t index = 0; index < contours.size(); ++index) {
    contours[index]->next = PointerAt(contours, contourRecords[index].next);
    contours[index]->edges = PointerAt(contourEdges, contourRecords[index].edge);
  }
  for (std::size_t index = 0; index < nodes.size(); ++index) {
    nodes[index]->next = PointerAt(nodes, nodeRecords[index].next);
    nodes[index]->link = PointerAt(links, nodeRecords[index].link);
  }
  for (std::size_t index = 0; index < edges.size(); ++index) {
    edges[index]->next = PointerAt(edges, edgeRecords[index].next);
    edges[index]->node1 = PointerAt(nodes, edgeRecords[index].node1);
    edges[index]->node2 = PointerAt(nodes, edgeRecords[index].node2);
    edges[index]->contour = PointerAt(contours, edgeRecords[index].contour);
    edges[index]->contourEdge = PointerAt(contourEdges, edgeRecords[index].contourEdge);
  }
  for (std::size_t index = 0; index < links.size(); ++index) {
    links[index]->next = PointerAt(links, linkRecords[index].next);
    links[index]->edge = PointerAt(edges, linkRecords[index].edge);
    links[index]->node = PointerAt(nodes, linkRecords[index].node);
  }

  const std::uint32_t nodeMappingCount = ReadBigInt(file);
  value->nodes.ResizeForPort(static_cast<int>(nodeMappingCount));
  value->nodes.num = std::min(static_cast<int>(nodeMappingCount), value->nodes.size);
  for (std::uint32_t index = 0; index < nodeMappingCount; ++index)
    value->nodes.list[index].data = nodes[ReadBigShort(file)];
  const std::uint32_t edgeMappingCount = ReadBigInt(file);
  value->edges.ResizeForPort(static_cast<int>(edgeMappingCount));
  value->edges.num = std::min(static_cast<int>(edgeMappingCount), value->edges.size);
  for (std::uint32_t index = 0; index < edgeMappingCount; ++index)
    value->edges.list[index].data = edges[ReadBigShort(file)];
  const std::uint32_t linkMappingCount = ReadBigInt(file);
  value->links.ResizeForPort(static_cast<int>(linkMappingCount));
  value->links.num = std::min(static_cast<int>(linkMappingCount), value->links.size);
  for (std::uint32_t index = 0; index < linkMappingCount; ++index)
    value->links.list[index].data = links[ReadBigShort(file)];

  const std::uint16_t outerIndex = ReadBigShort(file);
  value->contoursOuter = outerIndex != 0 ? contours[outerIndex - 1] : nullptr;
  const std::uint16_t innerIndex = ReadBigShort(file);
  value->contoursInner = innerIndex != 0 ? contours[innerIndex - 1] : nullptr;
  value->idheap.Read(file);

  int nameLength = 0;
  file->Read(&nameLength, sizeof(nameLength));
  if (nameLength <= 0) {
    value->name.Empty();
  } else {
    std::vector<char> text(static_cast<std::size_t>(nameLength) + 1U, '\0');
    file->Read(text.data(), static_cast<std::size_t>(nameLength));
    value->name = idStr(text.data(), 0, nameLength);
  }

  float* inverse = &value->inverse.mat[0].x;
  float* orientation = &value->orientation.mat[0].x;
  for (int index = 0; index < 9; ++index)
    inverse[index] = ReadBigFloat(file);
  for (int index = 0; index < 9; ++index)
    orientation[index] = ReadBigFloat(file);
  value->position = {ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file)};
  value->plane = {ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file)};
  value->left = {ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file)};
  value->down = {ReadBigFloat(file), ReadBigFloat(file), ReadBigFloat(file)};

  ApplyGraph(value);
}
