#include <Windows.h>
#include <map>

struct module_t {
  std::uint64_t base{};
  std::uint64_t size{};
};
class idDiaModuleManager {
public:
  ~idDiaModuleManager() {
    for (auto& entry : modulesMap)
      delete entry.second;
  }
  std::map<std::uint64_t, module_t*> modulesMap;
  void* hProcess{};
};
