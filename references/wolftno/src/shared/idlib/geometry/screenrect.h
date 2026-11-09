#pragma once

struct idScreenRect {
  int x1;
  int y1;
  int x2;
  int y2;
  void Clear();
  void Zero();
};

static_assert(sizeof(idScreenRect) == 16, "idScreenRect ABI drift");
