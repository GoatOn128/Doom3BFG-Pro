#include "engine/network/packetprocessor.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstring>

idCVar net_maxRate("net_maxRate", "50", CVAR_INTEGER,
                   "max send rate in kilobytes per second");
idCVar net_showReliableCompression("net_showReliableCompression", "0", CVAR_BOOL,
                                   "Show reliable compression ratio.");
idCVar net_verifyReliableQueue("net_verifyReliableQueue", "2", CVAR_INTEGER,
                               "0: warn only, 1: error, 2: fixup, 3: fixup and verbose, 4: force test");

extern idCVar net_verbose;
extern idCVar net_verboseReliable;

idPacketProcessor::idPacketProcessor()
    : outgoingPacketSize("outgoingPacketSize", "size of outgoing packets"),
      incomingPacketSize("incomingPacketSize", "size of incoming packets"),
      handledReliableMsgSize("handledReliableSize", "size of handled reliable msgs"),
      queuedReliableMsgSize("queuedReliableSize", "size of queued reliable msgs"),
      fragmentedPacketSize("fragmentedPacketSize", "size of fragmented packets"),
      sentSnapShotSize("sentSnapShotSize", "size of sent snapshots") {
  Reset();
}

void idPacketProcessor::Reset() {
  msgWritePos = 0;
  fragmentSequence = 0;
  droppedFrags = 0;
  fragmentedSend = false;
  reliable.Clear();
  reliableSequenceSend = 1;
  reliableSequenceRecv = 0;
  numReliable = 0;
  queuedReliableAck = -1;
  unsentMsg = idBitMsg();
  lastSendTime = 0;
  outgoingRateTime = 0;
  outgoingRateBytes = 0.0F;
  incomingRateTime = 0;
  incomingRateBytes = 0.0F;
  outgoingBytes = 0;
  incomingBytes = 0;
  currentOutgoingRate = 0;
  lastOutgoingRateTime = 0;
  lastOutgoingBytes = 0;
  currentIncomingRate = 0;
  lastIncomingRateTime = 0;
  lastIncomingBytes = 0;
  fragmentAccumulator = 0;
}

void idPacketProcessor::UpdateOutgoingRate(int time, int size) {
  outgoingBytes += size;
  if (time > outgoingRateTime) {
    outgoingRateBytes -= static_cast<float>(time - outgoingRateTime) * outgoingRateBytes / 1000.0F;
    if (outgoingRateBytes < 0.0F) {
      outgoingRateBytes = 0.0F;
    }
  }
  outgoingRateTime = time;
  outgoingRateBytes += static_cast<float>(size);
  const int elapsed = time - lastOutgoingRateTime;
  if (elapsed > BANDWIDTH_AVERAGE_PERIOD) {
    lastOutgoingRateTime = time;
    currentOutgoingRate = 1000 * (outgoingBytes - lastOutgoingBytes) / elapsed;
    lastOutgoingBytes = outgoingBytes;
  }
  if (size != 0) {
    outgoingPacketSize.Log(static_cast<float>(size), false);
  }
}

void idPacketProcessor::UpdateIncomingRate(int time, int size) {
  incomingBytes += size;
  if (time > incomingRateTime) {
    incomingRateBytes -= static_cast<float>(time - incomingRateTime) * incomingRateBytes / 1000.0F;
    if (incomingRateBytes < 0.0F) {
      incomingRateBytes = 0.0F;
    }
  }
  incomingRateTime = time;
  incomingRateBytes += static_cast<float>(size);
  const int elapsed = time - lastIncomingRateTime;
  if (elapsed > BANDWIDTH_AVERAGE_PERIOD) {
    lastIncomingRateTime = time;
    currentIncomingRate = 1000 * (incomingBytes - lastIncomingBytes) / elapsed;
    lastIncomingBytes = incomingBytes;
  }
  if (size != 0) {
    incomingPacketSize.Log(static_cast<float>(size), false);
  }
}

