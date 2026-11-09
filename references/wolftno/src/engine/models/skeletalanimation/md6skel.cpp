#include "engine/models/skeletalanimation/md6skel.h"

#include "engine/framework/timemanager.h"
#include "engine/models/skeletalanimation/animation.h"
#include "engine/models/skeletalanimation/md6anim.h"
#include "engine/models/skeletalanimation/md6model.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/hashing.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <new>
#include <vector>

idTypedResourceList<idMD6Skel> idMD6Skel::resourceList("MD6Skel");
idTypedResourceList<idMD6SkeletonConfig>
    idMD6SkeletonConfig::resourceList("skeletonconfig");

namespace {

idMD6SkeletonConfigDeclLoader skeletonConfigDeclLoader = nullptr;

std::uint16_t Swap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

std::uint32_t Swap32(std::uint32_t value) {
  return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
         ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}

void SwapFloats(float* values, int count) {
  for (int index = 0; index < count; ++index) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, values + index, sizeof(bits));
    bits = Swap32(bits);
    std::memcpy(values + index, &bits, sizeof(bits));
  }
}

bool ReadExact(idFile* file, void* destination, std::size_t size) {
  return file != nullptr && file->Read(destination, size) == size;
}

bool WriteExact(idFile* file, const void* source, std::size_t size) {
  return file != nullptr && file->Write(source, size) == size;
}

bool ReadBig16(idFile* file, std::uint16_t& value) {
  return ReadExact(file, &value, sizeof(value)) && ((value = Swap16(value)), true);
}

bool ReadBig32(idFile* file, std::uint32_t& value) {
  return ReadExact(file, &value, sizeof(value)) && ((value = Swap32(value)), true);
}

bool WriteBig16(idFile* file, std::uint16_t value) {
  value = Swap16(value);
  return WriteExact(file, &value, sizeof(value));
}

bool WriteBig32(idFile* file, std::uint32_t value) {
  value = Swap32(value);
  return WriteExact(file, &value, sizeof(value));
}

bool ReadNativeString(idFile* file, idStr& value) {
  int length = 0;
  if (!ReadExact(file, &length, sizeof(length)) || length < 0 ||
      length > 1024 * 1024)
    return false;
  if (length == 0) {
    value.Empty();
    return true;
  }
  value.Fill(' ', length);
  return ReadExact(file, &value[0], static_cast<std::size_t>(length));
}

std::uint16_t Align16(std::uint32_t value) {
  return static_cast<std::uint16_t>((value + 15u) & ~15u);
}

std::uint16_t Align2(std::uint32_t value) {
  return static_cast<std::uint16_t>((value + 1u) & ~1u);
}

std::uint8_t WeightByte(float value) {
  const int scaled = static_cast<int>(value * 255.0f + 0.5f);
  return static_cast<std::uint8_t>(scaled < 0 ? 0 : scaled > 255 ? 255
                                                                  : scaled);
}

constexpr std::uint32_t MD6_SKEL_MAGIC = 0x0b36534du;
constexpr std::uint32_t MD6_SKEL_OLD_MAGIC = 0x0836534du;
constexpr const char* SKELETONCONFIG_VERSION_STRING =
    "SkeletonConfigVersion";
constexpr memTag_t TAG_MD6_SKEL = static_cast<memTag_t>(0x13);

struct idMD6SkelFileHeader {
  std::uint32_t timestamp;
  std::uint32_t userChannelTimestamp;
  std::uint32_t skelSize;
  std::uint16_t jointConversionChecksum;
  std::uint16_t padding;
};

static_assert(sizeof(idMD6SkelFileHeader) == 16);

void InitializeJointMatrices(idJointMat* joints, int count,
                             const idVec3& offset) {
  if (joints == nullptr)
    return;
  for (int index = 0; index < count; ++index) {
    std::memset(joints[index].mat, 0, sizeof(joints[index].mat));
    joints[index].mat[0] = 1.0f;
    joints[index].mat[5] = 1.0f;
    joints[index].mat[10] = 1.0f;
    joints[index].mat[3] = offset.x;
    joints[index].mat[7] = offset.y;
    joints[index].mat[11] = offset.z;
  }
}

idStr ListedSkeletonPath(const char* basePath, const char* listedName) {
  idStr result(listedName != nullptr ? listedName : "");
  if (basePath == nullptr || basePath[0] == '\0' || result.IsEmpty())
    return result;
  const int baseLength = idStr::Length(basePath);
  if (idStr::Icmpn(result.data, basePath, baseLength) == 0 &&
      (result[baseLength] == '/' || result[baseLength] == '\\' ||
       result[baseLength] == '\0')) {
    return result;
  }
  result = basePath;
  result.AppendPath(listedName);
  return result;
}

} // namespace

void SetMD6SkeletonConfigDeclLoader(
    idMD6SkeletonConfigDeclLoader loader) {
  skeletonConfigDeclLoader = loader;
}

idMD6Skel::idMD6Skel() { resourceListPtr = &resourceList; }

idMD6Skel::~idMD6Skel() noexcept(false) {
  if (data != nullptr)
    idMem::Singleton()->Free(data, ALIGN_16);
  data = nullptr;
}

idResourceList* idMD6Skel::GetResourceList() const { return &resourceList; }

idUserChannelHandle_t
idMD6Skel::GetUserChannelHandle(int userChannelIndex) const {
  if (data == nullptr || userChannelIndex < 0 ||
      userChannelIndex >= data->numUserChannels) {
    return NULL_USERCHANNEL;
  }
  const auto* handles = reinterpret_cast<const std::uint16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) +
      data->userChannelHandleTblOffset);
  return idUserChannelHandle_t(handles[userChannelIndex]);
}

idJointIndex_t idMD6Skel::GetJointIndex(idJointHandle_t handle) const {
  if (data == nullptr || !handle.IsValid())
    return idJointIndex_t(static_cast<std::int16_t>(-1));
  const auto* handles = reinterpret_cast<const std::uint16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->jointHandleTblOffset);
  for (int index = 0; index < data->numJoints; ++index) {
    if (handles[index] == handle.value)
      return idJointIndex_t(static_cast<std::int16_t>(index));
  }
  return idJointIndex_t(static_cast<std::int16_t>(-1));
}

idJointIndex_t idMD6Skel::GetJointIndex(const char* jointName) const {
  return animation != nullptr
             ? GetJointIndex(animation->RegisterJoint(jointName))
             : idJointIndex_t(static_cast<std::int16_t>(-1));
}

idJointHandle_t idMD6Skel::GetJointHandle(int jointIndex) const {
  if (data == nullptr || jointIndex < 0 || jointIndex >= data->numJoints)
    return NULL_JOINT;
  const auto* handles = reinterpret_cast<const std::uint16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->jointHandleTblOffset);
  return idJointHandle_t(handles[jointIndex]);
}

idUserChannelIndex
idMD6Skel::GetUserChannelIndex(idUserChannelHandle_t handle) const {
  if (data == nullptr)
    return idUserChannelIndex(static_cast<std::int16_t>(-1));
  const auto* handles = reinterpret_cast<const std::uint16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) +
      data->userChannelHandleTblOffset);
  for (int index = 0; index < data->numUserChannels; ++index) {
    if (handles[index] == handle.value)
      return idUserChannelIndex(static_cast<std::int16_t>(index));
  }
  return idUserChannelIndex(static_cast<std::int16_t>(-1));
}

idUserChannelIndex
idMD6Skel::GetUserChannelIndex(const char* channelName) const {
  return animation != nullptr
             ? GetUserChannelIndex(animation->RegisterUserChannel(channelName))
             : idUserChannelIndex(static_cast<std::int16_t>(-1));
}

bool idMD6Skel::IsJointChildOf(idJointIndex_t joint,
                               idJointIndex_t parent) const {
  if (data == nullptr || joint.value < 0 || joint.value >= data->numJoints)
    return false;
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->parentTblOffset);
  int current = parents[joint.value];
  while (current >= 0) {
    if (current == parent.value)
      return true;
    current = parents[current];
  }
  return false;
}

