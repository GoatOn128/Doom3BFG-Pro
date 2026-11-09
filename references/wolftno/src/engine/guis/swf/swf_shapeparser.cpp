#include "engine/guis/swf/swf_shapeparser.h"

#include "engine/guis/swf/swf_bitstream.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cassert>
#include <cfloat>
#include <cmath>
#include <utility>

namespace {
bool SameVert(const idVec2& lhs, const idVec2& rhs) {
  return lhs.x == rhs.x && lhs.y == rhs.y;
}

template <typename T, int Tag>
int AddUnique(idList<T, Tag>& values, const T& value) {
  for (int i = 0; i < values.Num(); ++i) {
    if (SameVert(values[i], value)) {
      return i;
    }
  }
  return values.Append(value);
}

template <typename T, int Tag>
void RemoveIndexFast(idList<T, Tag>& values, int index) {
  if (index >= 0 && index < values.Num()) {
    if (index != values.Num() - 1) {
      values[index] = std::move(values[values.Num() - 1]);
    }
    values.SetNum(values.Num() - 1);
  }
}

idVec3 MultiplyRowVector(const idVec3& vector, const idMat3& matrix) {
  return {vector.x * matrix[0].x + vector.y * matrix[1].x + vector.z * matrix[2].x,
          vector.x * matrix[0].y + vector.y * matrix[1].y + vector.z * matrix[2].y,
          vector.x * matrix[0].z + vector.y * matrix[1].z + vector.z * matrix[2].z};
}

struct earVert_t {
  int i1;
  int i2;
  int i3;
  float cross;
};
}

void idSWFShapeParser::ParseEdge(idSWFBitStream& bitstream, int& penX, int& penY,
                                 swfSPEdge_t& edge) {
  const bool straight = bitstream.ReadBool();
  const unsigned int numBits = bitstream.ReadU(4) + 2;
  edge.v0 = static_cast<std::uint16_t>(AddUnique(verts, idVec2{penX * 0.05f, penY * 0.05f}));

  if (straight) {
    edge.cp = 0xffff;
    if (bitstream.ReadBool()) {
      penX += bitstream.ReadS(numBits);
      penY += bitstream.ReadS(numBits);
    } else if (bitstream.ReadBool()) {
      penY += bitstream.ReadS(numBits);
    } else {
      penX += bitstream.ReadS(numBits);
    }
  } else {
    penX += bitstream.ReadS(numBits);
    penY += bitstream.ReadS(numBits);
    edge.cp = static_cast<std::uint16_t>(AddUnique(verts, idVec2{penX * 0.05f, penY * 0.05f}));
    penX += bitstream.ReadS(numBits);
    penY += bitstream.ReadS(numBits);
  }
  edge.v1 = static_cast<std::uint16_t>(AddUnique(verts, idVec2{penX * 0.05f, penY * 0.05f}));
}

void idSWFShapeParser::AddUniqueVert(idSWFShapeDrawFill& drawFill, idVec2& start,
                                     idVec2& end) {
  int index = 0;
  if (morph) {
    for (; index < drawFill.startVerts.Num(); ++index) {
      if (SameVert(drawFill.startVerts[index], start) && SameVert(drawFill.endVerts[index], end)) {
        drawFill.indices.Append(static_cast<std::uint16_t>(index));
        return;
      }
    }
    index = drawFill.startVerts.Append(start);
    drawFill.endVerts.Append(end);
  } else {
    index = AddUnique(drawFill.startVerts, start);
  }
  drawFill.indices.Append(static_cast<std::uint16_t>(index));
}