bool idPacketProcessor::CanSendMoreData() const {
  return net_maxRate.valueInteger == 0 ||
         static_cast<float>(net_maxRate.valueInteger << 10) >= outgoingRateBytes;
}

bool idPacketProcessor::GetSendFragment(int time, sessionId_t sessionID, idBitMsg& output) {
  lastSendTime = time;
  if (unsentMsg.curSize - unsentMsg.readCount <= 0) {
    return false;
  }
  output.BeginWriting();
  output.WriteBits(sessionID, 16);
  if (fragmentedSend) {
    const int remaining = unsentMsg.curSize - unsentMsg.readCount;
    const int currentSize = std::clamp(remaining, 0, MAX_PACKET_SIZE);
    const bool moreFragments = remaining - currentSize > 0;
    output.WriteBits(PACKET_TYPE_FRAGMENTED, 2);
    output.WriteBits(unsentMsg.readCount == 0 ? FRAGMENT_START
                                              : (moreFragments ? FRAGMENT_MIDDLE : FRAGMENT_END),
                     6);
    output.WriteBits(fragmentSequence, 32);
    output.WriteData(unsentMsg.readData + unsentMsg.readCount, currentSize);
    unsentMsg.ReadData(nullptr, currentSize);
    ++fragmentSequence;
    ++fragmentAccumulator;
    fragmentedSend = moreFragments;
  } else {
    output.WriteData(unsentMsg.readData, unsentMsg.GetSize());
    unsentMsg.curSize = 0;
  }
  UpdateOutgoingRate(time, output.GetSize());
  return true;
}

bool idPacketProcessor::ProcessConnectionlessOutgoing(idBitMsg& message,
                                                       idBitMsg& output,
                                                       std::int16_t lobbyType,
                                                       int userData) {
  output.WriteBits(static_cast<std::uint16_t>(lobbyType + 1), 16);
  output.WriteBits(PACKET_TYPE_OOB, 2);
  output.WriteBits(userData, 6);
  output.WriteData(message.readData, message.GetSize());
  return true;
}

bool idPacketProcessor::ProcessConnectionlessIncoming(idBitMsg& message,
                                                       idBitMsg& output,
                                                       int& userData,
                                                       bool /*useBackend*/) {
  const sessionId_t sessionID = static_cast<sessionId_t>(message.ReadBits(16));
  if (sessionID != SESSION_ID_CONNECTIONLESS_PARTY &&
      sessionID != SESSION_ID_CONNECTIONLESS_GAME &&
      sessionID != SESSION_ID_CONNECTIONLESS_GAME_STATE) {
    if (net_verbose.valueInteger != 0) {
      idLib::Printf("ProcessConnectionlessIncoming: Invalid session ID - %d\n", sessionID);
    }
    return false;
  }
  const int type = message.ReadBits(2);
  const int value = message.ReadBits(6);
  if (type != PACKET_TYPE_OOB) {
    idLib::Printf("ProcessConnectionlessIncoming: header.Type() != PACKET_TYPE_OOB\n");
    return false;
  }
  userData = value;
  output.BeginWriting();
  output.WriteData(message.readData + message.readCount, message.curSize - message.readCount);
  output.curSize = std::min(message.curSize - message.readCount, output.maxSize);
  return true;
}

idPacketProcessor::sessionId_t idPacketProcessor::GetSessionID(idBitMsg& message,
                                                                bool /*useBackend*/) {
  const int readCount = message.readCount;
  const int readBit = message.readBit;
  const sessionId_t result = static_cast<sessionId_t>(message.ReadBits(16));
  message.readCount = readCount;
  message.readBit = readBit & 7;
  return result;
}

void idPacketProcessor::QueueReliableAck(int lastReliable) {
  if (lastReliable >= reliableSequenceRecv) {
    queuedReliableAck = lastReliable;
    reliableSequenceRecv = lastReliable;
  }
}