int idMD6Skel::NumJoints() const { return data != nullptr ? data->numJoints : 0; }

const char* idMD6Skel::GetJointName(int jointIndex) const {
  if (animation == nullptr)
    return "";
  return animation->GetJointName(GetJointHandle(jointIndex));
}

void idJointQuat::FromMat4(const idMat4& matrix) {
  const float trace = matrix.mat[0].x + matrix.mat[1].y + matrix.mat[2].z;
  if (trace > 0.0f) {
    const float root = std::sqrt(std::max(trace + 1.0f,
                                         std::numeric_limits<float>::min()));
    const float scale = 0.5f / root;
    jointQuat[3] = 0.5f * root;
    jointQuat[0] = (matrix.mat[2].y - matrix.mat[1].z) * scale;
    jointQuat[1] = (matrix.mat[0].z - matrix.mat[2].x) * scale;
    jointQuat[2] = (matrix.mat[1].x - matrix.mat[0].y) * scale;
  } else {
    static constexpr int next[3] = {1, 2, 0};
    int first = matrix.mat[1].y > matrix.mat[0].x ? 1 : 0;
    if (matrix.mat[2].z > matrix.mat[first][first])
      first = 2;
    const int second = next[first];
    const int third = next[second];
    const float root = std::sqrt(std::max(
        matrix.mat[first][first] - matrix.mat[second][second] -
                matrix.mat[third][third] + 1.0f,
        std::numeric_limits<float>::min()));
    const float difference =
        matrix.mat[third][second] - matrix.mat[second][third];
    const float scale = std::copysign(0.5f / root, difference);
    jointQuat[first] = scale * (root * root);
    jointQuat[3] = difference * scale;
    jointQuat[second] =
        (matrix.mat[first][second] + matrix.mat[second][first]) * scale;
    jointQuat[third] =
        (matrix.mat[third][first] + matrix.mat[first][third]) * scale;
  }
  jointQuat[4] = matrix.mat[0].w;
  jointQuat[5] = matrix.mat[1].w;
  jointQuat[6] = matrix.mat[2].w;
  jointQuat[7] = 0.0f;
}

void idMD6Skel::SwapHeader(idMD6SkelData* skeletonData) {
  if (skeletonData == nullptr)
    return;
  auto* words = reinterpret_cast<std::uint16_t*>(skeletonData);
  for (int index = 0; index < 26; ++index)
    words[index] = Swap16(words[index]);
}

void idMD6Skel::SwapData(idMD6SkelData* skeletonData) {
  if (skeletonData == nullptr)
    return;
  const int paddedJoints = (skeletonData->numJoints + 7) & ~7;
  const int paddedChannels = (skeletonData->numUserChannels + 7) & ~7;
  auto* bytes = reinterpret_cast<std::uint8_t*>(skeletonData);
  SwapFloats(reinterpret_cast<float*>(bytes + skeletonData->basePoseOffset),
             paddedJoints * 10 + paddedChannels);
  SwapFloats(
      reinterpret_cast<float*>(bytes + skeletonData->inverseBasePoseOffset),
      paddedJoints * 12);
  SwapFloats(reinterpret_cast<float*>(
                 bytes + skeletonData->inverseBasePoseQuatOffset),
             paddedJoints * 8);
  for (std::uint16_t offset : {skeletonData->parentTblOffset,
                               skeletonData->jointHandleTblOffset,
                               skeletonData->userChannelHandleTblOffset}) {
    const int count = offset == skeletonData->userChannelHandleTblOffset
                          ? paddedChannels
                          : paddedJoints;
    auto* values = reinterpret_cast<std::uint16_t*>(bytes + offset);
    for (int index = 0; index < count; ++index)
      values[index] = Swap16(values[index]);
  }
}

bool idMD6Skel::ReloadIfStale() {
  const bool conversionCurrent =
      jointConversion.IsValid() && jointConversionChecksum.IsValid() &&
      animation != nullptr &&
      animation->GetJointConversionCrc(jointConversion) ==
          jointConversionChecksum;
  const bool skeletonCurrent =
      fileSystem != nullptr &&
      fileSystem->GetTimestamp(name.str, false) == timestamp;
  const bool channelsCurrent =
      userChannelFilename.str[0] == '\0' ||
      (fileSystem != nullptr &&
       fileSystem->GetTimestamp(userChannelFilename.str, false) ==
           userChannelTimestamp);
  if (conversionCurrent && skeletonCurrent && channelsCurrent)
    return false;

  if (fileSystem != nullptr) {
    char binaryFileName[256]{};
    fileSystem->FixLongFilename("generated", "bmd6skl", name.str,
                                binaryFileName, sizeof(binaryFileName));
    fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  }
  if (data != nullptr)
    idMem::Singleton()->Free(data, ALIGN_16);
  data = nullptr;
  LoadResource();
  return true;
}

