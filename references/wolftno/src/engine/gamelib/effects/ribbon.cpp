#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/effects/ribbon.h"

#include "engine/decls/declsimple.h"
#include "engine/models/rendermodelbeam.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/color.h"
#include "shared/idlib/math/mathlib.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI

#include <cmath>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

idRandom2 idRibbon::random{};

namespace {

constexpr float kTwoPi = 6.2831855f;
constexpr float kInvTwoPi = 0.15915494f;
constexpr float kMillisecondsToSeconds = 0.001f;

float WrapAngle(float angle) {
  if (angle < 0.0f || angle >= kTwoPi) {
    angle = angle - floorf(angle * kInvTwoPi) * kTwoPi;
  }
  return angle;
}

float SqrtClamped(float value) {
  const __m128 clamped =
      _mm_max_ps(_mm_set_ss(value), idMath::vector_float_smallestNonDenorm);
  return _mm_cvtss_f32(_mm_sqrt_ps(clamped));
}

float MinOneSse(float value) {
  return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(1.0f), _mm_set_ss(value)));
}

int WrapSubtract(int left, int right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) -
                                   static_cast<std::uint32_t>(right));
}

int WrapAdd(int left, int right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

int TruncateFloat(float value) {
  return _mm_cvttss_si32(_mm_set_ss(value));
}

idVec3 TransformVector(const idVec3& vector, const idMat3& axis) {
  idVec3 result;
  result.x = (vector.y * axis.mat[1].x + vector.x * axis.mat[0].x) +
             vector.z * axis.mat[2].x;
  result.y = (vector.y * axis.mat[1].y + vector.x * axis.mat[0].y) +
             vector.z * axis.mat[2].y;
  result.z = (vector.y * axis.mat[1].z + vector.x * axis.mat[0].z) +
             vector.z * axis.mat[2].z;
  return result;
}

std::uint8_t ColorByte(float value) {
  const int converted = TruncateFloat(value * 255.0f);
  if (converted < 0) {
    return 0;
  }
  if (converted > 255) {
    return 255;
  }
  return static_cast<std::uint8_t>(converted);
}

std::uint8_t TangentByte(float value) {
  __m128 packed = _mm_set_ss(value);
  packed = _mm_add_ss(packed, _mm_set_ss(1.0f));
  packed = _mm_mul_ss(packed, _mm_set_ss(127.5f));
  packed = _mm_add_ss(packed, _mm_set_ss(0.5f));
  const int converted = _mm_cvtss_si32(packed);
  if (converted < 0) {
    return 0;
  }
  if (converted > 255) {
    return 255;
  }
  return static_cast<std::uint8_t>(converted);
}

void SetPackedTangent(beamNodeParms_t& node, int index, float x, float y,
                      float z) {
  node.tangent[index][0] = TangentByte(x);
  node.tangent[index][1] = TangentByte(y);
  node.tangent[index][2] = TangentByte(z);
}

bool HasTranslation(const idVec3& translate) {
  std::uint32_t x;
  std::uint32_t y;
  std::uint32_t z;
  std::memcpy(&x, &translate.x, sizeof(x));
  std::memcpy(&y, &translate.y, sizeof(y));
  std::memcpy(&z, &translate.z, sizeof(z));
  return ((x | y | z) & 0x7FFFFFFFu) != 0;
}

} // namespace

idRibbon::idRibbon(const idDeclRibbon* decl)
    : ribbonState(RIBBON_INACTIVE), spawnOrg(vec3_origin), spawnTime(0),
      numActiveNodes(0), currHeadIdx(-1), startVariance(0.0f), prevTime(-1),
      prevOrigin(vec3_origin), prevOrigin2(vec3_origin), ribbonDecl(decl),
      globalColor{ idColor::colorWhite.r, idColor::colorWhite.g,
                   idColor::colorWhite.b, idColor::colorWhite.a },
      ribbonNodes{}, explicitDefineBothEdges(false) {
  ResetNodes();
}

