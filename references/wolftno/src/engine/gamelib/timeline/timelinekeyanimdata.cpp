#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#include "engine/gamelib/timeline/timelineeventdata.h"
#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI

#include <cstdint>

extern bool forceIdListsToReallocateEveryAppend;

template <>
bool idList<float, 13>::Resize(int newSize);

template <>
__declspec(noinline) int idList<float, 5>::Insert(const float& value,
                                                  int index) {
  idList<float, 13>* const resizeList =
      reinterpret_cast<idList<float, 13>*>(this);

  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!resizeList->Resize(initialSize)) {
      return -1;
    }
  }

  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity;
      newSize -= newSize % granularity;
    } else {
      newSize = size * 2;
      if (newSize == 0) {
        newSize = 1;
      }
    }
    if (!resizeList->Resize(newSize)) {
      return -1;
    }
  }

  if (index < 0) {
    index = 0;
  } else if (index > num) {
    index = num;
  }

  if (num < size) {
    for (int sourceIndex = num; sourceIndex > index; --sourceIndex) {
      list[sourceIndex] = list[sourceIndex - 1];
    }
    ++num;
  }
  list[index] = value;
  return index;
}

template <typename T> class idSearch {
public:
  __forceinline virtual ~idSearch() {}

  virtual int Search(const T* base, unsigned int num,
                     const T& value) const = 0;
  virtual int Search_FirstGreater(const T* base, int num,
                                  const T& value) const = 0;
  virtual int Search_FirstGreaterEqual(const T* base, int num,
                                       const T& value) const = 0;
  virtual int Search_LastLess(const T* base, int num,
                              const T& value) const = 0;
  virtual int Search_LastLessEqual(const T* base, int num,
                                   const T& value) const = 0;

  static void operator delete(void* memory) noexcept {
    if (memory != nullptr) {
      idMem::Singleton().Free(memory, ALIGN_16);
    }
  }
};

template <typename T, typename Derived>
class idSearch_Binary : public idSearch<T> {
public:
  __forceinline ~idSearch_Binary() override {}

  int Search(const T* base, unsigned int num,
             const T& value) const override {
    if (num == 0 || base == nullptr) {
      return -1;
    }

    unsigned int offset = 0;
    while (num > 0) {
      const unsigned int half = num >> 1;
      const T difference = base[offset + half] - value;
      const int comparison =
          difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
      if (comparison <= 0) {
        offset += half;
      }
      num -= half;
      if (half == 0) {
        break;
      }
    }
    const T difference = base[offset] - value;
    const int comparison =
        difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
    return comparison == 0 ? static_cast<int>(offset) : -1;
  }

  int Search_FirstGreater(const T* base, int num,
                          const T& value) const override {
    int offset = 0;
    int greater = 0;
    while (num > 0) {
      const int half = num >> 1;
      const T difference = base[offset + half] - value;
      const int comparison =
          difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
      if (comparison <= 0) {
        offset += half;
        greater = 1;
      } else {
        greater = 0;
      }
      num -= half;
      if (half == 0) {
        break;
      }
    }
    return offset + greater;
  }

  int Search_FirstGreaterEqual(const T* base, int num,
                               const T& value) const override {
    int offset = 0;
    int greaterEqual = 0;
    while (num > 0) {
      const int half = num >> 1;
      const T difference = base[offset + half] - value;
      const int comparison =
          difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
      if (comparison < 0) {
        offset += half;
        greaterEqual = 1;
      } else {
        greaterEqual = 0;
      }
      num -= half;
      if (half == 0) {
        break;
      }
    }
    return offset + greaterEqual;
  }

  int Search_LastLess(const T* base, int num,
                      const T& value) const override {
    int offset = 0;
    while (num > 0) {
      const int half = num >> 1;
      const T difference = base[offset + half] - value;
      const int comparison =
          difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
      if (comparison < 0) {
        offset += half;
      }
      num -= half;
      if (half == 0) {
        break;
      }
    }
    return offset;
  }

