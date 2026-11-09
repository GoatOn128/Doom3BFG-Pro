#include "shared/idlib/networking/amqp/mq.h"
bool idMQConnection::Connect(const amqpEndpoint_t& e) {
  Close();
  if (!tcp.Connect(e.host.c_str(), e.port))
    return false;
  const unsigned char protocol[8] = {'A', 'M', 'Q', 'P', 0, 0, 9, 1};
  return tcp.Write(protocol, sizeof(protocol));
}
void idMQConnection::Close() {
  tcp.Close();
  nextChannel = 1;
}
bool idMQConnection::Send(const idMQFrame& f) {
  const auto bytes = f.Encode();
  return tcp.Write(bytes.data(), bytes.size());
}
bool idMQConnection::Receive(idMQFrame& f, int timeout) {
  if (handler.Pop(f))
    return true;
  unsigned char buffer[4096];
  const int n = tcp.Read(buffer, sizeof(buffer), timeout);
  if (n <= 0)
    return false;
  handler.Feed(buffer, n);
  return handler.Pop(f);
}
