#include "engine/models/cuttable/cuttercontour.h"

#include "shared/idlib/sys/sys_alloc.h"

#include <cstdint>
#include <limits>

namespace {

constexpr memTag_t kGameTag = static_cast<memTag_t>(0x4D);

template <typename Type>
Type* Allocate(const char* location) {
  return static_cast<Type*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(Type), kGameTag, false, ALIGN_16, HEAP_DEFAULTHEAP));
}

std::int32_t WrapAdd(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapSubtract(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) -
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapMultiply(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) *
                                   static_cast<std::uint32_t>(right));
}

bool SamePoint(const idVec2i& left, const idVec2i& right) {
  return left.x == right.x && left.y == right.y;
}

ContourNode_t* ReverseNodes(ContourNode_t* node) {
  ContourNode_t* previous = nullptr;
  while (node != nullptr) {
    ContourNode_t* next = node->next;
    node->next = previous;
    previous = node;
    node = next;
  }
  return previous;
}

} // namespace

idContourManager::idContourManager()
    : contoursPool(nullptr), edgesPool(nullptr), nodesPool(nullptr) {}

idContourManager::~idContourManager() {
  while (contoursPool != nullptr) {
    Contour* next = contoursPool->next;
    idMem::Singleton()->Free(contoursPool, ALIGN_16);
    contoursPool = next;
  }
  while (edgesPool != nullptr) {
    ContourEdge_t* next = edgesPool->next;
    idMem::Singleton()->Free(edgesPool, ALIGN_16);
    edgesPool = next;
  }
  while (nodesPool != nullptr) {
    ContourNode_t* next = nodesPool->next;
    idMem::Singleton()->Free(nodesPool, ALIGN_16);
    nodesPool = next;
  }
}

void idContourManager::Free(Contour* contour) {
  for (ContourEdge_t* edge = contour->edges; edge != nullptr;) {
    ContourEdge_t* nextEdge = edge->next;
    for (ContourNode_t* node = edge->node; node != nullptr;) {
      ContourNode_t* nextNode = node->next;
      node->next = nodesPool;
      nodesPool = node;
      node = nextNode;
    }
    edge->next = edgesPool;
    edgesPool = edge;
    edge = nextEdge;
  }
  contour->next = contoursPool;
  contoursPool = contour;
}

