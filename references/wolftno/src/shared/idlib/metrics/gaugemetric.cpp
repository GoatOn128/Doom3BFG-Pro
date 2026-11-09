#include "gaugemetric.h"

#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

int idGaugeMetric::lastCheckedPushFrame = -1;
int idGaugeMetric::currentPushFrame = 0;
std::uint64_t idGaugeMetric::currentPushTime = 0;

idGaugeMetric::idGaugeMetric(const idStr& shortName, const idStr& description)
    : idForgeMetricRecord(shortName), count(0), min(0), max(0), mean(0), mean2(0), m2(0),
      lastPushFrame(0) {
  baseName = description;
}

bool idGaugeMetric::CheckPushState() {
  constexpr std::uint64_t heartbeatMilliseconds = 60000;
  const std::uint64_t now = Sys_TimeSinceUnixEpoch();
  if (idLib::frameNumber != lastCheckedPushFrame) {
    lastCheckedPushFrame = idLib::frameNumber;
    if (now - currentPushTime > heartbeatMilliseconds) {
      currentPushTime = now;
      currentPushFrame = idLib::frameNumber;
    }
  }
  if (currentPushFrame <= lastPushFrame || count == 0)
    return false;
  SerializeEntry();
  lastPushFrame = currentPushFrame;
  count = 0;
  min = max = mean = mean2 = m2 = 0.0f;
  return true;
}

void idGaugeMetric::Log(float value, bool skipPush) {
  if (count == 0)
    min = max = value;
  else {
    if (value < min)
      min = value;
    if (value > max)
      max = value;
  }
  const int nextCount = count + 1;
  const float delta = value - mean;
  mean2 += value * value;
  mean += delta / static_cast<float>(nextCount);
  m2 += (value - mean) * delta;
  count = nextCount;
  if (!skipPush)
    CheckPushState();
}