int idSWFShapeParser::FindEarVert(swfSPLineLoop_t& loop) {
  assert(loop.vindex1.Num() == loop.vindex2.Num());
  const int num = loop.vindex1.Num();
  idList<earVert_t, 72> ears;
  ears.SetNum(num);

  for (int i1 = 0; i1 < num; ++i1) {
    const int i2 = (i1 + 1) % num;
    const int i3 = (i1 + 2) % num;
    const idVec2 a = verts[loop.vindex1[i1]] - verts[loop.vindex1[i2]];
    const idVec2 b = verts[loop.vindex1[i2]] - verts[loop.vindex1[i3]];
    ears[i1] = {i1, i2, i3, a.x * b.y - a.y * b.x};
  }
  std::sort(ears.list, ears.list + ears.Num(),
            [](const earVert_t& lhs, const earVert_t& rhs) { return lhs.cross < rhs.cross; });

  for (int i = 0; i < ears.Num(); ++i) {
    if (ears[i].cross < 0.0f) {
      continue;
    }
    const int i1 = ears[i].i1;
    const int i2 = ears[i].i2;
    const int i3 = ears[i].i3;
    const idVec2& v1s = verts[loop.vindex1[i1]];
    const idVec2& v2s = verts[loop.vindex1[i2]];
    const idVec2& v3s = verts[loop.vindex1[i3]];
    const idVec2& v1e = verts[loop.vindex2[i1]];
    const idVec2& v2e = verts[loop.vindex2[i2]];
    const idVec2& v3e = verts[loop.vindex2[i3]];

    idMat3 edgeEquations1;
    edgeEquations1[0] = {v1s.x, v1s.y, 1.0f};
    edgeEquations1[1] = {v2s.x, v2s.y, 1.0f};
    edgeEquations1[2] = {v3s.x, v3s.y, 1.0f};
    idMat3 edgeEquations2;
    edgeEquations2[0] = {v1e.x, v1e.y, 1.0f};
    edgeEquations2[1] = {v2e.x, v2e.y, 1.0f};
    edgeEquations2[2] = {v3e.x, v3e.y, 1.0f};
    edgeEquations1.InverseSelf();
    edgeEquations2.InverseSelf();

    bool isEar = true;
    for (int j = 0; j < num; ++j) {
      if (j == i1 || j == i2 || j == i3) {
        continue;
      }
      const idVec2& start = verts[loop.vindex1[j]];
      const idVec3 signs1 = MultiplyRowVector({start.x, start.y, 1.0f}, edgeEquations1);
      if ((signs1.x > 0.0f) == (signs1.y > 0.0f) &&
          (signs1.x > 0.0f) == (signs1.z > 0.0f)) {
        isEar = false;
        break;
      }
      const idVec2& end = verts[loop.vindex2[j]];
      const idVec3 signs2 = MultiplyRowVector({end.x, end.y, 1.0f}, edgeEquations2);
      if ((signs2.x > 0.0f) == (signs2.y > 0.0f) &&
          (signs2.x > 0.0f) == (signs2.z > 0.0f)) {
        isEar = false;
        break;
      }
    }
    if (isEar) {
      return i1;
    }
  }
  return -1;
}

