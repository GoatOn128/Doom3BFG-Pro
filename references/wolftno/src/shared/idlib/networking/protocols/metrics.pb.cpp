#include "shared/idlib/networking/protocols/protocols.h"

#include <cstring>

namespace idproto {
void WriteVarint(std::string& o, std::uint64_t v) {
  while (v >= 128) {
    o.push_back(static_cast<char>((v & 127) | 128));
    v >>= 7;
  }
  o.push_back(static_cast<char>(v));
}
void WriteString(std::string& o, unsigned f, const std::string& v) {
  WriteVarint(o, (f << 3) | 2);
  WriteVarint(o, v.size());
  o += v;
}
void WriteUInt(std::string& o, unsigned f, std::uint64_t v) {
  WriteVarint(o, f << 3);
  WriteVarint(o, v);
}
void WriteFloat(std::string& o, unsigned f, float v) {
  WriteVarint(o, (f << 3) | 5);
  const char* p = reinterpret_cast<const char*>(&v);
  o.append(p, 4);
}
bool Next(const std::string& i, std::size_t& p, unsigned& field, unsigned& wire,
          std::uint64_t& number, std::string& bytes) {
  auto varint = [&](std::uint64_t& v) {
    v = 0;
    int shift = 0;
    while (p < i.size() && shift < 64) {
      const auto b = static_cast<unsigned char>(i[p++]);
      v |= std::uint64_t(b & 127) << shift;
      if (!(b & 128))
        return true;
      shift += 7;
    }
    return false;
  };
  std::uint64_t tag;
  if (!varint(tag))
    return false;
  field = static_cast<unsigned>(tag >> 3);
  wire = tag & 7;
  number = 0;
  bytes.clear();
  if (wire == 0)
    return varint(number);
  if (wire == 2) {
    std::uint64_t n;
    if (!varint(n) || p + n > i.size())
      return false;
    bytes.assign(i, p, static_cast<std::size_t>(n));
    p += static_cast<std::size_t>(n);
    return true;
  }
  if (wire == 5 && p + 4 <= i.size()) {
    bytes.assign(i, p, 4);
    p += 4;
    return true;
  }
  return false;
}
} // namespace idproto
namespace {
template <class F> bool Parse(const std::string& i, F&& f) {
  std::size_t p = 0;
  while (p < i.size()) {
    unsigned field, wire;
    std::uint64_t number;
    std::string bytes;
    if (!idproto::Next(i, p, field, wire, number, bytes))
      return false;
    f(field, wire, number, bytes);
  }
  return true;
}
} // namespace
namespace idmetrics {
void SessionHeader::Clear() {
  runtimeSessionId.clear();
  matchId.clear();
  sessionState.clear();
}
bool SessionHeader::SerializeToString(std::string* o) const {
  if (!o)
    return false;
  o->clear();
  idproto::WriteString(*o, 1, runtimeSessionId);
  idproto::WriteString(*o, 2, matchId);
  idproto::WriteString(*o, 3, sessionState);
  return true;
}
bool SessionHeader::ParseFromString(const std::string& i) {
  Clear();
  return Parse(i, [&](unsigned f, unsigned, std::uint64_t, const std::string& b) {
    if (f == 1)
      runtimeSessionId = b;
    else if (f == 2)
      matchId = b;
    else if (f == 3)
      sessionState = b;
  });
}
bool Vec3::SerializeToString(std::string* o) const {
  if (!o)
    return false;
  o->clear();
  idproto::WriteFloat(*o, 1, x);
  idproto::WriteFloat(*o, 2, y);
  idproto::WriteFloat(*o, 3, z);
  return true;
}
bool Vec3::ParseFromString(const std::string& i) {
  Clear();
  return Parse(i, [&](unsigned f, unsigned, std::uint64_t, const std::string& b) {
    if (b.size() == 4) {
      float v;
      std::memcpy(&v, b.data(), 4);
      if (f == 1)
        x = v;
      else if (f == 2)
        y = v;
      else if (f == 3)
        z = v;
    }
  });
}
void ReliableMessageMetric::Clear() {
  header.Clear();
  peerCount = peerNumber = 0;
  reliables.clear();
}
bool ReliableMessageMetric::SerializeToString(std::string* o) const {
  if (!o)
    return false;
  o->clear();
  std::string h;
  header.SerializeToString(&h);
  idproto::WriteString(*o, 1, h);
  idproto::WriteUInt(*o, 2, peerCount);
  idproto::WriteUInt(*o, 3, peerNumber);
  for (auto v : reliables)
    idproto::WriteUInt(*o, 4, v);
  return true;
}
bool ReliableMessageMetric::ParseFromString(const std::string& i) {
  Clear();
  return Parse(i, [&](unsigned f, unsigned, std::uint64_t n, const std::string& b) {
    if (f == 1)
      header.ParseFromString(b);
    else if (f == 2)
      peerCount = static_cast<std::uint32_t>(n);
    else if (f == 3)
      peerNumber = static_cast<std::uint32_t>(n);
    else if (f == 4)
      reliables.push_back(static_cast<std::uint32_t>(n));
  });
}
void SnapshotMetric::Clear() {
  header.Clear();
  report.clear();
}
bool SnapshotMetric::SerializeToString(std::string* o) const {
  if (!o)
    return false;
  o->clear();
  std::string h;
  header.SerializeToString(&h);
  idproto::WriteString(*o, 1, h);
  idproto::WriteString(*o, 2, report);
  return true;
}
bool SnapshotMetric::ParseFromString(const std::string& i) {
  Clear();
  return Parse(i, [&](unsigned f, unsigned, std::uint64_t, const std::string& b) {
    if (f == 1)
      header.ParseFromString(b);
    else if (f == 2)
      report = b;
  });
}
void SessionStateRecord::Clear() {
  header.Clear();
  state.clear();
}
bool SessionStateRecord::SerializeToString(std::string* o) const {
  if (!o)
    return false;
  o->clear();
  std::string h;
  header.SerializeToString(&h);
  idproto::WriteString(*o, 1, h);
  idproto::WriteString(*o, 2, state);
  return true;
}
bool SessionStateRecord::ParseFromString(const std::string& i) {
  Clear();
  return Parse(i, [&](unsigned f, unsigned, std::uint64_t, const std::string& b) {
    if (f == 1)
      header.ParseFromString(b);
    else if (f == 2)
      state = b;
  });
}
} // namespace idmetrics
