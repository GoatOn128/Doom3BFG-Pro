#include "shared/idlib/math/lcp.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

void Multiply_SIMD(float* dst, const float* a, const float* b, int count) {
  for (int i = 0; i < count; ++i)
    dst[i] = a[i] * b[i];
}
void MultiplyAdd_SIMD(float* dst, double constant, const float* src, int count) {
  const float c = static_cast<float>(constant);
  for (int i = 0; i < count; ++i)
    dst[i] += c * src[i];
}
float DotProduct_SIMD(const float* a, const float* b, int count) {
  double sum = 0.0;
  for (int i = 0; i < count; ++i)
    sum += static_cast<double>(a[i]) * b[i];
  return static_cast<float>(sum);
}

void LowerTriangularSolve_SIMD(const idMatX& lower, float* x, const float* b, int n, int skip) {
  for (int i = 0; i < skip; ++i)
    x[i] = b[i];
  for (int i = skip; i < n; ++i) {
    double sum = b[i];
    for (int j = 0; j < i; ++j)
      sum -= lower[i][j] * x[j];
    x[i] = static_cast<float>(sum);
  }
}
void LowerTriangularSolveTranspose_SIMD(const idMatX& lower, float* x, const float* b, int n) {
  for (int i = n - 1; i >= 0; --i) {
    double sum = b[i];
    for (int j = i + 1; j < n; ++j)
      sum -= lower[j][i] * x[j];
    x[i] = static_cast<float>(sum);
  }
}
bool LDLT_Factor_SIMD(idMatX& matrix, idVecX& inverseDiagonal, int n) {
  if (matrix.numRows < n || matrix.numColumns < n)
    return false;
  inverseDiagonal.SetSize(n);
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < i; ++j) {
      double value = matrix[i][j];
      for (int k = 0; k < j; ++k)
        value -= matrix[i][k] * matrix[j][k] / inverseDiagonal[k];
      matrix[i][j] = static_cast<float>(value * inverseDiagonal[j]);
    }
    double diagonal = matrix[i][i];
    for (int k = 0; k < i; ++k)
      diagonal -= static_cast<double>(matrix[i][k]) * matrix[i][k] / inverseDiagonal[k];
    if (std::fabs(diagonal) < 1.0e-12)
      return false;
    inverseDiagonal[i] = static_cast<float>(1.0 / diagonal);
  }
  return true;
}

idLCP::~idLCP() = default;
idLCP_Symmetric::~idLCP_Symmetric() {
  delete[] rowPtrs;
  delete[] boxIndex;
  delete[] boxScale;
  delete[] side;
  delete[] permuted;
}

bool idLCP_Symmetric::FactorClamped() {
  if (numClamped <= 0)
    return true;
  clamped.SetSize(numClamped, numClamped);
  for (int i = 0; i < numClamped; ++i)
    for (int j = 0; j < numClamped; ++j)
      clamped[i][j] = m[i][j];
  return LDLT_Factor_SIMD(clamped, diagonal, numClamped);
}

void idLCP_Symmetric::CalcAccelDelta(int direction) {
  delta_a.SetSize(m.numRows);
  for (int i = 0; i < m.numRows; ++i)
    delta_a[i] = m[i][direction];
  if (direction >= 0 && direction < delta_a.size)
    delta_a[direction] -= 1.0f;
}

void idLCP_Symmetric::Swap(int first, int second) {
  if (first == second || first < 0 || second < 0 || first >= m.numRows || second >= m.numRows)
    return;
  for (int c = 0; c < m.numColumns; ++c)
    std::swap(m[first][c], m[second][c]);
  for (int r = 0; r < m.numRows; ++r)
    std::swap(m[r][first], m[r][second]);
  if (first < b.size && second < b.size) {
    std::swap(b[first], b[second]);
    std::swap(lo[first], lo[second]);
    std::swap(hi[first], hi[second]);
    std::swap(f[first], f[second]);
    std::swap(a[first], a[second]);
  }
  if (permuted)
    std::swap(permuted[first], permuted[second]);
  if (side)
    std::swap(side[first], side[second]);
}