void idSWFShapeParser::MakeLoops() {
  for (int i = 0; i < fillDraws.Num(); ++i) {
    swfSPDrawFill_t& fill = fillDraws[i];
    for (int e = 0; e < fill.edges.Num(); ++e) {
      if (fill.edges[e].start.v0 == fill.edges[e].start.v1 ||
          fill.edges[e].end.v0 == fill.edges[e].end.v1) {
        RemoveIndexFast(fill.edges, e--);
      }
    }

    idList<int, 72> unusedEdges;
    unusedEdges.SetNum(fill.edges.Num());
    for (int e = 0; e < fill.edges.Num(); ++e) {
      unusedEdges[e] = e;
    }

    while (unusedEdges.Num() > 0) {
      swfSPLineLoop_t& loop = *fill.loops.Alloc();
      loop.hole = false;
      int e1 = unusedEdges[unusedEdges.Num() - 1];
      unusedEdges.SetNum(unusedEdges.Num() - 1);

      for (;;) {
        const swfSPMorphEdge_t& current = fill.edges[e1];
        loop.vindex1.Append(current.start.v0);
        loop.vindex2.Append(current.end.v0);
        if (current.start.cp != 0xffff) {
          const float length1 = (verts[current.start.v0] - verts[current.start.v1]).Length();
          const float length2 = (verts[current.end.v0] - verts[current.end.v1]).Length();
          const int numPoints = 1 + static_cast<int>(std::max(length1, length2) / 10.0f);
          for (int ti = 0; ti < numPoints; ++ti) {
            const float t0 = (ti + 1) / (static_cast<float>(numPoints) + 1.0f);
            const float t1 = 1.0f - t0;
            const float c1 = t1 * t1;
            const float c2 = t0 * t1 * 2.0f;
            const float c3 = t0 * t0;
            const idVec2 p1 = verts[current.start.v0] * c1 + verts[current.start.cp] * c2 +
                              verts[current.start.v1] * c3;
            const idVec2 p2 = verts[current.end.v0] * c1 + verts[current.end.cp] * c2 +
                              verts[current.end.v1] * c3;
            loop.vindex1.Append(static_cast<std::uint16_t>(AddUnique(verts, p1)));
            loop.vindex2.Append(static_cast<std::uint16_t>(AddUnique(verts, p2)));
          }
        }

        const swfSPEdge_t& edge1 = current.start;
        float bestNormal = FLT_MAX;
        int best = -1;
        for (int e = 0; e < unusedEdges.Num(); ++e) {
          const swfSPEdge_t& edge2 = fill.edges[unusedEdges[e]].start;
          if (edge1.v1 != edge2.v0) {
            continue;
          }
          const idVec2 a = verts[edge1.v0] - verts[edge1.v1];
          const idVec2 b = verts[edge2.v1] - verts[edge1.v1];
          const float normal = a.x * b.y - a.y * b.x;
          if (normal < bestNormal) {
            bestNormal = normal;
            best = e;
          }
        }
        if (best < 0) {
          break;
        }
        e1 = unusedEdges[best];
        RemoveIndexFast(unusedEdges, best);
      }

      if (loop.vindex1.Num() < 3) {
        idLib::Warning("idSWFShapeParser: loop with < 3 verts");
        fill.loops.SetNum(fill.loops.Num() - 1);
        continue;
      }
      float leftMostX = FLT_MAX;
      int leftMostIndex = 0;
      for (int j = 0; j < loop.vindex1.Num(); ++j) {
        if (verts[loop.vindex1[j]].x < leftMostX) {
          leftMostIndex = j;
          leftMostX = verts[loop.vindex1[j]].x;
        }
      }
      const int count = loop.vindex1.Num();
      const idVec2 a = verts[loop.vindex1[(count + leftMostIndex - 1) % count]] -
                       verts[loop.vindex1[leftMostIndex]];
      const idVec2 b = verts[loop.vindex1[(leftMostIndex + 1) % count]] -
                       verts[loop.vindex1[leftMostIndex]];
      loop.hole = a.x * b.y - a.y * b.x > 0.0f;
    }

    for (;;) {
      int hole = -1;
      int holeVert = -1;
      float rightMostX = -1.0e10f;
      for (int j = 0; j < fill.loops.Num(); ++j) {
        swfSPLineLoop_t& loop = fill.loops[j];
        if (!loop.hole) {
          continue;
        }
        for (int v = 0; v < loop.vindex1.Num(); ++v) {
          if (verts[loop.vindex1[v]].x > rightMostX) {
            hole = j;
            holeVert = v;
            rightMostX = verts[loop.vindex1[v]].x;
          }
        }
      }
      if (hole == -1) {
        break;
      }

      swfSPLineLoop_t& loopHole = fill.loops[hole];
      const idVec2 holePoint = verts[loopHole.vindex1[holeVert]];
      int shapeIndex = -1;
      for (int j = 0; j < fill.loops.Num(); ++j) {
        swfSPLineLoop_t& candidate = fill.loops[j];
        if (candidate.hole) {
          continue;
        }
        bool inside = false;
        for (int k = 0; k < candidate.vindex1.Num(); ++k) {
          const idVec2& v1 = verts[candidate.vindex1[k]];
          const idVec2& v2 = verts[candidate.vindex1[(k + 1) % candidate.vindex1.Num()]];
          if ((v1.x < holePoint.x && v2.x < holePoint.x) ||
              ((v1.y < holePoint.y) == (v2.y < holePoint.y))) {
            continue;
          }
          inside = !inside;
        }
        if (inside) {
          shapeIndex = j;
          break;
        }
      }
      if (shapeIndex == -1) {
        idLib::Warning("idSWFShapeParser: Hole not in a shape");
        RemoveIndexFast(fill.loops, hole);
        continue;
      }

      swfSPLineLoop_t& loopShape = fill.loops[shapeIndex];
      float bestDist = 1.0e10f;
      int shapeVert = -1;
      for (int j = 0; j < loopShape.vindex1.Num(); ++j) {
        const idVec2& point = verts[loopShape.vindex1[j]];
        if (point.x < holePoint.x) {
          continue;
        }
        const float distance = (point - holePoint).Length();
        if (distance < bestDist) {
          shapeVert = j;
          bestDist = distance;
        }
      }

      idList<std::uint16_t, 72> merged;
      merged.SetNum(loopShape.vindex1.Num() + loopHole.vindex1.Num() + 1);
      merged.SetNum(0);
      for (int j = 0; j <= shapeVert; ++j) merged.Append(loopShape.vindex1[j]);
      for (int j = holeVert; j < loopHole.vindex1.Num(); ++j) merged.Append(loopHole.vindex1[j]);
      for (int j = 0; j <= holeVert; ++j) merged.Append(loopHole.vindex1[j]);
      for (int j = shapeVert; j < loopShape.vindex1.Num(); ++j) merged.Append(loopShape.vindex1[j]);
      loopShape.vindex1 = merged;

      merged.Clear();
      for (int j = 0; j <= shapeVert; ++j) merged.Append(loopShape.vindex2[j]);
      for (int j = holeVert; j < loopHole.vindex2.Num(); ++j) merged.Append(loopHole.vindex2[j]);
      for (int j = 0; j <= holeVert; ++j) merged.Append(loopHole.vindex2[j]);
      for (int j = shapeVert; j < loopShape.vindex2.Num(); ++j) merged.Append(loopShape.vindex2[j]);
      loopShape.vindex2 = merged;
      RemoveIndexFast(fill.loops, hole);
    }
  }
}

