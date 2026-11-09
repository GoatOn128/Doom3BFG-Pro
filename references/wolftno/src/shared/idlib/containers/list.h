#pragma once

#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

class idMD6MorphSet;
class idAnimator_FaceTracks;
class idAnimator_EndAdditiveChannels;
class idAnimator_ChannelWeight;
class idAnimator_JointMod;
class idAnimator_Synced;
class idAnimWebBlendTree;
class idAFTree;
class idObstacleAvoidanceManagerLocal;
class idPush;
class idClass;
class idEffectPhysicsPieceEmitter;
class idDeferredVisCache;
class idAnimator_DragJoints;
class idRibbon;
class idPhysics_DynamicBase;
class idClipModel;
class idFXManager;

#if defined(WOLFTNO_RETAIL_IDLIST_EXACT_GROWTH_ABI)
extern bool forceIdListsToReallocateEveryAppend;
#endif

template <typename T, typename = void>
struct idListAllocReturnsReference : std::false_type {};

// Some retail idList instantiations expose Alloc as T&.  A nested marker lets
// a type request that ABI before the containing list instantiates, including
// for nested types whose enclosing class is not complete soon enough for an
// ordinary explicit specialization.
template <typename T>
struct idListAllocReturnsReference<
    T, std::void_t<typename T::idListAllocReturnsReferenceMarker>>
    : std::true_type {};

template <typename T, int DefaultTag = 5> class idList {
public:
  template <typename, int> friend class idList;
  friend class idAnimator_FaceTracks;
  friend class idAnimator_EndAdditiveChannels;
  friend class idAnimator_ChannelWeight;
  friend class idAnimator_JointMod;
  friend class idAnimator_Synced;
  friend class idAnimWebBlendTree;
  friend class idAFTree;
  friend class idObstacleAvoidanceManagerLocal;
  friend class idPush;
  friend class idClass;
  friend class idEffectPhysicsPieceEmitter;
  friend class idDeferredVisCache;
  friend class idAnimator_DragJoints;
  friend class idPhysics_DynamicBase;
  friend class idClipModel;
  friend class idFXManager;
  T* list;
  int num;
  int size;
  std::int16_t granularity;
  std::uint8_t memTag;
  std::uint8_t listStatic;

#if defined(WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI) ||              \
    defined(WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_ABI)
  __forceinline
#endif
  idList()
#if !defined(WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI)
      : list(nullptr)
#endif
  {
    // Retail initializes every list through the engine allocator boundary,
    // even when the initial allocation is empty.
    idMem::Singleton();
#if !defined(WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI)
    granularity = 0;
    memTag = DefaultTag;
    listStatic = 0;
    num = 0;
    size = 0;
    ClearFree();
#endif
  }
#if defined(WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI)
  __forceinline
#endif
  idList(T* staticStorage, int staticSize, int staticGranularity) {
    idMem::Singleton();
    size = staticSize;
    num = 0;
    list = staticStorage;
    *reinterpret_cast<std::uint32_t*>(&granularity) =
        static_cast<std::uint16_t>(staticGranularity) |
        (static_cast<std::uint32_t>(DefaultTag) << 16) | 0x01000000u;
  }
  idList(const idList& other) : idList() { *this = other; }
  idList& operator=(const idList& other) {
    if (this == &other)
      return *this;
    SetNum(other.num);
    for (int i = 0; i < num; ++i)
      list[i] = other.list[i];
    return *this;
  }
  template <int OtherTag>
  idList& operator=(const idList<T, OtherTag>& other) {
    if (reinterpret_cast<const void*>(this) ==
        reinterpret_cast<const void*>(&other)) {
      return *this;
    }
    ClearFree();
    if (listStatic != 0) {
      num = other.num < size ? other.num : size;
    } else {
      num = other.num;
      size = other.size;
      granularity = other.granularity;
      memTag = other.memTag;
      if (size > 0) {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
        idMem* const memory = &idMem::Singleton();
#else
        idMem* const memory = idMem::Singleton();
#endif
        list = static_cast<T*>(memory->AllocWithLocation(
            "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : "
            "tag",
            sizeof(T) * static_cast<std::size_t>(size),
            static_cast<memTag_t>(memTag), false, ALIGN_16,
            HEAP_DEFAULTHEAP));
        if (list != nullptr) {
          for (int constructed = 0; constructed < size; ++constructed) {
            ::new (static_cast<void*>(&list[constructed])) T;
          }
        }
      }
    }
    for (int index = 0; index < num; ++index) {
      list[index] = other.list[index];
    }
    return *this;
  }
#if defined(WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI)
  __forceinline
#endif
  ~idList()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
  {
    ClearFree();
  }

  int Num() const { return num; }
  bool IsFull() const { return listStatic != 0 && num >= size; }
  T& operator[](int index) { return list[index]; }
  const T& operator[](int index) const { return list[index]; }
#if defined(WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI)
  int Append(const T& value);
#else
#if defined(WOLFTNO_RETAIL_IDLIST_NOINLINE_APPEND_ABI)
  __declspec(noinline)
#endif
  int Append(const T& value) {
#if defined(WOLFTNO_RETAIL_IDLIST_EXACT_GROWTH_ABI)
    const int required = num + 1;
    if (required > size) {
      int newSize;
      if (forceIdListsToReallocateEveryAppend) {
        newSize = required;
      } else if (granularity != 0) {
        newSize = num + granularity -
                  (num + granularity) % granularity;
      } else {
        for (newSize = 1; newSize < required; newSize *= 2) {}
      }
      Resize(newSize);
    }
#else
    EnsureCapacity(num + 1);
#endif
    if (num >= size)
      return num - 1;
    list[num] = value;
    return num++;
  }
#endif
  template <int OtherTag> int Append(const idList<T, OtherTag>& other) {
    if (other.num <= 0)
      return num;
    EnsureCapacity(num + other.num);
    if (num + other.num > size)
      return -1;
    for (int i = 0; i < other.num; ++i)
      list[num++] = other.list[i];
    return num;
  }
  int Insert(const T& value, int index) {
    index = std::max(0, std::min(index, num));
    EnsureCapacity(num + 1);
    for (int i = num; i > index; --i)
      list[i] = std::move(list[i - 1]);
    list[index] = value;
    ++num;
    return index;
  }
  bool RemoveIndex(int index) {
    if (index < 0 || index >= num)
      return false;
    for (int i = index + 1; i < num; ++i)
      list[i - 1] = std::move(list[i]);
    --num;
    return true;
  }
  bool RemoveFast(const T& value) {
    for (int index = 0; index < num; ++index) {
      if (list[index] == value) {
        list[index] = std::move(list[num - 1]);
        --num;
        return true;
      }
    }
    return false;
  }
  bool RemoveIndexFast(int index) {
    if (index < 0 || index >= num)
      return false;
    if (index != num - 1)
      list[index] = std::move(list[num - 1]);
    --num;
    return true;
  }
#if defined(WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOCINDEX_ABI)
  int AllocIndex();
#else
  int AllocIndex() {
    EnsureCapacity(num + 1);
    if (num >= size)
      return size - 1;
    return num++;
  }
#endif
  using AllocResult = std::conditional_t<
      DefaultTag == 85 || std::is_same_v<T, idMD6MorphSet> ||
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_USHORT_ALLOC_REF_ABI)
          (DefaultTag == 5 && std::is_same_v<T, std::uint16_t>) ||
