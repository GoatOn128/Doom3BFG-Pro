#pragma once

#include "shared/idlib/networking/networking.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern idCVar mq_enable;

class idMQBuffer {
public:
  idMQBuffer() = default;
  explicit idMQBuffer(const void*, std::size_t);
  void Clear();
  void ResetRead() { cursor = 0; }
  std::size_t Size() const { return data.size(); }
  const std::uint8_t* Data() const { return data.data(); }
  void WriteU8(std::uint8_t);
  void WriteU16(std::uint16_t);
  void WriteU32(std::uint32_t);
  void WriteU64(std::uint64_t);
  void WriteShortString(const std::string&);
  void WriteLongString(const std::string&);
  void WriteBytes(const void*, std::size_t);
  bool ReadU8(std::uint8_t&);
  bool ReadU16(std::uint16_t&);
  bool ReadU32(std::uint32_t&);
  bool ReadU64(std::uint64_t&);
  bool ReadShortString(std::string&);
  bool ReadLongString(std::string&);
  bool ReadBytes(void*, std::size_t);
  std::vector<std::uint8_t> data;
  std::size_t cursor{};
};

class idMQMethod {
public:
  idMQMethod(std::uint16_t classId = 0, std::uint16_t methodId = 0)
      : classId(classId), methodId(methodId) {}
  virtual ~idMQMethod() = default;
  virtual void Encode(idMQBuffer&) const;
  virtual bool Decode(idMQBuffer&);
  std::uint32_t MethodKey() const { return (std::uint32_t(classId) << 16) | methodId; }
  std::uint16_t classId{}, methodId{};
  idMQBuffer arguments;
};
class idMQContentHeader {
public:
  std::uint16_t classId{60}, weight{};
  std::uint64_t bodySize{};
  std::uint16_t propertyFlags{};
  idMQBuffer properties;
  void Encode(idMQBuffer&) const;
  bool Decode(idMQBuffer&);
};
class idMQTCP {
public:
  bool Connect(const char*, std::uint16_t, int timeoutMs = 5000);
  void Close();
  bool Write(const void*, std::size_t, int timeoutMs = 5000);
  int Read(void*, std::size_t, int timeoutMs = 5000);
  bool IsOpen() const { return socket.IsOpen(); }

private:
  idTCP socket;
};

enum amqpFrameType_t : std::uint8_t {
  AMQP_FRAME_METHOD = 1,
  AMQP_FRAME_HEADER = 2,
  AMQP_FRAME_BODY = 3,
  AMQP_FRAME_HEARTBEAT = 8
};
class idMQFrame {
public:
  amqpFrameType_t type{AMQP_FRAME_METHOD};
  std::uint16_t channel{};
  idMQBuffer payload;
  std::vector<std::uint8_t> Encode() const;
  static bool Decode(const void*, std::size_t, idMQFrame&, std::size_t& consumed);
};
class idMQFrameHandler {
public:
  void Feed(const void*, std::size_t);
  bool Pop(idMQFrame&);

private:
  std::vector<std::uint8_t> pending;
  std::deque<idMQFrame> frames;
};
struct amqpEndpoint_t {
  std::string host{"localhost"}, virtualHost{"/"}, user{"guest"}, password{"guest"};
  std::uint16_t port{5672};
  static amqpEndpoint_t Parse(const std::string&);
};

class idMQConnection {
public:
  bool Connect(const amqpEndpoint_t&);
  void Close();
  bool Send(const idMQFrame&);
  bool Receive(idMQFrame&, int timeoutMs = 1000);
  bool IsOpen() const { return tcp.IsOpen(); }
  std::uint16_t AllocateChannel() { return nextChannel++; }

private:
  idMQTCP tcp;
  idMQFrameHandler handler;
  std::uint16_t nextChannel{1};
};
class idMQChannel {
public:
  idMQChannel(idMQConnection& connection, std::uint16_t channel)
      : connection(connection), channel(channel) {}
  bool SendMethod(const idMQMethod&);
  bool Publish(const std::string& exchange, const std::string& routingKey, const void* body,
               std::size_t size);
  std::uint16_t Number() const { return channel; }

private:
  idMQConnection& connection;
  std::uint16_t channel;
};

