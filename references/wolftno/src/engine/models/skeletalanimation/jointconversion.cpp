#include "engine/models/skeletalanimation/jointconversion.h"

#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/hashing.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <vector>

const char* idJointConversion::ALL_COMPRESSIONGROUP_NAME = "all";
idTypedResourceList<idJointConversion> idJointConversion::resourceList(
    "jointconversion");

namespace {

constexpr std::uint32_t JOINT_CONVERSION_MAGIC = 0x07364a4dU;
constexpr int JOINT_CONVERSION_VERSION = 2;
constexpr int JOINT_CONVERSION_PARSER_FLAGS = 0x40230;
constexpr const char* JOINT_CONVERSION_BINARY_EXTENSION =
    "bmd6jointconversion";
constexpr const char* JOINT_CONVERSION_BINARY_DIRECTORY = "generated";

idJointConversionAnimation* jointConversionAnimation = nullptr;

std::uint16_t Swap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t Swap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00ff0000U) |
         ((value >> 8) & 0x0000ff00U) | (value >> 24);
}

bool ReadExact(idFile* file, void* data, std::size_t size) {
  return file != nullptr && file->Read(data, size) == size;
}

bool WriteExact(idFile* file, const void* data, std::size_t size) {
  return file != nullptr && file->Write(data, size) == size;
}

bool ReadBig16(idFile* file, std::uint16_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = Swap16(value);
  return true;
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  if (!ReadExact(file, &value, sizeof(value)))
    return false;
  value = Swap32(value);
  return true;
}

bool WriteBig16(idFile* file, std::uint16_t value) {
  value = Swap16(value);
  return WriteExact(file, &value, sizeof(value));
}

bool WriteBig32(idFile* file, std::uint32_t value) {
  value = Swap32(value);
  return WriteExact(file, &value, sizeof(value));
}

bool ReadBigFloat(idFile* file, float& value) {
  std::uint32_t bits = 0;
  if (!ReadBig32(file, bits))
    return false;
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

bool WriteBigFloat(idFile* file, float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return WriteBig32(file, bits);
}

bool ReadNativeString(idFile* file, idStr& value) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)))
    return false;
  if (length <= 0) {
    value.Empty();
    return true;
  }
  std::vector<char> bytes(static_cast<std::size_t>(length) + 1, 0);
  if (!ReadExact(file, bytes.data(), static_cast<std::size_t>(length)))
    return false;
  value = bytes.data();
  return true;
}

bool WriteNativeString(idFile* file, const char* value) {
  const char* safe = value != nullptr ? value : "";
  const int length = idStr::Length(safe);
  return WriteExact(file, &length, sizeof(length)) &&
         (length == 0 || WriteExact(file, safe, static_cast<std::size_t>(length)));
}

void UpdateChecksum(std::uint16_t& crc, const char* name, int index) {
  const char* safe = name != nullptr ? name : "";
  CRC16_UpdateChecksum(&crc,
                       reinterpret_cast<std::uint8_t*>(const_cast<char*>(safe)),
                       idStr::Length(safe));
  std::uint8_t indexBytes[2] = {
      static_cast<std::uint8_t>((index >> 8) & 0xff),
      static_cast<std::uint8_t>(index & 0xff)};
  CRC16_UpdateChecksum(&crc, indexBytes, 2);
}

idJointHandle_t RegisterJoint(const char* name) {
  return jointConversionAnimation != nullptr
             ? jointConversionAnimation->RegisterJoint(name)
             : NULL_JOINT;
}

idUserChannelHandle_t RegisterUserChannel(const char* name) {
  return jointConversionAnimation != nullptr
             ? jointConversionAnimation->RegisterUserChannel(name)
             : NULL_USERCHANNEL;
}

idJointHandle_t FindJoint(const char* name) {
  return jointConversionAnimation != nullptr
             ? jointConversionAnimation->FindJointHandle(name)
             : NULL_JOINT;
}

