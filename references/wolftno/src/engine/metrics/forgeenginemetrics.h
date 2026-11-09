#pragma once

#include "shared/idlib/metrics/eventrecord.h"

class idReliableMessageMetric final : public idForgeMetricRecord {
public:
  static constexpr int MAX_RELIABLES = 64;
  static const char* const eventType;
  static const char* const routingKey;

  idReliableMessageMetric();
  ~idReliableMessageMetric() override;
  void SerializeEntry() override;

  int peerCount;
  int peerNumber;
  int reliables[MAX_RELIABLES];
  int reliableCount;
};

class idSnapshotMetricRecord final : public idForgeMetricRecord {
public:
  static const char* const eventType;
  static const char* const routingKey;

  idSnapshotMetricRecord();
  ~idSnapshotMetricRecord() override;
  void SerializeEntry() override;

  idStr text;
};

class idStateRecord final : public idForgeMetricRecord {
public:
  static const char* const eventType;
  static const char* const routingKey;

  idStateRecord();
  ~idStateRecord() override;
  void SerializeEntry() override;

  idStr state;
};

static_assert(sizeof(idReliableMessageMetric) == 368, "reliable metric ABI drift");
static_assert(sizeof(idSnapshotMetricRecord) == 136, "snapshot metric ABI drift");
static_assert(sizeof(idStateRecord) == 136, "state metric ABI drift");