int idPacketProcessor::FinalizeRead(idBitMsg& input, idBitMsg& output, int& userValue) {
  userValue = 0;
  int type = input.ReadBits(2);
  int value = input.ReadBits(6);
  if (type == PACKET_TYPE_FRAGMENTED) {
    idLib::Printf("Received invalid fragmented packet.\n");
    return RETURN_TYPE_NONE;
  }
  if (type == PACKET_TYPE_RELIABLE_ACK) {
    reliable.RemoveOlderThan(input.ReadBits(32) + 1);
    type = input.ReadBits(2);
    value = input.ReadBits(6);
  }
  if (type == PACKET_TYPE_OOB) {
    userValue = value;
  } else {
    if (type != PACKET_TYPE_INBAND) {
      idLib::Printf("In-band packet expected, received type %i instead.\n", type);
      return RETURN_TYPE_NONE;
    }
    numReliable = 0;
    int bufferPosition = 0;
    if (value > 0) {
      input.readBit = 0;
      const int compressedSize = input.ReadBits(-16);
      lzwCompressionData_t lzwData{};
      idLZWCompressor compressor(&lzwData);
      compressor.Start(const_cast<std::uint8_t*>(input.readData + input.readCount), compressedSize);
      int reliableSequence = 0;
      compressor.ReadAgnostic(reliableSequence);
      for (int index = 0; index < value; ++index) {
        std::uint16_t reliableDataLength = 0;
        std::uint8_t uncompressed[MAX_MSG_SIZE];
        compressor.ReadAgnostic(reliableDataLength);
        compressor.Read(uncompressed, reliableDataLength);
        const int currentSequence = reliableSequence + index;
        if (currentSequence > reliableSequenceRecv) {
          if (bufferPosition + reliableDataLength > MAX_MSG_SIZE) {
            idLib::Printf("Reliable msg size overflow.\n");
            return RETURN_TYPE_NONE;
          }
          if (numReliable >= MAX_RELIABLE_QUEUE) {
            idLib::Printf("Reliable msg count overflow.\n");
            return RETURN_TYPE_NONE;
          }
          std::memmove(reliableBuffer + bufferPosition, uncompressed, reliableDataLength);
          reliableMsgSize[numReliable] = reliableDataLength;
          reliableMsgPtrs[numReliable++] = reliableBuffer + bufferPosition;
          bufferPosition += reliableDataLength;
        } else if (net_verboseReliable.valueInteger != 0) {
          idLib::Printf("Ignoring reliable msg %i because %i was already acked\n",
                        currentSequence, reliableSequenceRecv);
        }
        if (compressor.IsOverflowed()) {
          idLib::Printf("lzwCompressor.IsOverflowed() == true.\n");
          return RETURN_TYPE_NONE;
        }
      }
      input.readCount += compressedSize;
      QueueReliableAck(reliableSequence + value - 1);
    }
  }
  output.BeginWriting();
  output.WriteData(input.readData + input.readCount, input.curSize - input.readCount);
  output.curSize = std::min(input.curSize - input.readCount, output.maxSize);
  return type == PACKET_TYPE_OOB ? RETURN_TYPE_OOB : RETURN_TYPE_INBAND;
}

