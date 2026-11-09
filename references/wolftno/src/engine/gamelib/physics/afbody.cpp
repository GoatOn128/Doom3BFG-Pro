#define WOLFTNO_RETAIL_AFBODY_SPATIAL_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/physics/aftree.h"

#include "engine/models/serializer.h"
#include "shared/idlib/lib.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_AFBODY_SPATIAL_CTOR_ABI

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

bool RetailInverseSelf(idMat3& matrix) {
  const float cofactor00 =
      matrix[1].y * matrix[2].z - matrix[1].z * matrix[2].y;
  const float cofactor01 =
      matrix[2].x * matrix[1].z - matrix[1].x * matrix[2].z;
  const float cofactor02 =
      matrix[1].x * matrix[2].y - matrix[2].x * matrix[1].y;
  const float determinant =
      matrix[0].x * cofactor00 + matrix[0].y * cofactor01 +
      matrix[0].z * cofactor02;
  if (std::fabs(determinant) < 1.0e-14f) {
    return false;
  }

  const double inverseDeterminant = 1.0 / static_cast<double>(determinant);
  idMat3 inverse;
  inverse[0] = {
      static_cast<float>(static_cast<double>(cofactor00) *
                         inverseDeterminant),
      static_cast<float>(static_cast<double>(
                             matrix[0].z * matrix[2].y -
                             matrix[0].y * matrix[2].z) *
                         inverseDeterminant),
      static_cast<float>(static_cast<double>(
                             matrix[0].y * matrix[1].z -
                             matrix[0].z * matrix[1].y) *
                         inverseDeterminant)};
  inverse[1] = {
      static_cast<float>(static_cast<double>(cofactor01) *
                         inverseDeterminant),
      static_cast<float>(static_cast<double>(
                             matrix[0].x * matrix[2].z -
                             matrix[0].z * matrix[2].x) *
                         inverseDeterminant),
      static_cast<float>(static_cast<double>(
                             matrix[0].z * matrix[1].x -
                             matrix[0].x * matrix[1].z) *
                         inverseDeterminant)};
  inverse[2] = {
      static_cast<float>(static_cast<double>(cofactor02) *
                         inverseDeterminant),
      static_cast<float>(static_cast<double>(
                             matrix[0].y * matrix[2].x -
                             matrix[0].x * matrix[2].y) *
                         inverseDeterminant),
      static_cast<float>(static_cast<double>(
                             matrix[0].x * matrix[1].y -
                             matrix[0].y * matrix[1].x) *
                         inverseDeterminant)};
  matrix = inverse;
  return true;
}

}  // namespace

AFBodyPState_t& AFBodyPState_t::operator=(const AFBodyPState_t& other) {
  worldOrigin = other.worldOrigin;
  worldAxis = other.worldAxis;
  atRestOrigin = other.atRestOrigin;
  atRestAxis = other.atRestAxis;
  spatialVelocity = other.spatialVelocity;
  externalForce = other.externalForce;
  return *this;
}

bool idMat3::IsDiagonal(float epsilon) const {
  if (std::fabs(mat[0].y) > epsilon) {
    return false;
  }
  if (std::fabs(mat[0].z) > epsilon) {
    return false;
  }
  if (std::fabs(mat[1].x) > epsilon) {
    return false;
  }
  if (std::fabs(mat[1].z) > epsilon) {
    return false;
  }
  if (std::fabs(mat[2].x) > epsilon) {
    return false;
  }
  if (std::fabs(mat[2].y) > epsilon) {
    return false;
  }
  return true;
}

__declspec(noinline) inline void idSpatialVec::ChangeSize(int newSize) {
  int clearFrom = std::min<int>(size, newSize);
  while (clearFrom < 8) {
    p[clearFrom++] = 0.0f;
  }
  size = static_cast<short>(newSize);
}

