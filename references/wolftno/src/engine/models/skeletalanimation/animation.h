#pragma once

#include "engine/animation/md6.h"
#include "engine/models/skeletalanimation/jointconversion.h"
#include "shared/idlib/containers/hashindex.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

extern const idUserChannelIndex NULL_USERCHANNEL_INDEX;

enum animRefType_t : int {
  REF_EMPTY = 0,
  REF_FREED = 1,
  REF_WEAK = 2,
  REF_STRONG = 3,
};

template <typename Object, typename Parent, typename Handle, int InitialSize>
class idAnimationHash {
public:
  struct entry_t {
    Object* ptr = nullptr;
    const Parent* parent = nullptr;
    idAtomicString name;
    Handle handle;
    std::uint16_t recoveredPadding = 0;
    animRefType_t refType = REF_EMPTY;
  };

  idAnimationHash();
  ~idAnimationHash();

  int IndexForName(const char* name, int* key = nullptr) const;
  Handle FindHandleByName(const char* name) const;
  Handle Add(Object* object, const Parent* parent, const char* name,
             entry_t** entry = nullptr);
  void Free();
  void FreeDynamic();

  idList<entry_t, 30> list;
  idHashIndex nameHash;
  int markStaticNum = 0;
  std::uint32_t recoveredPadding = 0;

private:
  static std::string NormalizeName(const char* name);
  static void FreeObject(Object*& object);
  void RebuildHash();
};

using idNameHash = idAnimationHash<char, char, idJointHandle_t, 256>;
using idUserChannelHash =
    idAnimationHash<char, char, idUserChannelHandle_t, 256>;
using idAliasDeclList = idList<const idDeclMD6*, 30>;
using idAliasHash =
    idAnimationHash<idAliasDeclList, idDeclMD6, idAliasHandle_t, 4096>;
using idJointConversionHash =
    idAnimationHash<idJointConversion, idJointConversion,
                    idJointConversionHandle_t, 256>;

class idAnimation : public idJointConversionAnimation {
public:
  ~idAnimation() override = default;
  virtual void Initialize() = 0;
  virtual void Shutdown() = 0;
  virtual void FreeDynamic() = 0;
  virtual idJointConversionHandle_t LoadJointConversion(const char* name) = 0;
  virtual idJointConversionHandle_t FindJointConversionHandle(
      const char* name) = 0;
  virtual const char* GetJointConversionName(
      idJointConversionHandle_t handle) const = 0;
  virtual idCrcHandle_t GetJointConversionCrc(
      idJointConversionHandle_t handle) const = 0;
  virtual idJointIndex_t GetJointIndex(idJointConversionHandle_t conversion,
                                       const char* jointName) const = 0;
  virtual idUserChannelIndex GetUserChannelIndex(
      idJointConversionHandle_t conversion,
      const char* channelName) const = 0;
  virtual void MakeDefaultJointConversion(
      const char* name, const idList<idStr, 5>& joints,
      const idList<idStr, 5>& channels) = 0;
};

class idJointResource : public idResource {
public:
  idResourceList* GetResourceList() const override;
  static idTypedResourceList<idJointResource> resourceList;
};

template <>
idJointResource*
idTypedResourceList<idJointResource>::Alloc(const char* name) const;
template <>
int idTypedResourceList<idJointResource>::AllocSize() const;

class idAnimationLocal final : public idAnimation {
public:
  idAnimationLocal();
  ~idAnimationLocal() override;

  void Initialize() override;
  void Shutdown() override;
  void FreeDynamic() override;

  idJointHandle_t RegisterJoint(const char* name) override;
  idJointHandle_t FindJointHandle(const char* name) const override;
  const char* GetJointName(idJointHandle_t handle) const override;
  idUserChannelHandle_t RegisterUserChannel(const char* name) override;
  idUserChannelHandle_t FindUserChannelHandle(const char* name) const override;
  const char* GetUserChannelName(idUserChannelHandle_t handle) const override;
  void UnregisterJointConversion(idJointConversion* conversion) override;

