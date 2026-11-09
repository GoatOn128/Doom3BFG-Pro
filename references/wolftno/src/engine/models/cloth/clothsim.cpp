#include "engine/models/cloth/clothsim.h"

#include "engine/models/renderworld.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>

extern idCVar clothsim_useJobs;

idCVar g_showClothBounds("g_showClothBounds", "0", CVAR_BOOL, "");

namespace {

const idClothSimDebugCallbacks* debugCallbacks = nullptr;
constexpr idColor colorWhite{1.0f, 1.0f, 1.0f, 1.0f};
constexpr idColor colorMagenta{1.0f, 0.0f, 1.0f, 1.0f};
constexpr idColor colorOrange{1.0f, 0.5f, 0.0f, 1.0f};
constexpr idColor colorYellow{1.0f, 1.0f, 0.0f, 1.0f};

idVec3 TransformPoint(const idVec3& point, const idVec3& offset,
                      const idMat3& axis) {
  return axis * point + offset;
}

float Distance(const idVec3& lhs, const idVec3& rhs) {
  const idVec3 delta = lhs - rhs;
  return std::sqrt((std::max)(delta.LengthSqr(), FLT_MIN));
}

idClothSpring* AllocSpring(idList<idClothSpring, 81>& springs) {
  return springs.Alloc();
}

void InitializeGeneratedState(idClothSim& sim, const idDeclCloth* clothSystem) {
  sim.clothDecl = clothSystem;
  sim.origin = vec3_origin;
  sim.axis = mat3_identity;
  sim.windDirection = vec3_origin;
  sim.gravity = {0.0f, 0.0f, -1.0f};
  sim.numIterations = 1;
  sim.timeDelta = 0.016000001f;
  sim.currentDeferred = 0;
  sim.clothBoundsFromJob = new idBounds{};
  sim.clothParms = new clothParms_t{};
  sim.clothBounds = {};
}

} // namespace

void SetClothSimDebugCallbacks(const idClothSimDebugCallbacks* callbacks) {
  debugCallbacks = callbacks;
}

idClothSim::idClothSim(const idDeclCloth* clothSystem) {
  InitializeGeneratedState(*this, clothSystem);
  width = 0;
  height = 0;
  hSpacing = 0.0f;
  vSpacing = 0.0f;
  cloth = nullptr;
  numClothParticles = 0;
  isRect = false;

  switch (clothDecl->type) {
  case CLOTH_ROPE:
    GenerateRope();
    break;
  case CLOTH_SQUARE:
    GenerateSquare();
    break;
  case CLOTH_GENERIC:
    GenerateGeneric();
    break;
  default:
    idLib::Error("idClothSim - unsupported type");
  }
}

idClothSim::idClothSim(int w, int h, float hGridSpacing,
                       float vGridSpacing, const idDeclCloth* clothSystem,
                       bool rect) {
  InitializeGeneratedState(*this, clothSystem);
  const int requestedParticles = w * h;
  if (requestedParticles > MAX_CLOTH_COLLISIONS) {
    idLib::Error(
        "Cloth system '%s' has too many particles (%d).  Decrease the width and/or height to be less than %d.",
        clothSystem->name.str, requestedParticles, MAX_CLOTH_COLLISIONS);
  }

  width = w;
  isRect = rect;
  if (rect) {
    height = h;
    numClothParticles = requestedParticles;
  } else {
    if ((width & 1) == 0)
      ++width;
    height = (width + 1) / 2;
    numClothParticles = height * height;
  }

  cloth = new idClothParticle[numClothParticles];
  for (int i = 0; i < numClothParticles; ++i)
    cloth[i].Reset();
  hSpacing = hGridSpacing;
  vSpacing = vGridSpacing;
  if (rect)
    GenerateRect(clothSystem);
  else
    GenerateTriangle(clothSystem);
}

idClothSim::~idClothSim() {
  delete[] cloth;
  delete clothBoundsFromJob;
  delete clothParms;
}

void idClothSim::Reset() {
  for (int i = 0; i < numClothParticles; ++i)
    cloth[i].state = 0;
}