  int Search_LastLessEqual(const T* base, int num,
                           const T& value) const override {
    int offset = 0;
    while (num > 0) {
      const int half = num >> 1;
      const T difference = base[offset + half] - value;
      const int comparison =
          difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
      if (comparison <= 0) {
        offset += half;
      }
      num -= half;
      if (half == 0) {
        break;
      }
    }
    return offset;
  }
};

template <typename T>
class idSearch_BinaryDefault
    : public idSearch_Binary<T, idSearch_BinaryDefault<T>> {
public:
  ~idSearch_BinaryDefault() override {}

  __forceinline int Compare(const T& left, const T& right) const {
    const T difference = left - right;
    return difference > T(0) ? 1 : (difference < T(0) ? -1 : 0);
  }
};

template class idSearch<float>;

static idSearch_BinaryDefault<float> timelineKeyAnimDataSearch;

mgTimelineKeyAnimData::mgTimelineKeyAnimData() : times(), data() {
  type = TL_KEYTYPE_MAX;
  keySize = 0;
}

mgTimelineKeyAnimData::~mgTimelineKeyAnimData() noexcept(false) = default;

void mgTimelineKeyAnimData::PostLoadInit() {
  keySize = mgTimelineKeyUtil::KeySize(type);
}

template <>
__declspec(noinline) bool mgTimelineKeyAnimData::GetLerped<bool>(
    float time, bool* value) const {
  const int numKeys = times.num;
  if (numKeys == 0) {
    return false;
  }

  const int lowerIndex = timelineKeyAnimDataSearch.Search_LastLessEqual(
      times.list, numKeys, time);
  if (times.list[lowerIndex] > time) {
    return false;
  }
  *value = data.list[lowerIndex] != 0;
  return true;
}

template <>
__declspec(noinline) bool mgTimelineKeyAnimData::GetLerped<int>(
    float time, int* value) const {
  const int numKeys = times.num;
  if (numKeys == 0) {
    return false;
  }

  const int lowerIndex = timelineKeyAnimDataSearch.Search_LastLessEqual(
      times.list, numKeys, time);
  const float lowerTime = times.list[lowerIndex];
  if (lowerTime > time) {
    return false;
  }

  int upperIndex = lowerIndex;
  float fraction = 0.0f;
  if (lowerIndex < numKeys - 1) {
    ++upperIndex;
    fraction = (time - lowerTime) / (times.list[upperIndex] - lowerTime);
  }

  const int* const values = reinterpret_cast<const int*>(data.list);
  const int lowerValue = values[lowerIndex];
  const int upperValue = values[upperIndex];
  *value = static_cast<int>((static_cast<float>(upperValue) -
                             static_cast<float>(lowerValue)) *
                                fraction +
                            static_cast<float>(lowerValue));
  return true;
}

template <>
__declspec(noinline) bool mgTimelineKeyAnimData::GetLerped<float>(
    float time, float* value) const {
  const int numKeys = times.num;
  if (numKeys == 0) {
    return false;
  }

  const int lowerIndex = timelineKeyAnimDataSearch.Search_LastLessEqual(
      times.list, numKeys, time);
  const float lowerTime = times.list[lowerIndex];
  if (lowerTime > time) {
    return false;
  }

  int upperIndex = lowerIndex;
  float fraction = 0.0f;
  if (lowerIndex < numKeys - 1) {
    ++upperIndex;
    fraction = (time - lowerTime) / (times.list[upperIndex] - lowerTime);
  }

  const float* const values = reinterpret_cast<const float*>(data.list);
  const float lowerValue = values[lowerIndex];
  const float upperValue = values[upperIndex];
  *value = (upperValue - lowerValue) * fraction + lowerValue;
  return true;
}

