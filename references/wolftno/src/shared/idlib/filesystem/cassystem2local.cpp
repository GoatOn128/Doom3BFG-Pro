#include "shared/idlib/filesystem/cassystemlocal.h"

#include <string>

idCasSystemLocal2 casSystemLocal2;
idCasSystem2* casSystem2 = &casSystemLocal2;
idStr idContentAddress2::ToString() const {
  idStr result = content.ToString();
  result.Append("-");
  result.Append(std::to_string(size).c_str());
  return result;
}
idContentAddress2 idCasSystemLocal2::Store(const void* data, std::size_t size) {
  return {legacy.Store(data, size), static_cast<std::uint64_t>(size)};
}
idFile* idCasSystemLocal2::OpenPayloadForRead(const idContentAddress2& a) {
  idFile* file = legacy.OpenCasFileRead(a.content);
  if (file && static_cast<std::uint64_t>(file->Length()) != a.size) {
    delete file;
    return nullptr;
  }
  return file;
}
