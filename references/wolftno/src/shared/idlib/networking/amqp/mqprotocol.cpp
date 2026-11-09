#include "shared/idlib/networking/amqp/mq.h"
bool idMQChannel::SendMethod(const idMQMethod& m) {
  idMQFrame frame;
  frame.type = AMQP_FRAME_METHOD;
  frame.channel = channel;
  m.Encode(frame.payload);
  return connection.Send(frame);
}
bool idMQChannel::Publish(const std::string& exchange, const std::string& routing, const void* body,
                          std::size_t size) {
  AMQPBasicPublish publish;
  publish.arguments.WriteU16(0);
  publish.arguments.WriteShortString(exchange);
  publish.arguments.WriteShortString(routing);
  publish.arguments.WriteU8(0);
  if (!SendMethod(publish))
    return false;
  idMQContentHeader header;
  header.bodySize = size;
  idMQFrame hf;
  hf.type = AMQP_FRAME_HEADER;
  hf.channel = channel;
  header.Encode(hf.payload);
  if (!connection.Send(hf))
    return false;
  idMQFrame bf;
  bf.type = AMQP_FRAME_BODY;
  bf.channel = channel;
  bf.payload.WriteBytes(body, size);
  return connection.Send(bf);
}