void idLCP_Symmetric::AddClamped(int row, bool) {
  if (row < 0 || row >= m.numRows || numClamped >= m.numRows)
    return;
  Swap(row, numClamped);
  ++numClamped;
  FactorClamped();
}
void idLCP_Symmetric::RemoveClamped(int row) {
  if (row < 0 || row >= numClamped)
    return;
  Swap(row, numClamped - 1);
  --numClamped;
  FactorClamped();
}

bool idLCP_Symmetric::Solve(const idMatX& matrix, idVecX& x, const idVecX& rhs, const idVecX& low,
                            const idVecX& high, const int* sourceBoxIndex,
                            const float* sourceBoxScale) {
  const int n = matrix.numRows;
  if (n <= 0 || matrix.numColumns != n || rhs.size != n || low.size != n || high.size != n)
    return false;
  m = matrix;
  b = rhs;
  lo = low;
  hi = high;
  x.SetSize(n);
  f.SetSize(n);
  a.SetSize(n);
  delta_f.SetSize(n);
  delta_a.SetSize(n);
  delete[] boxIndex;
  delete[] boxScale;
  delete[] side;
  delete[] permuted;
  delete[] rowPtrs;
  boxIndex = new int[n];
  boxScale = new float[n];
  side = new int[n]{};
  permuted = new int[n];
  rowPtrs = new float*[n];
  for (int i = 0; i < n; ++i) {
    boxIndex[i] = sourceBoxIndex ? sourceBoxIndex[i] : -1;
    boxScale[i] = sourceBoxScale ? sourceBoxScale[i] : 1.0f;
    permuted[i] = i;
    rowPtrs[i] = m[i];
    x[i] = std::clamp(0.0f, low[i], high[i]);
  }
  numUnbounded = 0;
  for (int i = 0; i < n; ++i)
    if (!std::isfinite(low[i]) && !std::isfinite(high[i]))
      ++numUnbounded;
  const int iterations = maxIterations > 0 ? maxIterations : 64;
  bool converged = false;
  for (int iteration = 0; iteration < iterations; ++iteration) {
    float largestChange = 0.0f;
    for (int i = 0; i < n; ++i) {
      float lower = low[i], upper = high[i];
      if (boxIndex[i] >= 0 && boxIndex[i] < n) {
        const float scale = std::fabs(x[boxIndex[i]] * (sourceBoxScale ? sourceBoxScale[i] : 1.0f));
        lower = -std::fabs(low[i]) * scale;
        upper = std::fabs(high[i]) * scale;
      }
      const float diagonalValue = matrix[i][i];
      if (std::fabs(diagonalValue) < 1.0e-12f)
        continue;
      double residual = rhs[i];
      for (int j = 0; j < n; ++j)
        residual -= static_cast<double>(matrix[i][j]) * x[j];
      const float old = x[i];
      x[i] = std::clamp(old + static_cast<float>(residual / diagonalValue), lower, upper);
      largestChange = std::max(largestChange, std::fabs(x[i] - old));
    }
    if (largestChange < 1.0e-6f) {
      converged = true;
      break;
    }
  }
  numClamped = 0;
  for (int i = 0; i < n; ++i) {
    double value = -rhs[i];
    for (int j = 0; j < n; ++j)
      value += static_cast<double>(matrix[i][j]) * x[j];
    a[i] = static_cast<float>(value);
    f[i] = x[i];
    if (x[i] > low[i] + 1.0e-5f && x[i] < high[i] - 1.0e-5f)
      ++numClamped;
  }
  return converged;
}

class idLCP_Square final : public idLCP {
public:
  bool Solve(const idMatX& matrix, idVecX& x, const idVecX& rhs, const idVecX& low,
             const idVecX& high, const int* box, const float* scale) override {
    idLCP_Symmetric solver;
    solver.SetMaxIterations(maxIterations);
    return solver.Solve(matrix, x, rhs, low, high, box, scale);
  }
};

idLCP* idLCP::AllocSquare() {
  return new idLCP_Square;
}
idLCP* idLCP::AllocSymmetric() {
  return new idLCP_Symmetric;
}