bool idMD6Skel::LoadBinary(const char* fileName, bool directBinary) {
  if (fileSystem == nullptr || fileName == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileRead(fileName, true, directBinary));
  if (file.file == nullptr)
    return false;

  std::uint32_t magic = 0;
  if (!ReadBig32(file.file, magic))
    return false;

  idMD6SkelFileHeader header{0xffffffffu, 0xffffffffu, 0u, 0xffffu, 0u};
  if (magic == MD6_SKEL_MAGIC) {
    if (!ReadBig32(file.file, header.timestamp) ||
        !ReadBig32(file.file, header.userChannelTimestamp) ||
        !ReadBig32(file.file, header.skelSize) ||
        !ReadBig16(file.file, header.jointConversionChecksum) ||
        !ReadBig16(file.file, header.padding)) {
      return false;
    }
    idStr channelFile;
    if (!ReadNativeString(file.file, channelFile))
      return false;
    userChannelFilename.Set(channelFile.data);
  } else if (magic == MD6_SKEL_OLD_MAGIC &&
             idLib::production != PROD_DEVELOPMENT) {
    idLib::Note("idMD6Skel '%s' was loaded in old format", name.str);
    if (!ReadBig32(file.file, header.timestamp) ||
        !ReadBig32(file.file, header.skelSize)) {
      return false;
    }
  } else {
    if ((magic & 0x00ffffffu) != 0x0036534du)
      idLib::Warning("File is not a valid bmd6skl file.");
    return false;
  }

  timestamp = header.timestamp;
  userChannelTimestamp = header.userChannelTimestamp;
  jointConversionChecksum = idCrcHandle_t(header.jointConversionChecksum);

  const bool legacySupersoldat =
      idStr::Icmp(name.str,
                  "md6/models/characters/supersoldat/supersoldat46/"
                  "supersoldat46.md6skl") == 0 ||
      idStr::Icmp(name.str,
                  "md6/models/characters/supersoldat/supersoldat60/"
                  "supersoldat60.md6skl") == 0;
  const std::size_t allocationSize =
      static_cast<std::size_t>(header.skelSize) +
      (legacySupersoldat ? 64u : 0u);
  idMD6SkelData* loaded = static_cast<idMD6SkelData*>(
      idMem::Singleton()->AllocWithLocation(
          "engine/models/skeletalanimation/md6skel.cpp:LoadBinary",
          allocationSize, TAG_MD6_SKEL, false, ALIGN_16, HEAP_DEFAULTHEAP));
  if (loaded == nullptr || !ReadExact(file.file, loaded, header.skelSize)) {
    if (loaded != nullptr)
      idMem::Singleton()->Free(loaded, ALIGN_16);
    return false;
  }
  SwapHeader(loaded);
  SwapData(loaded);
  if (loaded->size != header.skelSize || loaded->size < sizeof(*loaded)) {
    idMem::Singleton()->Free(loaded, ALIGN_16);
    return false;
  }

  if (legacySupersoldat) {
    const std::uint16_t oldInverseOffset = loaded->inverseBasePoseOffset;
    const std::uint16_t oldSize = loaded->size;
    std::vector<std::uint8_t> tail(oldSize - oldInverseOffset);
    std::memcpy(tail.data(), reinterpret_cast<std::uint8_t*>(loaded) +
                                 oldInverseOffset,
                tail.size());
    for (int index = 3; index < idMD6SkelData::MAX_WEIGHT_SETS; ++index)
      loaded->jointWeightOffsets[index] += 64;
    loaded->inverseBasePoseOffset += 64;
    loaded->inverseBasePoseQuatOffset += 64;
    loaded->size += 64;
    std::memcpy(reinterpret_cast<std::uint8_t*>(loaded) +
                    loaded->inverseBasePoseOffset,
                tail.data(), tail.size());
    auto* addedWeights = reinterpret_cast<std::uint8_t*>(loaded) +
                         loaded->jointWeightOffsets[2];
    std::memset(addedWeights, 0, 64);
    constexpr std::uint8_t torsoJoints[] = {
        2, 3, 4, 5, 6, 7, 8, 9, 11, 27, 29, 30, 31, 32, 34, 50, 52};
    for (std::uint8_t joint : torsoJoints)
      addedWeights[joint] = 0xffu;
  }

  idStr conversionName;
  if (!ReadNativeString(file.file, conversionName)) {
    idMem::Singleton()->Free(loaded, ALIGN_16);
    return false;
  }
  jointConversion =
      animation != nullptr
          ? animation->FindJointConversionHandle(conversionName.data)
          : NULL_JOINT_CONVERSION;
  if (magic == MD6_SKEL_OLD_MAGIC) {
    std::uint16_t checksum = 0xffffu;
    if (!ReadBig16(file.file, checksum)) {
      idMem::Singleton()->Free(loaded, ALIGN_16);
      return false;
    }
    jointConversionChecksum = idCrcHandle_t(checksum);
  }

  auto* jointHandles = reinterpret_cast<std::uint16_t*>(
      reinterpret_cast<std::uint8_t*>(loaded) + loaded->jointHandleTblOffset);
  auto* userHandles = reinterpret_cast<std::uint16_t*>(
      reinterpret_cast<std::uint8_t*>(loaded) +
      loaded->userChannelHandleTblOffset);
  const int paddedJoints = (loaded->numJoints + 7) & ~7;
  const int paddedChannels = (loaded->numUserChannels + 7) & ~7;
  std::fill(jointHandles, jointHandles + paddedJoints,
            static_cast<std::uint16_t>(0xffffu));
  std::fill(userHandles, userHandles + paddedChannels,
            static_cast<std::uint16_t>(0xffffu));
  const int serializedJoints = magic == MD6_SKEL_MAGIC ? loaded->numJoints
                                                       : paddedJoints;
  const int serializedChannels = magic == MD6_SKEL_MAGIC
                                     ? loaded->numUserChannels
                                     : paddedChannels;
  idStr entryName;
  for (int index = 0; index < serializedJoints; ++index) {
    if (!ReadNativeString(file.file, entryName)) {
      idMem::Singleton()->Free(loaded, ALIGN_16);
      return false;
    }
    if (!entryName.IsEmpty() && animation != nullptr)
      jointHandles[index] = animation->RegisterJoint(entryName.data).value;
  }
  for (int index = 0; index < serializedChannels; ++index) {
    if (!ReadNativeString(file.file, entryName)) {
      idMem::Singleton()->Free(loaded, ALIGN_16);
      return false;
    }
    if (!entryName.IsEmpty() && animation != nullptr)
      userHandles[index] =
          animation->RegisterUserChannel(entryName.data).value;
  }

  float* defaults = reinterpret_cast<float*>(
      reinterpret_cast<std::uint8_t*>(loaded) + loaded->basePoseOffset) +
                    paddedJoints * 10;
  std::fill(defaults, defaults + paddedChannels, 0.0f);
  if (!ReadExact(file.file, defaults,
                 sizeof(float) * static_cast<std::size_t>(serializedChannels))) {
    idMem::Singleton()->Free(loaded, ALIGN_16);
    return false;
  }
  SwapFloats(defaults, serializedChannels);
  std::uint32_t trailingMagic = 0;
  if (!ReadBig32(file.file, trailingMagic) ||
      (trailingMagic != MD6_SKEL_MAGIC &&
       trailingMagic != MD6_SKEL_OLD_MAGIC)) {
    idLib::Warning("%s is corrupt", fileName);
    idMem::Singleton()->Free(loaded, ALIGN_16);
    return false;
  }

  if (data != nullptr)
    idMem::Singleton()->Free(data, ALIGN_16);
  data = loaded;
  return true;
}

bool idMD6Skel::WriteBinary(const char* fileName) {
  if (fileSystem == nullptr || fileName == nullptr || data == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileWrite(fileName, FSPATH_BASE));
  if (file.file == nullptr) {
    idLib::Warning("Could not open %s", fileName);
    return false;
  }
  idLib::Printf("Writing %s\n", fileName);

  if (!WriteBig32(file.file, MD6_SKEL_MAGIC) ||
      !WriteBig32(file.file, timestamp) ||
      !WriteBig32(file.file, userChannelTimestamp) ||
      !WriteBig32(file.file, data->size) ||
      !WriteBig16(file.file, jointConversionChecksum.value) ||
      !WriteBig16(file.file, 0) ||
      file.file->WriteString(userChannelFilename.str) !=
          sizeof(int) + std::strlen(userChannelFilename.str)) {
    return false;
  }

  std::vector<std::uint8_t> swapped(data->size);
  std::memcpy(swapped.data(), data, data->size);
  auto* swappedData = reinterpret_cast<idMD6SkelData*>(swapped.data());
  SwapData(swappedData);
  SwapHeader(swappedData);
  if (!WriteExact(file.file, swapped.data(), swapped.size()))
    return false;

  const char* conversionName =
      animation != nullptr
          ? animation->GetJointConversionName(jointConversion)
          : nullptr;
  if (file.file->WriteString(conversionName) !=
      sizeof(int) + (conversionName != nullptr ? std::strlen(conversionName)
                                               : 0u))
    return false;
  for (int index = 0; index < data->numJoints; ++index) {
    const idJointHandle_t handle = GetJointHandle(index);
    const char* jointName =
        handle.IsValid() && animation != nullptr
            ? animation->GetJointName(handle)
            : nullptr;
    if (file.file->WriteString(jointName) !=
        sizeof(int) + (jointName != nullptr ? std::strlen(jointName) : 0u))
      return false;
  }
  for (int index = 0; index < data->numUserChannels; ++index) {
    const idUserChannelHandle_t handle = GetUserChannelHandle(index);
    const char* channelName =
        handle.IsValid() && animation != nullptr
            ? animation->GetUserChannelName(handle)
            : nullptr;
    if (file.file->WriteString(channelName) !=
        sizeof(int) +
            (channelName != nullptr ? std::strlen(channelName) : 0u))
      return false;
  }
  const int paddedJoints = (data->numJoints + 7) & ~7;
  const float* defaults = reinterpret_cast<const float*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->basePoseOffset) +
                          paddedJoints * 10;
  for (int index = 0; index < data->numUserChannels; ++index) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, defaults + index, sizeof(bits));
    if (!WriteBig32(file.file, bits))
      return false;
  }
  return WriteBig32(file.file, MD6_SKEL_MAGIC);
}