void idSWFShapeParser::TriangulateSoup(idSWFShape& shape) {
  MakeLoops();
  shape.fillDraws.SetNum(fillDraws.Num());
  for (int i = 0; i < fillDraws.Num(); ++i) {
    swfSPDrawFill_t& source = fillDraws[i];
    idSWFShapeDrawFill& output = shape.fillDraws[i];
    output.style = source.style;
    for (int j = 0; j < source.loops.Num(); ++j) {
      swfSPLineLoop_t& loop = source.loops[j];
      const int numVerts = loop.vindex1.Num();
      for (int k = 0; k < numVerts - 2; ++k) {
        const int v1 = FindEarVert(loop);
        if (v1 == -1) {
          idLib::Warning("idSWFShapeParser: could not find an ear vert");
          break;
        }
        const int num = loop.vindex1.Num();
        const int v2 = (v1 + 1) % num;
        const int v3 = (v1 + 2) % num;
        AddUniqueVert(output, verts[loop.vindex1[v1]], verts[loop.vindex2[v1]]);
        AddUniqueVert(output, verts[loop.vindex1[v2]], verts[loop.vindex2[v2]]);
        AddUniqueVert(output, verts[loop.vindex1[v3]], verts[loop.vindex2[v3]]);
        loop.vindex1.RemoveIndex(v2);
        loop.vindex2.RemoveIndex(v2);
      }
    }
  }
}