idUserChannelHandle_t FindUserChannel(const char* name) {
  return jointConversionAnimation != nullptr
             ? jointConversionAnimation->FindUserChannelHandle(name)
             : NULL_USERCHANNEL;
}

const char* JointName(idJointHandle_t handle) {
  return jointConversionAnimation != nullptr
             ? jointConversionAnimation->GetJointName(handle)
             : nullptr;
}

const char* UserChannelName(idUserChannelHandle_t handle) {
  return jointConversionAnimation != nullptr
             ? jointConversionAnimation->GetUserChannelName(handle)
             : nullptr;
}

} // namespace

void SetJointConversionAnimation(idJointConversionAnimation* service) {
  jointConversionAnimation = service;
}

idCompressionGroup::idCompressionGroup()
    : groupName(), jointHandles(), userChannelHandles() {
  jointHandles.ClearFree();
  userChannelHandles.ClearFree();
}

template <>
bool idList<idCompressionGroup::userChannel_t, 30>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearFree();
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;

  idMem* memory = idMem::Singleton();
  auto* replacement = static_cast<idCompressionGroup::userChannel_t*>(
      memory->AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
          sizeof(idCompressionGroup::userChannel_t) *
              static_cast<std::size_t>(newSize),
          static_cast<memTag_t>(memTag), false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (replacement == nullptr)
    return false;
  for (int index = 0; index < newSize; ++index)
    ::new (static_cast<void*>(&replacement[index]))
        idCompressionGroup::userChannel_t;
  const int copyCount = std::min(num, newSize);
  for (int index = 0; index < copyCount; ++index)
    replacement[index] = list[index];
  if ((listStatic == 0 || listStatic == 2) && list != nullptr)
    memory->Free(list, ALIGN_16);
  list = replacement;
  size = newSize;
  num = copyCount;
  listStatic = 0;
  return true;
}

const float
idCompressionGroup::GetJointModifier(idJointHandle_t handle) const {
  for (int index = 0; index < jointHandles.Num(); ++index)
    if (jointHandles[index].handle == handle)
      return jointHandles[index].modifier;
  return 1.0f;
}

const float idCompressionGroup::GetUserChannelModifier(
    idUserChannelHandle_t handle) const {
  for (int index = 0; index < userChannelHandles.Num(); ++index)
    if (userChannelHandles[index].handle == handle)
      return userChannelHandles[index].modifier;
  return 1.0f;
}

void idCompressionGroup::Write(idFile* file) const {
  file->WriteString(groupName.str);
  WriteBig32(file, static_cast<std::uint32_t>(jointHandles.Num()));
  for (int index = 0; index < jointHandles.Num(); ++index) {
    const joint_t& joint = jointHandles[index];
    // Retail's group writer uses "}\n" for an unresolved joint entry.
    const char* name = joint.handle.IsValid() ? JointName(joint.handle) : "}\n";
    WriteNativeString(file, name);
    WriteBigFloat(file, joint.modifier);
  }

  WriteBig32(file, static_cast<std::uint32_t>(userChannelHandles.Num()));
  for (int index = 0; index < userChannelHandles.Num(); ++index) {
    const userChannel_t& channel = userChannelHandles[index];
    const char* name =
        channel.handle.IsValid() ? UserChannelName(channel.handle) : "";
    WriteNativeString(file, name);
    WriteBigFloat(file, channel.modifier);
  }
}

void idCompressionGroup::Read(idFile* file) {
  idStr group;
  if (!ReadNativeString(file, group))
    return;
  groupName.Set(group.data);

  std::uint32_t count = 0;
  if (!ReadBig32(file, count))
    return;
  jointHandles.SetNum(static_cast<int>(count));
  idStr entryName;
  for (std::uint32_t index = 0; index < count; ++index) {
    if (!ReadNativeString(file, entryName))
      return;
    jointHandles[static_cast<int>(index)].handle =
        entryName.IsEmpty() ? NULL_JOINT : RegisterJoint(entryName.data);
    if (!ReadBigFloat(file, jointHandles[static_cast<int>(index)].modifier))
      return;
  }

  if (!ReadBig32(file, count))
    return;
  userChannelHandles.SetNum(static_cast<int>(count));
  for (std::uint32_t index = 0; index < count; ++index) {
    if (!ReadNativeString(file, entryName))
      return;
    userChannelHandles[static_cast<int>(index)].handle =
        entryName.IsEmpty() ? NULL_USERCHANNEL
                            : RegisterUserChannel(entryName.data);
    if (!ReadBigFloat(
            file, userChannelHandles[static_cast<int>(index)].modifier))
      return;
  }
}