bool idMD6Skel::MakeSkeletonData(
    unsigned int numJoints, float* basePose,
    const float* secondaryBasePose, idJointHandle_t* jointHandleTable,
    std::int16_t* parentTable, int numUserChannels, float* userChannels,
    idUserChannelHandle_t* userChannelHandleTable,
    std::uint8_t** jointWeights, std::uint8_t** userWeights) {
  if (numJoints > 0xffffu || numUserChannels < 0 ||
      numUserChannels > 0xffff ||
      (numJoints != 0 &&
       (basePose == nullptr || jointHandleTable == nullptr ||
        parentTable == nullptr)) ||
      (numUserChannels != 0 &&
       (userChannels == nullptr || userChannelHandleTable == nullptr))) {
    return false;
  }

  const int paddedJoints = (static_cast<int>(numJoints) + 7) & ~7;
  const int paddedChannels = (numUserChannels + 7) & ~7;
  std::vector<float> baseR(static_cast<std::size_t>(paddedJoints) * 4, 0.0f);
  std::vector<float> baseS(static_cast<std::size_t>(paddedJoints) * 3, 1.0f);
  std::vector<float> baseT(static_cast<std::size_t>(paddedJoints) * 3, 0.0f);
  std::vector<float> baseU(static_cast<std::size_t>(paddedChannels), 0.0f);
  std::vector<std::int16_t> parents(static_cast<std::size_t>(paddedJoints),
                                    -1);
  std::vector<idJointHandle_t> jointHandles(
      static_cast<std::size_t>(paddedJoints), NULL_JOINT);
  std::vector<idUserChannelHandle_t> userHandles(
      static_cast<std::size_t>(paddedChannels), NULL_USERCHANNEL);

  for (unsigned int joint = 0; joint < numJoints; ++joint) {
    const float* sourceR = basePose + joint * 4;
    float* destinationR = baseR.data() + joint * 4;
    const float sign = sourceR[3] < 0.0f ? -1.0f : 1.0f;
    for (int component = 0; component < 4; ++component)
      destinationR[component] = sourceR[component] * sign;
    const float* sourceS = basePose + paddedJoints * 4 + joint * 4;
    const float* sourceT = basePose + paddedJoints * 8 + joint * 4;
    std::copy_n(sourceS, 3, baseS.data() + joint * 3);
    std::copy_n(sourceT, 3, baseT.data() + joint * 3);
    parents[joint] = parentTable[joint];
    jointHandles[joint] = jointHandleTable[joint];
  }
  for (int channel = 0; channel < numUserChannels; ++channel) {
    baseU[channel] = userChannels[channel];
    userHandles[channel] = userChannelHandleTable[channel];
  }
  for (int joint = static_cast<int>(numJoints); joint < paddedJoints;
       ++joint)
    baseR[joint * 4 + 3] = 1.0f;

  std::array<std::vector<std::uint8_t>, idMD6SkelData::MAX_WEIGHT_SETS>
      packedJointWeights;
  std::array<std::vector<std::uint8_t>, idMD6SkelData::MAX_WEIGHT_SETS>
      packedUserWeights;
  for (int set = 0; set < idMD6SkelData::MAX_WEIGHT_SETS; ++set) {
    if (jointWeights != nullptr && jointWeights[set] != nullptr) {
      packedJointWeights[set].assign(static_cast<std::size_t>(paddedJoints),
                                     0u);
      std::copy_n(jointWeights[set], numJoints,
                  packedJointWeights[set].data());
    }
    if (userWeights != nullptr && userWeights[set] != nullptr) {
      packedUserWeights[set].assign(static_cast<std::size_t>(paddedChannels),
                                    0u);
      std::copy_n(userWeights[set], numUserChannels,
                  packedUserWeights[set].data());
    }
  }

  std::vector<std::uint8_t> parentBigEndian(numJoints * 2u);
  for (unsigned int joint = 0; joint < numJoints; ++joint) {
    const std::uint16_t value = static_cast<std::uint16_t>(parentTable[joint]);
    parentBigEndian[joint * 2] = static_cast<std::uint8_t>(value >> 8u);
    parentBigEndian[joint * 2 + 1] = static_cast<std::uint8_t>(value);
  }
  std::uint16_t parentCrc = 0;
  CRC16_InitChecksum(&parentCrc);
  CRC16_UpdateChecksum(&parentCrc, parentBigEndian.data(),
                       static_cast<int>(parentBigEndian.size()));

  idMD6SkelData layout{};
  layout.numJoints = static_cast<std::uint16_t>(numJoints);
  layout.numUserChannels = static_cast<std::uint16_t>(numUserChannels);
  layout.parentTblCrc = idCrcHandle_t(parentCrc);
  layout.basePoseOffset = sizeof(idMD6SkelData);
  std::uint32_t cursor = layout.basePoseOffset +
                         static_cast<std::uint32_t>(
                             (baseR.size() + baseS.size() + baseT.size() +
                              baseU.size()) *
                             sizeof(float));
  layout.parentTblOffset = Align2(cursor);
  cursor = layout.parentTblOffset + paddedJoints * sizeof(std::int16_t);
  layout.jointHandleTblOffset = Align2(cursor);
  cursor = layout.jointHandleTblOffset +
           paddedJoints * sizeof(idJointHandle_t);
  layout.userChannelHandleTblOffset = Align2(cursor);
  cursor = layout.userChannelHandleTblOffset +
           paddedChannels * sizeof(idUserChannelHandle_t);
  for (int set = 0; set < idMD6SkelData::MAX_WEIGHT_SETS; ++set) {
    layout.jointWeightOffsets[set] = static_cast<std::uint16_t>(cursor);
    cursor += static_cast<std::uint32_t>(packedJointWeights[set].size());
  }
  for (int set = 0; set < idMD6SkelData::MAX_WEIGHT_SETS; ++set) {
    layout.userWeightOffsets[set] = static_cast<std::uint16_t>(cursor);
    cursor += static_cast<std::uint32_t>(packedUserWeights[set].size());
  }
  layout.inverseBasePoseOffset = Align16(cursor);
  cursor = layout.inverseBasePoseOffset + paddedJoints * 12u * sizeof(float);
  layout.inverseBasePoseQuatOffset = Align16(cursor);
  cursor = layout.inverseBasePoseQuatOffset +
           paddedJoints * sizeof(idJointQuat);
  const std::uint32_t finalSize = (cursor + 15u) & ~15u;
  if (finalSize > 0xffffu)
    return false;
  layout.size = static_cast<std::uint16_t>(finalSize);

  auto* replacement = static_cast<idMD6SkelData*>(
      idMem::Singleton()->AllocWithLocation(
          "engine/models/skeletalanimation/md6skel.cpp:MakeSkeletonData",
          finalSize, TAG_MD6_SKEL, true, ALIGN_16, HEAP_DEFAULTHEAP));
  if (replacement == nullptr)
    return false;
  *replacement = layout;
  auto* bytes = reinterpret_cast<std::uint8_t*>(replacement);
  float* packedBase = reinterpret_cast<float*>(bytes + layout.basePoseOffset);
  std::memcpy(packedBase, baseR.data(), baseR.size() * sizeof(float));
  packedBase += baseR.size();
  std::memcpy(packedBase, baseS.data(), baseS.size() * sizeof(float));
  packedBase += baseS.size();
  std::memcpy(packedBase, baseT.data(), baseT.size() * sizeof(float));
  packedBase += baseT.size();
  std::memcpy(packedBase, baseU.data(), baseU.size() * sizeof(float));
  std::memcpy(bytes + layout.parentTblOffset, parents.data(),
              parents.size() * sizeof(parents[0]));
  std::memcpy(bytes + layout.jointHandleTblOffset, jointHandles.data(),
              jointHandles.size() * sizeof(jointHandles[0]));
  std::memcpy(bytes + layout.userChannelHandleTblOffset, userHandles.data(),
              userHandles.size() * sizeof(userHandles[0]));
  for (int set = 0; set < idMD6SkelData::MAX_WEIGHT_SETS; ++set) {
    if (!packedJointWeights[set].empty())
      std::memcpy(bytes + layout.jointWeightOffsets[set],
                  packedJointWeights[set].data(),
                  packedJointWeights[set].size());
    if (!packedUserWeights[set].empty())
      std::memcpy(bytes + layout.userWeightOffsets[set],
                  packedUserWeights[set].data(),
                  packedUserWeights[set].size());
  }

  float* inverseMatrices =
      reinterpret_cast<float*>(bytes + layout.inverseBasePoseOffset);
  auto* inverseQuats = reinterpret_cast<idJointQuat*>(
      bytes + layout.inverseBasePoseQuatOffset);
  if (paddedJoints > 0) {
    alignas(16) std::array<float, 0x1000> temporary{};
    idMD6Blend::TransformLocalJointsToModelMatrices(
        replacement, temporary.data(), sizeof(temporary), nullptr, 0, 0,
        &vec3_origin.x, true, basePose,
        reinterpret_cast<__m128i*>(basePose + paddedJoints * 4),
        basePose + paddedJoints * 8,
        reinterpret_cast<__m128*>(inverseMatrices), nullptr, nullptr);
    for (int joint = 0; joint < paddedJoints; ++joint) {
      float* matrixRows = inverseMatrices + joint * 12;
      idMat4 matrix{};
      for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
          matrix[row][column] = matrixRows[row * 4 + column];
      matrix[3][3] = 1.0f;
      matrix.InverseSelf();
      for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 4; ++column)
          matrixRows[row * 4 + column] = matrix[row][column];
      inverseQuats[joint].FromMat4(matrix);
    }
  }

  if (secondaryBasePose != nullptr) {
    for (unsigned int joint = 0; joint < numJoints; ++joint) {
      const float* sourceR = secondaryBasePose + joint * 4;
      float* destinationR = baseR.data() + joint * 4;
      const float sign = sourceR[3] < 0.0f ? -1.0f : 1.0f;
      for (int component = 0; component < 4; ++component)
        destinationR[component] = sourceR[component] * sign;
      std::copy_n(secondaryBasePose + paddedJoints * 4 + joint * 4, 3,
                  baseS.data() + joint * 3);
      std::copy_n(secondaryBasePose + paddedJoints * 8 + joint * 4, 3,
                  baseT.data() + joint * 3);
    }
    packedBase = reinterpret_cast<float*>(bytes + layout.basePoseOffset);
    std::memcpy(packedBase, baseR.data(), baseR.size() * sizeof(float));
    packedBase += baseR.size();
    std::memcpy(packedBase, baseS.data(), baseS.size() * sizeof(float));
    packedBase += baseS.size();
    std::memcpy(packedBase, baseT.data(), baseT.size() * sizeof(float));
  }

  if (data != nullptr)
    idMem::Singleton()->Free(data, ALIGN_16);
  data = replacement;
  return true;
}