bool idPacketProcessor::ProcessOutgoing(int /*time*/,
                                        const idBitMsg& message,
                                        bool isOOB,
                                        int userData) {
  if (fragmentedSend) {
    idLib::Warning("ProcessOutgoing: fragmentedSend == true!");
    return false;
  }
  if (unsentMsg.curSize != unsentMsg.readCount) {
    idLib::Warning("ProcessOutgoing: unsentMsg.GetRemainingData() > 0!");
    return false;
  }
  unsentMsg.InitWrite(unsentBuffer, sizeof(unsentBuffer));
  if (queuedReliableAck >= 0) {
    unsentMsg.WriteBits(PACKET_TYPE_RELIABLE_ACK, 2);
    unsentMsg.WriteBits(0, 6);
    unsentMsg.WriteBits(queuedReliableAck, 32);
    queuedReliableAck = -1;
  }
  if (isOOB) {
    if (unsentMsg.GetSize() + message.GetSize() > MAX_OOB_MSG_SIZE) {
      idLib::Printf("Out-of-band packet too large %i\n", unsentMsg.GetSize());
      return false;
    }
    unsentMsg.WriteBits(PACKET_TYPE_OOB, 2);
    unsentMsg.WriteBits(userData, 6);
  } else {
    unsentMsg.WriteBits(PACKET_TYPE_INBAND, 2);
    unsentMsg.WriteBits(reliable.Num(), 6);
    if (reliable.Num() > 0) {
      if (unsentMsg.writeBit != 0) {
        unsentMsg.writeBit = 0;
        ++unsentMsg.curSize;
      }
      unsentMsg.tempValue = 0;
      lzwCompressionData_t lzwData{};
      idLZWCompressor compressor(&lzwData);
      compressor.Start(unsentMsg.writeData + unsentMsg.curSize + 2,
                       unsentMsg.maxSize - unsentMsg.curSize - 2);
      int uncompressedSize = 4;
      const int firstSequence = reliable.ItemSequence(0);
      compressor.WriteAgnostic(firstSequence);
      for (int i = 0; i < reliable.Num(); ++i) {
        const std::uint16_t length = static_cast<std::uint16_t>(reliable.ItemLength(i));
        compressor.WriteAgnostic(length);
        compressor.Write(reliable.ItemData(i), length);
        uncompressedSize += 2 + length;
      }
      compressor.End();
      if (compressor.IsOverflowed()) {
        idLib::Error("reliable msg compressor overflow.");
      }
      unsentMsg.WriteBits(compressor.Length(), -16);
      unsentMsg.curSize = std::min(unsentMsg.curSize + compressor.Length() +
                                      (unsentMsg.writeBit != 0 ? 1 : 0),
                                  unsentMsg.maxSize);
      if (net_showReliableCompression.valueInteger != 0) {
        static int totalUncompressed = 0;
        static int totalCompressed = 0;
        totalUncompressed += uncompressedSize;
        totalCompressed += compressor.Length();
        idLib::Printf("Uncompressed: %i, Compressed: %i, TotalUncompressed: %i, "
                      "TotalCompressed: %i, (%2.2f / %2.2f )\n",
                      uncompressedSize, compressor.Length(), totalUncompressed,
                      totalCompressed,
                      static_cast<float>(compressor.Length()) / uncompressedSize,
                      static_cast<float>(totalCompressed) / totalUncompressed);
      }
    }
  }
  unsentMsg.WriteData(message.readData, message.GetSize());
  if (unsentMsg.GetSize() > MAX_PACKET_SIZE) {
    if (isOOB) {
      idLib::Error("oob msg's cannot fragment");
    }
    fragmentedSend = true;
  }
  return true;
}

