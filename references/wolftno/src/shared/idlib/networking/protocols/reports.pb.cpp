#include "shared/idlib/networking/protocols/protocols.h"

namespace idreports {
const std::string& DynamicReport::Get(const std::string& key) const {
  static const std::string empty;
  const auto it = fields.find(key);
  return it == fields.end() ? empty : it->second;
}
bool DynamicReport::SerializeToString(std::string* out) const {
  if (!out)
    return false;
  out->clear();
  for (const auto& entry : fields) {
    std::string item;
    idproto::WriteString(item, 1, entry.first);
    idproto::WriteString(item, 2, entry.second);
    idproto::WriteString(*out, 1, item);
  }
  for (const auto& tag : tags)
    idproto::WriteString(*out, 2, tag);
  return true;
}
bool DynamicReport::ParseFromString(const std::string& input) {
  Clear();
  std::size_t p = 0;
  while (p < input.size()) {
    unsigned field, wire;
    std::uint64_t number;
    std::string bytes;
    if (!idproto::Next(input, p, field, wire, number, bytes))
      return false;
    if (field == 2) {
      tags.push_back(bytes);
      continue;
    }
    if (field != 1)
      continue;
    std::size_t q = 0;
    std::string key, value;
    while (q < bytes.size()) {
      unsigned f, w;
      std::uint64_t n;
      std::string b;
      if (!idproto::Next(bytes, q, f, w, n, b))
        return false;
      if (f == 1)
        key = b;
      else if (f == 2)
        value = b;
    }
    if (!key.empty())
      fields[key] = value;
  }
  return true;
}
} // namespace idreports
