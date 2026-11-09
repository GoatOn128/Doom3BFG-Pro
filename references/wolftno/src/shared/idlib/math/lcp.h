#pragma once

#include "shared/idlib/math/vecx.h"

class idLCP {
public:
  idLCP() = default;
  virtual ~idLCP();
  virtual bool Solve(const idMatX& matrix, idVecX& x, const idVecX& b, const idVecX& low,
                     const idVecX& high, const int* boxIndex = nullptr,
                     const float* boxScale = nullptr) = 0;
  virtual void SetMaxIterations(int maximum) { maxIterations = maximum; }
  virtual int GetMaxIterations() const { return maxIterations; }
  static idLCP* AllocSquare();
  static idLCP* AllocSymmetric();

protected:
  int maxIterations{64};
};

class idLCP_Symmetric final : public idLCP {
public:
  ~idLCP_Symmetric() override;
  bool Solve(const idMatX& matrix, idVecX& x, const idVecX& rhs, const idVecX& low,
             const idVecX& high, const int* boxIndex = nullptr,
             const float* boxScale = nullptr) override;
  bool FactorClamped();
  void CalcAccelDelta(int direction);
  void Swap(int first, int second);
  void AddClamped(int row, bool useSolveCache);
  void RemoveClamped(int row);

  idMatX m;
  idVecX b, lo, hi, f, a, delta_f, delta_a;
  idMatX clamped;
  idVecX diagonal, solveCache1, solveCache2;
  int numUnbounded{}, numClamped{}, clampedChangeStart{};
  float** rowPtrs{};
  int* boxIndex{};
  float* boxScale{};
  int* side{};
  int* permuted{};
  bool padded{};
};

void Multiply_SIMD(float* destination, const float* left, const float* right, int count);
void MultiplyAdd_SIMD(float* destination, double constant, const float* source, int count);
float DotProduct_SIMD(const float* left, const float* right, int count);
void LowerTriangularSolve_SIMD(const idMatX& lower, float* x, const float* b, int n, int skip = 0);
void LowerTriangularSolveTranspose_SIMD(const idMatX& lower, float* x, const float* b, int n);
bool LDLT_Factor_SIMD(idMatX& matrix, idVecX& inverseDiagonal, int n);