ContourEdge_t* idContourManager::AllocEdge() {
  if (edgesPool == nullptr) {
    edgesPool = Allocate<ContourEdge_t>(
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterContour.cpp(409) : TAG_GAME");
    edgesPool->next = nullptr;
  }
  ContourEdge_t* result = edgesPool;
  edgesPool = result->next;
  return result;
}

Contour* idContourManager::AllocContour() {
  if (contoursPool == nullptr) {
    contoursPool = Allocate<Contour>(
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterContour.cpp(393) : TAG_GAME");
    contoursPool->next = nullptr;
  }
  Contour* result = contoursPool;
  contoursPool = contoursPool->next;
  return result;
}

ContourNode_t* idContourManager::AllocNode() {
  if (nodesPool == nullptr) {
    nodesPool = Allocate<ContourNode_t>(
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterContour.cpp(425) : TAG_GAME");
    nodesPool->next = nullptr;
  }
  ContourNode_t* result = nodesPool;
  nodesPool = result->next;
  return result;
}

Contour* idContourManager::Create(const idList<idVec2i>& points) {
  Contour* contour = AllocContour();
  const int numPoints = points.Num();
  int nextNext = 2;
  int current = 0;
  int next = 1;
  if (numPoints > 3) {
    int previous;
    do {
      previous = current;
      current = next;
      next = nextNext;
      nextNext = (nextNext + 1) % numPoints;
    } while (Contour::SlopesEqual(points[previous], points[current],
                                  points[next]) &&
             current > 0);
  }

  int minimumX = std::numeric_limits<std::int32_t>::max();
  int minimumY = std::numeric_limits<std::int32_t>::max();
  int maximumX = std::numeric_limits<std::int32_t>::min();
  int maximumY = std::numeric_limits<std::int32_t>::min();
  int numEdges = 0;
  ContourNode_t* firstNode = nullptr;
  ContourNode_t* lastNode = nullptr;
  ContourEdge_t* firstEdge = nullptr;
  ContourEdge_t* lastEdge = nullptr;
  const int start = current;

  do {
    if (lastNode == nullptr) {
      lastNode = AllocNode();
      firstNode = lastNode;
      lastNode->next = nullptr;
      lastNode->pos = points[current];
      if (minimumX > points[current].x)
        minimumX = points[current].x;
      if (minimumY > points[current].y)
        minimumY = points[current].y;
      if (maximumX < points[current].x)
        maximumX = points[current].x;
      if (maximumY < points[current].y)
        maximumY = points[current].y;
    }

    ContourNode_t* appended = AllocNode();
    lastNode->next = appended;
    appended->next = nullptr;
    appended->pos = points[next];
    lastNode = appended;

    if (!Contour::SlopesEqual(points[current], points[next],
                              points[nextNext])) {
      ContourEdge_t* edge = AllocEdge();
      if (lastEdge != nullptr)
        lastEdge->next = edge;
      else
        firstEdge = edge;
      lastEdge = edge;
      ++numEdges;
      edge->node = firstNode;
      edge->next = nullptr;
      lastNode = nullptr;
    }

    current = next;
    next = nextNext;
    nextNext = (nextNext + 1) % numPoints;
  } while (current != start);

  contour->edges = firstEdge;
  contour->numEdges = numEdges;
  contour->next = nullptr;
  const float boundX = static_cast<float>(WrapSubtract(minimumX, 1));
  const float boundY = static_cast<float>(WrapSubtract(minimumY, 1));
  contour->bound.bounds[0] = {boundX, boundY};
  contour->bound.bounds[1].x =
      boundX + static_cast<float>(WrapAdd(WrapSubtract(maximumX, minimumX), 2));
  contour->bound.bounds[1].y =
      boundY + static_cast<float>(WrapAdd(WrapSubtract(maximumY, minimumY), 2));
  return contour;
}

void idContourManager::Merge(Contour* contours) {
  ContourNode_t* recycled = nullptr;
  for (Contour* leftContour = contours; leftContour != nullptr;
       leftContour = leftContour->next) {
    for (Contour* rightContour = leftContour->next; rightContour != nullptr;
         rightContour = rightContour->next) {
      for (ContourEdge_t* leftEdge = leftContour->edges; leftEdge != nullptr;
           leftEdge = leftEdge->next) {
        for (ContourEdge_t* rightEdge = rightContour->edges;
             rightEdge != nullptr; rightEdge = rightEdge->next) {
          ContourNode_t* leftShared = nullptr;
          ContourNode_t* rightShared = nullptr;
          if (Contour::SlopesEqual(leftEdge->node->pos,
                                   leftEdge->node->next->pos,
                                   rightEdge->node->pos,
                                   rightEdge->node->next->pos)) {
            for (ContourNode_t* left = leftEdge->node;
                 left != nullptr && leftShared == nullptr; left = left->next) {
              for (ContourNode_t* right = rightEdge->node; right != nullptr;
                   right = right->next) {
                if (SamePoint(left->pos, right->pos)) {
                  leftShared = left;
                  rightShared = right;
                  break;
                }
              }
            }
          }
          if (rightShared == nullptr)
            continue;

          ContourNode_t* leftNode = leftEdge->node;
          ContourNode_t* rightNode = rightEdge->node;
          const int dot = WrapAdd(
              WrapMultiply(WrapSubtract(leftNode->next->pos.x,
                                        leftNode->pos.x),
                           WrapSubtract(rightNode->next->pos.x,
                                        rightNode->pos.x)),
              WrapMultiply(WrapSubtract(rightNode->next->pos.y,
                                        rightNode->pos.y),
                           WrapSubtract(leftNode->next->pos.y,
                                        leftNode->pos.y)));
          if (dot <= 0) {
            rightNode = ReverseNodes(rightNode);
            rightEdge->node = rightNode;
          }

          ContourNode_t* nodes[256];
          int count = 0;
          if (leftShared == leftNode) {
            while (rightNode != rightShared) {
              nodes[count++] = rightNode;
              rightNode = rightNode->next;
            }
          } else {
            while (leftNode != leftShared) {
              nodes[count++] = leftNode;
              leftNode = leftNode->next;
            }
          }

          while (leftNode != nullptr && rightNode != nullptr) {
            nodes[count++] = leftNode;
            leftNode = leftNode->next;
            ContourNode_t* discarded = rightNode;
            rightNode = rightNode->next;
            discarded->next = recycled;
            recycled = discarded;
          }
          while (leftNode != nullptr) {
            nodes[count++] = leftNode;
            leftNode = leftNode->next;
          }
          while (rightNode != nullptr) {
            nodes[count++] = rightNode;
            rightNode = rightNode->next;
          }
          nodes[count] = nullptr;
          for (int index = 0; index < count; ++index)
            nodes[index]->next = nodes[index + 1];
          leftEdge->node = nodes[0];

          for (int index = 0; index < count; ++index) {
            ContourNode_t* copy;
            if (recycled != nullptr) {
              copy = recycled;
              recycled = recycled->next;
            } else {
              copy = AllocNode();
            }
            const idVec2i position = nodes[index]->pos;
            nodes[index] = copy;
            copy->pos = position;
          }
          while (recycled != nullptr) {
            ContourNode_t* nextRecycled = recycled->next;
            recycled->next = nodesPool;
            nodesPool = recycled;
            recycled = nextRecycled;
          }
          for (int index = 0; index < count; ++index)
            nodes[index]->next = nodes[index + 1];
          rightEdge->node = nodes[0];
          break;
        }
      }
    }
  }
}

void idContourManager::SplitEdge(ContourEdge_t* edge, const idVec2i& v0,
                                 const idVec2i& v1, const idVec2i& pos) {
  ContourNode_t* node = edge->node;
  ContourNode_t* next = node->next;
  while (next != nullptr) {
    if ((SamePoint(v0, node->pos) && SamePoint(v1, next->pos)) ||
        (SamePoint(v0, next->pos) && SamePoint(v1, node->pos))) {
      ContourNode_t* inserted = AllocNode();
      inserted->next = next;
      inserted->pos = pos;
      node->next = inserted;
      for (ContourNode_t* scan = edge->node; scan != nullptr;
           scan = scan->next) {
      }
      return;
    }
    node = next;
    next = next->next;
  }
}