  int GetNumUserChannels() const;
  idJointConversionHandle_t LoadJointConversion(const char* name) override;
  idJointConversionHandle_t FindJointConversionHandle(
      const char* name) override;
  const char* GetJointConversionName(
      idJointConversionHandle_t handle) const override;
  idCrcHandle_t GetJointConversionCrc(
      idJointConversionHandle_t handle) const override;
  idJointIndex_t GetJointIndex(idJointConversionHandle_t conversion,
                               idJointHandle_t joint) const;
  idJointIndex_t GetJointIndex(idJointConversionHandle_t conversion,
                               const char* jointName) const override;
  idJointIndex_t GetJointIndex(const char* conversion,
                               const char* jointName);
  idUserChannelIndex GetUserChannelIndex(
      idJointConversionHandle_t conversion,
      idUserChannelHandle_t channel) const;
  idUserChannelIndex GetUserChannelIndex(
      idJointConversionHandle_t conversion,
      const char* channelName) const override;
  idUserChannelIndex GetUserChannelIndex(const char* conversion,
                                         const char* channelName);
  const idCompressionGroup* GetCompressionGroup(
      idJointConversionHandle_t conversion, int index) const;
  const idCompressionGroup* GetCompressionGroup(
      idJointConversionHandle_t conversion, const char* name) const;
  int GetNumCompressionGroups(idJointConversionHandle_t conversion) const;
  void MakeDefaultJointConversion(const char* name,
                                  const idList<idStr, 5>& joints,
                                  const idList<idStr, 5>& channels) override;

  idAliasHandle_t RegisterAlias(const idDeclMD6* decl,
                                const char* aliasName);
  void UnregisterAlias(const idDeclMD6* decl, idAliasHandle_t handle);
  void UnregisterAliases(const idDeclMD6* decl);
  idAliasHandle_t FindAliasHandle(const char* aliasName) const;
  idAliasHandle_t FindAliasHandle(const idDeclMD6* decl,
                                  const char* aliasName) const;
  const char* FindAliasName(idAliasHandle_t handle) const;
  idAliasHandle_t StrongLoadAlias(const idDeclMD6* decl,
                                  const char* aliasName);
  const idMD6Alias* StrongLoadAlias(const idDeclMD6* decl,
                                    idAliasHandle_t handle);
  idAliasHandle_t AddStrongAliasRef(const char* aliasName);
  void GetAliasRefs(idAliasHandle_t handle, bool strongOnly,
                    idList<const idDeclMD6*, 5>& decls) const;
  void FinalizeAliasRefs(idAliasHandle_t handle);
  void FinalizeAliasRefs();

  const char* GetOriginJointName() const;
  idJointHandle_t GetOriginJointHandle() const;
  static void VerifyAnimations(const class idCmdArgs* args);

  std::uint32_t randomSeed = 0;
  std::uint32_t recoveredSeedPadding = 0;
  idNameHash jointHash;
  idUserChannelHash userChannelHash;
  idAliasHash aliasHash;
  idJointConversionHash jointConversionHash;
  idJointHandle_t originJointHandle;
  std::uint8_t recoveredOriginPadding[6]{};
  idList<idAliasHandle_t, 30> strongAliasRefs;
};

char* idAnimation_StrDup(const char* text);

extern bool anim_showLoads;
extern bool anim_forceReload;
extern bool anim_allowWeakRefs;
extern const char* anim_debugAnimName;
extern idAnimationLocal animationLocal;
extern idAnimation* animation;

static_assert(sizeof(idAliasHandle_t) == 2);
static_assert(sizeof(idNameHash::entry_t) == 0x20);
static_assert(sizeof(idAliasHash::entry_t) == 0x20);
static_assert(sizeof(idJointConversionHash::entry_t) == 0x20);
static_assert(sizeof(idNameHash) == 0x48);
static_assert(offsetof(idAnimationLocal, jointHash) == 0x10);
static_assert(offsetof(idAnimationLocal, userChannelHash) == 0x58);
static_assert(offsetof(idAnimationLocal, aliasHash) == 0xA0);
static_assert(offsetof(idAnimationLocal, jointConversionHash) == 0xE8);
static_assert(offsetof(idAnimationLocal, originJointHandle) == 0x130);
static_assert(offsetof(idAnimationLocal, strongAliasRefs) == 0x138);
static_assert(sizeof(idAnimationLocal) == 0x150);
static_assert(sizeof(idJointResource) == 0x30);