class idMQCommand {
public:
  std::string name;
  std::vector<std::string> arguments;
};
class idMQCommandBuilder {
public:
  idMQCommandBuilder& Name(const std::string&);
  idMQCommandBuilder& Arg(const std::string&);
  idMQCommand Build() const { return command; }

private:
  idMQCommand command;
};
class idMQSession {
public:
  explicit idMQSession(std::shared_ptr<idMQConnection>);
  bool Publish(const std::string&, const std::string&, const std::string&);
  bool Poll(idMQFrame&, int timeout = 0);

private:
  std::shared_ptr<idMQConnection> connection;
  std::unique_ptr<idMQChannel> channel;
};
class idMQSessionManager {
public:
  std::shared_ptr<idMQSession> Open(const std::string& name, const amqpEndpoint_t&);
  void Close(const std::string&);
  void Clear();

private:
  std::mutex mutex;
  std::map<std::string, std::shared_ptr<idMQSession>> sessions;
};
class idMQClientThread final : public idSysThread {
public:
  idMQClientThread();
  ~idMQClientThread() override;
  int Run() override;
  void Enqueue(idMQCommand);
  bool TryDequeue(idMQCommand&);

private:
  std::mutex mutex;
  std::deque<idMQCommand> commands;
};

#define MQ_METHOD_TYPES(X)                                                                         \
  X(AMQPConnectionStart)                                                                           \
  X(AMQPConnectionStartOk) X(AMQPConnectionSecure) X(AMQPConnectionSecureOk) X(                    \
      AMQPConnectionTune) X(AMQPConnectionTuneOk) X(AMQPConnectionOpen) X(AMQPConnectionOpenOk)    \
      X(AMQPConnectionRedirect) X(AMQPConnectionClose) X(AMQPConnectionCloseOk) X(AMQPChannelOpen) \
          X(AMQPChannelOpenOk) X(AMQPChannelFlow) X(AMQPChannelFlowOk) X(AMQPChannelAlert)         \
              X(AMQPChannelClose) X(AMQPChannelCloseOk) X(AMQPExchangeDeclare)                     \
                  X(AMQPExchangeDeclareOk) X(AMQPExchangeDelete) X(AMQPExchangeDeleteOk) X(        \
                      AMQPQueueDeclare) X(AMQPQueueDeclareOk) X(AMQPQueueBind) X(AMQPQueueBindOk)  \
                      X(AMQPQueueUnbind) X(AMQPQueueUnbindOk) X(AMQPQueuePurge)                    \
                          X(AMQPQueuePurgeOk) X(AMQPQueueDelete) X(AMQPQueueDeleteOk)              \
                              X(AMQPBasicQos) X(AMQPBasicQosOk) X(AMQPBasicConsume)                \
                                  X(AMQPBasicConsumeOk) X(AMQPBasicCancel) X(AMQPBasicCancelOk)    \
                                      X(AMQPBasicPublish) X(AMQPBasicReturn) X(AMQPBasicDeliver)   \
                                          X(AMQPBasicGet) X(AMQPBasicGetOk) X(AMQPBasicGetEmpty)   \
                                              X(AMQPBasicAck) X(AMQPBasicReject)                   \
                                                  X(AMQPBasicRecoverAsync) X(AMQPBasicRecover)     \
                                                      X(AMQPBasicRecoverOk)
#define DECLARE_MQ_METHOD(Name)                                                                    \
  struct Name : idMQMethod {                                                                       \
    Name();                                                                                        \
  };
MQ_METHOD_TYPES(DECLARE_MQ_METHOD)
#undef DECLARE_MQ_METHOD