void idRibbon::ResetNodes() {
  ribbonState = RIBBON_INACTIVE;
  prevTime = -1;
  currHeadIdx = -1;
  spawnTime = 0;
  numActiveNodes = 0;
  explicitDefineBothEdges = false;
  for (ribbonNode_t& node : ribbonNodes) {
    node.Clear();
  }
}

void idRibbon::StartRibbon(int startTime, const idVec3& startOrigin) {
  ResetNodes();
  spawnTime = startTime;
  spawnOrg = startOrigin;
  ribbonState = RIBBON_ACTIVE;
  const std::uint32_t seed = random.seed * 1664525u + 1013904223u;
  random.seed = seed;
  startVariance =
      static_cast<float>((seed >> 10) & 0x7FFFu) * 0.000030518509f;
}

void idRibbon::RemoveOldNodes(int currentTime) {
  const int originalCount = numActiveNodes;
  const idDeclRibbon* const decl = ribbonDecl;
  if (originalCount == 0 || decl == nullptr) {
    return;
  }

  if (decl->maxLength != 0.0f) {
    float accumulatedLength = 0.0f;
    const int tail = currHeadIdx - originalCount + 1;
    int ring = currHeadIdx - originalCount + 129;
    for (int index = tail; index <= currHeadIdx; ++index, ++ring) {
      accumulatedLength += ribbonNodes[ring % MAX_NUM_RIBBON_NODES].nodeLength;
      if (accumulatedLength > decl->maxLength) {
        --numActiveNodes;
      }
    }
  }

  if (decl->duration <= 0) {
    if (ribbonState == RIBBON_DEACTIVATING) {
      --numActiveNodes;
    }
  } else {
    const int tail = currHeadIdx - numActiveNodes + 1;
    int ring = currHeadIdx - numActiveNodes + 129;
    for (int index = tail; index <= currHeadIdx; ++index, ++ring) {
      const int expiration =
          WrapAdd(decl->duration,
                  ribbonNodes[ring % MAX_NUM_RIBBON_NODES].spawnTime);
      if (expiration < currentTime) {
        --numActiveNodes;
      }
    }
  }

  if (numActiveNodes <= 0) {
    numActiveNodes = 0;
    explicitDefineBothEdges = false;
    ribbonState = RIBBON_INACTIVE;
  }
}

int idRibbon::GetRemainingTime(int currentTime) const {
  const idDeclRibbon* const decl = ribbonDecl;
  if (decl == nullptr || decl->duration <= 0) {
    return 0;
  }

  int remaining = 0;
  if (numActiveNodes > 0) {
    int ring = currHeadIdx - numActiveNodes + 129;
    unsigned int count = static_cast<unsigned int>(numActiveNodes);
    do {
      const int expiration =
          WrapAdd(decl->duration,
                  ribbonNodes[ring % MAX_NUM_RIBBON_NODES].spawnTime);
      const int candidate = WrapSubtract(expiration, currentTime);
      if (candidate > remaining) {
        remaining = candidate;
      }
      ++ring;
    } while (--count != 0);
  }
  return remaining;
}

void idRibbon::CalcTurbulence(const idVec3& origin, const idMat3& axis,
                              idVec3& turbulence) const {
  turbulence = {0.0f, 0.0f, 0.0f};
  const idDeclRibbon* const decl = ribbonDecl;
  if (decl == nullptr ||
      !(decl->turbulence.magnitude > idMath::FLOAT_EPSILON)) {
    return;
  }

  const float frequency = decl->turbulence.frequency;
  const float magnitude = decl->turbulence.magnitude;
  const idVec4& magic = decl->turbulence.magic;

  const float xWave =
      sinf(WrapAngle((frequency * origin.x) * magic.x)) -
      cosf(WrapAngle((frequency * origin.y) * magic.w));
  const float yWave =
      sinf(WrapAngle((frequency * origin.y) * magic.y)) -
      cosf(WrapAngle((frequency * origin.x) * magic.w));
  const float frequencyZ = frequency * origin.z;
  const float zWave = cosf(WrapAngle(frequencyZ * magic.z)) -
                      sinf(WrapAngle(frequencyZ * magic.w));

  const float localX = xWave * magnitude;
  const float localY = yWave * magnitude;
  const float localZ = zWave * magnitude;
  turbulence.x =
      (localY * axis.mat[1].x + localX * axis.mat[0].x) +
      localZ * axis.mat[2].x;
  turbulence.y =
      (localY * axis.mat[1].y + localX * axis.mat[0].y) +
      localZ * axis.mat[2].y;
  turbulence.z =
      (localY * axis.mat[1].z + localX * axis.mat[0].z) +
      localZ * axis.mat[2].z;
}

