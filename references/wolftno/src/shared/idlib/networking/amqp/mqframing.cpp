#include "shared/idlib/networking/amqp/mq.h"

#include <algorithm>
#include <cstring>
std::vector<std::uint8_t> idMQFrame::Encode() const {
  idMQBuffer out;
  out.WriteU8(type);
  out.WriteU16(channel);
  out.WriteU32(static_cast<std::uint32_t>(payload.Size()));
  out.WriteBytes(payload.Data(), payload.Size());
  out.WriteU8(0xCE);
  return out.data;
}
bool idMQFrame::Decode(const void* d, std::size_t n, idMQFrame& frame, std::size_t& consumed) {
  consumed = 0;
  if (n < 8)
    return false;
  idMQBuffer in(d, n);
  std::uint8_t type;
  std::uint32_t size;
  if (!in.ReadU8(type) || !in.ReadU16(frame.channel) || !in.ReadU32(size) || n < 8 + size)
    return false;
  frame.type = static_cast<amqpFrameType_t>(type);
  frame.payload.data.assign(in.data.begin() + 7, in.data.begin() + 7 + size);
  if (in.data[7 + size] != 0xCE)
    return false;
  consumed = 8 + size;
  return true;
}
void idMQFrameHandler::Feed(const void* d, std::size_t n) {
  const auto* b = static_cast<const std::uint8_t*>(d);
  pending.insert(pending.end(), b, b + n);
  while (true) {
    idMQFrame frame;
    std::size_t consumed;
    if (!idMQFrame::Decode(pending.data(), pending.size(), frame, consumed))
      break;
    frames.push_back(std::move(frame));
    pending.erase(pending.begin(), pending.begin() + static_cast<std::ptrdiff_t>(consumed));
  }
}
bool idMQFrameHandler::Pop(idMQFrame& f) {
  if (frames.empty())
    return false;
  f = std::move(frames.front());
  frames.pop_front();
  return true;
}
amqpEndpoint_t amqpEndpoint_t::Parse(const std::string& url) {
  amqpEndpoint_t e;
  std::string s = url;
  const auto scheme = s.find("://");
  if (scheme != std::string::npos)
    s = s.substr(scheme + 3);
  const auto at = s.find('@');
  if (at != std::string::npos) {
    const auto colon = s.find(':');
    if (colon < at) {
      e.user = s.substr(0, colon);
      e.password = s.substr(colon + 1, at - colon - 1);
    }
    s = s.substr(at + 1);
  }
  const auto slash = s.find('/');
  if (slash != std::string::npos) {
    e.virtualHost = "/" + s.substr(slash + 1);
    s.resize(slash);
  }
  const auto colon = s.rfind(':');
  if (colon != std::string::npos) {
    e.port = static_cast<std::uint16_t>(std::stoi(s.substr(colon + 1)));
    s.resize(colon);
  }
  if (!s.empty())
    e.host = s;
  return e;
}