void idMD6Skel::GetJointList(
    const idMD6Model* model, const char* jointNames,
    idList<idJointIndex_t, 5>& jointList) const {
  jointList.ClearFree();
  idLexer source(LEXFL_NOFATALERRORS | LEXFL_ALLOWPATHNAMES);
  source.LoadMemory(jointNames != nullptr ? jointNames : "",
                    jointNames != nullptr ? idStr::Length(jointNames) : 0,
                    "GetJointList");
  int modifiers = 0;
  idToken token;
  while (source.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION) {
      if (token == "*")
        modifiers |= 1;
      else if (token == "-")
        modifiers |= 2;
      continue;
    }
    if (token.type != TT_NAME)
      continue;
    token.ToLower();
    const idJointIndex_t joint = GetJointIndex(token.data);
    if (joint.value < 0) {
      idLib::Warning("Unknown joint '%s' in '%s' for model '%s'", token.data,
                     jointNames != nullptr ? jointNames : "",
                     model != nullptr ? model->name.str : "");
      modifiers = 0;
      continue;
    }
    if ((modifiers & 2) != 0) {
      for (int index = 0; index < jointList.Num(); ++index) {
        if (jointList[index].value == joint.value) {
          jointList.RemoveIndex(index);
          break;
        }
      }
      if ((modifiers & 1) != 0)
        SubChildren_r(this, joint, jointList);
    } else {
      bool exists = false;
      for (int index = 0; index < jointList.Num(); ++index)
        exists = exists || jointList[index].value == joint.value;
      if (!exists)
        jointList.Append(joint);
      if ((modifiers & 1) != 0)
        AddChildren_r(this, joint, jointList);
    }
    modifiers = 0;
  }
}