int idPacketProcessor::ProcessIncoming(int time,
                                       sessionId_t expectedSessionID,
                                       idBitMsg& message,
                                       idBitMsg& output,
                                       int& userData,
                                       int peerNum) {
  UpdateIncomingRate(time, message.GetSize());
  const sessionId_t sessionID = static_cast<sessionId_t>(message.ReadBits(16));
  if (sessionID == SESSION_ID_CONNECTIONLESS_PARTY ||
      sessionID == SESSION_ID_CONNECTIONLESS_GAME ||
      sessionID == SESSION_ID_CONNECTIONLESS_GAME_STATE) {
    idLib::Printf("Expected non connectionless ID, but got a connectionless one\n");
    return RETURN_TYPE_NONE;
  }
  if (sessionID != expectedSessionID) {
    idLib::Printf("Expected session id: %8x but got %8x instead\n", expectedSessionID, sessionID);
    return RETURN_TYPE_NONE;
  }
  const int savedReadBit = message.readBit;
  const int savedReadCount = message.readCount;
  const int type = message.ReadBits(2);
  const int fragmentType = message.ReadBits(6);
  if (type != PACKET_TYPE_FRAGMENTED) {
    message.readCount = savedReadCount;
    message.readBit = savedReadBit & 7;
    return FinalizeRead(message, output, userData);
  }
  const int readSequence = message.ReadBits(32);
  if (fragmentType == FRAGMENT_START) {
    msgWritePos = 0;
  } else if (fragmentSequence == -1 || readSequence != fragmentSequence + 1) {
    ++droppedFrags;
    idLib::Printf("Dropped Fragments - PeerNum: %i FragmentSeq: %i, ReadSeq: %i, Total: %i\n",
                  peerNum, fragmentSequence, readSequence, droppedFrags);
    fragmentSequence = -1;
    return RETURN_TYPE_NONE;
  }
  fragmentSequence = readSequence;
  const int remaining = message.curSize - message.readCount;
  if (msgWritePos + remaining >= MAX_MSG_SIZE) {
    idLib::Error("ProcessIncoming: Fragmented msg buffer overflow.");
  }
  std::memmove(msgBuffer + msgWritePos, message.readData + message.readCount,
               static_cast<std::size_t>(remaining));
  msgWritePos += remaining;
  if (fragmentType != FRAGMENT_END) {
    if (fragmentType >= FRAGMENT_END) {
      idLib::Printf("ProcessIncoming: Invalid packet.\n");
    }
    return RETURN_TYPE_NONE;
  }
  idBitMsg reconstructed;
  reconstructed.InitRead(msgBuffer, std::min(msgWritePos, MAX_MSG_SIZE));
  return FinalizeRead(reconstructed, output, userData);
}

bool idPacketProcessor::QueueReliableMessage(std::uint8_t type,
                                              const std::uint8_t* data,
                                              int dataLength) {
  return reliable.Append(reliableSequenceSend++, &type, 1, data, dataLength);
}

void idPacketProcessor::VerifyEmptyReliableQueue(std::uint8_t keepMessageBelow,
                                                  std::uint8_t replacementMessage) {
  if (net_verifyReliableQueue.valueInteger == 4) {
    idLib::Printf("pushing a fake game reliable\n");
    static constexpr char garbage[] = "garbage";
    QueueReliableMessage(static_cast<std::uint8_t>(keepMessageBelow + 4),
                         reinterpret_cast<const std::uint8_t*>(garbage), 8);
    QueueReliableMessage(replacementMessage, nullptr, 0);
  }
  if (reliable.Num() == 0) {
    return;
  }
  if (net_verifyReliableQueue.valueInteger == 1) {
    idLib::Error("reliable queue is not empty: %d messages", reliable.Num());
  }
  idLib::Warning("reliable queue is not empty: %d messages", reliable.Num());
  if (net_verifyReliableQueue.valueInteger == 0) {
    return;
  }
  idDataQueue<MAX_RELIABLE_QUEUE, MAX_MSG_SIZE> clean;
  if (net_verifyReliableQueue.valueInteger >= 3) {
    idLib::Printf("rollback send sequence from %d to %d\n", reliableSequenceSend,
                  reliable.ItemSequence(0));
  }
  for (int i = 0; i < reliable.Num(); ++i) {
    const std::uint8_t type = reliable.ItemData(i)[0];
    if (type < keepMessageBelow) {
      if (net_verifyReliableQueue.valueInteger >= 3) {
        idLib::Printf("keeping %d\n", type);
      }
      clean.Append(reliable.ItemSequence(i), reliable.ItemData(i), reliable.ItemLength(i));
    } else {
      clean.Append(reliable.ItemSequence(i), &replacementMessage, 1);
      if (net_verifyReliableQueue.valueInteger >= 3) {
        idLib::Printf("dropping %d\n", type);
      }
    }
  }
  reliable = clean;
}
