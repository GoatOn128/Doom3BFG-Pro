#include "shared/idlib/networking/amqp/mq.h"

// AMQP method identifiers are part of the wire protocol. Keep them explicit:
// deriving them from declaration order creates invalid frames after reordering.
#define DEFINE_MQ_METHOD(Name, ClassId, MethodId)                                                  \
  Name::Name() {                                                                                   \
    classId = ClassId;                                                                             \
    methodId = MethodId;                                                                           \
  }

DEFINE_MQ_METHOD(AMQPConnectionStart, 10, 10)
DEFINE_MQ_METHOD(AMQPConnectionStartOk, 10, 11)
DEFINE_MQ_METHOD(AMQPConnectionSecure, 10, 20)
DEFINE_MQ_METHOD(AMQPConnectionSecureOk, 10, 21)
DEFINE_MQ_METHOD(AMQPConnectionTune, 10, 30)
DEFINE_MQ_METHOD(AMQPConnectionTuneOk, 10, 31)
DEFINE_MQ_METHOD(AMQPConnectionOpen, 10, 40)
DEFINE_MQ_METHOD(AMQPConnectionOpenOk, 10, 41)
DEFINE_MQ_METHOD(AMQPConnectionRedirect, 10, 50)
DEFINE_MQ_METHOD(AMQPConnectionClose, 10, 60)
DEFINE_MQ_METHOD(AMQPConnectionCloseOk, 10, 61)

DEFINE_MQ_METHOD(AMQPChannelOpen, 20, 10)
DEFINE_MQ_METHOD(AMQPChannelOpenOk, 20, 11)
DEFINE_MQ_METHOD(AMQPChannelFlow, 20, 20)
DEFINE_MQ_METHOD(AMQPChannelFlowOk, 20, 21)
DEFINE_MQ_METHOD(AMQPChannelAlert, 20, 30)
DEFINE_MQ_METHOD(AMQPChannelClose, 20, 40)
DEFINE_MQ_METHOD(AMQPChannelCloseOk, 20, 41)

DEFINE_MQ_METHOD(AMQPExchangeDeclare, 40, 10)
DEFINE_MQ_METHOD(AMQPExchangeDeclareOk, 40, 11)
DEFINE_MQ_METHOD(AMQPExchangeDelete, 40, 20)
DEFINE_MQ_METHOD(AMQPExchangeDeleteOk, 40, 21)

DEFINE_MQ_METHOD(AMQPQueueDeclare, 50, 10)
DEFINE_MQ_METHOD(AMQPQueueDeclareOk, 50, 11)
DEFINE_MQ_METHOD(AMQPQueueBind, 50, 20)
DEFINE_MQ_METHOD(AMQPQueueBindOk, 50, 21)
DEFINE_MQ_METHOD(AMQPQueueUnbind, 50, 50)
DEFINE_MQ_METHOD(AMQPQueueUnbindOk, 50, 51)
DEFINE_MQ_METHOD(AMQPQueuePurge, 50, 30)
DEFINE_MQ_METHOD(AMQPQueuePurgeOk, 50, 31)
DEFINE_MQ_METHOD(AMQPQueueDelete, 50, 40)
DEFINE_MQ_METHOD(AMQPQueueDeleteOk, 50, 41)

DEFINE_MQ_METHOD(AMQPBasicQos, 60, 10)
DEFINE_MQ_METHOD(AMQPBasicQosOk, 60, 11)
DEFINE_MQ_METHOD(AMQPBasicConsume, 60, 20)
DEFINE_MQ_METHOD(AMQPBasicConsumeOk, 60, 21)
DEFINE_MQ_METHOD(AMQPBasicCancel, 60, 30)
DEFINE_MQ_METHOD(AMQPBasicCancelOk, 60, 31)
DEFINE_MQ_METHOD(AMQPBasicPublish, 60, 40)
DEFINE_MQ_METHOD(AMQPBasicReturn, 60, 50)
DEFINE_MQ_METHOD(AMQPBasicDeliver, 60, 60)
DEFINE_MQ_METHOD(AMQPBasicGet, 60, 70)
DEFINE_MQ_METHOD(AMQPBasicGetOk, 60, 71)
DEFINE_MQ_METHOD(AMQPBasicGetEmpty, 60, 72)
DEFINE_MQ_METHOD(AMQPBasicAck, 60, 80)
DEFINE_MQ_METHOD(AMQPBasicReject, 60, 90)
DEFINE_MQ_METHOD(AMQPBasicRecoverAsync, 60, 100)
DEFINE_MQ_METHOD(AMQPBasicRecover, 60, 110)
DEFINE_MQ_METHOD(AMQPBasicRecoverOk, 60, 111)

#undef DEFINE_MQ_METHOD