__declspec(noinline) inline void idSpatialVec::SetData(int length,
                                                       float* data) {
  if (p != nullptr && allocated > 0) {
    idMem::Singleton().Free(p, ALIGN_16);
  }
  p = data;
  size = static_cast<short>(length);
  allocated = -8;
  for (int index = length; index < 8; ++index) {
    p[index] = 0.0f;
  }
}

__declspec(noinline) inline void idSpatialVec::SetSize(int newSize) {
  if (size == newSize && p != nullptr) {
    return;
  }
  if (p == nullptr) {
    p = static_cast<float*>(idMem::Singleton().AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\math/SpatialVec.h(218) : TAG_MATH",
        32, TAG_MATH, false, ALIGN_16, HEAP_DEFAULTHEAP));
    allocated = 8;
  }
  ChangeSize(newSize);
}

__declspec(noinline) inline idSpatialVec::~idSpatialVec() {
  if (p != nullptr && allocated > 0) {
    idMem::Singleton().Free(p, ALIGN_16);
  }
}

__declspec(noinline) inline idStaticSpatialVec::~idStaticSpatialVec() =
    default;

__declspec(noinline) inline idSpatialMat::~idSpatialMat() {
  if (mat != nullptr && allocatedRows > 0) {
    idMem::Singleton().Free(mat, ALIGN_16);
  }
}

void idSerializer::Serialize(idVec6& value) {
  for (float& component : value.p) {
    std::uint32_t bits = 0;
    if (writing) {
      std::memcpy(&bits, &component, sizeof(bits));
      msg->WriteBits(static_cast<int>(bits), 32);
    } else {
      bits = static_cast<std::uint32_t>(msg->ReadBits(32));
      std::memcpy(&component, &bits, sizeof(bits));
    }
  }
}

void idSerializer::Serialize(idMat3& value) {
  Serialize(value.mat[0]);
  Serialize(value.mat[1]);
  Serialize(value.mat[2]);
}

idAFBody::idAFBody() {
  Init();
}

idAFBody::idAFBody(const char* bodyName, idClipModel* bodyClipModel,
                   float density) {
  Init();
  name.Set(bodyName);
  clipModel = bodyClipModel;
  SetDensity(density, mat3_identity);
}

idAFBody::~idAFBody() {
  if (clipModel != nullptr) {
    clipModel->Delete();
  }
}

void idAFBody::Init() {
  name.Set("noname");
  linearFrictionWater = -1.0f;
  angularFrictionWater = -1.0f;
  linearFriction = -1.0f;
  angularFriction = -1.0f;
  contactFriction = -1.0f;
  bouncyness = -1.0f;
  clipModel = nullptr;
  clipMask = 0;
  frictionDir = vec3_origin;
  contactMotorDir = vec3_origin;
  contactMotorVelocity = 0.0f;
  contactMotorForce = 0.0f;
  mass = 1.0f;
  invMass = 1.0f;
  centerOfMass = vec3_origin;
  inertiaTensor = mat3_identity;
  inverseInertiaTensor = mat3_identity;
  parent = nullptr;
  primaryConstraint = nullptr;
  tree = nullptr;
  fl = 0x22u;
  current.worldOrigin = vec3_origin;
  current.worldAxis = mat3_identity;
  current.atRestOrigin = vec3_origin;
  current.atRestAxis = mat3_identity;
  current.spatialVelocity = {};
  current.externalForce = {};
  saved = current;

  idStaticSpatialVec* vectors[] = {&s, &totalForce, &auxForce,
                                   &acceleration};
  for (idStaticSpatialVec* vector : vectors) {
    vector->size = 6;
    vector->allocated = -8;
    vector->p = vector->data;
    vector->SetSize(6);
    for (float& component : vector->data) {
      component = 0.0f;
    }
  }

  response = nullptr;
  responseIndex = nullptr;
  numResponses = 0;
  maxAuxiliaryIndex = 0;
  maxSubTreeAuxiliaryIndex = 0;
}

