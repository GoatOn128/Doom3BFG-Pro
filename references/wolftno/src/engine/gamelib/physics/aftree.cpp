#define WOLFTNO_RETAIL_AFTREE_SPATIAL_CTOR_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/physics/aftree.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_AFTREE_SPATIAL_CTOR_ABI

#include <algorithm>
#include <cstdint>
#include <intrin.h>

extern bool forceIdListsToReallocateEveryAppend;
idRenderDebug* GetGameLibRenderDebug();

idCVar af_useSymmetry(
    "af_useSymmetry", "1", CVAR_BOOL,
    "use constraint matrix symmetry",
    static_cast<argCompletionRef_t>(nullptr));

void idSpatialMat::SetData(int rows, int columns, float* data) {
  if (mat != nullptr && allocatedRows > 0) {
    idMem::Singleton().Free(mat, ALIGN_16);
  }

  mat = data;
  allocatedRows = -6;
  numRows = rows;
  numColumns = columns;

  for (int row = 0; row < numRows; ++row) {
    for (int column = numColumns; column < 8; ++column) {
      mat[row * 8 + column] = 0.0f;
    }
  }
  for (int row = numRows; row < 6; ++row) {
    for (int column = 0; column < 8; ++column) {
      mat[row * 8 + column] = 0.0f;
    }
  }
}

bool idSpatialMat::Inverse(idSpatialMat& destination) const {
  destination.SetSize(numRows, numColumns);
  switch (numRows) {
    case 1:
      return Inverse1x1(destination);
    case 2:
      return Inverse2x2(destination);
    case 3:
      return Inverse3x3(destination);
    case 4:
      return Inverse4x4(destination);
    case 5:
      return Inverse5x5(destination);
    case 6:
      return Inverse6x6(destination);
    default:
      return false;
  }
}

const idSpatialVec idSpatialMat::SubSpatialVec(int row) const {
  idSpatialVec result;
  result.size = static_cast<short>(numColumns);
  result.allocated = -1;
  result.p = &mat[8 * row];
  return result;
}

void idAFTree::Solve(int auxiliaryIndex) const {
  for (int index = sortedBodies.num - 1; index >= 0; --index) {
    idAFBody* const body = sortedBodies.list[index];
    for (int childIndex = 0; childIndex < body->children.num;
         ++childIndex) {
      idAFBody* const child = body->children.list[childIndex];
      idAFConstraint* const primary = child->primaryConstraint;

      if ((child->fl & 0x20u) == 0u) {
        primary->fl &= static_cast<std::uint8_t>(~0x10u);
        child->J.TransposeMultiplySub(primary->lm, child->s);
      }
      if ((primary->fl & 0x10u) == 0u) {
        body->fl &= static_cast<std::uint8_t>(~0x20u);
        primary->J.TransposeMultiplySub(body->s, primary->lm);
      }
    }
  }

  const bool useSymmetry = af_useSymmetry.valueInteger != 0;
  for (int index = 0; index < sortedBodies.num; ++index) {
    idAFBody* const body = sortedBodies.list[index];
    idAFConstraint* const primary = body->primaryConstraint;

    if (primary != nullptr) {
      if (body->parent->maxSubTreeAuxiliaryIndex >= auxiliaryIndex ||
          !useSymmetry) {
        if ((primary->fl & 0x10u) == 0u) {
          primary->invI.Multiply(primary->lm, primary->lm);
        }
        if ((primary->body2->fl & 0x20u) == 0u) {
          primary->fl &= static_cast<std::uint8_t>(~0x10u);
          primary->J.MultiplySub(primary->lm, primary->body2->s);
        }
        if ((body->maxSubTreeAuxiliaryIndex >= auxiliaryIndex ||
             !useSymmetry) &&
            body->children.num != 0) {
          if ((body->fl & 0x20u) == 0u) {
            body->invI.Multiply(body->s, body->s);
          }
          if ((primary->fl & 0x10u) == 0u) {
            body->fl &= static_cast<std::uint8_t>(~0x20u);
            body->J.MultiplySub(primary->lm, body->s);
          }
        }
      }
    } else if (body->children.num != 0 && (body->fl & 0x20u) == 0u) {
      body->invI.Multiply(body->s, body->s);
    }
  }
}

void idAFTree::SetMaxSubTreeAuxiliaryIndex() {
  for (int index = sortedBodies.num - 1; index >= 0; --index) {
    idAFBody* const body = sortedBodies.list[index];
    body->maxSubTreeAuxiliaryIndex = body->maxAuxiliaryIndex;
    for (int childIndex = 0; childIndex < body->children.num;
         ++childIndex) {
      const int childMaximum =
          body->children.list[childIndex]->maxSubTreeAuxiliaryIndex;
      if (childMaximum > body->maxSubTreeAuxiliaryIndex) {
        body->maxSubTreeAuxiliaryIndex = childMaximum;
      }
    }
  }
}