void idClothSim::DebugDraw(const idVec3& offset, const idMat3& modelAxis,
                           idRenderDebug* renderDebug, int debugLevel) {
  if (g_showClothBounds.valueInteger != 0 && debugCallbacks != nullptr &&
      debugCallbacks->debugBounds != nullptr) {
    const idVec3 scale{1.0f, 1.0f, 1.0f};
    idBounds transformed;
    transformed.FromTransformedBounds(clothBounds, offset, modelAxis, scale);
    debugCallbacks->debugBounds(renderDebug, idColor::colorRed, transformed,
                                vec3_origin, 0, false);
  }

  if (debugLevel == 0)
    return;

  if (debugLevel == 1 || debugLevel >= 4) {
    for (int i = 0; i < numClothParticles; ++i) {
      const idVec3 point = TransformPoint(cloth[i].origin, offset, modelAxis);
      renderDebug->DebugPoint(cloth[i].anchored != 0 ? idColor::colorGreen
                                                     : idColor::colorBlue,
                              point, 0, false);
      if (debugCallbacks != nullptr && debugCallbacks->debugText != nullptr) {
        char text[16];
        std::snprintf(text, sizeof(text), "%d", i);
        debugCallbacks->debugText(renderDebug, text, point, 1.0f, colorYellow,
                                  0, false);
      }
    }
  }

  if (debugLevel == 2 || debugLevel >= 4) {
    if (debugCallbacks != nullptr && debugCallbacks->debugLine != nullptr) {
      for (int i = 0; i < springs.num; ++i) {
        const idClothSpring& spring = springs[i];
        const idColor& color = spring.band   ? idColor::colorGreen
                               : spring.edge ? colorMagenta
                                             : colorWhite;
        debugCallbacks->debugLine(
            renderDebug, color,
            TransformPoint(cloth[spring.p1].origin, offset, modelAxis),
            TransformPoint(cloth[spring.p2].origin, offset, modelAxis), 0,
            false);
      }
    }
  }

  if (debugLevel > 2 && debugCallbacks != nullptr &&
      debugCallbacks->debugSphere != nullptr) {
    for (int i = 0; i < permanentCollisionSpheres.num; ++i) {
      idSphere sphere = permanentCollisionSpheres[i];
      sphere.origin = TransformPoint(sphere.origin, offset, modelAxis);
      debugCallbacks->debugSphere(renderDebug, colorOrange, sphere, 12, 0,
                                  true);
    }
    const idList<idSphere, 81>& temporary =
        temporaryCollisionSpheres[currentDeferred ^ 1];
    for (int i = 0; i < temporary.num; ++i) {
      idSphere sphere = temporary[i];
      sphere.origin = TransformPoint(sphere.origin, offset, modelAxis);
      debugCallbacks->debugSphere(renderDebug, colorOrange, sphere, 12, 0,
                                  true);
    }
  }
}

void idClothSim::SetOriginAxis(const idVec3& newOrigin,
                               const idMat3& newAxis,
                               bool updateParticles) {
  origin = newOrigin;
  axis = newAxis;
  if (!updateParticles)
    return;

  if (clothDecl->type == CLOTH_ROPE) {
    int particleIndex = 0;
    for (int ropeIndex = 0; ropeIndex < clothDecl->ropeData.num; ++ropeIndex) {
      const idDeclCloth::ropeInfo& rope = clothDecl->ropeData[ropeIndex];
      const idVec3& base = rope.positions[rope.positions.num - 1];
      for (int pointIndex = rope.positions.num - 1; pointIndex >= 0;
           --pointIndex, ++particleIndex) {
        const idVec3 local = rope.positions[pointIndex] - base;
        cloth[particleIndex].origin = newAxis * local + newOrigin;
        cloth[particleIndex].lastOrigin = cloth[particleIndex].origin;
        cloth[particleIndex].axis = newAxis;
      }
    }
    return;
  }

  for (int i = 0; i < numClothParticles; ++i) {
    cloth[i].origin = newAxis * clothDecl->particleData[i].position + newOrigin;
    cloth[i].lastOrigin = cloth[i].origin;
  }
}