void idSWFShapeParser::ReadFillStyle(idSWFBitStream& bitstream) {
  std::uint16_t fillStyleCount = bitstream.ReadU8();
  if (extendedCount && fillStyleCount == 0xff) {
    fillStyleCount = bitstream.ReadU16();
  }
  fillDraws.EnsureCapacity(fillDraws.Num() + fillStyleCount);
  for (int i = 0; i < fillStyleCount; ++i) {
    const std::uint8_t fillStyleType = bitstream.ReadU8();
    swfFillStyle_t& style = fillDraws.Alloc()->style;
    style.type = fillStyleType >> 4;
    style.subType = fillStyleType & 0xf;
    if (style.type == 0) {
      if (morph) {
        bitstream.ReadColorRGBA(style.startColor);
        bitstream.ReadColorRGBA(style.endColor);
      } else {
        if (rgba) bitstream.ReadColorRGBA(style.startColor);
        else bitstream.ReadColorRGB(style.startColor);
        style.endColor = style.startColor;
      }
    } else if (style.type == 1) {
      bitstream.ReadMatrix(style.startMatrix);
      if (morph) {
        bitstream.ReadMatrix(style.endMatrix);
        bitstream.ReadMorphGradient(style.gradient);
      } else {
        style.endMatrix = style.startMatrix;
        bitstream.ReadGradient(style.gradient, rgba);
      }
      if (style.subType == 3) {
        style.focalPoint = bitstream.ReadFixed8();
      }
    } else if (style.type == 4) {
      style.bitmapID = bitstream.ReadU16();
      bitstream.ReadMatrix(style.startMatrix);
      if (morph) bitstream.ReadMatrix(style.endMatrix);
      else style.endMatrix = style.startMatrix;
    }
  }

  std::uint16_t lineStyleCount = bitstream.ReadU8();
  if (extendedCount && lineStyleCount == 0xff) {
    lineStyleCount = bitstream.ReadU16();
  }
  lineDraws.EnsureCapacity(lineDraws.Num() + lineStyleCount);
  for (int i = 0; i < lineStyleCount; ++i) {
    swfLineStyle_t& style = lineDraws.Alloc()->style;
    style.startWidth = bitstream.ReadU16();
    if (lineStyle2) {
      style.endWidth = style.startWidth;
      bitstream.ReadU(2);
      const std::uint8_t joinStyle = static_cast<std::uint8_t>(bitstream.ReadU(2));
      const bool hasFill = bitstream.ReadBool();
      bitstream.ReadBool();
      bitstream.ReadBool();
      bitstream.ReadBool();
      bitstream.ReadU(5);
      bitstream.ReadBool();
      bitstream.ReadU(2);
      if (joinStyle == 2) bitstream.ReadU16();
      if (hasFill) {
        idLib::Warning("idSWFShapeParser: Ignoring hasFillFlag");
      } else {
        bitstream.ReadColorRGBA(style.startColor);
        style.endColor = style.startColor;
      }
    } else {
      style.endWidth = morph ? bitstream.ReadU16() : style.startWidth;
      if (rgba) bitstream.ReadColorRGBA(style.startColor);
      else bitstream.ReadColorRGB(style.startColor);
      if (morph) bitstream.ReadColorRGBA(style.endColor);
      else style.endColor = style.startColor;
    }
  }
}