void idAFBody::SetClipModel(idClipModel* bodyClipModel) {
  if (clipModel != nullptr && clipModel != bodyClipModel) {
    clipModel->Delete();
  }
  clipModel = bodyClipModel;
}

void idAFBody::SetBouncyness(float bounce) {
  if (bounce < 0.0f || bounce > 1.0f) {
    idLib::Warning(
        "idAFBody::SetBouncyness: bouncyness out of range, bounce = %.1f",
        bounce);
    return;
  }
  bouncyness = bounce;
}

void idAFBody::SetLinearFriction(float value) {
  linearFriction = value;
  const float waterValue = value * 1.2f;
  if (waterValue < 0.0f) {
    linearFrictionWater = 0.0f;
  } else if (waterValue > 1.0f) {
    linearFrictionWater = 1.0f;
  } else {
    linearFrictionWater = waterValue;
  }
}

void idAFBody::SetAngularFriction(float value) {
  angularFriction = value;
  const float waterValue = value * 1.2f;
  if (waterValue < 0.0f) {
    angularFrictionWater = 0.0f;
  } else if (waterValue > 1.0f) {
    angularFrictionWater = 1.0f;
  } else {
    angularFrictionWater = waterValue;
  }
}

void idAFBody::SetFriction(float linear, float angular, float contact) {
  if (linear < 0.0f || linear > 1.0f || angular < 0.0f ||
      angular > 1.0f || contact < 0.0f) {
    idLib::Warning(
        "idAFBody::SetFriction: friction out of range, linear = %.1f, "
        "angular = %.1f, contact = %.1f",
        linear, angular, contact);
    return;
  }
  SetLinearFriction(linear);
  SetAngularFriction(angular);
  contactFriction = contact;
}

void idAFBody::SetDensity(float density, const idMat3& inertiaScale) {
  if (clipModel == nullptr) {
    return;
  }

  clipModel->GetMassProperties(density, mass, centerOfMass, inertiaTensor);
  if (mass <= 0.0f || std::isnan(mass)) {
    idLib::Warning("idAFBody::SetDensity: invalid mass for body '%s'",
                   name.str);
    mass = 1.0f;
    centerOfMass = vec3_origin;
    inertiaTensor = mat3_identity;
  }

  centerOfMass = vec3_origin;
  invMass = 1.0f / mass;

  bool identityScale = true;
  for (int row = 0; row < 3 && identityScale; ++row) {
    for (int column = 0; column < 3; ++column) {
      if (inertiaScale[row][column] != mat3_identity[row][column]) {
        identityScale = false;
        break;
      }
    }
  }
  if (!identityScale) {
    inertiaTensor *= inertiaScale;
  }

  if (inertiaTensor.IsDiagonal(0.001f)) {
    inertiaTensor[0].y = 0.0f;
    inertiaTensor[0].z = 0.0f;
    inertiaTensor[1].x = 0.0f;
    inertiaTensor[1].z = 0.0f;
    inertiaTensor[2].x = 0.0f;
    inertiaTensor[2].y = 0.0f;
    inverseInertiaTensor = mat3_identity;
    inverseInertiaTensor[0].x = 1.0f / inertiaTensor[0].x;
    inverseInertiaTensor[1].y = 1.0f / inertiaTensor[1].y;
    inverseInertiaTensor[2].z = 1.0f / inertiaTensor[2].z;
  } else {
    inverseInertiaTensor = inertiaTensor;
    RetailInverseSelf(inverseInertiaTensor);
  }
}

void idAFBody::SetFrictionDirection(const idVec3& direction) {
  const idMat3& axis = clipModel->axis;
  frictionDir.x =
      (direction.x * axis[0].x + direction.y * axis[0].y) +
      direction.z * axis[0].z;
  frictionDir.y =
      (direction.x * axis[1].x + direction.y * axis[1].y) +
      direction.z * axis[1].z;
  frictionDir.z =
      (direction.x * axis[2].x + direction.y * axis[2].y) +
      direction.z * axis[2].z;
  fl |= 0x08u;
}