template <>
__declspec(noinline) bool mgTimelineKeyAnimData::GetLerped<idVec3>(
    float time, idVec3* value) const {
  const int numKeys = times.num;
  if (numKeys == 0) {
    return false;
  }

  const int lowerIndex = timelineKeyAnimDataSearch.Search_LastLessEqual(
      times.list, numKeys, time);
  const float lowerTime = times.list[lowerIndex];
  if (lowerTime > time) {
    return false;
  }

  int upperIndex = lowerIndex;
  float fraction = 0.0f;
  if (lowerIndex < numKeys - 1) {
    ++upperIndex;
    fraction = (time - lowerTime) / (times.list[upperIndex] - lowerTime);
  }

  const idVec3* const values = reinterpret_cast<const idVec3*>(data.list);
  const idVec3 lowerValue = values[lowerIndex];
  const idVec3 upperValue = values[upperIndex];
  value->x = (upperValue.x - lowerValue.x) * fraction + lowerValue.x;
  value->y = (upperValue.y - lowerValue.y) * fraction + lowerValue.y;
  value->z = (upperValue.z - lowerValue.z) * fraction + lowerValue.z;
  return true;
}

template <>
__declspec(noinline) bool mgTimelineKeyAnimData::GetLerped<idVec4>(
    float time, idVec4* value) const {
  const int numKeys = times.num;
  if (numKeys == 0) {
    return false;
  }

  const int lowerIndex = timelineKeyAnimDataSearch.Search_LastLessEqual(
      times.list, numKeys, time);
  const float lowerTime = times.list[lowerIndex];
  if (lowerTime > time) {
    return false;
  }

  int upperIndex = lowerIndex;
  float fraction = 0.0f;
  if (lowerIndex < numKeys - 1) {
    ++upperIndex;
    fraction = (time - lowerTime) / (times.list[upperIndex] - lowerTime);
  }

  const idVec4* const values = reinterpret_cast<const idVec4*>(data.list);
  const idVec4 lowerValue = values[lowerIndex];
  const idVec4 upperValue = values[upperIndex];
  value->x = (upperValue.x - lowerValue.x) * fraction + lowerValue.x;
  value->y = (upperValue.y - lowerValue.y) * fraction + lowerValue.y;
  value->z = (upperValue.z - lowerValue.z) * fraction + lowerValue.z;
  value->w = (upperValue.w - lowerValue.w) * fraction + lowerValue.w;
  return true;
}

template <>
__declspec(noinline) bool mgTimelineKeyAnimData::GetLerped<idVec6>(
    float time, idVec6* value) const {
  const int numKeys = times.num;
  if (numKeys == 0) {
    return false;
  }

  const int lowerIndex = timelineKeyAnimDataSearch.Search_LastLessEqual(
      times.list, numKeys, time);
  const float lowerTime = times.list[lowerIndex];
  if (lowerTime > time) {
    return false;
  }

  int upperIndex = lowerIndex;
  float fraction = 0.0f;
  if (lowerIndex < numKeys - 1) {
    ++upperIndex;
    fraction = (time - lowerTime) / (times.list[upperIndex] - lowerTime);
  }

  const idVec6* const values = reinterpret_cast<const idVec6*>(data.list);
  const idVec6 lowerValue = values[lowerIndex];
  const idVec6 upperValue = values[upperIndex];
  for (int component = 0; component < 6; ++component) {
    value->p[component] =
        (upperValue.p[component] - lowerValue.p[component]) * fraction +
        lowerValue.p[component];
  }
  return true;
}

bool mgTimelineKeyAnimData::GetData(float time, bool* value) const {
  return GetLerped(time, value);
}

bool mgTimelineKeyAnimData::GetData(float time, int* value) const {
  return GetLerped(time, value);
}

bool mgTimelineKeyAnimData::GetData(float time, float* value) const {
  return GetLerped(time, value);
}

bool mgTimelineKeyAnimData::GetData(float time, idVec3* value) const {
  return GetLerped(time, value);
}

bool mgTimelineKeyAnimData::GetData(float time, idVec4* value) const {
  return GetLerped(time, value);
}

bool mgTimelineKeyAnimData::GetData(float time, idVec6* value) const {
  return GetLerped(time, value);
}
