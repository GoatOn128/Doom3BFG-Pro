#include "eventrecord.h"

idForgeMetricRecord::idForgeMetricRecord(const idStr& recordName)
    : name(recordName), baseName(recordName), severity(LOG_SEV_TRASH) {}