void idAFTree::DebugDraw(const idVec4& color) const {
  for (int index = 1; index < sortedBodies.num; ++index) {
    idAFBody* const body = sortedBodies.list[index];
    GetGameLibRenderDebug()->DebugArrow(
        color, body->parent->clipModel->origin, body->clipModel->origin,
        1.0f, 0, false);
  }
}

void idAFTree::Factor() const {
  alignas(16) float childData[48];
  alignas(16) float temporaryData[48];
  idSpatialMat childI;
  childI.SetData(6, 6, childData);
  idSpatialMat temporary;
  temporary.SetData(6, 6, temporaryData);

  for (int index = sortedBodies.num - 1; index >= 0; --index) {
    idAFBody* const body = sortedBodies.list[index];
    if (body->children.num != 0) {
      for (int childIndex = 0; childIndex < body->children.num;
           ++childIndex) {
        idAFConstraint* const child =
            body->children.list[childIndex]->primaryConstraint;
        child->body1->J.TransposeMultiply(temporary, child->body1->I);
        temporary.Multiply(childI, child->body1->J);

        for (int row = 0; row < childI.numRows; ++row) {
          for (int column = 0; column < 8; ++column) {
            const std::uint32_t bits =
                *reinterpret_cast<std::uint32_t*>(
                    &childI.mat[row * 8 + column]);
            *reinterpret_cast<std::uint32_t*>(
                &childI.mat[row * 8 + column]) = bits ^ 0x80000000u;
          }
        }

        if (!childI.Inverse(child->invI)) {
          idLib::Warning(
              "idAFTree::Factor: couldn't invert %dx%d matrix for constraint '%s'",
              child->invI.numRows, child->invI.numColumns,
              child->name.str);
        }

        child->invI.Multiply(temporary, child->J);
        child->J.SetSize(temporary.numRows, temporary.numColumns);
        for (int row = 0; row < 6; ++row) {
          for (int column = 0; column < 8; ++column) {
            child->J.mat[row * 8 + column] =
                temporary.mat[row * 8 + column];
          }
        }

        child->J.TransposeMultiply(temporary, childI);
        temporary.Multiply(childI, child->J);
        body->I.Subtract(childI);
      }

      if (!body->I.Inverse(body->invI)) {
        idLib::Warning(
            "idAFTree::Factor: couldn't invert %dx%d matrix for body %s",
            body->invI.numRows, body->invI.numColumns, body->name.str);
      }
      if (body->primaryConstraint != nullptr) {
        body->invI.Multiply(temporary, body->J);
        body->J.SetSize(temporary.numRows, temporary.numColumns);
        for (int row = 0; row < 6; ++row) {
          for (int column = 0; column < 8; ++column) {
            body->J.mat[row * 8 + column] =
                temporary.mat[row * 8 + column];
          }
        }
      }
    } else if (body->primaryConstraint != nullptr) {
      body->inverseWorldSpatialInertia.Multiply(temporary, body->J);
      body->J.SetSize(temporary.numRows, temporary.numColumns);
      for (int row = 0; row < 6; ++row) {
        for (int column = 0; column < 8; ++column) {
          body->J.mat[row * 8 + column] = temporary.mat[row * 8 + column];
        }
      }
    }
  }
}

