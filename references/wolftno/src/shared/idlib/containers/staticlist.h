#pragma once

#include "list.h"

template <typename T, int Capacity>
class idStaticList : public idList<T, 5> {
public:
#if defined(WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI)
  __forceinline
#elif defined(WOLFTNO_RETAIL_STATICLIST_NOINLINE_CTOR_ABI)
  __declspec(noinline)
#endif
  idStaticList()
#if !defined(WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI)
      noexcept
#endif
#if defined(WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI)
      : idList<T, 5>(staticList, Capacity, 1)
#endif
  {
#if defined(WOLFTNO_RETAIL_CUTTABLE_STATICLIST_CTOR_ABI)
    this->size = Capacity;
    this->num = 0;
    this->list = staticList;
    *reinterpret_cast<std::uint32_t*>(&this->granularity) = 0x01050001u;
#elif defined(WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI)
    // The base constructor establishes the static-list invariant before any
    // non-trivial elements are constructed, matching the retail EH boundary.
#else
    this->list = staticList;
    this->num = 0;
    this->size = Capacity;
    this->granularity = 1;
    this->listStatic = 1;
#endif
  }
  idStaticList(const idStaticList& other) : idStaticList() {
    *this=other;
  }
  idStaticList& operator=(const idStaticList& other) {
    if(this!=&other) {
      this->num=other.num;
      for(int i=0;i<Capacity;++i) staticList[i]=other.staticList[i];
    }
    return *this;
  }
  ~idStaticList() = default;

  T* begin() noexcept { return staticList; }
  const T* begin() const noexcept { return staticList; }
  T* end() noexcept { return staticList+this->num; }
  const T* end() const noexcept { return staticList+this->num; }

  void fill(const T& value) {
    for(int i=0;i<Capacity;++i) staticList[i]=value;
    this->num=Capacity;
  }

  T staticList[Capacity];
};