void idCompressionGroup::Parse(idParser& source) {
  idToken token;
  source.ExpectTokenString("{");
  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (idStr::Icmp(token.data, "joints") == 0) {
      source.ExpectTokenString("{");
      while (source.ReadToken(token)) {
        if (token == "}")
          break;
        idStr name(token);
        name.ToLower();
        joint_t entry;
        entry.handle = FindJoint(name.data);
        entry.modifier = source.ParseFloat();
        jointHandles.Append(entry);
      }
    } else if (idStr::Icmp(token.data, "userchannels") == 0) {
      source.ExpectTokenString("{");
      while (source.ReadToken(token)) {
        if (token == "}")
          break;
        idStr name(token);
        name.ToLower();
        userChannel_t entry;
        entry.handle = FindUserChannel(name.data);
        entry.modifier = source.ParseFloat();
        userChannelHandles.Append(entry);
      }
    }
  }
}

idJointConversion::idJointConversion()
    : idResource(), compressionGroups(), handle(), tableJoints(),
      tableUserChannels(), checksum() {
  compressionGroups.ClearFree();
  tableJoints.SetNum(256, NULL_JOINT);
  tableUserChannels.SetNum(256, NULL_USERCHANNEL);
}

idJointConversion::~idJointConversion() noexcept(false) {
  if (jointConversionAnimation != nullptr)
    jointConversionAnimation->UnregisterJointConversion(this);
  compressionGroups.DeleteContents();
}

idResourceList* idJointConversion::GetResourceList() const {
  return &resourceList;
}

const char* idJointConversion::GetResourceTypeName() const {
  return "jointconversion";
}

idJointIndex_t
idJointConversion::GetJointIndex(const idJointHandle_t& jointHandle) const {
  if (!jointHandle.IsValid())
    return idJointIndex_t(static_cast<std::int16_t>(-1));
  for (int index = 0; index < 256; ++index)
    if (tableJoints[index] == jointHandle)
      return idJointIndex_t(static_cast<std::int16_t>(index));
  return idJointIndex_t(static_cast<std::int16_t>(-1));
}

idUserChannelIndex idJointConversion::GetUserChannelIndex(
    const idUserChannelHandle_t& channelHandle) const {
  if (!channelHandle.IsValid())
    return idUserChannelIndex(static_cast<std::int16_t>(-1));
  for (int index = 0; index < 256; ++index)
    if (tableUserChannels[index] == channelHandle)
      return idUserChannelIndex(static_cast<std::int16_t>(index));
  return idUserChannelIndex(static_cast<std::int16_t>(-1));
}

const idCompressionGroup*
idJointConversion::GetCompressionGroup(const char* groupName) const {
  for (int index = 0; index < compressionGroups.Num(); ++index)
    if (idStr::Icmp(compressionGroups[index]->groupName.str, groupName) == 0)
      return compressionGroups[index];
  return nullptr;
}

bool idJointConversion::IsJointConversionEqual(const idMD6Skel* first,
                                                const idMD6Skel* second) {
  if (first == nullptr || second == nullptr)
    return first == second;
  const idJointConversionHandle_t firstHandle(first->jointConversion);
  const idJointConversionHandle_t secondHandle(second->jointConversion);
  return IsJointConversionEqual(
      firstHandle, idCrcHandle_t(first->jointConversionChecksum), secondHandle,
      idCrcHandle_t(second->jointConversionChecksum));
}