int idClothSim::RopeParticleOffset(int index) const {
  int offset = 0;
  for (int i = 0; i < index; ++i)
    offset += clothDecl->ropeData[i].positions.num;
  return offset;
}

void idClothSim::SetAnchor(int index, bool tempAnchor) {
  if (index < 0 || index >= numClothParticles) {
    idLib::Warning("idClothSim::SetAnchor() - invalid index");
    return;
  }
  const int particle = clothDecl->type == CLOTH_ROPE
                           ? RopeParticleOffset(index)
                           : index;
  cloth[particle].anchored = tempAnchor ? 2 : 1;
}

void idClothSim::SetAnchorPosition(int index, const idVec3& org) {
  if (index < 0 || index >= numClothParticles) {
    idLib::Warning("idClothSim::SetAnchorPosition() - invalid index");
    return;
  }
  const int particle = clothDecl->type == CLOTH_ROPE
                           ? RopeParticleOffset(index)
                           : index;
  cloth[particle].origin = org;
}

void idClothSim::SetAnchorAxis(int index, const idMat3& modelAxis) {
  if (index < 0 || index >= numClothParticles) {
    idLib::Warning("idClothSim::SetAnchorAxis() - invalid index");
    return;
  }
  const int particle = clothDecl->type == CLOTH_ROPE
                           ? RopeParticleOffset(index)
                           : index;
  cloth[particle].axis = modelAxis;
}

void idClothSim::GenerateGeneric() {
  if (clothDecl->particleData.num == 0 || clothDecl->springData.num == 0)
    idLib::Error("Bad flag data in %s", clothDecl->name.str);

  height = 1;
  width = clothDecl->particleData.num;
  numClothParticles = clothDecl->particleData.num;
  cloth = new idClothParticle[numClothParticles];
  for (int i = 0; i < numClothParticles; ++i) {
    cloth[i].Reset();
    cloth[i].origin = clothDecl->particleData[i].position;
    cloth[i].lastOrigin = clothDecl->particleData[i].position;
    cloth[i].mass = clothDecl->mass;
    if (clothDecl->particleData[i].anchored)
      cloth[i].anchored = 1;
  }

  springs.SetNum(clothDecl->springData.num);
  for (int i = 0; i < clothDecl->springData.num; ++i) {
    const idDeclCloth::springInfo_t& spring = clothDecl->springData[i];
    springs[i].Init2(spring.index1, spring.index2,
                     Distance(cloth[spring.index1].origin,
                              cloth[spring.index2].origin),
                     clothDecl->springFriction,
                     clothDecl->springMaxLengthMultiplier);
  }
}

void idClothSim::AddCollisionSphere(const idSphere& collisionSphere,
                                    bool isAPermanentSphere) {
  idList<idSphere, 81>& temporary =
      temporaryCollisionSpheres[currentDeferred];
  if (permanentCollisionSpheres.num + temporary.num >=
      MAX_CLOTH_COLLISIONS_SIZE)
    return;
  if (isAPermanentSphere)
    permanentCollisionSpheres.Append(collisionSphere);
  else
    temporary.Append(collisionSphere);
}

void idClothSim::GetRopeParticleOriginAxis(
    int index, idList<idVec3, 5>& originList,
    idList<idMat3, 5>& axisList) {
  const int particleOffset = RopeParticleOffset(index);
  const int count = clothDecl->ropeData[index].positions.num;
  originList.SetNum(count);
  axisList.SetNum(count);
  for (int i = 0; i < count; ++i) {
    originList[i] = cloth[particleOffset + i].origin;
    axisList[i] = cloth[particleOffset + i].axis;
  }
}