void idRibbon::CalcHelix(int currentTime, const idVec3& origin,
                         const idMat3& axis, const idVec3& velocity,
                         idVec3& helix) const {
  static_cast<void>(velocity);
  helix = {0.0f, 0.0f, 0.0f};
  const idDeclRibbon* const decl = ribbonDecl;
  if (decl == nullptr || !(decl->helix.radius > idMath::FLOAT_EPSILON)) {
    return;
  }

  const float radius = decl->helix.radius;
  const float rotation =
      static_cast<float>(currentTime) * decl->helix.rotationScale +
      static_cast<float>(spawnTime);
  const float rotationSin = sinf(rotation);
  const float rotationCos = cosf(rotation);

  const float deltaX = origin.x - spawnOrg.x;
  const float deltaY = origin.y - spawnOrg.y;
  const float deltaZ = origin.z - spawnOrg.z;
  const float distanceSquared =
      (deltaY * deltaY + deltaX * deltaX) + deltaZ * deltaZ;
  const float distanceScale =
      MinOneSse(SqrtClamped(distanceSquared) * 0.0049999999f);

  const float distortionX = WrapAngle(
      (spawnOrg.x + origin.x) * decl->helix.distortion0.x);
  const float distortionY = WrapAngle(
      (spawnOrg.y + origin.y) * decl->helix.distortion0.y);
  const float axisOne =
      (cosf(distortionX) * decl->helix.distortion1.x +
       rotationSin * radius) *
      distanceScale;
  const float axisTwo =
      (sinf(distortionY) * decl->helix.distortion1.y +
       rotationCos * radius) *
      distanceScale;

  helix.x = (axisOne * axis.mat[1].x + axis.mat[0].x * 0.0f) +
            axisTwo * axis.mat[2].x;
  helix.y = (axisOne * axis.mat[1].y + axis.mat[0].y * 0.0f) +
            axisTwo * axis.mat[2].y;
  helix.z = (axisOne * axis.mat[1].z + axis.mat[0].z * 0.0f) +
            axisTwo * axis.mat[2].z;
}