void idSWFShapeParser::ParseShapes(idSWFBitStream& bitstream1, idSWFBitStream* bitstream2,
                                   bool swap) {
  int pen1X = 0, pen1Y = 0, pen2X = 0, pen2Y = 0;
  std::uint8_t fillStyle0 = 0, fillStyle1 = 0, lineStyle = 0;
  std::uint16_t baseFillStyle = 0, baseLineStyle = 0;
  std::uint8_t numBits = bitstream1.ReadU8();
  std::uint8_t numFillBits1 = numBits >> 4;
  std::uint8_t numLineBits1 = numBits & 0xf;
  std::uint8_t numFillBits2 = 0, numLineBits2 = 0;
  if (bitstream2 != nullptr) {
    numBits = bitstream2->ReadU8();
    numFillBits2 = numBits >> 4;
    numLineBits2 = numBits & 0xf;
  }

  for (;;) {
    if (!bitstream1.ReadBool()) {
      const bool stateNewStyles = bitstream1.ReadBool();
      const bool stateLineStyle = bitstream1.ReadBool();
      const bool stateFillStyle1 = bitstream1.ReadBool();
      const bool stateFillStyle0 = bitstream1.ReadBool();
      const bool stateMoveTo = bitstream1.ReadBool();
      if (!(stateNewStyles || stateLineStyle || stateFillStyle1 || stateFillStyle0 || stateMoveTo)) {
        if (bitstream2 != nullptr && bitstream2->ReadU(6) != 0) {
          idLib::Warning("idSWFShapeParser: morph stream 1 ends before 2");
        }
        break;
      }
      if (stateMoveTo) {
        const std::uint8_t moveBits = static_cast<std::uint8_t>(bitstream1.ReadU(5));
        pen1X = bitstream1.ReadS(moveBits);
        pen1Y = bitstream1.ReadS(moveBits);
      }
      if (stateFillStyle0) fillStyle0 = static_cast<std::uint8_t>(bitstream1.ReadU(numFillBits1));
      if (stateFillStyle1) fillStyle1 = static_cast<std::uint8_t>(bitstream1.ReadU(numFillBits1));
      if (stateLineStyle) lineStyle = static_cast<std::uint8_t>(bitstream1.ReadU(numLineBits1));
      if (stateNewStyles) {
        baseFillStyle = static_cast<std::uint16_t>(fillDraws.Num());
        baseLineStyle = static_cast<std::uint16_t>(lineDraws.Num());
        ReadFillStyle(bitstream1);
        numBits = bitstream1.ReadU8();
        numFillBits1 = numBits >> 4;
        numLineBits1 = numBits & 0xf;
      }
      if (bitstream2 != nullptr) {
        if (bitstream2->ReadBool()) {
          idLib::Warning("idSWFShapeParser: morph stream 1 defines style change, but stream 2 does not");
          break;
        }
        const bool stateNewStyles2 = bitstream2->ReadBool();
        const bool stateLineStyle2 = bitstream2->ReadBool();
        const bool stateFillStyle12 = bitstream2->ReadBool();
        const bool stateFillStyle02 = bitstream2->ReadBool();
        const bool stateMoveTo2 = bitstream2->ReadBool();
        if (stateMoveTo2) {
          const std::uint8_t moveBits = static_cast<std::uint8_t>(bitstream2->ReadU(5));
          pen2X = bitstream2->ReadS(moveBits);
          pen2Y = bitstream2->ReadS(moveBits);
        }
        if (stateFillStyle02 && bitstream2->ReadU(numFillBits2) != fillStyle0) {
          idLib::Warning("idSWFShapeParser: morph stream 2 defined a different fillStyle0 from stream 1");
          break;
        }
        if (stateFillStyle12 && bitstream2->ReadU(numFillBits2) != fillStyle1) {
          idLib::Warning("idSWFShapeParser: morph stream 2 defined a different fillStyle1 from stream 1");
          break;
        }
        if (stateLineStyle2 && bitstream2->ReadU(numLineBits2) != lineStyle) {
          idLib::Warning("idSWFShapeParser: morph stream 2 defined a different lineStyle from stream 1");
          break;
        }
        if (stateNewStyles2) {
          idLib::Warning("idSWFShapeParser: morph stream 2 defines new styles");
          break;
        }
      }
    } else {
      swfSPMorphEdge_t edge{};
      ParseEdge(bitstream1, pen1X, pen1Y, edge.start);
      if (bitstream2 != nullptr) {
        if (!bitstream2->ReadBool()) {
          idLib::Warning("idSWFShapeParser: morph stream 1 defines an edge, but stream 2 does not");
          break;
        }
        ParseEdge(*bitstream2, pen2X, pen2Y, edge.end);
      } else {
        edge.end = edge.start;
      }
      if (edge.start.cp != 0xffff && edge.end.cp == 0xffff) {
        edge.end.cp = static_cast<std::uint16_t>(AddUnique(
            verts, (verts[edge.end.v0] + verts[edge.end.v1]) * 0.5f));
      } else if (edge.start.cp == 0xffff && edge.end.cp != 0xffff) {
        edge.start.cp = static_cast<std::uint16_t>(AddUnique(
            verts, (verts[edge.start.v0] + verts[edge.start.v1]) * 0.5f));
      }
      if (lineStyle != 0) lineDraws[baseLineStyle + lineStyle - 1].edges.Append(edge);
      if (swap) {
        std::swap(edge.start.v0, edge.start.v1);
        std::swap(edge.end.v0, edge.end.v1);
      }
      if (fillStyle1 != 0) fillDraws[baseFillStyle + fillStyle1 - 1].edges.Append(edge);
      if (fillStyle0 != 0) {
        swfSPMorphEdge_t reversed = edge;
        std::swap(reversed.start.v0, reversed.start.v1);
        std::swap(reversed.end.v0, reversed.end.v1);
        fillDraws[baseFillStyle + fillStyle0 - 1].edges.Append(reversed);
      }
    }
  }
}