void idClothSim::GenerateRope() {
  if (clothDecl->ropeData.num <= 0)
    idLib::Error("Bad rope data in %s", clothDecl->name.str);

  for (int i = 0; i < clothDecl->ropeData.num; ++i)
    numClothParticles += clothDecl->ropeData[i].positions.num;
  cloth = new idClothParticle[numClothParticles];

  int particleOffset = 0;
  for (int ropeIndex = 0; ropeIndex < clothDecl->ropeData.num; ++ropeIndex) {
    const idDeclCloth::ropeInfo& rope = clothDecl->ropeData[ropeIndex];
    for (int i = 0; i < rope.positions.num - 1; ++i) {
      const int sourceIndex = rope.positions.num - i - 1;
      idClothParticle& particle = cloth[particleOffset + i];
      particle.Reset();
      particle.origin = rope.positions[sourceIndex];
      particle.lastOrigin = particle.origin;
      particle.mass = clothDecl->mass;
      idClothSpring* spring = AllocSpring(springs);
      spring->Init2(particleOffset + i, particleOffset + i + 1,
                    Distance(rope.positions[sourceIndex],
                             rope.positions[sourceIndex - 1]),
                    clothDecl->springFriction,
                    clothDecl->springMaxLengthMultiplier);
    }
    cloth[particleOffset].anchored = 1;
    idClothParticle& last = cloth[particleOffset + rope.positions.num - 1];
    last.Reset();
    last.origin = rope.positions[0];
    last.lastOrigin = last.origin;
    last.mass = clothDecl->mass;
    particleOffset += rope.positions.num;
  }
}

void idClothSim::GenerateSquare() {
  if (clothDecl->particleData.num == 0)
    idLib::Error("Bad flag data in %s", clothDecl->name.str);

  width = clothDecl->width;
  height = clothDecl->height;
  numClothParticles = width * height;
  cloth = new idClothParticle[numClothParticles];
  for (int i = 0; i < numClothParticles; ++i) {
    cloth[i].Reset();
    cloth[i].origin = clothDecl->particleData[i].position;
    cloth[i].lastOrigin = cloth[i].origin;
    cloth[i].mass = clothDecl->mass;
    if (clothDecl->particleData[i].anchored)
      cloth[i].anchored = 1;
  }

  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const int particle = column + row * width;
      if (row < height - 1) {
        const int below = column + width * (row + 1);
        AllocSpring(springs)->Init2(
            particle, below, Distance(cloth[particle].origin,
                                      cloth[below].origin),
            clothDecl->springFriction,
            clothDecl->springMaxLengthMultiplier);
      }
      if (column > 0) {
        const int left = row * width + column - 1;
        AllocSpring(springs)->Init2(
            particle, left, Distance(cloth[particle].origin,
                                     cloth[left].origin),
            clothDecl->springFriction,
            clothDecl->springMaxLengthMultiplier);
      }
    }
  }
}

void idClothSim::Swap() {
  currentDeferred ^= 1;
  weaponTraces[currentDeferred].Clear();
  temporaryCollisionSpheres[currentDeferred].Clear();
  collisionPlane[currentDeferred] = {};
  clothBounds = *clothBoundsFromJob;
}

void idClothSim::Run(idParallelJobList* parallelJobList) {
  if (numClothParticles > MAX_CLOTH_COLLISIONS)
    return;

  clothParms->type = clothDecl->type;
  clothParms->numIterations = 1;
  clothParms->cloth = cloth;
  clothParms->numClothParticles = numClothParticles;
  clothParms->springs = springs.list;
  clothParms->numSprings = springs.num;
  temporaryCollisionSpheres[currentDeferred].Append(
      permanentCollisionSpheres);
  clothParms->collisions = temporaryCollisionSpheres[currentDeferred].list;
  clothParms->numCollisions =
      temporaryCollisionSpheres[currentDeferred].num;
  clothParms->weaponTraces = weaponTraces[currentDeferred].list;
  clothParms->numWeaponTraces = weaponTraces[currentDeferred].num;
  clothParms->vertices = nullptr;
  clothParms->bounds = clothBoundsFromJob;
  clothParms->org = origin;
  clothParms->axis = axis;
  clothParms->friction = clothDecl->friction;
  clothParms->gravity = {0.0f, 0.0f, clothDecl->gravity};
  clothParms->windDirection = windDirection;
  clothParms->collisionPlane = collisionPlane[currentDeferred];
  clothParms->hasCollisionPlane =
      collisionPlane[currentDeferred].a != 0.0f ||
      collisionPlane[currentDeferred].b != 0.0f ||
      collisionPlane[currentDeferred].c != 0.0f;
  clothParms->collisionFriction = clothDecl->collisionFriction;
  clothParms->width = width;
  clothParms->height = height;
  clothParms->vSpacing = vSpacing;
  clothParms->hSpacing = hSpacing;
  clothParms->rect = isRect;
  clothParms->timeDelta = timeDelta;
  clothParms->normalWind = true;

  if (parallelJobList != nullptr && clothsim_useJobs.valueInteger == 1)
    parallelJobList->AddJob(reinterpret_cast<jobRun_t>(ClothGenJob), clothParms);
  else
    ClothGenJob(clothParms);
  Swap();
}

