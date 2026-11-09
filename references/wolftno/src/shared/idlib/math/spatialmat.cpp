#include "spatialmat.h"

#include <cmath>
#include <utility>
#include <vector>

idSpatialVec::idSpatialVec(const idSpatialVec& other) {
  *this = other;
}

idSpatialVec::~idSpatialVec() {
  if (allocated >= 0)
    delete[] p;
}

idSpatialVec& idSpatialVec::operator=(const idSpatialVec& other) {
  if (this != &other) {
    SetSize(other.size);
    std::copy_n(other.p, other.size, p);
  }
  return *this;
}

void idSpatialVec::SetSize(int count) {
  if (count <= allocated) {
    size = static_cast<short>(count);
    return;
  }
  float* replacement = new float[static_cast<std::size_t>(count)]{};
  std::copy_n(p, size, replacement);
  if (allocated >= 0)
    delete[] p;
  p = replacement;
  size = allocated = static_cast<short>(count);
}

idStaticSpatialVec::idStaticSpatialVec() {
  size = 6;
  allocated = -1;
  p = data;
}

idStaticSpatialVec::~idStaticSpatialVec() {
  p = nullptr;
  allocated = 0;
}

idSpatialMat::idSpatialMat(const idSpatialMat& other) {
  *this = other;
}

idSpatialMat& idSpatialMat::operator=(const idSpatialMat& other) {
  if (this != &other) {
    SetSize(other.numRows, other.numColumns);
    std::copy_n(other.mat, other.numRows * other.numColumns, mat);
  }
  return *this;
}

idSpatialMat::~idSpatialMat() {
  delete[] mat;
}

void idSpatialMat::SetSize(int rows, int columns) {
  if (rows < 0 || columns < 0)
    return;
  if (rows > allocatedRows || columns != numColumns) {
    delete[] mat;
    mat = new float[static_cast<std::size_t>(rows * columns)]{};
    allocatedRows = rows;
  }
  numRows = rows;
  numColumns = columns;
}

void idSpatialMat::Subtract(const idSpatialMat& other) {
  const int count = std::min(numRows * numColumns, other.numRows * other.numColumns);
  for (int index = 0; index < count; ++index)
    mat[index] -= other.mat[index];
}

void idSpatialMat::Multiply(idSpatialMat& destination, const idSpatialMat& other) const {
  idSpatialMat result(numRows, other.numColumns);
  for (int row = 0; row < numRows; ++row)
    for (int column = 0; column < other.numColumns; ++column)
      for (int inner = 0; inner < numColumns; ++inner)
        result[row][column] += (*this)[row][inner] * other[inner][column];
  destination = result;
}

void idSpatialMat::TransposeMultiply(idSpatialMat& destination, const idSpatialMat& other) const {
  idSpatialMat result(numColumns, other.numColumns);
  for (int row = 0; row < numColumns; ++row)
    for (int column = 0; column < other.numColumns; ++column)
      for (int inner = 0; inner < numRows; ++inner)
        result[row][column] += (*this)[inner][row] * other[inner][column];
  destination = result;
}

void idSpatialMat::Transpose(idSpatialMat& destination) const {
  idSpatialMat result(numColumns, numRows);
  for (int row = 0; row < numRows; ++row)
    for (int column = 0; column < numColumns; ++column)
      result[column][row] = (*this)[row][column];
  destination = result;
}

void idSpatialMat::Multiply(idSpatialVec& destination, const idSpatialVec& vector) const {
  idSpatialVec result(numRows);
  for (int row = 0; row < numRows; ++row) {
    result[row] = 0.0f;
    for (int column = 0; column < numColumns; ++column)
      result[row] += (*this)[row][column] * vector[column];
  }
  destination = result;
}

void idSpatialMat::MultiplyAdd(idSpatialVec& destination, const idSpatialVec& vector) const {
  idSpatialVec result;
  Multiply(result, vector);
  destination.SetSize(numRows);
  for (int row = 0; row < numRows; ++row)
    destination[row] += result[row];
}

void idSpatialMat::MultiplySub(idSpatialVec& destination, const idSpatialVec& vector) const {
  idSpatialVec result;
  Multiply(result, vector);
  destination.SetSize(numRows);
  for (int row = 0; row < numRows; ++row)
    destination[row] -= result[row];
}

void idSpatialMat::TransposeMultiplyAdd(idSpatialVec& destination,
                                        const idSpatialVec& vector) const {
  destination.SetSize(numColumns);
  for (int column = 0; column < numColumns; ++column)
    for (int row = 0; row < numRows; ++row)
      destination[column] += (*this)[row][column] * vector[row];
}

void idSpatialMat::TransposeMultiplySub(idSpatialVec& destination,
                                        const idSpatialVec& vector) const {
  destination.SetSize(numColumns);
  for (int column = 0; column < numColumns; ++column)
    for (int row = 0; row < numRows; ++row)
      destination[column] -= (*this)[row][column] * vector[row];
}

bool idSpatialMat::Inverse(int dimension, idSpatialMat& destination) const {
  if (numRows < dimension || numColumns < dimension)
    return false;
  std::vector<double> augmented(static_cast<std::size_t>(dimension * dimension * 2), 0.0);
  const int width = dimension * 2;
  for (int row = 0; row < dimension; ++row) {
    for (int column = 0; column < dimension; ++column)
      augmented[row * width + column] = (*this)[row][column];
    augmented[row * width + dimension + row] = 1.0;
  }
  for (int column = 0; column < dimension; ++column) {
    int pivot = column;
    for (int row = column + 1; row < dimension; ++row)
      if (std::fabs(augmented[row * width + column]) > std::fabs(augmented[pivot * width + column]))
        pivot = row;
    if (std::fabs(augmented[pivot * width + column]) < 1.0e-14)
      return false;
    if (pivot != column)
      for (int index = 0; index < width; ++index)
        std::swap(augmented[pivot * width + index], augmented[column * width + index]);
    const double inversePivot = 1.0 / augmented[column * width + column];
    for (int index = 0; index < width; ++index)
      augmented[column * width + index] *= inversePivot;
    for (int row = 0; row < dimension; ++row) {
      if (row == column)
        continue;
      const double factor = augmented[row * width + column];
      for (int index = 0; index < width; ++index)
        augmented[row * width + index] -= factor * augmented[column * width + index];
    }
  }
  destination.SetSize(dimension, dimension);
  for (int row = 0; row < dimension; ++row)
    for (int column = 0; column < dimension; ++column)
      destination[row][column] = static_cast<float>(augmented[row * width + dimension + column]);
  return true;
}

bool idSpatialMat::Inverse1x1(idSpatialMat& destination) const {
  return Inverse(1, destination);
}
bool idSpatialMat::Inverse2x2(idSpatialMat& destination) const {
  return Inverse(2, destination);
}
bool idSpatialMat::Inverse3x3(idSpatialMat& destination) const {
  return Inverse(3, destination);
}
bool idSpatialMat::Inverse4x4(idSpatialMat& destination) const {
  return Inverse(4, destination);
}
bool idSpatialMat::Inverse5x5(idSpatialMat& destination) const {
  return Inverse(5, destination);
}
bool idSpatialMat::Inverse6x6(idSpatialMat& destination) const {
  return Inverse(6, destination);
}
