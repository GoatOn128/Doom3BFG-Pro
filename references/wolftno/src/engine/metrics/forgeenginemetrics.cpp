#include "engine/metrics/forgeenginemetrics.h"

#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/networking/protocols/protocols.h"

#include <algorithm>

const char* const idReliableMessageMetric::eventType = "ReliableMessageMetric";
const char* const idReliableMessageMetric::routingKey =
    "idtech5.events.metrics.reliable_message";
const char* const idSnapshotMetricRecord::eventType = "SnapshotMetric";
const char* const idSnapshotMetricRecord::routingKey =
    "idtech5.events.metrics.snapshot";
const char* const idStateRecord::eventType = "SessionStateRecord";
const char* const idStateRecord::routingKey =
    "idtech5.events.metrics.session_state";

idReliableMessageMetric::idReliableMessageMetric()
    : idForgeMetricRecord(idStr("ReliableMessage")), peerCount(0), peerNumber(-2),
      reliables{}, reliableCount(0) {}

idReliableMessageMetric::~idReliableMessageMetric() = default;

void idReliableMessageMetric::SerializeEntry() {
  idmetrics::ReliableMessageMetric metric;
  metric.peerCount = static_cast<std::uint32_t>(peerCount);
  metric.peerNumber = static_cast<std::uint32_t>(peerNumber);
  const int count = std::clamp(reliableCount, 0, MAX_RELIABLES);
  metric.reliables.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i)
    metric.reliables.push_back(static_cast<std::uint32_t>(reliables[i]));

  idForgeMetricsClient* client = GetForgeMetricsClient();
  metric.header = client->Header();
  client->ReportMetric(&metric, "Reliable Message Metric", eventType, routingKey);
}

idSnapshotMetricRecord::idSnapshotMetricRecord()
    : idForgeMetricRecord(idStr("SnapshotReport")), text() {}

idSnapshotMetricRecord::~idSnapshotMetricRecord() = default;

void idSnapshotMetricRecord::SerializeEntry() {
  idmetrics::SnapshotMetric metric;
  metric.report = text.data;
  idForgeMetricsClient* client = GetForgeMetricsClient();
  metric.header = client->Header();
  client->ReportMetric(&metric, "Snapshot Metric", eventType, routingKey);
}

idStateRecord::idStateRecord()
    : idForgeMetricRecord(idStr("stateRecord")), state("") {}

idStateRecord::~idStateRecord() = default;

void idStateRecord::SerializeEntry() {
  idmetrics::SessionStateRecord metric;
  metric.state = state.data;
  idForgeMetricsClient* client = GetForgeMetricsClient();
  metric.header = client->Header();
  client->ReportMetric(&metric, "Session State Record", eventType, routingKey);
}