void idAFTree::Response(const idAFConstraint* constraint, int row,
                        int auxiliaryIndex) const {
  if (sortedBodies.num == 1) {
    idAFBody* body = constraint->body1;
    const float* source = constraint->J1.mat + row * 8;
    if (body->tree != this) {
      body = constraint->body2;
      source = constraint->J2.mat + row * 8;
    }
    float* const destination = body->response + body->numResponses * 8;
    for (int index = 0; index < 8; ++index) {
      destination[index] = source[index];
    }
    body->responseIndex[body->numResponses++] = auxiliaryIndex;
    return;
  }

  for (int index = 0; index < sortedBodies.num; ++index) {
    idAFBody* const body = sortedBodies.list[index];
    if (body->primaryConstraint != nullptr) {
      for (int element = 0; element < 8; ++element) {
        body->primaryConstraint->lm.p[element] = 0.0f;
      }
      body->primaryConstraint->fl |= 0x10u;
    }
    for (int element = 0; element < 8; ++element) {
      body->s.p[element] = 0.0f;
      body->response[body->numResponses * 8 + element] = 0.0f;
    }
    body->fl |= 0x20u;
  }

  idStaticSpatialVec vector;
  idAFBody* body = constraint->body1;
  if (body->tree == this) {
    const float* const source = constraint->J1.SubSpatialVec(row).p;
    const float* const inertia = body->inverseWorldSpatialInertia.mat;
    vector.p[0] = inertia[0] * source[0];
    vector.p[1] = inertia[9] * source[1];
    vector.p[2] = inertia[18] * source[2];
    vector.p[3] = inertia[27] * source[3] + inertia[28] * source[4] +
                  inertia[29] * source[5];
    vector.p[4] = inertia[35] * source[3] + inertia[36] * source[4] +
                  inertia[37] * source[5];
    vector.p[5] = inertia[43] * source[3] + inertia[44] * source[4] +
                  inertia[45] * source[5];

    if (body->primaryConstraint != nullptr) {
      body->primaryConstraint->fl &= static_cast<std::uint8_t>(~0x10u);
      body->primaryConstraint->J1.Multiply(
          body->primaryConstraint->lm, vector);
    }
    for (int index = 0; index < body->children.num; ++index) {
      idAFConstraint* const child =
          body->children.list[index]->primaryConstraint;
      child->fl &= static_cast<std::uint8_t>(~0x10u);
      child->J2.Multiply(child->lm, vector);
    }
    float* const destination = body->response + body->numResponses * 8;
    const float* const rowData = constraint->J1.mat + row * 8;
    for (int index = 0; index < 8; ++index) {
      destination[index] = rowData[index];
    }
  }

  body = constraint->body2;
  if (body != nullptr && body->tree == this) {
    const float* const source = constraint->J2.SubSpatialVec(row).p;
    const float* const inertia = body->inverseWorldSpatialInertia.mat;
    vector.p[0] = inertia[0] * source[0];
    vector.p[1] = inertia[9] * source[1];
    vector.p[2] = inertia[18] * source[2];
    vector.p[3] = inertia[27] * source[3] + inertia[28] * source[4] +
                  inertia[29] * source[5];
    vector.p[4] = inertia[35] * source[3] + inertia[36] * source[4] +
                  inertia[37] * source[5];
    vector.p[5] = inertia[43] * source[3] + inertia[44] * source[4] +
                  inertia[45] * source[5];

    if (body->primaryConstraint != nullptr) {
      body->primaryConstraint->fl &= static_cast<std::uint8_t>(~0x10u);
      body->primaryConstraint->J1.MultiplyAdd(
          body->primaryConstraint->lm, vector);
    }
    for (int index = 0; index < body->children.num; ++index) {
      idAFConstraint* const child =
          body->children.list[index]->primaryConstraint;
      child->fl &= static_cast<std::uint8_t>(~0x10u);
      child->J2.MultiplyAdd(child->lm, vector);
    }
    float* const destination = body->response + body->numResponses * 8;
    const float* const rowData = constraint->J2.mat + row * 8;
    for (int index = 0; index < 8; ++index) {
      destination[index] = rowData[index];
    }
  }

  Solve(auxiliaryIndex);
  const bool useSymmetry = af_useSymmetry.valueInteger != 0;
  idSpatialVec force;
  for (int index = 0; index < sortedBodies.num; ++index) {
    body = sortedBodies.list[index];
    if (useSymmetry && body->maxAuxiliaryIndex < auxiliaryIndex) {
      continue;
    }

    if (force.p != nullptr && force.allocated > 0) {
      idMem::Singleton().Free(force.p, ALIGN_16);
    }
    force.p = body->response + body->numResponses * 8;
    force.size = 6;
    force.allocated = -8;
    force.p[6] = 0.0f;
    force.p[7] = 0.0f;

    if (body->primaryConstraint != nullptr) {
      body->primaryConstraint->J1.TransposeMultiplyAdd(
          force, body->primaryConstraint->lm);
    }
    for (int childIndex = 0; childIndex < body->children.num;
         ++childIndex) {
      idAFConstraint* const child =
          body->children.list[childIndex]->primaryConstraint;
      child->J2.TransposeMultiplyAdd(force, child->lm);
    }
    body->responseIndex[body->numResponses++] = auxiliaryIndex;
  }
}

