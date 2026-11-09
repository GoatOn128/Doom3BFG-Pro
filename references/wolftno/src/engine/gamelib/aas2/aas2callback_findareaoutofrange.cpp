#include "engine/gamelib/physics/clip.h"

bool idClip::TracePoint(trace_t& result, const idVec3& start,
                        const idVec3& end, int clipMask,
                        int passEntityNumber, const char* userName) {
  Translation(&result, start, end, nullptr, mat3_identity, clipMask,
              passEntityNumber, false, userName);
  return result.fraction < 1.0f;
}