void idSWFShapeParser::Parse(idSWFBitStream& bitstream, idSWFShape& shape, int recordType) {
  morph = false;
  extendedCount = recordType > 1;
  lineStyle2 = recordType == 4;
  rgba = recordType >= 3;
  bitstream.ReadRect(shape.startBounds);
  shape.endBounds = shape.startBounds;
  if (recordType == 4) {
    swfRect_t edgeBounds;
    bitstream.ReadRect(edgeBounds);
    bitstream.ReadU8();
  }
  ReadFillStyle(bitstream);
  ParseShapes(bitstream, nullptr, false);
  TriangulateSoup(shape);

  shape.lineDraws.SetNum(lineDraws.Num());
  for (int i = 0; i < lineDraws.Num(); ++i) {
    idSWFShapeDrawLine& output = shape.lineDraws[i];
    swfSPDrawLine_t& source = lineDraws[i];
    output.style = source.style;
    output.indices.SetNum(source.edges.Num() * 3);
    output.indices.SetNum(0);
    for (int e = 0; e < source.edges.Num(); ++e) {
      const swfSPMorphEdge_t& edge = source.edges[e];
      const int v0 = AddUnique(output.startVerts, verts[edge.start.v0]);
      output.indices.Append(static_cast<std::uint16_t>(v0));
      output.indices.Append(static_cast<std::uint16_t>(v0));
      if (edge.start.cp != 0xffff) {
        const float length1 = (verts[edge.start.v0] - verts[edge.start.v1]).Length();
        const float length2 = (verts[edge.end.v0] - verts[edge.end.v1]).Length();
        const int numPoints = 1 + static_cast<int>(std::max(length1, length2) / 10.0f);
        for (int ti = 0; ti < numPoints; ++ti) {
          const float t0 = (ti + 1) / (static_cast<float>(numPoints) + 1.0f);
          const float t1 = 1.0f - t0;
          const float c1 = t1 * t1;
          const float c2 = t0 * t1 * 2.0f;
          const float c3 = t0 * t0;
          const idVec2 point = verts[edge.start.v0] * c1 + verts[edge.start.cp] * c2 +
                               verts[edge.start.v1] * c3;
          const int index = AddUnique(output.startVerts, point);
          output.indices.Append(static_cast<std::uint16_t>(index));
          output.indices.Append(static_cast<std::uint16_t>(index));
          output.indices.Append(static_cast<std::uint16_t>(index));
        }
      }
      output.indices.Append(
          static_cast<std::uint16_t>(AddUnique(output.startVerts, verts[edge.start.v1])));
    }
  }
}

