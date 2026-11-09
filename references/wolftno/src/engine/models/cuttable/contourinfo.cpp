#include "engine/models/cuttable/contourinfo.h"

#include <cstdint>

namespace {

bool SamePoint(const idVec2i& left, const idVec2i& right) {
  return left.x == right.x && left.y == right.y;
}

std::int32_t WrapSubtract(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) -
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapMultiply(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) *
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapAdd(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

bool SlopesEqual(const idVec2i& first, const idVec2i& second,
                 const idVec2i& third, const idVec2i& fourth) {
  if (first.y == second.y)
    return third.y == fourth.y;
  if (first.x == second.x)
    return third.x == fourth.x;
  return WrapMultiply(WrapSubtract(first.y, second.y),
                      WrapSubtract(third.x, fourth.x)) ==
         WrapMultiply(WrapSubtract(first.x, second.x),
                      WrapSubtract(third.y, fourth.y));
}

} // namespace

bool Contour::SlopesEqual(const idVec2i& v1, const idVec2i& v2,
                          const idVec2i& v3, const idVec2i& v4) {
  return ::SlopesEqual(v1, v2, v3, v4);
}

bool Contour::SlopesEqual(const idVec2i& v1, const idVec2i& v2,
                          const idVec2i& v3) {
  return ::SlopesEqual(v1, v2, v2, v3);
}

bool Contour::SlopesEqual(const idList<idVec2i>& points) {
  const int count = points.Num() - 2;
  for (int index = 0; index < count; ++index) {
    if (!::SlopesEqual(points[index], points[index + 1], points[index + 1],
                       points[index + 2]))
      return false;
  }
  return true;
}

ContourEdge_t* Contour::FindEdge(const idVec2i& v0, const idVec2i& v1) {
  for (Contour* contour = this; contour != nullptr; contour = contour->next) {
    const idVec2 point0{static_cast<float>(v0.x), static_cast<float>(v0.y)};
    const idVec2 point1{static_cast<float>(v1.x), static_cast<float>(v1.y)};
    if (!contour->bound.ContainsPoint(point0) &&
        !contour->bound.ContainsPoint(point1))
      continue;

    for (ContourEdge_t* edge = contour->edges; edge != nullptr;
         edge = edge->next) {
      ContourNode_t* node = edge->node;
      while (node != nullptr) {
        const idVec2i* target = nullptr;
        if (SamePoint(node->pos, v0))
          target = &v1;
        else if (SamePoint(node->pos, v1))
          target = &v0;
        if (target != nullptr) {
          while (node != nullptr) {
            if (SamePoint(node->pos, *target))
              return edge;
            node = node->next;
          }
          break;
        }
        node = node->next;
      }
    }
  }
  return nullptr;
}

ContourNode_t* Contour::FindNode(ContourEdge_t* edge, const idVec2i& v0,
                                 const idVec2i& v1) {
  ContourNode_t* const first = edge->node;
  for (ContourNode_t* node = first; node != nullptr; node = node->next) {
    if (SamePoint(node->pos, v1)) {
      ContourNode_t* reversed = nullptr;
      ContourNode_t* current = first;
      while (current != nullptr) {
        ContourNode_t* next = current->next;
        current->next = reversed;
        reversed = current;
        current = next;
      }
      edge->node = reversed;
      for (node = reversed; node != nullptr; node = node->next) {
        if (SamePoint(node->pos, v0))
          return node;
      }
      return nullptr;
    }
    if (SamePoint(node->pos, v0))
      return node;
  }
  return nullptr;
}

bool Contour::ContainsPoint(const idVec2i& pos) const {
  bool inside = false;
  for (const ContourEdge_t* edge = edges; edge != nullptr;
       edge = edge->next) {
    const ContourNode_t* node = edge->node;
    const idVec2i first = node->pos;
    if (SamePoint(first, pos))
      return true;

    idVec2i last{};
    while (node != nullptr) {
      last = node->pos;
      node = node->next;
      if (SamePoint(last, pos))
        return true;
    }

    const bool onLine = ::SlopesEqual(first, pos, pos, last);
    const std::int32_t edgeX = WrapSubtract(last.x, first.x);
    const std::int32_t pointY = WrapSubtract(pos.y, first.y);
    const std::int32_t edgeY = WrapSubtract(last.y, first.y);
    if (onLine) {
      const std::int32_t dot =
          WrapAdd(WrapMultiply(edgeX, WrapSubtract(pos.x, first.x)),
                  WrapMultiply(pointY, edgeY));
      const std::int32_t lengthSquared =
          WrapAdd(WrapMultiply(edgeY, edgeY), WrapMultiply(edgeX, edgeX));
      if (dot >= 0 && dot <= lengthSquared)
        return true;
    }

    if (((first.y < pos.y && last.y >= pos.y) ||
         (last.y < pos.y && first.y >= pos.y)) &&
        WrapAdd(first.x,
                WrapMultiply(edgeX, pointY / edgeY)) < pos.x) {
      inside = !inside;
    }
  }
  return inside;
}