bool idAFBody::GetFrictionDirection(idVec3& direction) const {
  if ((fl & 0x08u) == 0u) {
    return false;
  }
  const idMat3& axis = clipModel->axis;
  direction.x =
      (frictionDir.y * axis[1].x + frictionDir.x * axis[0].x) +
      frictionDir.z * axis[2].x;
  direction.y =
      (frictionDir.x * axis[0].y + frictionDir.y * axis[1].y) +
      frictionDir.z * axis[2].y;
  direction.z =
      (frictionDir.x * axis[0].z + frictionDir.y * axis[1].z) +
      frictionDir.z * axis[2].z;
  return true;
}

void idAFBody::SetContactMotorDirection(const idVec3& direction) {
  const idMat3& axis = clipModel->axis;
  contactMotorDir.x =
      (direction.x * axis[0].x + direction.y * axis[0].y) +
      direction.z * axis[0].z;
  contactMotorDir.y =
      (direction.x * axis[1].x + direction.y * axis[1].y) +
      direction.z * axis[1].z;
  contactMotorDir.z =
      (direction.x * axis[2].x + direction.y * axis[2].y) +
      direction.z * axis[2].z;
  fl |= 0x10u;
}

bool idAFBody::GetContactMotorDirection(idVec3& direction) const {
  if ((fl & 0x10u) == 0u) {
    return false;
  }
  const idMat3& axis = clipModel->axis;
  direction.x =
      (contactMotorDir.y * axis[1].x + contactMotorDir.x * axis[0].x) +
      contactMotorDir.z * axis[2].x;
  direction.y =
      (contactMotorDir.x * axis[0].y + contactMotorDir.y * axis[1].y) +
      contactMotorDir.z * axis[2].y;
  direction.z =
      (contactMotorDir.x * axis[0].z + contactMotorDir.y * axis[1].z) +
      contactMotorDir.z * axis[2].z;
  return true;
}

const idVec3 idAFBody::GetPointVelocity(const idVec3& point) const {
  const float relativeX = point.x - clipModel->origin.x;
  const float relativeY = point.y - clipModel->origin.y;
  const float relativeZ = point.z - clipModel->origin.z;
  const float angularX = current.spatialVelocity.p[3];
  const float angularY = current.spatialVelocity.p[4];
  const float angularZ = current.spatialVelocity.p[5];

  idVec3 result;
  result.x = (angularY * relativeZ - angularZ * relativeY) +
             current.spatialVelocity.p[0];
  result.y = (angularZ * relativeX - angularX * relativeZ) +
             current.spatialVelocity.p[1];
  result.z = (angularX * relativeY - angularY * relativeX) +
             current.spatialVelocity.p[2];
  return result;
}

void idAFBody::AddForce(const idVec3& point, const idVec3& force) {
  current.externalForce.p[0] = current.externalForce.p[0] + force.x;
  current.externalForce.p[1] = force.y + current.externalForce.p[1];
  current.externalForce.p[2] = force.z + current.externalForce.p[2];

  const float relativeX = point.x - clipModel->origin.x;
  const float relativeY = point.y - clipModel->origin.y;
  const float relativeZ = point.z - clipModel->origin.z;
  current.externalForce.p[3] =
      (force.z * relativeY - force.y * relativeZ) +
      current.externalForce.p[3];
  current.externalForce.p[5] =
      (force.y * relativeX - force.x * relativeY) +
      current.externalForce.p[5];
  current.externalForce.p[4] =
      (force.x * relativeZ - force.z * relativeX) +
      current.externalForce.p[4];
}

void idAFBody::Serialize(idSerializer& serializer) {
  serializer.Serialize(current.worldOrigin);
  serializer.Serialize(current.worldAxis);
  serializer.Serialize(current.spatialVelocity);
  if (!serializer.writing) {
    clipModel->SetLinkedPosition(current.worldOrigin, current.worldAxis);
  }
  clipModel->Serialize(serializer);
}
