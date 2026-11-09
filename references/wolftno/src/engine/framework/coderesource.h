#pragma once

// NEEDS_WORK(engine/framework): ABI-only interface recovered for declaration
// dependencies. Implement code-resource registration when this folder is
// recovered; do not treat this header as a completed framework module.

class idResource;
class idResourceList;

class idBaseCodeResource {
public:
  const char* name = nullptr;
  idBaseCodeResource* next = nullptr;
  idResource* r = nullptr;
  idResourceList* rl = nullptr;

  static void AllocateCodeResources();
  static void LoadCodeResources(const char* ofType = nullptr);

  static idBaseCodeResource* codeResourceList;
  static bool allocCodeResourcesHasBeenCalled;
};

template <typename T> class idCodeResource : public idBaseCodeResource {
public:
  explicit idCodeResource(const char* resourceName = nullptr) {
    name = resourceName;
    r = nullptr;
    rl = &T::resourceList;
    next = codeResourceList;
    codeResourceList = this;
  }
  T* Ptr() { return static_cast<T*>(r); }
  const T* ConstPtr() const { return static_cast<const T*>(r); }
  T* operator->() { return Ptr(); }
  const T* operator->() const { return ConstPtr(); }
  operator T*() { return Ptr(); }
  operator const T*() const { return ConstPtr(); }
};

static_assert(sizeof(idBaseCodeResource) == 32, "idBaseCodeResource ABI drift");