#endif
#if defined(WOLFTNO_RETAIL_RIBBON_ALLOC_REF_ABI)
          std::is_same_v<T, int> || std::is_same_v<T, idRibbon*> ||
#endif
          idListAllocReturnsReference<T>::value,
      T&, T*>;
#if defined(WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI)
  __declspec(noinline)
#endif
#if defined(WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI)
  AllocResult Alloc();
#else
  AllocResult Alloc() {
#if defined(WOLFTNO_RETAIL_IDLIST_EXACT_GROWTH_ABI)
    const int required = num + 1;
    if (required > size) {
      int newSize;
      if (forceIdListsToReallocateEveryAppend) {
        newSize = required;
      } else if (granularity != 0) {
        newSize = num + granularity -
                  (num + granularity) % granularity;
      } else {
        for (newSize = 1; newSize < required; newSize *= 2) {}
      }
      Resize(newSize);
    }
#else
    EnsureCapacity(num + 1);
#endif
    const int index = num >= size ? size - 1 : num++;
    if constexpr (DefaultTag == 85 || std::is_same_v<T, idMD6MorphSet> ||
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_USHORT_ALLOC_REF_ABI)
                  (DefaultTag == 5 && std::is_same_v<T, std::uint16_t>) ||
#endif
#if defined(WOLFTNO_RETAIL_RIBBON_ALLOC_REF_ABI)
                  std::is_same_v<T, int> ||
                  std::is_same_v<T, idRibbon*> ||
#endif
                  idListAllocReturnsReference<T>::value)
      return list[index];
    else
      return index >= 0 ? &list[index] : nullptr;
  }
#endif
  void Clear() { num = 0; }
