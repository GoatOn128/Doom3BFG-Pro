#include "atomicstring.h"

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>

namespace {
std::mutex atomicMutex;
std::unordered_set<std::string> globalStrings;
std::unordered_set<std::string> dynamicStrings;
} // namespace

idAtomicStringManager atomicStringManager;
const char* const idAtomicString::nullAtomicString = "";
idAtomicString atsTestStatic("aStaticAtomicString");

const char* idAtomicStringManager::MakeAtomic(const char* text) {
  if (!text || !*text)
    return idAtomicString::nullAtomicString;
  std::lock_guard<std::mutex> lock(atomicMutex);
  auto [it, inserted] = dynamicStrings.emplace(text);
  return it->c_str();
}
void idAtomicStringManager::FreeDynamic() {
  std::lock_guard<std::mutex> lock(atomicMutex);
  dynamicStrings.clear();
}
void idAtomicString::Set(const char* text) {
  str = atomicStringManager.MakeAtomic(text);
}
void idAtomicString::FreeDynamic() {
  atomicStringManager.FreeDynamic();
}
