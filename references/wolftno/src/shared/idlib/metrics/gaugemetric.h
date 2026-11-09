#pragma once

#include "eventrecord.h"

class idGaugeMetric : public idForgeMetricRecord {
public:
  idGaugeMetric(const idStr& shortName, const idStr& description);
  void Log(float value, bool skipPush = false);
  bool CheckPushState();
  void SerializeEntry() override {}
  float Variance() const { return count > 1 ? m2 / static_cast<float>(count - 1) : 0.0f; }
  int count;
  float min;
  float max;
  float mean;
  float mean2;
  float m2;
  int lastPushFrame;
  static int lastCheckedPushFrame;
  static int currentPushFrame;
  static std::uint64_t currentPushTime;
};
static_assert(sizeof(idGaugeMetric) == 128, "gauge metric ABI drift");
