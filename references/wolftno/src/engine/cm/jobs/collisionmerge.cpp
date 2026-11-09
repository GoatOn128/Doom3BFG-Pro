#include "engine/cm/collisionquery.h"

#include <algorithm>
#include <cmath>

idVec3 idCollisionDetectionMerge::ClipVelocity(const idVec3& input, const idVec3& normal,
                                               float overbounce) {
  float backoff = input * normal;
  backoff *= backoff < 0.0f ? overbounce : 1.0f / overbounce;
  idVec3 output = input - normal * backoff;
  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(output[axis]) < 1.0e-5f) {
      output[axis] = 0.0f;
    }
  }
  return output;
}

idVec3 ClipVelocity(const idVec3& input, const idVec3& normal, float overbounce) {
  return idCollisionDetectionMerge::ClipVelocity(input, normal, overbounce);
}

void idCollisionDetectionMerge::InitSlideMoveState(slideMoveState_t& state, const idVec3& position,
                                                   const idVec3& velocity) {
  state.position = position;
  state.velocity = velocity;
  state.clipPlanes.clear();
  state.blocked = false;
}

bool idCollisionDetectionMerge::UpdateSlideMoveState(slideMoveState_t& state,
                                                     const trace_t& trace) {
  state.position = trace.endpos;
  if (trace.fraction >= 1.0f && !trace.startSolid) {
    return false;
  }
  state.blocked = true;
  if (trace.contact.normal.LengthSqr() > 0.0f) {
    state.clipPlanes.push_back(trace.contact.normal);
    state.velocity = ClipVelocity(state.velocity, trace.contact.normal);
    for (const idVec3& plane : state.clipPlanes) {
      if (state.velocity * plane < 0.0f) {
        state.velocity = ClipVelocity(state.velocity, plane);
      }
    }
  } else {
    state.velocity = {};
  }
  return state.velocity.LengthSqr() > 1.0e-8f;
}

trace_t idCollisionDetectionMerge::FinishSlideMoveState(const slideMoveState_t& state,
                                                        const trace_t& lastTrace) {
  trace_t result = lastTrace;
  result.endpos = state.position;
  result.allSolid = result.allSolid || (state.blocked && state.velocity.LengthSqr() <= 1.0e-8f);
  return result;
}

trace_t idCollisionDetectionMerge::MergeMotionResults(const trace_t& translation,
                                                      const trace_t& rotation) {
  trace_t result = translation.fraction <= rotation.fraction ? translation : rotation;
  result.startSolid = translation.startSolid || rotation.startSolid;
  result.allSolid = translation.allSolid || rotation.allSolid;
  return result;
}

trace_t idCollisionDetectionMerge::MergeStepMoveResults(const trace_t& forward,
                                                        const trace_t& stepUp,
                                                        const trace_t& stepDown) {
  const trace_t stepped = stepDown.fraction < 1.0f ? stepDown : stepUp;
  return forward.fraction >= stepped.fraction ? forward : stepped;
}

std::vector<trace_t>
idCollisionDetectionMerge::MergeMultiTraceResults(const std::vector<trace_t>& traces,
                                                  std::size_t maxHits) {
  std::vector<trace_t> result = traces;
  std::stable_sort(result.begin(), result.end(), [](const trace_t& first, const trace_t& second) {
    return first.fraction < second.fraction;
  });
  result.erase(std::remove_if(result.begin(), result.end(),
                              [](const trace_t& trace) {
                                return trace.fraction >= 1.0f && !trace.startSolid;
                              }),
               result.end());
  if (result.size() > maxHits) {
    result.resize(maxHits);
  }
  return result;
}

trace_t idCollisionDetectionMerge::MergeQueryResults(const std::vector<trace_t>& traces) {
  return MergeTraceResults(traces);
}

trace_t idCollisionDetectionMerge::MergeTraceResults(const std::vector<trace_t>& traces) {
  trace_t result;
  for (const trace_t& trace : traces) {
    if (trace.fraction < result.fraction || (trace.startSolid && !result.startSolid)) {
      result = trace;
    }
    result.startSolid = result.startSolid || trace.startSolid;
    result.allSolid = result.allSolid || trace.allSolid;
  }
  return result;
}

int idCollisionDetectionMerge::MergeContentsResults(const std::vector<int>& contents) {
  int result = 0;
  for (int value : contents) {
    result |= value;
  }
  return result;
}

contactsResult_t
idCollisionDetectionMerge::MergeContactsResults(const std::vector<contactsResult_t>& inputs) {
  contactsResult_t result;
  for (const contactsResult_t& input : inputs) {
    result.contacts.insert(result.contacts.end(), input.contacts.begin(), input.contacts.end());
  }
  return result;
}

clipResult_t idCollisionDetectionMerge::MergeClipResults(const std::vector<clipResult_t>& inputs) {
  clipResult_t result;
  for (const clipResult_t& input : inputs) {
    const std::uint32_t base = static_cast<std::uint32_t>(result.vertices.size());
    result.vertices.insert(result.vertices.end(), input.vertices.begin(), input.vertices.end());
    for (std::uint32_t index : input.indices) {
      result.indices.push_back(base + index);
    }
  }
  return result;
}
