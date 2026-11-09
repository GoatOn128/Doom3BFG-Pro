#include "vecx.h"

#include <algorithm>
#include <cmath>

void idMatX::CopyLowerToUpperTriangle() {
  const int dimension = std::min(numRows, numColumns);
  for (int row = 0; row < dimension; ++row)
    for (int column = row + 1; column < dimension; ++column)
      (*this)[row][column] = (*this)[column][row];
}

void idMatX::HouseholderReduction(idVecX& diagonal, idVecX& subDiagonal) {
  const int dimension = numRows;
  diagonal.SetSize(dimension);
  subDiagonal.SetSize(dimension);
  for (int index = dimension - 1; index > 0; --index) {
    const int last = index - 1;
    float scale = 0.0f;
    float h = 0.0f;
    for (int column = 0; column <= last; ++column)
      scale += std::fabs((*this)[index][column]);
    if (scale == 0.0f) {
      subDiagonal[index] = (*this)[index][last];
    } else {
      for (int column = 0; column <= last; ++column) {
        (*this)[index][column] /= scale;
        h += (*this)[index][column] * (*this)[index][column];
      }
      float f = (*this)[index][last];
      float g = f >= 0.0f ? -std::sqrt(h) : std::sqrt(h);
      subDiagonal[index] = scale * g;
      h -= f * g;
      (*this)[index][last] = f - g;
      f = 0.0f;
      for (int row = 0; row <= last; ++row) {
        (*this)[row][index] = (*this)[index][row] / h;
        g = 0.0f;
        for (int column = 0; column <= row; ++column)
          g += (*this)[row][column] * (*this)[index][column];
        for (int column = row + 1; column <= last; ++column)
          g += (*this)[column][row] * (*this)[index][column];
        subDiagonal[row] = g / h;
        f += subDiagonal[row] * (*this)[index][row];
      }
      const float hh = f / (h + h);
      for (int row = 0; row <= last; ++row) {
        f = (*this)[index][row];
        g = subDiagonal[row] - hh * f;
        subDiagonal[row] = g;
        for (int column = 0; column <= row; ++column)
          (*this)[row][column] -= f * subDiagonal[column] + g * (*this)[index][column];
      }
    }
    diagonal[index] = h;
  }
  diagonal[0] = 0.0f;
  subDiagonal[0] = 0.0f;
  for (int index = 0; index < dimension; ++index) {
    const int last = index - 1;
    if (diagonal[index] != 0.0f) {
      for (int column = 0; column <= last; ++column) {
        float g = 0.0f;
        for (int inner = 0; inner <= last; ++inner)
          g += (*this)[index][inner] * (*this)[inner][column];
        for (int row = 0; row <= last; ++row)
          (*this)[row][column] -= g * (*this)[row][index];
      }
    }
    diagonal[index] = (*this)[index][index];
    (*this)[index][index] = 1.0f;
    for (int column = 0; column <= last; ++column) {
      (*this)[index][column] = 0.0f;
      (*this)[column][index] = 0.0f;
    }
  }
}

bool idMatX::QL(idVecX& diagonal, idVecX& subDiagonal) {
  const int dimension = diagonal.size;
  for (int index = 1; index < dimension; ++index)
    subDiagonal[index - 1] = subDiagonal[index];
  subDiagonal[dimension - 1] = 0.0f;
  for (int left = 0; left < dimension; ++left) {
    int iteration = 0;
    int middle;
    do {
      for (middle = left; middle < dimension - 1; ++middle) {
        const float threshold = std::fabs(diagonal[middle]) + std::fabs(diagonal[middle + 1]);
        if (std::fabs(subDiagonal[middle]) + threshold == threshold)
          break;
      }
      if (middle != left) {
        if (++iteration > 32)
          return false;
        float g = (diagonal[left + 1] - diagonal[left]) / (2.0f * subDiagonal[left]);
        float r = std::hypot(g, 1.0f);
        g = diagonal[middle] - diagonal[left] + subDiagonal[left] / (g + std::copysign(r, g));
        float sine = 1.0f;
        float cosine = 1.0f;
        float pValue = 0.0f;
        int index;
        for (index = middle - 1; index >= left; --index) {
          const float f = sine * subDiagonal[index];
          const float b = cosine * subDiagonal[index];
          r = std::hypot(f, g);
          subDiagonal[index + 1] = r;
          if (r == 0.0f) {
            diagonal[index + 1] -= pValue;
            subDiagonal[middle] = 0.0f;
            break;
          }
          sine = f / r;
          cosine = g / r;
          g = diagonal[index + 1] - pValue;
          r = (diagonal[index] - g) * sine + 2.0f * cosine * b;
          pValue = sine * r;
          diagonal[index + 1] = g + pValue;
          g = cosine * r - b;
          for (int row = 0; row < dimension; ++row) {
            const float value = (*this)[row][index + 1];
            (*this)[row][index + 1] = sine * (*this)[row][index] + cosine * value;
            (*this)[row][index] = cosine * (*this)[row][index] - sine * value;
          }
        }
        if (r == 0.0f && index >= left)
          continue;
        diagonal[left] -= pValue;
        subDiagonal[left] = g;
        subDiagonal[middle] = 0.0f;
      }
    } while (middle != left);
  }
  return true;
}

bool idMatX::Eigen_SolveSymmetric(idVecX& eigenValues) {
  if (numRows != numColumns || numRows <= 0)
    return false;
  idVecX subDiagonal(numRows);
  HouseholderReduction(eigenValues, subDiagonal);
  return QL(eigenValues, subDiagonal);
}

void idMatX::Eigen_SortIncreasing(idVecX& eigenValues) {
  for (int first = 0; first < eigenValues.size - 1; ++first) {
    int minimum = first;
    for (int index = first + 1; index < eigenValues.size; ++index)
      if (eigenValues[index] < eigenValues[minimum])
        minimum = index;
    if (minimum == first)
      continue;
    std::swap(eigenValues[first], eigenValues[minimum]);
    for (int row = 0; row < numRows; ++row)
      std::swap((*this)[row][first], (*this)[row][minimum]);
  }
}