bool idMD6Skel::Parse(idParser& parser) {
  if (!parser.ExpectTokenString("MD6Version"))
    return false;
  const int version = parser.ParseInt();
  if (version != 1) {
    parser.Error("Invalid skeleton version %d. Should be version %d.\n",
                 version, 1);
    return false;
  }

  int numJoints = 0;
  int numUserChannels = 0;
  idStr jointConversionName;
  userChannelFilename.Set("");
  userChannelTimestamp = 0xffffffffu;
  if (!parser.ExpectTokenString("init") ||
      !parser.ExpectTokenString("{"))
    return false;
  idToken token;
  while (parser.ReadToken(token) && token != "}") {
    if (token.Icmp("commandLine") == 0 || token.Icmp("sourceFile") == 0) {
      parser.ExpectTokenType(TT_STRING, 0, token);
    } else if (token.Icmp("numJoints") == 0) {
      numJoints = parser.ParseInt();
      if (numJoints < 0 || numJoints > 255)
        parser.Error("Invalid number of joints %d", numJoints);
    } else if (token.Icmp("numUserChannels") == 0) {
      numUserChannels = parser.ParseInt();
      if (numUserChannels < 0 || numUserChannels > 255)
        parser.Error("Invalid number of user channels %d", numUserChannels);
    } else if (token.Icmp("jointconversion") == 0) {
      parser.ExpectTokenType(TT_STRING, 0, token);
      jointConversionName = token.data;
    } else if (token.Icmp("userChannelFile") == 0) {
      parser.ExpectTokenType(TT_STRING, 0, token);
      userChannelFilename.Set(token.data);
    } else {
      parser.Error("Unknown token '%s' in 'init' section", token.data);
      break;
    }
  }
  if (parser.HadError())
    return false;

  std::vector<char> userChannelText;
  idParser userChannelParser(LEXFL_NOSTRINGESCAPECHARS |
                             LEXFL_ALLOWPATHNAMES);
  idParser* channelSource = &parser;
  if (userChannelFilename.str[0] != '\0') {
    idFileLocal channelFile(fileSystem != nullptr
                                ? fileSystem->OpenFileRead(
                                      userChannelFilename.str, true, false)
                                : nullptr);
    if (channelFile.file == nullptr) {
      parser.Error("UserChannels file not found %s",
                   userChannelFilename.str);
      return false;
    }
    userChannelTimestamp =
        fileSystem->GetTimestamp(userChannelFilename.str, false);
    const std::int64_t length = channelFile.file->Length();
    if (length < 0 || length > 16 * 1024 * 1024)
      return false;
    userChannelText.resize(static_cast<std::size_t>(length) + 1u);
    if (!ReadExact(channelFile.file, userChannelText.data(),
                   static_cast<std::size_t>(length)))
      return false;
    userChannelText[static_cast<std::size_t>(length)] = '\0';
    if (!userChannelParser.LoadMemory(userChannelText.data(),
                                      static_cast<int>(length) + 1,
                                      userChannelFilename.str) ||
        !userChannelParser.ExpectTokenString("MD6Version"))
      return false;
    const int channelVersion = userChannelParser.ParseInt();
    if (channelVersion != 0 && channelVersion != 2)
      userChannelParser.Error(
          "Invalid userchannel version %d. Should be version %d\n",
          channelVersion, 2);
    userChannelParser.ExpectTokenString("numUserChannels");
    numUserChannels = userChannelParser.ParseInt();
    if (numUserChannels < 0 || numUserChannels > 255)
      userChannelParser.Error("Invalid number of user channels %d",
                              numUserChannels);
    if (userChannelParser.HadError())
      return false;
    channelSource = &userChannelParser;
  }

  const int paddedJoints = (numJoints + 7) & ~7;
  const int paddedChannels = (numUserChannels + 7) & ~7;
  std::vector<float> basePose(static_cast<std::size_t>(paddedJoints) * 12,
                              0.0f);
  std::vector<float> secondaryPose(
      static_cast<std::size_t>(paddedJoints) * 12, 0.0f);
  std::vector<std::int16_t> parents(static_cast<std::size_t>(paddedJoints),
                                    -1);
  std::vector<idJointHandle_t> jointHandles(
      static_cast<std::size_t>(paddedJoints), NULL_JOINT);
  std::vector<idUserChannelHandle_t> userHandles(
      static_cast<std::size_t>(paddedChannels), NULL_USERCHANNEL);
  std::vector<float> channelDefaults(static_cast<std::size_t>(paddedChannels),
                                     0.0f);
  std::array<std::vector<std::uint8_t>, 8> jointWeightStorage;
  std::array<std::vector<std::uint8_t>, 8> userWeightStorage;
  std::array<bool, 8> jointWeightValid{};
  std::array<bool, 8> userWeightValid{};
  for (int set = 0; set < 8; ++set) {
    jointWeightStorage[set].resize(static_cast<std::size_t>(paddedJoints), 0);
    userWeightStorage[set].resize(static_cast<std::size_t>(paddedChannels), 0);
  }
  idList<idStr, 5> jointNames;
  idList<idStr, 5> channelNames;
  jointNames.SetNum(numJoints);
  channelNames.SetNum(numUserChannels);

  if (!parser.ExpectTokenString("joints") ||
      !parser.ExpectTokenString("{"))
    return false;
  for (int joint = 0; joint < numJoints; ++joint) {
    if (!parser.ReadToken(token))
      return false;
    token.ToLower();
    jointNames[joint] = token.data;
    jointHandles[joint] = animation != nullptr
                              ? animation->RegisterJoint(token.data)
                              : NULL_JOINT;
    parents[joint] = static_cast<std::int16_t>(parser.ParseInt());
    if (parents[joint] >= numJoints)
      parser.Error("Invalid parent for joint '%s'", token.data);
    float weights[8]{};
    if (!parser.Parse1DMatrix(8, weights))
      return false;
    jointWeightStorage[0][joint] = 0xffu;
    jointWeightValid[0] = true;
    for (int set = 1; set < 8; ++set) {
      jointWeightStorage[set][joint] = WeightByte(weights[set]);
      jointWeightValid[set] =
          jointWeightValid[set] || jointWeightStorage[set][joint] != 0;
    }
    if (!parser.Parse1DMatrix(4, basePose.data() + joint * 4) ||
        !parser.Parse1DMatrix(
            3, basePose.data() + paddedJoints * 4 + joint * 4) ||
        !parser.Parse1DMatrix(
            3, basePose.data() + paddedJoints * 8 + joint * 4))
      return false;
    basePose[paddedJoints * 4 + joint * 4 + 3] = 1.0f;
    basePose[paddedJoints * 8 + joint * 4 + 3] = 0.0f;
  }
  if (!parser.ExpectTokenString("}"))
    return false;

  bool hasSecondaryPose = false;
  if (parser.CheckTokenString("secondaryjoints")) {
    hasSecondaryPose = true;
    if (!parser.ExpectTokenString("{"))
      return false;
    for (int joint = 0; joint < numJoints; ++joint) {
      float ignoredWeights[8]{};
      if (!parser.ReadToken(token))
        return false;
      parser.ParseInt();
      if (!parser.Parse1DMatrix(8, ignoredWeights) ||
          !parser.Parse1DMatrix(4, secondaryPose.data() + joint * 4) ||
          !parser.Parse1DMatrix(
              3, secondaryPose.data() + paddedJoints * 4 + joint * 4) ||
          !parser.Parse1DMatrix(
              3, secondaryPose.data() + paddedJoints * 8 + joint * 4))
        return false;
      secondaryPose[paddedJoints * 4 + joint * 4 + 3] = 1.0f;
      secondaryPose[paddedJoints * 8 + joint * 4 + 3] = 0.0f;
    }
    if (!parser.ExpectTokenString("}"))
      return false;
  }

  if (channelSource->CheckTokenString("userChannels")) {
    if (!channelSource->ExpectTokenString("{"))
      return false;
    for (int channel = 0; channel < numUserChannels; ++channel) {
      if (!channelSource->ReadToken(token))
        return false;
      token.ToLower();
      channelNames[channel] = token.data;
      userHandles[channel] = animation != nullptr
                                 ? animation->RegisterUserChannel(token.data)
                                 : NULL_USERCHANNEL;
      float weights[8]{};
      if (!channelSource->Parse1DMatrix(8, weights))
        return false;
      for (int set = 0; set < 8; ++set) {
        userWeightStorage[set][channel] = WeightByte(weights[set]);
        userWeightValid[set] =
            userWeightValid[set] || userWeightStorage[set][channel] != 0;
      }
      channelDefaults[channel] = channelSource->ParseFloat();
    }
    if (!channelSource->ExpectTokenString("}"))
      return false;
  }
  if (parser.HadError() || userChannelParser.HadError())
    return false;

  idStr defaultConversionName(name.str);
  defaultConversionName.SetFileExtension("md6jointconversion");
  const bool useDefaultConversion = jointConversionName.IsEmpty();
  if (animation != nullptr) {
    animation->MakeDefaultJointConversion(defaultConversionName.data,
                                          jointNames, channelNames);
    if (useDefaultConversion)
      jointConversionName = defaultConversionName;
    jointConversion =
        animation->FindJointConversionHandle(jointConversionName.data);
    jointConversionChecksum =
        animation->GetJointConversionCrc(jointConversion);
  } else {
    jointConversion = NULL_JOINT_CONVERSION;
    jointConversionChecksum = NULL_CRC;
  }

  std::array<std::uint8_t*, 8> jointWeightPointers{};
  std::array<std::uint8_t*, 8> userWeightPointers{};
  for (int set = 0; set < 8; ++set) {
    jointWeightPointers[set] =
        jointWeightValid[set] ? jointWeightStorage[set].data() : nullptr;
    userWeightPointers[set] =
        userWeightValid[set] ? userWeightStorage[set].data() : nullptr;
  }

  if (useDefaultConversion || animation == nullptr) {
    return MakeSkeletonData(
        static_cast<unsigned int>(numJoints), basePose.data(),
        hasSecondaryPose ? secondaryPose.data() : nullptr,
        jointHandles.data(), parents.data(), numUserChannels,
        channelDefaults.data(), userHandles.data(),
        jointWeightPointers.data(), userWeightPointers.data());
  }
  if (!jointConversion.IsValid()) {
    parser.Error("Invalid jointconversion: %s", jointConversionName.data);
    return false;
  }

  std::vector<int> jointRemap(static_cast<std::size_t>(numJoints), -1);
  std::vector<int> channelRemap(static_cast<std::size_t>(numUserChannels),
                                -1);
  int convertedJointCount = 0;
  int convertedChannelCount = 0;
  for (int joint = 0; joint < numJoints; ++joint) {
    jointRemap[joint] =
        animation->GetJointIndex(jointConversion, jointNames[joint].data).value;
    if (jointRemap[joint] < 0) {
      parser.Error("Joint not found in jointconversion: %s",
                   jointNames[joint].data);
      return false;
    }
    convertedJointCount = (std::max)(convertedJointCount,
                                     jointRemap[joint] + 1);
  }
  for (int channel = 0; channel < numUserChannels; ++channel) {
    channelRemap[channel] =
        animation
            ->GetUserChannelIndex(jointConversion, channelNames[channel].data)
            .value;
    if (channelRemap[channel] < 0) {
      parser.Error("UserChannel not found in jointconversion: %s",
                   channelNames[channel].data);
      return false;
    }
    convertedChannelCount = (std::max)(convertedChannelCount,
                                       channelRemap[channel] + 1);
  }
  const int convertedPaddedJoints = (convertedJointCount + 7) & ~7;
  const int convertedPaddedChannels = (convertedChannelCount + 7) & ~7;
  std::vector<float> convertedPose(
      static_cast<std::size_t>(convertedPaddedJoints) * 12, 0.0f);
  std::vector<float> convertedSecondary(
      static_cast<std::size_t>(convertedPaddedJoints) * 12, 0.0f);
  std::vector<std::int16_t> convertedParents(
      static_cast<std::size_t>(convertedPaddedJoints), -1);
  std::vector<idJointHandle_t> convertedJointHandles(
      static_cast<std::size_t>(convertedPaddedJoints), NULL_JOINT);
  std::vector<idUserChannelHandle_t> convertedUserHandles(
      static_cast<std::size_t>(convertedPaddedChannels), NULL_USERCHANNEL);
  std::vector<float> convertedDefaults(
      static_cast<std::size_t>(convertedPaddedChannels), 0.0f);
  std::array<std::vector<std::uint8_t>, 8> convertedJointWeights;
  std::array<std::vector<std::uint8_t>, 8> convertedUserWeights;
  for (int set = 0; set < 8; ++set) {
    convertedJointWeights[set].resize(convertedPaddedJoints, 0);
    convertedUserWeights[set].resize(convertedPaddedChannels, 0);
  }
  for (int joint = 0; joint < convertedPaddedJoints; ++joint) {
    convertedPose[joint * 4 + 3] = 1.0f;
    convertedPose[convertedPaddedJoints * 4 + joint * 4 + 0] = 1.0f;
    convertedPose[convertedPaddedJoints * 4 + joint * 4 + 1] = 1.0f;
    convertedPose[convertedPaddedJoints * 4 + joint * 4 + 2] = 1.0f;
    convertedPose[convertedPaddedJoints * 4 + joint * 4 + 3] = 1.0f;
    convertedSecondary[joint * 4 + 3] = 1.0f;
    convertedSecondary[convertedPaddedJoints * 4 + joint * 4 + 0] = 1.0f;
    convertedSecondary[convertedPaddedJoints * 4 + joint * 4 + 1] = 1.0f;
    convertedSecondary[convertedPaddedJoints * 4 + joint * 4 + 2] = 1.0f;
    convertedSecondary[convertedPaddedJoints * 4 + joint * 4 + 3] = 1.0f;
  }
  std::fill_n(convertedParents.begin(), convertedJointCount,
              static_cast<std::int16_t>(0));
  for (int source = 0; source < numJoints; ++source) {
    const int destination = jointRemap[source];
    convertedJointHandles[destination] = jointHandles[source];
    convertedParents[destination] =
        parents[source] >= 0 ? static_cast<std::int16_t>(
                                  jointRemap[parents[source]])
                             : -1;
    if (destination != 0 && convertedParents[destination] == -1)
      convertedParents[destination] = 0;
    for (int block = 0; block < 3; ++block) {
      std::copy_n(basePose.data() + block * paddedJoints * 4 + source * 4,
                  4, convertedPose.data() +
                         block * convertedPaddedJoints * 4 + destination * 4);
      if (hasSecondaryPose)
        std::copy_n(
            secondaryPose.data() + block * paddedJoints * 4 + source * 4, 4,
            convertedSecondary.data() +
                block * convertedPaddedJoints * 4 + destination * 4);
    }
    for (int set = 0; set < 8; ++set)
      convertedJointWeights[set][destination] =
          jointWeightStorage[set][source];
  }
  for (int source = 0; source < numUserChannels; ++source) {
    const int destination = channelRemap[source];
    convertedUserHandles[destination] = userHandles[source];
    convertedDefaults[destination] = channelDefaults[source];
    for (int set = 0; set < 8; ++set)
      convertedUserWeights[set][destination] =
          userWeightStorage[set][source];
  }
  for (int set = 0; set < 8; ++set) {
    jointWeightPointers[set] =
        jointWeightValid[set] ? convertedJointWeights[set].data() : nullptr;
    userWeightPointers[set] =
        userWeightValid[set] ? convertedUserWeights[set].data() : nullptr;
  }
  return MakeSkeletonData(
      static_cast<unsigned int>(convertedJointCount), convertedPose.data(),
      hasSecondaryPose ? convertedSecondary.data() : nullptr,
      convertedJointHandles.data(), convertedParents.data(),
      convertedChannelCount, convertedDefaults.data(),
      convertedUserHandles.data(), jointWeightPointers.data(),
      userWeightPointers.data());
}

