#include "shared/idlib/networking/amqp/mq.h"

#include <cstring>
idMQBuffer::idMQBuffer(const void* p, std::size_t n)
    : data(static_cast<const std::uint8_t*>(p), static_cast<const std::uint8_t*>(p) + n) {}
void idMQBuffer::Clear() {
  data.clear();
  cursor = 0;
}
void idMQBuffer::WriteU8(std::uint8_t v) {
  data.push_back(v);
}
void idMQBuffer::WriteU16(std::uint16_t v) {
  data.push_back(static_cast<std::uint8_t>(v >> 8));
  data.push_back(static_cast<std::uint8_t>(v));
}
void idMQBuffer::WriteU32(std::uint32_t v) {
  WriteU16(static_cast<std::uint16_t>(v >> 16));
  WriteU16(static_cast<std::uint16_t>(v));
}
void idMQBuffer::WriteU64(std::uint64_t v) {
  WriteU32(static_cast<std::uint32_t>(v >> 32));
  WriteU32(static_cast<std::uint32_t>(v));
}
void idMQBuffer::WriteBytes(const void* p, std::size_t n) {
  const auto* b = static_cast<const std::uint8_t*>(p);
  data.insert(data.end(), b, b + n);
}
void idMQBuffer::WriteShortString(const std::string& s) {
  WriteU8(static_cast<std::uint8_t>(std::min<std::size_t>(s.size(), 255)));
  WriteBytes(s.data(), std::min<std::size_t>(s.size(), 255));
}
void idMQBuffer::WriteLongString(const std::string& s) {
  WriteU32(static_cast<std::uint32_t>(s.size()));
  WriteBytes(s.data(), s.size());
}
bool idMQBuffer::ReadBytes(void* p, std::size_t n) {
  if (cursor + n > data.size())
    return false;
  std::memcpy(p, data.data() + cursor, n);
  cursor += n;
  return true;
}
bool idMQBuffer::ReadU8(std::uint8_t& v) {
  return ReadBytes(&v, 1);
}
bool idMQBuffer::ReadU16(std::uint16_t& v) {
  std::uint8_t b[2];
  if (!ReadBytes(b, 2))
    return false;
  v = (b[0] << 8) | b[1];
  return true;
}
bool idMQBuffer::ReadU32(std::uint32_t& v) {
  std::uint16_t a, b;
  if (!ReadU16(a) || !ReadU16(b))
    return false;
  v = (std::uint32_t(a) << 16) | b;
  return true;
}
bool idMQBuffer::ReadU64(std::uint64_t& v) {
  std::uint32_t a, b;
  if (!ReadU32(a) || !ReadU32(b))
    return false;
  v = (std::uint64_t(a) << 32) | b;
  return true;
}
bool idMQBuffer::ReadShortString(std::string& s) {
  std::uint8_t n;
  if (!ReadU8(n) || cursor + n > data.size())
    return false;
  s.assign(reinterpret_cast<const char*>(data.data() + cursor), n);
  cursor += n;
  return true;
}
bool idMQBuffer::ReadLongString(std::string& s) {
  std::uint32_t n;
  if (!ReadU32(n) || cursor + n > data.size())
    return false;
  s.assign(reinterpret_cast<const char*>(data.data() + cursor), n);
  cursor += n;
  return true;
}
void idMQMethod::Encode(idMQBuffer& o) const {
  o.WriteU16(classId);
  o.WriteU16(methodId);
  o.WriteBytes(arguments.Data(), arguments.Size());
}
bool idMQMethod::Decode(idMQBuffer& i) {
  if (!i.ReadU16(classId) || !i.ReadU16(methodId))
    return false;
  arguments.data.assign(i.data.begin() + i.cursor, i.data.end());
  i.cursor = i.data.size();
  return true;
}
void idMQContentHeader::Encode(idMQBuffer& o) const {
  o.WriteU16(classId);
  o.WriteU16(weight);
  o.WriteU64(bodySize);
  o.WriteU16(propertyFlags);
  o.WriteBytes(properties.Data(), properties.Size());
}
bool idMQContentHeader::Decode(idMQBuffer& i) {
  return i.ReadU16(classId) && i.ReadU16(weight) && i.ReadU64(bodySize) &&
         i.ReadU16(propertyFlags) &&
         (properties.data.assign(i.data.begin() + i.cursor, i.data.end()), true);
}
bool idMQTCP::Connect(const char* h, std::uint16_t p, int) {
  return socket.Connect(h, p, false, true, false);
}
void idMQTCP::Close() {
  socket.Close();
}
bool idMQTCP::Write(const void* d, std::size_t n, int timeout) {
  return socket.WriteBlocking(static_cast<const char*>(d), static_cast<int>(n), timeout) ==
         static_cast<int>(n);
}
int idMQTCP::Read(void* d, std::size_t n, int timeout) {
  return socket.ReadBlocking(static_cast<char*>(d), static_cast<int>(n), timeout);
}