void idRibbon::AddNode(int currentTime, const idVec3& origin,
                       const idVec3& origin2, const idMat3& axis,
                       const idVec3& inputVelocity, const idVec4& color) {
  const idDeclRibbon* decl = ribbonDecl;
  if (decl == nullptr) {
    return;
  }

  explicitDefineBothEdges = &origin != &origin2;
  idVec3 turbulence{0.0f, 0.0f, 0.0f};
  idVec3 helix{0.0f, 0.0f, 0.0f};
  idVec3 turbulence2{0.0f, 0.0f, 0.0f};
  idVec3 helix2{0.0f, 0.0f, 0.0f};
  CalcTurbulence(origin, axis, turbulence);
  CalcHelix(currentTime, origin, axis, inputVelocity, helix);
  if (explicitDefineBothEdges) {
    CalcTurbulence(origin2, axis, turbulence2);
    CalcHelix(currentTime, origin2, axis, inputVelocity, helix2);
  }

  currHeadIdx = (currHeadIdx + 1) % MAX_NUM_RIBBON_NODES;
  ribbonNode_t& node = ribbonNodes[currHeadIdx];
  node.spawnTime = currentTime;
  node.origin.x = (turbulence.x + origin.x) + helix.x;
  node.origin.y = (turbulence.y + origin.y) + helix.y;
  node.origin.z = (turbulence.z + origin.z) + helix.z;
  if (explicitDefineBothEdges) {
    node.origin2.x = (turbulence2.x + origin2.x) + helix2.x;
    node.origin2.y = (turbulence2.y + origin2.y) + helix2.y;
    node.origin2.z = (turbulence2.z + origin2.z) + helix2.z;
  }

  decl = ribbonDecl;
  const idVec3 base = TransformVector(decl->velocity, axis);
  if (explicitDefineBothEdges) {
    node.velocity.x =
        ((base.x + inputVelocity.x) +
         ((turbulence2.x + turbulence.x) * decl->turbulence.velocity) *
             0.5f) +
        (helix2.x + helix.x) * decl->helix.velocity;
    node.velocity.y =
        ((base.y + inputVelocity.y) +
         ((turbulence2.y + turbulence.y) * decl->turbulence.velocity) *
             0.5f) +
        (helix2.y + helix.y) * decl->helix.velocity;
    node.velocity.z =
        ((base.z + inputVelocity.z) +
         ((turbulence2.z + turbulence.z) * decl->turbulence.velocity) *
             0.5f) +
        (helix2.z + helix.z) * decl->helix.velocity;
  } else {
    node.velocity.x =
        ((base.x + inputVelocity.x) +
         turbulence.x * decl->turbulence.velocity) +
        helix.x * decl->helix.velocity;
    node.velocity.y =
        ((base.y + inputVelocity.y) +
         turbulence.y * decl->turbulence.velocity) +
        helix.y * decl->helix.velocity;
    node.velocity.z =
        ((base.z + inputVelocity.z) +
         turbulence.z * decl->turbulence.velocity) +
        helix.z * decl->helix.velocity;
  }

  node.color.x = decl->color.x * color.x;
  node.color.y = decl->color.y * color.y;
  node.color.z = decl->color.z * color.z;
  node.color.w = decl->color.w * color.w;
  node.orientVec = axis.mat[0];

  if (decl->maxLength != 0.0f) {
    if (numActiveNodes != 0) {
      const int previous = (currHeadIdx + 127) % MAX_NUM_RIBBON_NODES;
      const float deltaZ = origin.z - ribbonNodes[previous].origin.z;
      const float deltaX = origin.x - ribbonNodes[previous].origin.x;
      const float deltaY = origin.y - ribbonNodes[previous].origin.y;
      const float lengthSquared =
          (deltaY * deltaY + deltaX * deltaX) + deltaZ * deltaZ;
      node.nodeLength =
          (1.0f / SqrtClamped(lengthSquared)) * lengthSquared;
    } else {
      node.nodeLength = 0.0f;
    }
  }

  if (numActiveNodes < MAX_NUM_RIBBON_NODES) {
    ++numActiveNodes;
  }
}