void idClothSim::GenerateTriangle(const idDeclCloth* clothSystem) {
  int particleIndex = 0;
  for (int row = 0, rowWidth = 1; row < height; ++row, rowWidth += 2) {
    const float firstX = -static_cast<float>(row) * hSpacing;
    for (int column = 0; column < rowWidth; ++column, ++particleIndex) {
      idClothParticle& particle = cloth[particleIndex];
      particle.origin = {firstX + static_cast<float>(column) * hSpacing,
                         static_cast<float>(row) * vSpacing, 0.0f};
      particle.lastOrigin = particle.origin;
      particle.mass = clothSystem->mass;

      if (particleIndex == 0) {
        particle.neighbors[0] = 0;
        particle.neighbors[1] = 3;
        particle.neighbors[2] = 2;
        particle.neighbors[3] = 0;
      } else if (row == height - 1) {
        particle.neighbors[0] = static_cast<std::uint8_t>(
            column == 0 ? particleIndex - rowWidth + 2
                        : column == rowWidth - 1
                              ? particleIndex - rowWidth
                              : particleIndex - rowWidth + 1);
        particle.neighbors[1] = static_cast<std::uint8_t>(
            column == rowWidth - 1 ? particleIndex : particleIndex + 1);
        particle.neighbors[2] = static_cast<std::uint8_t>(particleIndex);
        particle.neighbors[3] = static_cast<std::uint8_t>(
            column == 0 ? particleIndex : particleIndex - 1);
      } else {
        particle.neighbors[0] = static_cast<std::uint8_t>(
            column == 0 ? particleIndex - rowWidth + 2
                        : column == rowWidth - 1
                              ? particleIndex - rowWidth
                              : particleIndex - rowWidth + 1);
        particle.neighbors[1] = static_cast<std::uint8_t>(
            column == rowWidth - 1 ? particleIndex : particleIndex + 1);
        particle.neighbors[2] = static_cast<std::uint8_t>(
            column == 0 ? particleIndex + rowWidth
                        : column == rowWidth - 1
                              ? particleIndex + rowWidth + 2
                              : particleIndex + rowWidth + 1);
        particle.neighbors[3] = static_cast<std::uint8_t>(
            column == 0 ? particleIndex : particleIndex - 1);
      }
    }
  }

  int p1 = 0;
  for (int row = 0, rowWidth = 1; row < height; ++row, rowWidth += 2) {
    for (int column = 0; column < rowWidth; ++column, ++p1) {
      if (column < rowWidth - 1)
        AllocSpring(springs)->Init(clothSystem->friction, 0.0f, cloth,
                                   p1, p1 + 1, false, false);
      if (column < rowWidth - 2)
        AllocSpring(springs)->Init(clothSystem->friction, 1.0f, cloth,
                                   p1, p1 + 2, false, false);
      if (row < height - 1) {
        AllocSpring(springs)->Init(clothSystem->friction, 0.0f, cloth,
                                   p1, p1 + rowWidth + 1, false, false);
        AllocSpring(springs)->Init(clothSystem->friction, 0.0f, cloth,
                                   p1, p1 + rowWidth + 2, false, false);
        AllocSpring(springs)->Init(clothSystem->friction, 0.0f, cloth,
                                   p1, p1 + rowWidth, false, false);
      }
      if (row < height - 2) {
        AllocSpring(springs)->Init(clothSystem->friction, 1.0f, cloth,
                                   p1, p1 + 2 * rowWidth + 4, false, false);
        AllocSpring(springs)->Init(clothSystem->friction, 1.0f, cloth,
                                   p1, p1 + 2 * rowWidth + 2, false, false);
        AllocSpring(springs)->Init(clothSystem->friction, 1.0f, cloth,
                                   p1, p1 + 2 * rowWidth + 6, false, false);
      }
    }
  }
}