void idAFTree::CalculateForces(float timeStep) const {
  for (int index = 0; index < sortedBodies.num; ++index) {
    idAFBody* const body = sortedBodies.list[index];
    for (int element = 0; element < 6; ++element) {
      body->totalForce.p[element] =
          body->auxForce.p[element] + body->current.externalForce.p[element];
    }
  }

  if (sortedBodies.num == 1) {
    return;
  }

  const float inverseStep = 1.0f / timeStep;
  for (int index = 0; index < sortedBodies.num; ++index) {
    idAFBody* const body = sortedBodies.list[index];
    body->InverseWorldSpatialInertiaMultiply(body->acceleration,
                                              body->totalForce);
    for (int element = 0; element < 6; ++element) {
      body->acceleration.p[element] +=
          inverseStep * body->current.spatialVelocity.p[element];
    }

    idAFConstraint* const primary = body->primaryConstraint;
    if (primary != nullptr) {
      primary->fl &= static_cast<std::uint8_t>(~0x10u);
      const int rowCount = primary->J1.numRows;
      if (primary->lm.size != rowCount || primary->lm.p == nullptr) {
        if (primary->lm.p == nullptr) {
          primary->lm.p = static_cast<float*>(
              idMem::Singleton().AllocWithLocation(
                  "w:\\build\\tech5\\shared\\idlib\\math/SpatialVec.h(218) : TAG_MATH",
                  32, TAG_MATH, false, ALIGN_16, HEAP_DEFAULTHEAP));
          primary->lm.allocated = 8;
        }
        int clearFrom = std::min<int>(primary->lm.size, rowCount);
        while (clearFrom < 8) {
          primary->lm.p[clearFrom++] = 0.0f;
        }
        primary->lm.size = static_cast<short>(rowCount);
      }

      primary->J1.Multiply(primary->lm, primary->body1->acceleration);
      primary->J2.MultiplyAdd(primary->lm,
                              primary->body2->acceleration);
      for (int element = 0; element < 8; ++element) {
        primary->lm.p[element] +=
            inverseStep * (primary->c1.p[element] + primary->c2.p[element]);
      }
    }
    body->fl |= 0x20u;
    for (int element = 0; element < 8; ++element) {
      body->s.p[element] = 0.0f;
    }
  }

  Solve(0);
  for (int index = 0; index < sortedBodies.num; ++index) {
    idAFBody* const body = sortedBodies.list[index];
    if (body->primaryConstraint != nullptr) {
      body->primaryConstraint->J1.TransposeMultiplyAdd(
          body->totalForce, body->primaryConstraint->lm);
    }
    for (int childIndex = 0; childIndex < body->children.num;
         ++childIndex) {
      idAFConstraint* const child =
          body->children.list[childIndex]->primaryConstraint;
      child->J2.TransposeMultiplyAdd(body->totalForce, child->lm);
    }
  }
}

void idAFTree::SortBodies_r(idList<idAFBody*, 71>& sortedList,
                            idAFBody* body) {
  for (int index = 0; index < body->children.num; ++index) {
    if (sortedList.list == nullptr) {
      const int initialSize =
          sortedList.granularity > 0 ? sortedList.granularity : 1;
      sortedList.Resize(initialSize);
    }
    if (sortedList.num == sortedList.size) {
      int newSize;
      if (forceIdListsToReallocateEveryAppend) {
        newSize = sortedList.size + 1;
      } else if (sortedList.granularity != 0) {
        newSize = sortedList.size + sortedList.granularity -
                  sortedList.size % sortedList.granularity;
      } else if (sortedList.size > 0x3FFFFFFF) {
        newSize = 0x7FFFFFFF;
      } else {
        newSize = sortedList.size * 2;
        if (newSize == 0) {
          newSize = 1;
        }
      }
      sortedList.Resize(newSize);
    }
    if (sortedList.num < sortedList.size) {
      sortedList.list[sortedList.num++] = body->children.list[index];
    }
  }

  for (int index = 0; index < body->children.num; ++index) {
    SortBodies_r(sortedList, body->children.list[index]);
  }
}

void idAFTree::SortBodies() {
  int rootIndex = 0;
  while (rootIndex < sortedBodies.num &&
         sortedBodies.list[rootIndex]->parent != nullptr) {
    ++rootIndex;
  }
  if (rootIndex >= sortedBodies.num) {
    idLib::Error("Articulated figure tree has no root.");
    __debugbreak();
  }

  idAFBody* const root = sortedBodies.list[rootIndex];
  if (sortedBodies.listStatic == 0 || sortedBodies.listStatic == 2) {
    if (sortedBodies.list != nullptr) {
      idMem::Singleton().Free(sortedBodies.list, ALIGN_16);
    }
    sortedBodies.list = nullptr;
    sortedBodies.size = 0;
  }
  sortedBodies.num = 0;

  if (sortedBodies.list == nullptr) {
    const int initialSize =
        sortedBodies.granularity > 0 ? sortedBodies.granularity : 1;
    sortedBodies.Resize(initialSize);
  }
  if (sortedBodies.num == sortedBodies.size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = sortedBodies.size + 1;
    } else if (sortedBodies.granularity != 0) {
      newSize = sortedBodies.size + sortedBodies.granularity -
                sortedBodies.size % sortedBodies.granularity;
    } else if (sortedBodies.size > 0x3FFFFFFF) {
      newSize = 0x7FFFFFFF;
    } else {
      newSize = sortedBodies.size * 2;
      if (newSize == 0) {
        newSize = 1;
      }
    }
    sortedBodies.Resize(newSize);
  }
  if (sortedBodies.num < sortedBodies.size) {
    sortedBodies.list[sortedBodies.num++] = root;
  }
  SortBodies_r(sortedBodies, root);
}