void idRibbon::UpdatePosition(int currentTime, const idVec3& origin,
                              const idVec3& origin2, const idMat3& axis,
                              const idVec3& velocity, const idVec4& color) {
  const idDeclRibbon* const decl = ribbonDecl;
  if (decl == nullptr || ribbonState != RIBBON_ACTIVE) {
    return;
  }

  const int oldTime = prevTime;
  float distanceSquared = 0.0f;
  if (oldTime >= 0) {
    const float deltaY = prevOrigin.y - origin.y;
    const float deltaX = prevOrigin.x - origin.x;
    const float deltaZ = prevOrigin.z - origin.z;
    distanceSquared =
        (deltaY * deltaY + deltaX * deltaX) + deltaZ * deltaZ;
    const float minimumDistance = decl->minNodeDistance;
    if (minimumDistance > 0.0f &&
        minimumDistance * minimumDistance >= distanceSquared) {
      return;
    }
    if (distanceSquared >= 1000000.0f) {
      const idVec4 breakColor{0.0f, 0.0f, 0.0f, 1.0f};
      AddNode(oldTime, prevOrigin, prevOrigin, axis, vec3_origin,
              breakColor);
      AddNode(currentTime, origin, origin, axis, vec3_origin, breakColor);
      prevTime = currentTime;
      prevOrigin = origin;
      return;
    }
  }

  const bool definesBothEdges = &origin != &origin2;
  const float subdivision = decl->nodeSubdivision;
  int subdivisions = 1;
  if (subdivision > 0.0f && oldTime >= 0) {
    subdivisions =
        TruncateFloat(SqrtClamped(distanceSquared) / subdivision) + 1;
    if (subdivisions > MAX_NUM_RIBBON_NODES) {
      subdivisions = MAX_NUM_RIBBON_NODES;
    }
  }

  if (subdivisions <= 1) {
    AddNode(currentTime, origin, definesBothEdges ? origin2 : origin, axis,
            velocity, color);
  } else {
    const float inverseCount = 1.0f / static_cast<float>(subdivisions);
    const float currentTimeFloat = static_cast<float>(currentTime);
    for (int index = 0; index < subdivisions; ++index) {
      const float fraction =
          (static_cast<float>(index) + 1.0f) * inverseCount;
      idVec3 first;
      first.x = ((origin.x - prevOrigin.x) * fraction) + prevOrigin.x;
      first.y = ((origin.y - prevOrigin.y) * fraction) + prevOrigin.y;
      first.z = ((origin.z - prevOrigin.z) * fraction) + prevOrigin.z;
      const int interpolatedTime = TruncateFloat(
          ((currentTimeFloat - static_cast<float>(oldTime)) * fraction) +
          static_cast<float>(oldTime));
      if (definesBothEdges) {
        idVec3 second;
        second.x =
            ((origin2.x - prevOrigin2.x) * fraction) + prevOrigin2.x;
        second.y =
            ((origin2.y - prevOrigin2.y) * fraction) + prevOrigin2.y;
        second.z =
            ((origin2.z - prevOrigin2.z) * fraction) + prevOrigin2.z;
        AddNode(interpolatedTime, first, second, axis, velocity, color);
      } else {
        AddNode(interpolatedTime, first, first, axis, velocity, color);
      }
    }
  }

  prevTime = currentTime;
  prevOrigin = origin;
  if (definesBothEdges) {
    prevOrigin2 = origin2;
  }
}

void idRibbon::UpdatePosition(int currentTime, const idVec3& origin,
                              const idMat3& axis, const idVec3& velocity,
                              const idVec4& color) {
  UpdatePosition(currentTime, origin, origin, axis, velocity, color);
}

