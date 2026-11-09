#pragma once

#include <algorithm>
#include <cstddef>

class idSpatialVec {
public:
  idSpatialVec() = default;
  explicit idSpatialVec(int count) { SetSize(count); }
  idSpatialVec(const idSpatialVec& other);
  idSpatialVec& operator=(const idSpatialVec& other);
  ~idSpatialVec();

  void SetSize(int count);
  void ChangeSize(int count);
  void SetData(int count, float* data);
  int GetSize() const { return size; }
  float& operator[](int index) { return p[index]; }
  const float& operator[](int index) const { return p[index]; }

protected:
  friend class idAFTree;
  friend class idAFBody;
  friend class idSpatialMat;

  short size = 0;
  short allocated = 0;
  float* p = nullptr;
};

class idStaticSpatialVec : public idSpatialVec {
public:
#if defined(WOLFTNO_RETAIL_AFBODY_SPATIAL_CTOR_ABI)
  __forceinline idStaticSpatialVec() {
    SetData(0, data);
  }
#elif defined(WOLFTNO_RETAIL_AFTREE_SPATIAL_CTOR_ABI)
  __forceinline idStaticSpatialVec() {
    size = 6;
    allocated = -8;
    p = data;
    data[6] = 0.0f;
    data[7] = 0.0f;
  }
#else
  idStaticSpatialVec();
#endif
  ~idStaticSpatialVec();

private:
  friend class idAFTree;
  friend class idAFBody;

#if defined(WOLFTNO_RETAIL_AFBODY_SPATIAL_CTOR_ABI)
  float data[8];
#else
  float data[8]{};
#endif
};

class idSpatialMat {
public:
  idSpatialMat() = default;
  idSpatialMat(int rows, int columns) { SetSize(rows, columns); }
  idSpatialMat(const idSpatialMat& other);
  idSpatialMat& operator=(const idSpatialMat& other);
  ~idSpatialMat();

  void SetSize(int rows, int columns);
  void SetData(int rows, int columns, float* data);
  int GetNumRows() const { return numRows; }
  int GetNumColumns() const { return numColumns; }
  float* operator[](int row) { return mat + row * numColumns; }
  const float* operator[](int row) const { return mat + row * numColumns; }

  void Subtract(const idSpatialMat& other);
  void Multiply(idSpatialMat& destination, const idSpatialMat& other) const;
  void TransposeMultiply(idSpatialMat& destination, const idSpatialMat& other) const;
  void Transpose(idSpatialMat& destination) const;
  void Multiply(idSpatialVec& destination, const idSpatialVec& vector) const;
  void MultiplyAdd(idSpatialVec& destination, const idSpatialVec& vector) const;
  void MultiplySub(idSpatialVec& destination, const idSpatialVec& vector) const;
  void TransposeMultiplyAdd(idSpatialVec& destination, const idSpatialVec& vector) const;
  void TransposeMultiplySub(idSpatialVec& destination, const idSpatialVec& vector) const;
  bool Inverse(idSpatialMat& destination) const;
  const idSpatialVec SubSpatialVec(int row) const;
  bool Inverse1x1(idSpatialMat& destination) const;
  bool Inverse2x2(idSpatialMat& destination) const;
  bool Inverse3x3(idSpatialMat& destination) const;
  bool Inverse4x4(idSpatialMat& destination) const;
  bool Inverse5x5(idSpatialMat& destination) const;
  bool Inverse6x6(idSpatialMat& destination) const;

private:
  friend class idAFTree;

  bool Inverse(int dimension, idSpatialMat& destination) const;
  int numRows = 0;
  int numColumns = 0;
  int allocatedRows = 0;
  float* mat = nullptr;
};

static_assert(sizeof(idSpatialMat) == 24, "idSpatialMat ABI drift");
static_assert(sizeof(idSpatialVec) == 16, "idSpatialVec ABI drift");
static_assert(sizeof(idStaticSpatialVec) == 48, "idStaticSpatialVec ABI drift");