void idJointConversion::GenerateChecksum() {
  std::uint16_t tableCrc = 0;
  CRC16_InitChecksum(&tableCrc);
  for (int index = 0; index < tableJoints.Num(); ++index) {
    if (tableJoints[index].IsValid()) {
      idStr jointName(JointName(tableJoints[index]));
      UpdateChecksum(tableCrc, jointName.data, index);
    }
  }
  for (int index = 0; index < tableUserChannels.Num(); ++index) {
    if (tableUserChannels[index].IsValid()) {
      idStr channelName(UserChannelName(tableUserChannels[index]));
      UpdateChecksum(tableCrc, channelName.data, index);
    }
  }
  for (int groupIndex = 0; groupIndex < compressionGroups.Num(); ++groupIndex) {
    const idCompressionGroup* group = compressionGroups[groupIndex];
    for (int index = 0; index < group->jointHandles.Num(); ++index) {
      if (group->jointHandles[index].handle.IsValid()) {
        idStr jointName(JointName(group->jointHandles[index].handle));
        UpdateChecksum(tableCrc, jointName.data, index);
      }
    }
  }
  checksum.value = tableCrc;
}

bool idJointConversion::WriteJointConversion_Binary(const char* filename) {
  idFileLocal file(fileSystem != nullptr ? fileSystem->OpenFileWrite(filename)
                                         : nullptr);
  if (file.file == nullptr) {
    idLib::Warning("Could not write '%s'", filename);
    return false;
  }

  WriteBig32(file.file, JOINT_CONVERSION_MAGIC);
  WriteBig32(file.file, timestamp);
  WriteBig16(file.file, checksum.value);
  WriteBig32(file.file, static_cast<std::uint32_t>(tableJoints.Num()));
  for (int index = 0; index < tableJoints.Num(); ++index)
    WriteNativeString(file.file,
                      tableJoints[index].IsValid()
                          ? JointName(tableJoints[index])
                          : "");
  WriteBig32(file.file,
             static_cast<std::uint32_t>(tableUserChannels.Num()));
  for (int index = 0; index < tableUserChannels.Num(); ++index)
    WriteNativeString(file.file,
                      tableUserChannels[index].IsValid()
                          ? UserChannelName(tableUserChannels[index])
                          : "");
  WriteBig32(file.file,
             static_cast<std::uint32_t>(compressionGroups.Num()));
  for (int index = 0; index < compressionGroups.Num(); ++index)
    compressionGroups[index]->Write(file.file);
  return true;
}

bool idJointConversion::LoadJointConversion_Binary(const char* filename) {
  idFileLocal file(fileSystem != nullptr ? fileSystem->OpenFileRead(filename)
                                         : nullptr);
  if (file.file == nullptr)
    return false;

  ClearData();
  std::uint32_t magic = 0;
  if (!ReadBig32(file.file, magic) || magic != JOINT_CONVERSION_MAGIC)
    return false;
  if (!ReadBig32(file.file, timestamp) ||
      !ReadBig16(file.file, checksum.value))
    return false;

  std::uint32_t count = 0;
  idStr jointName;
  if (!ReadBig32(file.file, count))
    return false;
  for (std::uint32_t index = 0; index < count; ++index) {
    if (!ReadNativeString(file.file, jointName))
      return false;
    tableJoints[static_cast<int>(index)] =
        jointName.IsEmpty() ? NULL_JOINT : RegisterJoint(jointName.data);
  }

  if (!ReadBig32(file.file, count))
    return false;
  {
    idStr userChannelName;
    for (std::uint32_t index = 0; index < count; ++index) {
      if (!ReadNativeString(file.file, userChannelName))
        return false;
      tableUserChannels[static_cast<int>(index)] =
          userChannelName.IsEmpty()
              ? NULL_USERCHANNEL
              : RegisterUserChannel(userChannelName.data);
    }

    if (!ReadBig32(file.file, count))
      return false;
    compressionGroups.SetNum(static_cast<int>(count));
    for (std::uint32_t index = 0; index < count; ++index) {
      idCompressionGroup* group = new idCompressionGroup;
      compressionGroups[static_cast<int>(index)] = group;
      group->Read(file.file);
    }
  }
  return true;
}