void idRibbon::UpdateGeometry(int currentTime, idRenderModelBeam* beamModel,
                              const idMat3& axis,
                              const idVec3& translate) {
  const idDeclRibbon* decl = ribbonDecl;
  if (numActiveNodes < 2 || decl == nullptr) {
    return;
  }

  const int duration = decl->duration;
  const float inverseDuration =
      1.0f / (static_cast<float>(duration) * kMillisecondsToSeconds);
  const float inverseFadeIn = 1.0f / decl->fadeInFraction;
  const float inverseFadeOut = 1.0f / decl->fadeOutFraction;
  beam_t* const beam = beamModel->DrawBeamAlloc();
  if (beam == nullptr) {
    return;
  }

  float accumulatedLength = 0.0f;
  float previousLengthFraction = 0.0f;
  float currentLengthFraction = 0.0f;
  float inverseTotalLength = 0.0f;
  if (HasTranslation(translate) || decl->maxLength != 0.0f) {
    for (int segment = 0; segment < numActiveNodes - 1; ++segment) {
      const int nodeIndex =
          (currHeadIdx - segment + MAX_NUM_RIBBON_NODES) %
          MAX_NUM_RIBBON_NODES;
      ribbonNode_t& node = ribbonNodes[nodeIndex];
      accumulatedLength += node.nodeLength;
      node.origin.x = translate.x + node.origin.x;
      node.origin.y = node.origin.y + translate.y;
      node.origin.z = translate.z + node.origin.z;
    }
    inverseTotalLength = 1.0f / accumulatedLength;
    accumulatedLength = 0.0f;
  }

  beam->nodes.ClearFree();
  const float inverseSegmentCount =
      1.0f / static_cast<float>(numActiveNodes - 1);
  for (int segment = 0; segment < numActiveNodes - 1; ++segment) {
    const int startIndex =
        (currHeadIdx - segment + MAX_NUM_RIBBON_NODES) %
        MAX_NUM_RIBBON_NODES;
    const int endIndex =
        (startIndex + MAX_NUM_RIBBON_NODES - 1) %
        MAX_NUM_RIBBON_NODES;
    const ribbonNode_t& startNode = ribbonNodes[startIndex];
    const ribbonNode_t& endNode = ribbonNodes[endIndex];
    const float startAge =
        static_cast<float>(WrapSubtract(currentTime, startNode.spawnTime)) *
        kMillisecondsToSeconds;
    const float endAge =
        static_cast<float>(WrapSubtract(currentTime, endNode.spawnTime)) *
        kMillisecondsToSeconds;

    float lifeFraction = 0.5f;
    if (duration > 0) {
      lifeFraction = startAge * inverseDuration;
      if (lifeFraction < 0.0f) {
        lifeFraction = 0.0f;
      } else if (lifeFraction > 1.0f) {
        lifeFraction = 1.0f;
      }
    }

    beamNodeParms_t& node = beam->nodes.Alloc();
    decl = ribbonDecl;
    node.startPos.x =
        ((startNode.velocity.x - decl->gravity.x) * startAge) +
        startNode.origin.x;
    node.startPos.y =
        ((startNode.velocity.y - decl->gravity.y) * startAge) +
        startNode.origin.y;
    node.startPos.z =
        ((startNode.velocity.z - decl->gravity.z) * startAge) +
        startNode.origin.z;

    decl = ribbonDecl;
    node.endPos.x = ((endNode.velocity.x - decl->gravity.x) * endAge) +
                    endNode.origin.x;
    node.endPos.y = ((endNode.velocity.y - decl->gravity.y) * endAge) +
                    endNode.origin.y;
    node.endPos.z = ((endNode.velocity.z - decl->gravity.z) * endAge) +
                    endNode.origin.z;

    node.halfWidth =
        ((ribbonDecl->endWidth - ribbonDecl->startWidth) * lifeFraction) *
            0.5f +
        ribbonDecl->startWidth * 0.5f;

    if (explicitDefineBothEdges) {
      decl = ribbonDecl;
      node.startPos2.x =
          ((startNode.velocity.x - decl->gravity.x) * startAge) +
          startNode.origin2.x;
      node.startPos2.y =
          ((startNode.velocity.y - decl->gravity.y) * startAge) +
          startNode.origin2.y;
      node.startPos2.z =
          ((startNode.velocity.z - decl->gravity.z) * startAge) +
          startNode.origin2.z;

      decl = ribbonDecl;
      node.endPos2.x =
          ((endNode.velocity.x - decl->gravity.x) * endAge) +
          endNode.origin2.x;
      node.endPos2.y =
          ((endNode.velocity.y - decl->gravity.y) * endAge) +
          endNode.origin2.y;
      node.endPos2.z =
          ((endNode.velocity.z - decl->gravity.z) * endAge) +
          endNode.origin2.z;
    }

    decl = ribbonDecl;
    const float red = startNode.color.x * globalColor.x;
    const float green = startNode.color.y * globalColor.y;
    const float blue = startNode.color.z * globalColor.z;
    float alpha = startNode.color.w * globalColor.w;
    if (decl->fadeInFraction > 0.0f &&
        decl->fadeInFraction > lifeFraction) {
      alpha = lifeFraction * inverseFadeIn;
    } else if (decl->fadeOutFraction > 0.0f &&
               decl->fadeOutFraction > (1.0f - lifeFraction)) {
      alpha = (1.0f - lifeFraction) * inverseFadeOut;
    }
    node.color[0][0] = ColorByte(red);
    node.color[0][1] = ColorByte(green);
    node.color[0][2] = ColorByte(blue);
    node.color[0][3] = ColorByte(alpha);
    node.color[1][0] = node.color[0][0];
    node.color[1][1] = node.color[0][1];
    node.color[1][2] = node.color[0][2];
    node.color[1][3] = node.color[0][3];

    node.orientVec =
        decl->reorientNodes ? axis.mat[0] : startNode.orientVec;
    for (int vertex = 0; vertex < 4; ++vertex) {
      SetPackedTangent(node, vertex, vec3_origin.x, vec3_origin.y,
                       vec3_origin.z);
    }

    idVec2 sMinMax = {0.0f, 0.0f};
    idVec2 tMinMax = {0.0f, 0.0f};
    float sMinimum;
    float sMaximum;
    float textureTMinimum;
    float textureTMaximum;
    decl = ribbonDecl;
    if (decl->textureRepeatStretch) {
      textureTMinimum = decl->textureTMin;
      textureTMaximum = decl->textureTMax;
      sMinimum = static_cast<float>(startIndex);
      sMaximum = static_cast<float>(endIndex);
      sMinMax = {sMinimum, sMaximum};
      tMinMax = {textureTMinimum, textureTMaximum};

      const float normalizedStart =
          (static_cast<float>(segment) * inverseSegmentCount) * 2.0f -
          1.0f;
      const float normalizedEnd =
          ((static_cast<float>(segment) * inverseSegmentCount +
            inverseSegmentCount) *
           2.0f) -
          1.0f;
      const float variance = startVariance * 2.0f - 1.0f;
      SetPackedTangent(node, 0, normalizedStart, variance, -1.0f);
      SetPackedTangent(node, 1, normalizedStart, variance, 1.0f);
      SetPackedTangent(node, 2, normalizedEnd, variance, -1.0f);
      SetPackedTangent(node, 3, normalizedEnd, variance, 1.0f);
    } else {
      if (decl->textureRepeat) {
        sMinimum = 0.0f;
        sMaximum = 1.0f;
      } else if (decl->maxLength == 0.0f) {
        sMinimum = static_cast<float>(segment) * inverseSegmentCount;
        sMaximum = sMinimum + inverseSegmentCount;
      } else {
        sMinimum = previousLengthFraction;
        accumulatedLength += startNode.nodeLength;
        sMaximum = accumulatedLength * inverseTotalLength;
        currentLengthFraction = sMaximum;
        previousLengthFraction = currentLengthFraction;
      }
      sMinMax = {sMinimum, sMaximum};
      textureTMinimum = decl->textureTMin;
      textureTMaximum = decl->textureTMax;
      tMinMax = {textureTMinimum, textureTMaximum};
    }

    const idMaterial* const material = ribbonDecl->material;
    if (material != nullptr && (material->flags0 & 0x10u) != 0) {
      const float atlasX = material->atlasScaleBias.x;
      const float atlasY = material->atlasScaleBias.y;
      const float atlasZ = material->atlasScaleBias.z;
      const float atlasW = material->atlasScaleBias.w;
      const float sInset = 0.5f / (atlasX * 4096.0f);
      const float tInset = 0.5f / (atlasY * 4096.0f);
      sMinMax.x = ((sMinimum + sInset) * atlasX) + atlasZ;
      sMinMax.y = ((sMaximum - sInset) * atlasX) + atlasZ;
      tMinMax.x = ((textureTMinimum + tInset) * atlasY) + atlasW;
      tMinMax.y = ((textureTMaximum - tInset) * atlasY) + atlasW;
    }
    node.sMinMax = sMinMax;
    node.tMinMax = tMinMax;
  }

  if (explicitDefineBothEdges) {
    beam->orientType = BEAM_ORIENT_EXPLICIT;
  } else {
    beam->orientType = ribbonDecl->viewOriented ? BEAM_ORIENT_VIEWER
                                                : BEAM_ORIENT_AXIS;
  }
  beam->material = ribbonDecl->material;
}
