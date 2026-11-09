#include "win_files.h"

#include <Windows.h>
#include <algorithm>
#include <filesystem>
#include <string>

bool Sys_Mkdir(const char* path) {
  std::error_code error;
  return std::filesystem::create_directory(path, error) ||
         std::filesystem::is_directory(path, error);
}
bool Sys_Rmdir(const char* path) {
  std::error_code error;
  return std::filesystem::remove(path, error);
}
bool Sys_IsFileWritable(const char* path) {
  const DWORD attributes = GetFileAttributesA(path);
  return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_READONLY) == 0;
}
int Sys_IsFolder(const char* path) {
  const DWORD attributes = GetFileAttributesA(path);
  return attributes == INVALID_FILE_ATTRIBUTES ? -1
                                               : (attributes & FILE_ATTRIBUTE_DIRECTORY ? 1 : 0);
}
char* Sys_CWD() {
  static char cwd[4096];
  GetCurrentDirectoryA(sizeof(cwd), cwd);
  return cwd;
}
const char* Sys_Install() {
  return "";
}
char* Sys_EXEPath() {
  static char executable[4096];
  if (!executable[0])
    GetModuleFileNameA(nullptr, executable, sizeof(executable));
  return executable;
}
void Sys_ListFiles(const char* directory, const char* extension, idList<idStr, 44>& list) {
  list.Clear();
  if (!directory)
    return;
  const bool folders = extension && idStr::Cmp(extension, "/") == 0;
  std::string suffix = extension ? extension : "";
  if (folders)
    suffix.clear();
  std::error_code error;
  for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
    if (error)
      break;
    if (folders != entry.is_directory(error))
      continue;
    const std::string name = entry.path().filename().string();
    if (!suffix.empty() && suffix != "*" &&
        (name.size() < suffix.size() ||
         idStr::Icmp(name.c_str() + name.size() - suffix.size(), suffix.c_str()) != 0))
      continue;
    list.Append(idStr(name.c_str()));
  }
  std::sort(list.list, list.list + list.num,
            [](const idStr& a, const idStr& b) { return idStr::Icmp(a.data, b.data) < 0; });
}
