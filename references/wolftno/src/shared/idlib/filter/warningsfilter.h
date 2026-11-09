#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

struct warningRegex_t {
  std::uint64_t subExpressionCount = 0;
  void* value = nullptr;
};

struct warningfilter_t {
  idStr pattern;
  idStr owner;
  warningRegex_t reg;

  warningfilter_t() = default;
  warningfilter_t(const warningfilter_t& other)
      : pattern(other.pattern), owner(other.owner), reg{} {}
  warningfilter_t& operator=(const warningfilter_t& other) {
    if (this != &other) {
      pattern = other.pattern;
      owner = other.owner;
      reg = {};
    }
    return *this;
  }
  ~warningfilter_t();
};

idStr EscapeRegexString(const idStr& text);

class mgWarningsFilter {
public:
  mgWarningsFilter() = default;
  ~mgWarningsFilter();
  bool Handle(const char* text) const;
  void ParseWarningsFile(const char* file);
  void Init();
  void Init(const char* file, bool enabled);
  int NumFilters() const { return filters.Num(); }

private:
  idList<warningfilter_t> filters;
  bool init = false;
};

extern mgWarningsFilter warningsFilter;

static_assert(sizeof(warningfilter_t) == 96, "warningfilter_t ABI drift");
static_assert(sizeof(mgWarningsFilter) == 32, "mgWarningsFilter ABI drift");
