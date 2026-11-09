#pragma once

#include "engine/network/lightweightcompression.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/metrics/gaugemetric.h"
#include "shared/idlib/networking/bitmsg.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

template <int MaxItems, int MaxData> class idDataQueue {
public:
  struct msgItem_t {
    int sequence;
    int length;
    int dataOffset;
  };

  idDataQueue() = default;
  idDataQueue(const idDataQueue& other) { *this = other; }
  idDataQueue& operator=(const idDataQueue& other) {
    if (this == &other) {
      return *this;
    }
    items.Clear();
    for (int i = 0; i < other.items.Num(); ++i) {
      items.Append(other.items[i]);
    }
    dataLength = other.dataLength;
    std::memcpy(data, other.data, sizeof(data));
    return *this;
  }

  bool Append(int sequence,
              const std::uint8_t* first,
              int firstLength,
              const std::uint8_t* second = nullptr,
              int secondLength = 0) {
    if (items.Num() == MaxItems || dataLength + firstLength + secondLength >= MaxData) {
      return false;
    }
    msgItem_t& item = *items.Alloc();
    item.sequence = sequence;
    item.length = firstLength + secondLength;
    item.dataOffset = dataLength;
    if (firstLength > 0) {
      std::memmove(data + dataLength, first, static_cast<std::size_t>(firstLength));
      dataLength += firstLength;
    }
    if (secondLength > 0) {
      std::memmove(data + dataLength, second, static_cast<std::size_t>(secondLength));
      dataLength += secondLength;
    }
    return true;
  }

  void RemoveOlderThan(int sequence) {
    int bytesToRemove = 0;
    while (items.Num() > 0 && items[0].sequence < sequence) {
      bytesToRemove += items[0].length;
      for (int i = 1; i < items.Num(); ++i) {
        items[i - 1] = items[i];
      }
      --items.num;
    }
    if (bytesToRemove < dataLength) {
      if (bytesToRemove > 0) {
        std::memmove(data, data + bytesToRemove,
                     static_cast<std::size_t>(dataLength - bytesToRemove));
        dataLength -= bytesToRemove;
      }
    } else {
      dataLength = 0;
    }
    int offset = 0;
    for (int i = 0; i < items.Num(); ++i) {
      items[i].dataOffset = offset;
      offset += items[i].length;
    }
  }

  void Clear() {
    items.Clear();
    dataLength = 0;
  }
  int Num() const { return items.Num(); }
  int GetDataLength() const { return dataLength; }
  int ItemSequence(int index) const { return items[index].sequence; }
  int ItemLength(int index) const { return items[index].length; }
  const std::uint8_t* ItemData(int index) const { return data + items[index].dataOffset; }

  idStaticList<msgItem_t, MaxItems> items;
  int dataLength{};
  std::uint8_t data[MaxData]{};
};

class idPacketProcessor {
public:
  using sessionId_t = std::uint16_t;

  static constexpr int RETURN_TYPE_NONE = 0;
  static constexpr int RETURN_TYPE_OOB = 1;
  static constexpr int RETURN_TYPE_INBAND = 2;
  static constexpr sessionId_t SESSION_ID_INVALID = 0;
  static constexpr sessionId_t SESSION_ID_CONNECTIONLESS_PARTY = 1;
  static constexpr sessionId_t SESSION_ID_CONNECTIONLESS_GAME = 2;
  static constexpr sessionId_t SESSION_ID_CONNECTIONLESS_GAME_STATE = 3;
  static constexpr int BANDWIDTH_AVERAGE_PERIOD = 250;
  static constexpr int MAX_MSG_SIZE = 8000;
  static constexpr int MAX_FINAL_PACKET_SIZE = 1200;
  static constexpr int MAX_RELIABLE_QUEUE = 63;
  static constexpr int MAX_PACKET_SIZE = MAX_FINAL_PACKET_SIZE - 6 - 2;
  static constexpr int MAX_OOB_MSG_SIZE = MAX_PACKET_SIZE - 1;

  idPacketProcessor();
  void Reset();