#if defined(WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI)
  __forceinline
#endif
  void ClearFree() {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        T* const allocation = list;
        if constexpr (!std::is_trivially_destructible_v<T>) {
          for (int index = 0; index < size; ++index)
            list[index].~T();
        }
        idMem::Singleton()->Free(allocation, ALIGN_16);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
  }
#if defined(WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI)
  void DeleteContents();
#else
  template <typename U = T>
  void DeleteContents() {
    for (int index = 0; index < num; ++index) {
      delete list[index];
      list[index] = nullptr;
    }
    ClearFree();
  }
#endif
  bool SetNum(int count) {
    if (count > size && !Resize(count))
      return false;
    num = std::min(count, size);
    return true;
  }
  bool SetNum(int count, const T& initialValue) {
#if defined(WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI)
    if (count > size && !Resize(count))
      return false;
    const int oldNum = num;
    const int newNum = std::min(count, size);
    for (int i = oldNum; i < newNum; ++i)
      list[i] = initialValue;
    num = newNum;
    return true;
#else
    const int oldNum = num;
    EnsureCapacity(count);
    for (int i = oldNum; i < count; ++i)
      list[i] = initialValue;
    num = count;
    return true;
#endif
  }
  void EnsureCapacity(int required) {
    if (required <= size)
      return;
    if (listStatic != 0)
      return;
    int newSize = size;
    if (newSize <= 0)
      newSize = granularity > 0 ? granularity : 1;
    while (newSize < required) {
      if (granularity > 0)
        newSize += granularity;
      else if (newSize <= 0x3fffffff)
        newSize *= 2;
      else {
        newSize = required;
        break;
      }
    }
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem* memory = &idMem::Singleton();
#else
    idMem* memory = idMem::Singleton();
#endif
    T* replacement = static_cast<T*>(memory->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
        sizeof(T) * static_cast<std::size_t>(newSize),
        static_cast<memTag_t>(memTag), false, ALIGN_16, HEAP_DEFAULTHEAP));
    if (replacement == nullptr)
      return;
    int constructed = 0;
    try {
      for (; constructed < newSize; ++constructed)
        ::new (static_cast<void*>(&replacement[constructed])) T;
    } catch (...) {
      while (constructed > 0)
        replacement[--constructed].~T();
      memory->Free(replacement, ALIGN_16);
      throw;
    }
    for (int i = 0; i < num; ++i)
      replacement[i] = std::move(list[i]);
    if ((listStatic == 0 || listStatic == 2) && list != nullptr) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (int index = 0; index < size; ++index)
          list[index].~T();
      }
      idMem::Singleton()->Free(list, ALIGN_16);
    }
    list = replacement;
    size = newSize;
    listStatic = 0;
  }
  __forceinline bool ResizeForPort(int newSize) { return Resize(newSize); }

private:
  // Recovered compilands explicitly instantiate Resize for selected
  // element/tag combinations; private access is part of its MSVC ABI name.
#if defined(WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI)
  bool Resize(int newSize);
#else
#if defined(WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI)
  __declspec(noinline)
#endif
  bool Resize(int newSize) {
    if (newSize <= 0) {
      ClearFree();
      return true;
    }
    if (newSize == size || listStatic != 0) {
      return true;
    }
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem* memory = &idMem::Singleton();
#else
    idMem* memory = idMem::Singleton();
#endif
    T* replacement = static_cast<T*>(memory->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
        sizeof(T) * static_cast<std::size_t>(newSize),
        static_cast<memTag_t>(memTag), false, ALIGN_16, HEAP_DEFAULTHEAP));
    if (replacement == nullptr)
      return false;
    int constructed = 0;
    try {
      for (; constructed < newSize; ++constructed)
        ::new (static_cast<void*>(&replacement[constructed])) T;
    } catch (...) {
      while (constructed > 0)
        replacement[--constructed].~T();
      memory->Free(replacement, ALIGN_16);
      throw;
    }
    const int copyCount = std::min(num, newSize);
    for (int i = 0; i < copyCount; ++i)
      replacement[i] = std::move(list[i]);
    if ((listStatic == 0 || listStatic == 2) && list != nullptr) {
      if constexpr (!std::is_trivially_destructible_v<T>) {
        for (int index = 0; index < size; ++index)
          list[index].~T();
      }
      idMem::Singleton()->Free(list, ALIGN_16);
    }
    list = replacement;
    size = newSize;
    num = copyCount;
    listStatic = 0;
    return true;
  }
#endif
};

static_assert(sizeof(idList<int>) == 24, "idList ABI drift");
