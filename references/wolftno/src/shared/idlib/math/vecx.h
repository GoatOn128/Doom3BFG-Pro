#pragma once

struct idVecX {
  int size;
  int alloced;
  float* p;

  idVecX() : size(0), alloced(0), p(nullptr) {}
  explicit idVecX(int count) : idVecX() { SetSize(count); }
  idVecX(const idVecX& other);
  idVecX& operator=(const idVecX& other);
  ~idVecX();
  void SetSize(int count);
  float& operator[](int index) { return p[index]; }
  const float& operator[](int index) const { return p[index]; }

  static float temp[1028];
  static float* tempPtr;
  static int tempIndex;
};

struct idMatX {
  int numRows;
  int numColumns;
  int alloced;
  float* mat;

  idMatX() : numRows(0), numColumns(0), alloced(0), mat(nullptr) {}
  idMatX(int rows, int columns) : idMatX() { SetSize(rows, columns); }
  idMatX(const idMatX& other);
  idMatX& operator=(const idMatX& other);
  ~idMatX();

  void SetData(int rows, int columns, float* data);
  void SetSize(int rows, int columns);
  float* operator[](int row) { return mat + row * numColumns; }
  const float* operator[](int row) const { return mat + row * numColumns; }
  void CopyLowerToUpperTriangle();
  bool QL(idVecX& diagonal, idVecX& subDiagonal);
  void HouseholderReduction(idVecX& diagonal, idVecX& subDiagonal);
  bool Eigen_SolveSymmetric(idVecX& eigenValues);
  void Eigen_SortIncreasing(idVecX& eigenValues);
};

static_assert(sizeof(idVecX) == 16, "idVecX ABI drift");
static_assert(sizeof(idMatX) == 24, "idMatX ABI drift");
