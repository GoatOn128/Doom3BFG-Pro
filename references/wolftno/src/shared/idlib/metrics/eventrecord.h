#pragma once

#include "shared/idlib/text/str.h"

enum logEventSeverity_t {
  LOG_SEV_TRASH = 0,
  LOG_SEV_DEBUG = 10,
  LOG_SEV_INFO = 20,
  LOG_SEV_NOTICE = 30,
  LOG_SEV_WARNING = 40,
  LOG_SEV_ERROR = 50,
  LOG_SEV_CRITICAL = 60,
  LOG_SEV_ALERT = 70,
  LOG_SEV_EMERGENCY = 80
};

class idForgeMetricRecord {
public:
  explicit idForgeMetricRecord(const idStr& recordName);
  virtual ~idForgeMetricRecord() = default;
  virtual void SerializeEntry() {}
  idStr name;
  idStr baseName;
  logEventSeverity_t severity;
};
static_assert(sizeof(idForgeMetricRecord) == 96, "metric record ABI drift");