void idJointConversion::ClearData() {
  for (int index = 0; index < tableJoints.Num(); ++index)
    tableJoints[index] = NULL_JOINT;
  for (int index = 0; index < tableUserChannels.Num(); ++index)
    tableUserChannels[index] = NULL_USERCHANNEL;
  compressionGroups.DeleteContents();
}

idCompressionGroup*
idJointConversion::AllocCompressionGroup(const char* groupName) {
  if (idCompressionGroup* existing = GetCompressionGroup(groupName))
    return existing;
  idCompressionGroup* group = new idCompressionGroup;
  compressionGroups.Append(group);
  group->groupName.Set(groupName);
  return group;
}

void idJointConversion::RemoveCompressionGroup(const char* groupName) {
  for (int index = 0; index < compressionGroups.Num(); ++index) {
    if (idStr::Icmp(compressionGroups[index]->groupName.str, groupName) == 0) {
      delete compressionGroups[index];
      compressionGroups.RemoveIndex(index);
      return;
    }
  }
}

bool idJointConversion::ReloadIfStale() {
  if (fileSystem == nullptr ||
      fileSystem->GetTimestamp(name.str, false) == timestamp)
    return false;
  char binaryFileName[256];
  fileSystem->FixLongFilename(JOINT_CONVERSION_BINARY_DIRECTORY,
                              JOINT_CONVERSION_BINARY_EXTENSION, name.str,
                              binaryFileName, sizeof(binaryFileName));
  fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  ClearData();
  LoadResource();
  return true;
}

bool idJointConversion::LoadJointConversion_Text() {
  idFileLocal file(fileSystem != nullptr ? fileSystem->OpenFileRead(name.str)
                                         : nullptr);
  if (file.file == nullptr) {
    SetResourceError("Failed to open file for jointconversion '%s'", name.str);
    return false;
  }

  ClearData();
  timestamp = file.file->Timestamp();
  const int length = static_cast<int>(file.file->Length());
  std::vector<char> buffer(static_cast<std::size_t>(length) + 1, 0);
  if (file.file->Read(buffer.data(), static_cast<std::size_t>(length)) !=
      static_cast<std::size_t>(length)) {
    SetResourceError("Failed to read file for jointconversion '%s'", name.str);
    return false;
  }

  try {
    idParser source(JOINT_CONVERSION_PARSER_FLAGS);
    if (!source.LoadMemory(buffer.data(), length + 1, name.str)) {
      SetResourceError("Error parsing file for jointconversion '%s'", name.str);
      return false;
    }

    idToken token;
    source.ExpectTokenString("MD6JointConversionVersion");
    const int version = source.ParseInt();
    if (version != JOINT_CONVERSION_VERSION)
      source.Error(
          "MD6 Jointconversion has an invalid version %d should be %d.",
          version, JOINT_CONVERSION_VERSION);

    source.ExpectTokenString("jointConversion");
    source.ExpectTokenString("{");
    while (source.ReadToken(token)) {
    if (token == "}")
      break;
    const idJointHandle_t joint = RegisterJoint(token.data);
    const int index = source.ParseInt();
    if (joint.IsValid()) {
      for (int oldIndex = 0; oldIndex < 256; ++oldIndex) {
        if (tableJoints[oldIndex] == joint) {
          if (oldIndex != index)
            source.Error("MD6 Jointconversion: Jointhandle already specified "
                         "with different joint index: %s:%d.",
                         token.data, index);
          break;
        }
      }
    }
    if (static_cast<unsigned int>(index) > 255U)
      source.Error("MD6 Jointconversion has invalid joint index: %d for %s.",
                   index, token.data);
    else
      tableJoints[index] = joint;
  }

    source.ExpectTokenString("userChannelConversion");
    source.ExpectTokenString("{");
    while (source.ReadToken(token)) {
    if (token == "}")
      break;
    const idUserChannelHandle_t channel = RegisterUserChannel(token.data);
    const int index = source.ParseInt();
    if (channel.IsValid()) {
      for (int oldIndex = 0; oldIndex < 256; ++oldIndex) {
        if (tableUserChannels[oldIndex] == channel) {
          if (oldIndex != index)
            source.Error("MD6 Jointconversion: UserChannelhandle already "
                         "specified with different joint index: %s:%d.",
                         token.data, index);
          break;
        }
      }
    }
    if (static_cast<unsigned int>(index) > 255U)
      source.Error("MD6 Jointconversion has invalid joint index: %d for %s.",
                   index, token.data);
    else
      tableUserChannels[index] = channel;
  }

    idCompressionGroup* allGroup =
        AllocCompressionGroup(ALL_COMPRESSIONGROUP_NAME);
    if (source.CheckTokenString("compressionGroups")) {
    source.ExpectTokenString("{");
    while (source.ReadToken(token)) {
      if (token == "}")
        break;
      if (idStr::Icmp(token.data, "compressionGroup") != 0)
        continue;
      source.ExpectTokenType(TT_NAME, 0, token);
      idCompressionGroup* group =
          idStr::Icmp(token.data, ALL_COMPRESSIONGROUP_NAME) == 0
              ? allGroup
              : AllocCompressionGroup(token.data);
      group->Parse(source);
    }
  }
    GenerateChecksum();
    return true;
  } catch (...) {
    idLib::Error("idJointConversion::LoadJointConversion_Text - %s",
                 "parser exception");
  }
}

