#pragma once

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/hashing/md.h"

#include <array>

struct idContentAddress {
  std::array<std::uint8_t, 16> digest{};
  idStr ToString() const;
  bool IsValid() const;
  static idContentAddress FromData(const void* data, std::size_t size);
  bool operator==(const idContentAddress& other) const { return digest == other.digest; }
};
struct idContentAddress2 {
  idContentAddress content;
  std::uint64_t size{};
  idStr ToString() const;
  bool IsValid() const { return content.IsValid(); }
};

class idCasSystem {
public:
  virtual ~idCasSystem() = default;
  virtual void Init(const char* root) = 0;
  virtual void Shutdown() = 0;
  virtual idContentAddress Store(const void* data, std::size_t size) = 0;
  virtual idFile* OpenCasFileRead(const idContentAddress&) = 0;
  virtual bool Contains(const idContentAddress&) const = 0;
  void InitRetail() {
    using Fn=void(*)(idCasSystem*);
    reinterpret_cast<Fn>((*reinterpret_cast<const void* const* const*>(this))[1])(this);
  }
};
class idCasSystemLocal final : public idCasSystem {
public:
  void Init(const char* root) override;
  void Shutdown() override {};
  idContentAddress Store(const void* data, std::size_t size) override;
  idFile* OpenCasFileRead(const idContentAddress&) override;
  bool Contains(const idContentAddress&) const override;
  bool MigrateFile(const idContentAddress&, const char* destinationRoot) const;
  idStr PathFor(const idContentAddress&) const;

private:
  idStr rootPath;
};

class idCasSystem2 {
public:
  virtual ~idCasSystem2() = default;
  virtual void Init(const char* root) = 0;
  virtual void Shutdown() = 0;
  virtual idContentAddress2 Store(const void*, std::size_t) = 0;
  virtual idFile* OpenPayloadForRead(const idContentAddress2&) = 0;
  void InitRetail() {
    using Fn=void(*)(idCasSystem2*);
    reinterpret_cast<Fn>((*reinterpret_cast<const void* const* const*>(this))[1])(this);
  }
};
class idCasSystemLocal2 final : public idCasSystem2 {
public:
  void Init(const char* root) override { legacy.Init(root); }
  void Shutdown() override { legacy.Shutdown(); }
  idContentAddress2 Store(const void*, std::size_t) override;
  idFile* OpenPayloadForRead(const idContentAddress2&) override;

private:
  idCasSystemLocal legacy;
};

extern idCasSystemLocal casSystemLocal;
extern idCasSystem* casSystem;
extern idCasSystemLocal2 casSystemLocal2;
extern idCasSystem2* casSystem2;