  bool CanSendMoreData() const;
  void UpdateOutgoingRate(int time, int size);
  void UpdateIncomingRate(int time, int size);
  void RefreshRates(int time) { UpdateOutgoingRate(time, 0); UpdateIncomingRate(time, 0); }
  bool QueueReliableMessage(std::uint8_t type, const std::uint8_t* data, int dataLength);
  bool ProcessOutgoing(int time, const idBitMsg& message, bool isOOB, int userData);
  bool GetSendFragment(int time, sessionId_t sessionID, idBitMsg& output);
  int ProcessIncoming(int time,
                      sessionId_t expectedSessionID,
                      idBitMsg& message,
                      idBitMsg& output,
                      int& userData,
                      int peerNum);
  static bool ProcessConnectionlessOutgoing(idBitMsg& message,
                                            idBitMsg& output,
                                            std::int16_t lobbyType,
                                            int userData);
  static bool ProcessConnectionlessIncoming(idBitMsg& message,
                                            idBitMsg& output,
                                            int& userData,
                                            bool useBackend = false);
  static sessionId_t GetSessionID(idBitMsg& message, bool useBackend = false);
  void VerifyEmptyReliableQueue(std::uint8_t keepMessageBelow, std::uint8_t replacementMessage);

  bool HasMoreFragments() const { return unsentMsg.curSize - unsentMsg.readCount > 0; }
  int NumQueuedReliables() const { return reliable.Num(); }
  bool NeedToSendReliableAck() const { return queuedReliableAck >= 0; }
  int GetNumReliables() const { return numReliable; }
  const std::uint8_t* GetReliable(int index) const { return reliableMsgPtrs[index]; }
  int GetReliableSize(int index) const { return reliableMsgSize[index]; }
  void SetLastSendTime(int value) { lastSendTime = value; }
  int GetLastSendTime() const { return lastSendTime; }
  float GetOutgoingRateBytes() const { return outgoingRateBytes; }
  int GetOutgoingBytes() const { return outgoingBytes; }
  float GetIncomingRateBytes() const { return incomingRateBytes; }
  int GetIncomingBytes() const { return incomingBytes; }
  int GetOutgoingRate2() const { return currentOutgoingRate; }
  int GetIncomingRate2() const { return currentIncomingRate; }
  bool TickFragmentAccumulator() {
    if (fragmentAccumulator <= 0) {
      return false;
    }
    --fragmentAccumulator;
    return true;
  }
  int GetReliableDataSize() const { return reliable.GetDataLength(); }

private:
  static constexpr int PACKET_TYPE_INBAND = 0;
  static constexpr int PACKET_TYPE_OOB = 1;
  static constexpr int PACKET_TYPE_RELIABLE_ACK = 2;
  static constexpr int PACKET_TYPE_FRAGMENTED = 3;
  static constexpr int FRAGMENT_START = 0;
  static constexpr int FRAGMENT_MIDDLE = 1;
  static constexpr int FRAGMENT_END = 2;

  void QueueReliableAck(int lastReliable);
  int FinalizeRead(idBitMsg& input, idBitMsg& output, int& userValue);

  std::uint8_t msgBuffer[MAX_MSG_SIZE]{};
  int msgWritePos{};
  int fragmentSequence{};
  int droppedFrags{};
  bool fragmentedSend{};
  idDataQueue<MAX_RELIABLE_QUEUE, MAX_MSG_SIZE> reliable;
  int reliableSequenceSend{};
  int reliableSequenceRecv{};
  int numReliable{};
  std::uint8_t reliableBuffer[MAX_MSG_SIZE]{};
  const std::uint8_t* reliableMsgPtrs[MAX_RELIABLE_QUEUE]{};
  int reliableMsgSize[MAX_RELIABLE_QUEUE]{};
  int queuedReliableAck{};
  idBitMsg unsentMsg;
  std::uint8_t unsentBuffer[MAX_MSG_SIZE]{};
  int lastSendTime{};
  int outgoingRateTime{};
  float outgoingRateBytes{};
  int incomingRateTime{};
  float incomingRateBytes{};
  int outgoingBytes{};
  int incomingBytes{};
  int currentOutgoingRate{};
  int lastOutgoingRateTime{};
  int lastOutgoingBytes{};
  int currentIncomingRate{};
  int lastIncomingRateTime{};
  int lastIncomingBytes{};
  idGaugeMetric outgoingPacketSize;
  idGaugeMetric incomingPacketSize;
  idGaugeMetric handledReliableMsgSize;
  idGaugeMetric queuedReliableMsgSize;
  idGaugeMetric fragmentedPacketSize;
  idGaugeMetric sentSnapShotSize;
  int fragmentAccumulator{};
};

static_assert(sizeof(idDataQueue<63, 8000>) == 8792, "Wolf retail reliable queue ABI drift");
static_assert(sizeof(idPacketProcessor) == 34464, "Wolf retail packet processor ABI drift");
