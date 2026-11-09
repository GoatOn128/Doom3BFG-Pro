#pragma once

#include "shared/idlib/bv/bounds.h"

struct idVec5 {
  float x;
  float y;
  float z;
  float s;
  float t;

  idVec3 ToVec3() const { return {x, y, z}; }
};

class idWinding {
public:
  idWinding() = default;
  idWinding(const idWinding& other) { *this = other; }
  explicit idWinding(int points) { ReAllocate(points, false); }
  virtual ~idWinding();
  virtual void Clear();
  virtual bool ReAllocate(int points, bool keep);

  idWinding& operator=(const idWinding& other) {
    if (this == &other)
      return *this;
    if (other.numPoints <= 0 || ReAllocate(other.numPoints, false)) {
      for (int index = 0; index < other.numPoints; ++index)
        p[index] = other.p[index];
      numPoints = other.numPoints;
    } else {
      numPoints = 0;
    }
    return *this;
  }

  int GetNumPoints() const { return numPoints; }
  idVec5& operator[](int index) { return p[index]; }
  const idVec5& operator[](int index) const { return p[index]; }
  bool AddPoint(const idVec5& point);
  void ReverseSelf();
  void BaseForPlane(idVec3& normal, float distance, float radius = 8192.0f);
  bool ClipInPlace(const idPlane& plane, float epsilon, bool keepOn = false);
  idVec3 GetCenter() const;
  void GetBounds(idBounds& bounds) const;
  void AddToConvexHull(const idVec3& point, const idVec3& normal, float epsilon);
  bool IsHuge(float radius) const;
  bool PointInside(const idVec3& normal, const idVec3& point, float epsilon) const;
  void GetPlane(idPlane& plane) const;

protected:
  int numPoints = 0;
  idVec5* p = nullptr;
  int allocedSize = 0;
};

class idFixedWinding : public idWinding {
public:
  idFixedWinding();
  idFixedWinding(const idFixedWinding& other);
  idFixedWinding& operator=(const idFixedWinding& other);
  ~idFixedWinding() override;
  void Clear() override;
  bool ReAllocate(int points, bool keep) override;
  planeSide_t SplitInPlace(const idPlane& plane, float epsilon, idFixedWinding& back);

private:
  idVec5 data[64];
};

void CalculateSides(const idVec5* points, unsigned numPoints, const idPlane& plane, float epsilon,
                    float* distances, unsigned char* sides, int counts[3]);

static_assert(sizeof(idVec5) == 20, "idVec5 ABI drift");
static_assert(sizeof(idWinding) == 32, "idWinding ABI drift");
static_assert(sizeof(idFixedWinding) == 1312, "idFixedWinding ABI drift");