void idClothSim::AddWeaponTrace(const idVec3& start, const idVec3& dir,
                                float strength, bool causesHoles) {
  clothWeaponTrace_t* trace = weaponTraces[currentDeferred].Alloc();
  trace->start = start;
  trace->dir = dir;
  trace->strength = strength;
  trace->causesHoles = causesHoles;
}

void idClothSim::GenerateRect(const idDeclCloth* clothSystem) {
  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const int index = column + row * width;
      idClothParticle& particle = cloth[index];
      particle.origin = {static_cast<float>(column) * hSpacing,
                         static_cast<float>(row) * vSpacing, 0.0f};
      particle.lastOrigin = particle.origin;
      particle.mass = clothSystem->mass;
      particle.neighbors[0] = static_cast<std::uint8_t>(
          row == 0 ? index : index - width);
      particle.neighbors[1] = static_cast<std::uint8_t>(
          column == width - 1 ? index : index + 1);
      particle.neighbors[2] = static_cast<std::uint8_t>(
          row == height - 1 ? index : index + width);
      particle.neighbors[3] = static_cast<std::uint8_t>(
          column == 0 ? index : index - 1);
    }
  }

  for (int row = 0; row < height; ++row) {
    for (int column = 0; column < width; ++column) {
      const int p1 = column + width * row;
      if (column < width - 1) {
        const bool edge = row == 0 || row == height - 1;
        AllocSpring(springs)->Init(1.0f, clothSystem->friction, cloth,
                                   p1, p1 + 1, false, edge);
        if (clothSystem->springType != CLOTH_SPRING_QUAD &&
            row < height - 1) {
          AllocSpring(springs)->Init(1.0f, clothSystem->friction, cloth,
                                     p1, column + width * (row + 1) + 1,
                                     false, false);
        }
      }
      if (clothSystem->springType != CLOTH_SPRING_QUAD && column > 0 &&
          row < height - 1) {
        AllocSpring(springs)->Init(1.0f, clothSystem->friction, cloth,
                                   p1, column + width * (row + 1) - 1,
                                   false, false);
      }
      if (row < height - 1) {
        const bool edge = column == 0 || column == width - 1;
        AllocSpring(springs)->Init(1.0f, clothSystem->friction, cloth,
                                   p1, column + width * (row + 1), false,
                                   edge);
      }
      if (clothSystem->springType == CLOTH_SPRING_BANDED) {
        if (column < width - 2) {
          const bool edge = row == 0 || row == height - 1;
          AllocSpring(springs)->Init(clothSystem->bandSpringForce,
                                     clothSystem->friction, cloth, p1,
                                     p1 + 2, true, edge);
        }
        if (row < height - 2) {
          const bool edge = column == 0 || column == width - 1;
          AllocSpring(springs)->Init(clothSystem->bandSpringForce,
                                     clothSystem->friction, cloth, p1,
                                     column + width * (row + 2), true,
                                     edge);
        }
      }
    }
  }
}

// Recovered explicit template records owned by ClothSim.obj.
template idList<idSphere, 81>::idList();
template bool idList<idClothSpring, 81>::Resize(int);
template bool idList<idMat3, 5>::Resize(int);
template bool idList<clothWeaponTrace_t, 81>::Resize(int);
template int idList<idSphere, 81>::Append<81>(
    const idList<idSphere, 81>&);
template clothWeaponTrace_t* idList<clothWeaponTrace_t, 81>::Alloc();

// The recovered destructor and both constructors each contain four compiler
// cleanup funclets. Their 12 records are represented by the idList member
// destructors and the owning pointer cleanup above (normal C++ RAII).
// The recovered dynamic initializer and atexit destructor for
// g_showClothBounds are represented by the global idCVar object.
