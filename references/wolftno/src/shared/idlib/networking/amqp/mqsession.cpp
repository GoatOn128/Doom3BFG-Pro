#include "shared/idlib/networking/amqp/mq.h"
idMQSession::idMQSession(std::shared_ptr<idMQConnection> c) : connection(std::move(c)) {
  channel = std::make_unique<idMQChannel>(*connection, connection->AllocateChannel());
}
bool idMQSession::Publish(const std::string& e, const std::string& r, const std::string& b) {
  return channel->Publish(e, r, b.data(), b.size());
}
bool idMQSession::Poll(idMQFrame& f, int t) {
  return connection->Receive(f, t);
}
std::shared_ptr<idMQSession> idMQSessionManager::Open(const std::string& name,
                                                      const amqpEndpoint_t& e) {
  std::lock_guard<std::mutex> lock(mutex);
  auto it = sessions.find(name);
  if (it != sessions.end())
    return it->second;
  auto connection = std::make_shared<idMQConnection>();
  if (!connection->Connect(e))
    return {};
  auto session = std::make_shared<idMQSession>(connection);
  sessions[name] = session;
  return session;
}
void idMQSessionManager::Close(const std::string& name) {
  std::lock_guard<std::mutex> lock(mutex);
  sessions.erase(name);
}
void idMQSessionManager::Clear() {
  std::lock_guard<std::mutex> lock(mutex);
  sessions.clear();
}
