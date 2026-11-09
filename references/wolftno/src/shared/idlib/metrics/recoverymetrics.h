#pragma once

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/networking/amqp/mq.h"
#include "shared/idlib/networking/protocols/protocols.h"
#include "shared/idlib/sys/sys_time.h"

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

class idEventRecorderLight;
void RenderEventRecorderLightStartEventCopyString_Impl(const char* tag);
void RenderEventRecorderLightStartEvent_Impl(const char* tag);
void RenderEventRecorderLightStopEvent_Impl();
struct idEventRecorderLightTestAccess;

class idEventRecorderLight {
public:
  struct event {
    std::int64_t stamp;
    const char* tag;
  };

  enum { STRING_BUFFER_SIZE = 10240 };

  idEventRecorderLight() : enabled(false) {}
  idEventRecorderLight(const idEventRecorderLight&) = default;
  ~idEventRecorderLight() = default;
  idEventRecorderLight& operator=(const idEventRecorderLight&) = default;

  void StartCapture();
  void StopCapture();
  void Clear();
  void WriteReport(const char* filename, bool numbered, fsPath_t basePath);
  void StartEvent(const char* tag) {
    if (enabled) {
      const event ev = {GetTime(), tag};
      list.Append(ev);
    }
  }
  void StartEventCopyString(const char* tag) {
    if (enabled)
      StartEventCopyStringInternal(tag);
  }
  void StopEvent() {
    if (enabled) {
      const event ev = {GetTime(), nullptr};
      list.Append(ev);
    }
  }
  bool IsEnabled() { return enabled; }

private:
  std::int64_t GetTime() { return Sys_GetClockTicks(); }
  std::int64_t GetTicksPerSec() { return Sys_ClockTicksPerSecond(); }
  void StartEventCopyStringInternal(const char* tag);

  friend void RenderEventRecorderLightStartEventCopyString_Impl(const char* tag);
  friend void RenderEventRecorderLightStartEvent_Impl(const char* tag);
  friend void RenderEventRecorderLightStopEvent_Impl();
  friend struct idEventRecorderLightTestAccess;

  std::int64_t startStamp;
  bool enabled;
  idList<event, 5> list;
  idList<char, 5> strBuffer;
};

static_assert(sizeof(idEventRecorderLight::event) == 16,
              "event-recorder event ABI drift");
static_assert(sizeof(idEventRecorderLight) == 64,
              "event-recorder-light ABI drift");

class idForgeMetricsClient {
public:
  struct metricEvent_t {
    std::string route, payload, message, payloadType;
    std::uint64_t timestamp{};
  };
  idForgeMetricsClient();
  ~idForgeMetricsClient();
  bool StartMessageSystem(const amqpEndpoint_t& endpoint = {});
  void StopMessageSystem();
  void PreRun();
  void OnThreadTerminate();
  void SetHeaderInfo(const char* runtimeSession, const char* match, const char* state);
  bool ReportMetric(const char* route, const idProtoMessage&);
  void ReportMetric(const idProtoMessage* metric, const char* message,
                    const char* metricType, const char* routingKey);
  bool ReportMetric(const char* route, const std::string&);
  int ThreadSlice();
  std::size_t Pending() const;
  const idmetrics::SessionHeader& Header() const { return header; }

private:
  mutable std::mutex mutex;
  std::deque<metricEvent_t> queue;
  idmetrics::SessionHeader header;
  idMQSessionManager sessions;
  std::shared_ptr<idMQSession> session;
  bool running{};
};
idForgeMetricsClient* GetForgeMetricsClient();

class idPLog {
public:
  struct entry_t {
    idStr name;
    std::uint64_t start{}, ticks{};
    int depth{};
  };
  idPLog();
  ~idPLog();
  void Clear();
  void Free();
  void EnsureEntries(std::size_t);
  std::size_t Begin(const char* name, int depth = 0);
  void End(std::size_t);
  std::uint64_t GetOverHeadTicks() const;
  idStr ShowUniqueEntries() const;
  void ShowUniqueEntries(float thresholdMS,std::int64_t) const {
    std::uint64_t total=0;
    for(const entry_t& entry : entries) total+=entry.ticks;
    if(static_cast<float>(total)*0.001f>=thresholdMS) {
      const idStr output=ShowUniqueEntries();
      idLib::Printf("%s",output.c_str());
    }
  }
  idStr BuildLogData() const;
  idStr ShowCallGraph() const;
  void ShowCallGraph(float thresholdMS,std::int64_t) const {
    std::uint64_t total=0;
    for(const entry_t& entry : entries) total+=entry.ticks;
    if(static_cast<float>(total)*0.001f>=thresholdMS) {
      const idStr output=ShowCallGraph();
      idLib::Printf("%s",output.c_str());
    }
  }
  void Enable(std::uint64_t mask = ~0ull) {
    enabled = true;
    groupMask = mask;
  }
  void Disable() { enabled = false; }
  std::uint64_t GroupMask() const { return groupMask; }

private:
  bool enabled{true};
  std::uint64_t groupMask{0};
  std::vector<entry_t> entries;
};

class idPSyncLog {
public:
  struct sync_t {
    std::uint64_t time;
    std::uint32_t thread;
    idStr label;
  };
  idPSyncLog();
  ~idPSyncLog();
  void Add(const char*);
  void Clear();
  std::vector<sync_t> Snapshot() const;

private:
  mutable std::mutex mutex;
  std::vector<sync_t> events;
};