bool idMD6Skel::LoadText(const char* fileName) {
  idParser source(LEXFL_NOSTRINGCONCAT | LEXFL_NOSTRINGESCAPECHARS |
                  LEXFL_ALLOWPATHNAMES);
  if (fileName == nullptr || !source.LoadFile(fileName, false)) {
    SetResourceError("Failed to open file for md6skel '%s'",
                     fileName != nullptr ? fileName : "");
    return false;
  }
  if (!Parse(source)) {
    SetResourceError("Failed to load md6skel '%s'.", fileName);
    return false;
  }
  timestamp = fileSystem != nullptr
                  ? fileSystem->GetTimestamp(fileName, false)
                  : 0xffffffffu;
  return true;
}

void idMD6Skel::LoadResource() {
  char binaryFileName[256]{};
  if (fileSystem != nullptr)
    fileSystem->FixLongFilename("generated", "bmd6skl", name.str,
                                binaryFileName, sizeof(binaryFileName));
  if (!LoadBinary(binaryFileName, false)) {
    if (!LoadText(name.str)) {
      std::array<std::uint8_t*, 8> weights{};
      MakeSkeletonData(0, nullptr, nullptr, nullptr, nullptr, 0, nullptr,
                       nullptr, weights.data(), weights.data());
    }
    WriteBinary(binaryFileName);
  }
  idStr defaultAnimName(name.str);
  defaultAnimName.Append('/');
  defaultAnimName.Append("_default");
  defaultAnim = idMD6Anim::resourceList.Load(defaultAnimName.data, true, false);
}

idMD6SkeletonConfig::idMD6SkeletonConfig() {
  resourceListPtr = &resourceList;
  instances.ClearFree();
  boundsDecl = nullptr;
}

idMD6SkeletonConfig::~idMD6SkeletonConfig() noexcept(false) {
  instances.ClearFree();
}

idResourceList* idMD6SkeletonConfig::GetResourceList() const {
  return &resourceList;
}

const char* idMD6SkeletonConfig::GetResourceTypeName() const {
  return "skeletonconfig";
}

bool idMD6SkeletonConfig::WriteSkeletonConfig_Binary(
    const char* filename) {
  if (fileSystem == nullptr || filename == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileWrite(filename, FSPATH_BASE));
  if (file.file == nullptr) {
    idLib::Warning("Could not write '%s'", filename);
    return false;
  }
  constexpr std::uint32_t magic = 0x02364a4du;
  if (!WriteBig32(file.file, magic) ||
      !WriteBig32(file.file,
                  static_cast<std::uint32_t>(instances.Num())))
    return false;
  for (int index = 0; index < instances.Num(); ++index) {
    if (!WriteBig32(file.file, instances[index].timestamp))
      return false;
    const char* skeletonName = instances[index].skeleton != nullptr
                                   ? instances[index].skeleton->name.str
                                   : nullptr;
    const std::size_t expected =
        sizeof(int) +
        (skeletonName != nullptr ? std::strlen(skeletonName) : 0u);
    if (file.file->WriteString(skeletonName) != expected)
      return false;
  }
  const char* boundsName =
      boundsDecl != nullptr ? boundsDecl->name.str : nullptr;
  return file.file->WriteString(boundsName) ==
         sizeof(int) +
             (boundsName != nullptr ? std::strlen(boundsName) : 0u);
}

