#include "vecx.h"

#include <cstdint>
#include <algorithm>

alignas(16) float idVecX::temp[1028]{};
float* idVecX::tempPtr = reinterpret_cast<float*>(
    (reinterpret_cast<std::uintptr_t>(&idVecX::temp[3]) + 3u) & ~std::uintptr_t(15u));
int idVecX::tempIndex = 0;

idVecX::idVecX(const idVecX& other) : idVecX() {
  *this = other;
}

idVecX& idVecX::operator=(const idVecX& other) {
  if (this != &other) {
    SetSize(other.size);
    std::copy_n(other.p, other.size, p);
  }
  return *this;
}

idVecX::~idVecX() {
  if (alloced >= 0)
    delete[] p;
}

void idVecX::SetSize(int count) {
  if (count <= alloced && alloced >= 0) {
    size = count;
    return;
  }
  float* replacement = new float[static_cast<std::size_t>(count)]{};
  if (p != nullptr)
    std::copy_n(p, std::min(size, count), replacement);
  if (alloced >= 0)
    delete[] p;
  p = replacement;
  size = alloced = count;
}

idMatX::idMatX(const idMatX& other) : idMatX() {
  *this = other;
}

idMatX& idMatX::operator=(const idMatX& other) {
  if (this != &other) {
    SetSize(other.numRows, other.numColumns);
    std::copy_n(other.mat, other.numRows * other.numColumns, mat);
  }
  return *this;
}

idMatX::~idMatX() {
  if (alloced >= 0)
    delete[] mat;
}

void idMatX::SetSize(int rows, int columns) {
  const int count = rows * columns;
  if (count > alloced || alloced < 0) {
    float* replacement = new float[static_cast<std::size_t>(count)]{};
    if (mat != nullptr) {
      std::copy_n(mat, std::min(numRows * numColumns, count), replacement);
    }
    if (alloced >= 0)
      delete[] mat;
    mat = replacement;
    alloced = count;
  }
  numRows = rows;
  numColumns = columns;
}
