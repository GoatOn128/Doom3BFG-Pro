#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/sys/sys_time.h"
idForgeMetricsClient::idForgeMetricsClient() = default;
idForgeMetricsClient::~idForgeMetricsClient() {
  StopMessageSystem();
}
bool idForgeMetricsClient::StartMessageSystem(const amqpEndpoint_t& e) {
  running = true;
  session = sessions.Open("metrics", e);
  return true;
}
void idForgeMetricsClient::StopMessageSystem() {
  running = false;
  sessions.Clear();
  session.reset();
}
void idForgeMetricsClient::PreRun() {
  running = true;
}
void idForgeMetricsClient::OnThreadTerminate() {
  running = false;
}
void idForgeMetricsClient::SetHeaderInfo(const char* r, const char* m, const char* s) {
  header.runtimeSessionId = r ? r : "";
  header.matchId = m ? m : "";
  header.sessionState = s ? s : "";
}
bool idForgeMetricsClient::ReportMetric(const char* route, const idProtoMessage& message) {
  std::string data;
  if (!message.SerializeToString(&data))
    return false;
  return ReportMetric(route, data);
}
void idForgeMetricsClient::ReportMetric(const idProtoMessage* metric, const char* message,
                                        const char* metricType, const char* routingKey) {
  if (metric == nullptr)
    return;
  std::string data;
  if (!metric->SerializeToString(&data))
    return;
  std::lock_guard<std::mutex> lock(mutex);
  queue.push_back({routingKey ? routingKey : "", std::move(data),
                   message ? message : "", metricType ? metricType : "",
                   static_cast<std::uint64_t>(Sys_Microseconds())});
}
bool idForgeMetricsClient::ReportMetric(const char* route, const std::string& payload) {
  std::lock_guard<std::mutex> lock(mutex);
  queue.push_back({route ? route : "", payload, "", "",
                   static_cast<std::uint64_t>(Sys_Microseconds())});
  return true;
}
int idForgeMetricsClient::ThreadSlice() {
  int sent = 0;
  while (true) {
    metricEvent_t event;
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (queue.empty())
        break;
      event = std::move(queue.front());
      queue.pop_front();
    }
    if (session && !session->Publish("metrics", event.route, event.payload)) {
      std::lock_guard<std::mutex> lock(mutex);
      queue.push_front(std::move(event));
      break;
    }
    ++sent;
  }
  return sent;
}
std::size_t idForgeMetricsClient::Pending() const {
  std::lock_guard<std::mutex> lock(mutex);
  return queue.size();
}
idForgeMetricsClient* GetForgeMetricsClient() {
  static idForgeMetricsClient client;
  return &client;
}