bool idMD6SkeletonConfig::ReadSkeletonConfig_Binary(
    const char* filename, bool directBinary) {
  if (fileSystem == nullptr || filename == nullptr)
    return false;
  idFileLocal file(fileSystem->OpenFileRead(filename, true, directBinary));
  if (file.file == nullptr)
    return false;
  constexpr std::uint32_t magic = 0x02364a4du;
  std::uint32_t fileMagic = 0;
  std::uint32_t count = 0;
  if (!ReadBig32(file.file, fileMagic) || fileMagic != magic ||
      !ReadBig32(file.file, count) || count > 0x100000u)
    return false;
  instances.SetNum(static_cast<int>(count));
  idStr entryName;
  for (std::uint32_t index = 0; index < count; ++index) {
    if (!ReadBig32(file.file, instances[static_cast<int>(index)].timestamp) ||
        !ReadNativeString(file.file, entryName)) {
      instances.ClearFree();
      return false;
    }
    instances[static_cast<int>(index)].skeleton =
        entryName.IsEmpty()
            ? nullptr
            : static_cast<const idMD6Skel*>(
                  idMD6Skel::resourceList.Load(entryName.data, true, false));
  }
  if (!ReadNativeString(file.file, entryName)) {
    instances.ClearFree();
    return false;
  }
  boundsDecl = entryName.IsEmpty() || skeletonConfigDeclLoader == nullptr
                   ? nullptr
                   : skeletonConfigDeclLoader(entryName.data);
  return true;
}

bool idMD6SkeletonConfig::LoadSkeletonConfig(const char* basePath) {
  if (fileSystem == nullptr || basePath == nullptr)
    return false;
  idFileList* files = fileSystem->ListFiles(basePath, "md6skl", true);
  if (files == nullptr)
    return false;
  instances.SetNum(files->list.Num());
  for (int index = 0; index < files->list.Num(); ++index) {
    const idStr skeletonName =
        ListedSkeletonPath(basePath, files->list[index].data);
    instances[index].skeleton = static_cast<const idMD6Skel*>(
        idMD6Skel::resourceList.Load(skeletonName.data, false, false));
    instances[index].timestamp =
        instances[index].skeleton != nullptr
            ? instances[index].skeleton->timestamp
            : 0xffffffffu;
  }
  fileSystem->FreeFileList(files);

  idStr configFile(basePath);
  configFile.AppendPath(".skeletonconfig");
  idParser source(LEXFL_NOSTRINGCONCAT | LEXFL_NOSTRINGESCAPECHARS |
                  LEXFL_ALLOWPATHNAMES);
  if (source.LoadFile(configFile.data, false)) {
    if (!source.ExpectTokenString(SKELETONCONFIG_VERSION_STRING))
      return false;
    const int version = source.ParseInt();
    if (version != 2) {
      source.Error(
          "Invalid skeleton config version %d. Should be version %d.\n",
          version, 2);
      return false;
    }
    idToken declaration;
    if (!source.ExpectTokenType(TT_STRING, 0, declaration))
      return false;
    boundsDecl = skeletonConfigDeclLoader != nullptr
                     ? skeletonConfigDeclLoader(declaration.data)
                     : nullptr;
    if (boundsDecl == nullptr && skeletonConfigDeclLoader != nullptr) {
      source.Error("Skeleton config had invalid md6Decl %s. \n",
                   declaration.data);
      return false;
    }
  }
  return true;
}

bool idMD6SkeletonConfig::ReloadIfStale() {
  if (fileSystem == nullptr)
    return false;
  idFileList* files = fileSystem->ListFiles(name.str, "md6skl", true);
  bool stale = files == nullptr ? instances.Num() != 0 : false;
  if (files != nullptr) {
    for (int fileIndex = 0; !stale && fileIndex < files->list.Num();
         ++fileIndex) {
      const idStr fileName =
          ListedSkeletonPath(name.str, files->list[fileIndex].data);
      for (int instanceIndex = 0; instanceIndex < instances.Num();
           ++instanceIndex) {
        const idMD6Skel* skeleton = instances[instanceIndex].skeleton;
        if (skeleton != nullptr &&
            idStr::Icmp(skeleton->name.str, fileName.data) == 0) {
          stale = fileSystem->GetTimestamp(fileName.data, false) !=
                  instances[instanceIndex].timestamp;
          break;
        }
      }
    }
    fileSystem->FreeFileList(files);
  }
  if (!stale)
    return false;
  char binaryFileName[256]{};
  fileSystem->FixLongFilename("generated", "bmd6skeletonconfig", name.str,
                              binaryFileName, sizeof(binaryFileName), -2);
  fileSystem->RemoveFile(binaryFileName, FSPATH_BASE);
  instances.ClearFree();
  LoadResource();
  return true;
}

void idMD6SkeletonConfig::LoadResource() {
  char binaryFileName[256]{};
  if (fileSystem == nullptr)
    return;
  fileSystem->FixLongFilename("generated", "bmd6skeletonconfig", name.str,
                              binaryFileName, sizeof(binaryFileName), -2);
  if (!ReadSkeletonConfig_Binary(binaryFileName, false) &&
      LoadSkeletonConfig(name.str))
    WriteSkeletonConfig_Binary(binaryFileName);
}

void SubChildren_r(const idMD6Skel* skeleton, idJointIndex_t parent,
                   idList<idJointIndex_t, 5>& jointList) {
  if (skeleton == nullptr || skeleton->data == nullptr)
    return;
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(skeleton->data) +
      skeleton->data->parentTblOffset);
  for (int joint = 0; joint < skeleton->data->numJoints; ++joint) {
    if (parents[joint] != parent.value)
      continue;
    for (int index = 0; index < jointList.num; ++index) {
      if (jointList[index].value == joint) {
        jointList.RemoveIndex(index);
        break;
      }
    }
    SubChildren_r(skeleton, idJointIndex_t(static_cast<std::int16_t>(joint)),
                  jointList);
  }
}

void AddChildren_r(const idMD6Skel* skeleton, idJointIndex_t parent,
                   idList<idJointIndex_t, 5>& jointList) {
  if (skeleton == nullptr || skeleton->data == nullptr)
    return;
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(skeleton->data) +
      skeleton->data->parentTblOffset);
  for (int joint = 0; joint < skeleton->data->numJoints; ++joint) {
    if (parents[joint] != parent.value)
      continue;
    bool exists = false;
    for (int index = 0; index < jointList.Num(); ++index)
      exists = exists || jointList[index].value == joint;
    if (!exists)
      jointList.Append(idJointIndex_t(static_cast<std::int16_t>(joint)));
    AddChildren_r(skeleton, idJointIndex_t(static_cast<std::int16_t>(joint)),
                  jointList);
  }
}

bool idMD6Skel::GetJointsForAnimFrame(idJointMat* joints,
                                      const idMD6Anim* anim, int frame,
                                      const idVec3* offset,
                                      bool removeOriginOffset) const {
  if (anim == nullptr || !anim->WaitForValid())
    return false;
  const int frameRate = anim->animData != nullptr ? anim->animData->frameRate : 30;
  const int ticksPerSecond = timeManager.gameTimePerSecond.value;
  const int time = frame * ticksPerSecond / frameRate;
  return GetJointsForAnimTime(joints, anim, time, offset,
                              removeOriginOffset);
}

bool idMD6Skel::GetJointsForAnimTime(idJointMat* joints,
                                     const idMD6Anim* anim, int timeMS,
                                     const idVec3* offset,
                                     bool removeOriginOffset) const {
  if (anim == nullptr || !anim->WaitForValid())
    return false;
  InitializeJointMatrices(joints, data->numJoints, *offset);
  alignas(16) std::array<char, 0x4000> temporary{};
  alignas(16) std::array<__m128, 256 * 3> output{};
  idMD6Blend::DecodeSingleFrame(
      data, temporary.data(), static_cast<std::uint32_t>(temporary.size()),
      anim->animData, timeMS, timeManager.gameTimePerSecond.value, false,
      &offset->x, !removeOriginOffset, output.data(), nullptr, nullptr, 0);
  std::memcpy(joints, output.data(),
              sizeof(idJointMat) * static_cast<std::size_t>(data->numJoints));
  return true;
}

bool idMD6Skel::GetUserChannelsForAnimFrame(
    float* userChannels, unsigned int userChannelsSize, const idMD6Anim* anim,
    float frame) const {
  if (anim == nullptr || !anim->WaitForValid() || userChannels == nullptr) {
    return false;
  }
  (void)userChannelsSize;
  idMD6Blend::DecodeUserChannelsForAnimFrame(data, anim->animData, frame,
                                             userChannels);
  return true;
}
