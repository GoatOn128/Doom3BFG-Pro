#include "decltable.h"

#include <algorithm>

idDeclInfoTemplate<idDeclTrailBeam> idDeclTrailBeam::resourceList("trailbeam", "idDeclTrailBeam", "tbeam", false);

float idTrailParm::GetValue(float fraction) const {
  float interpolation = fraction;
  if (table != nullptr)
    interpolation = std::clamp(table->TableLookup(fraction, true), 0.0f, 1.0f);
  return (end - start) * interpolation + start;
}

idDeclTrailBeam::idDeclTrailBeam()
    : trailProperties{nullptr, {1, 1, 1, 1}, 2.0f, 0.1f, 0.75f, 32.0f, 4.0f, 1024.0f,
                      512.0f, {0, 0, nullptr}, {4.0f, 8.0f, nullptr}},
      quadProperties{nullptr, 0, 0} {
  resourceListPtr = &resourceList;
}

idDeclInfo* idDeclTrailBeam::GetDeclInfo() const { return &resourceList; }