void idJointConversion::LoadResource() {
  if (fileSystem == nullptr)
    return;
  char binaryFileName[256];
  fileSystem->FixLongFilename(JOINT_CONVERSION_BINARY_DIRECTORY,
                              JOINT_CONVERSION_BINARY_EXTENSION, name.str,
                              binaryFileName, sizeof(binaryFileName));
  if (!LoadJointConversion_Binary(binaryFileName) &&
      LoadJointConversion_Text())
    WriteJointConversion_Binary(binaryFileName);
}

bool idJointConversion::Write() {
  if (fileSystem == nullptr)
    return false;
  char binaryFileName[256];
  fileSystem->FixLongFilename(JOINT_CONVERSION_BINARY_DIRECTORY,
                              JOINT_CONVERSION_BINARY_EXTENSION, name.str,
                              binaryFileName, sizeof(binaryFileName));
  return WriteJointConversion_Binary(binaryFileName);
}

void idJointConversion::MakeDefault(
    const char* filename, const idList<idStr, 5>& jointNames,
    const idList<idStr, 5>& userChannelNames) {
  if (fileSystem == nullptr || fileSystem->FileExists(filename, true))
    return;
  char binaryFileName[256];
  fileSystem->FixLongFilename(JOINT_CONVERSION_BINARY_DIRECTORY,
                              JOINT_CONVERSION_BINARY_EXTENSION, filename,
                              binaryFileName, sizeof(binaryFileName));
  idJointConversion conversion;
  conversion.timestamp = ~0U;
  for (int index = 0; index < jointNames.Num(); ++index)
    conversion.tableJoints[index] = RegisterJoint(jointNames[index].data);
  for (int index = 0; index < userChannelNames.Num(); ++index)
    conversion.tableUserChannels[index] =
        RegisterUserChannel(userChannelNames[index].data);
  conversion.AllocCompressionGroup(ALL_COMPRESSIONGROUP_NAME);
  conversion.GenerateChecksum();
  conversion.WriteJointConversion_Binary(binaryFileName);
}

template <>
idJointConversion* idTypedResourceList<idJointConversion>::Alloc(
    const char* resourceName) const {
  idJointConversion* resource = new idJointConversion;
  resource->resourceListPtr =
      const_cast<idTypedResourceList<idJointConversion>*>(this);
  resource->SetName(resourceName);
  return resource;
}

template <>
int idTypedResourceList<idJointConversion>::AllocSize() const {
  return 1160;
}
