#include "engine/models/skeletalanimation/md6anim.h"
#include "engine/models/skeletalanimation/jobs/md6blend.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/text/parser.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <limits>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

idCVar anim_allowDefaultedBinaryAnims(
    "anim_allowDefaultedBinaryAnims", "1", CVAR_BOOL,
    "allow binary animations marked as generated defaults");
idCVar anim_removeDefaultedAnimFiles(
    "anim_removeDefaultedAnimFiles", "0", CVAR_BOOL,
    "remove generated default binary animations during development");
idCVar anim_forceCompression(
    "anim_forceCompression", "0", CVAR_BOOL,
    "force MD6 animation source recompression");
idCVar anim_generatorNumThreads(
    "anim_generatorNumThreads", "1", CVAR_INTEGER,
    "number of asynchronous MD6 animation generator threads");

namespace {

constexpr std::uint32_t kMD6BinaryMagic = 1296119339u;
idMD6AnimLoadCallback g_loadCallback = nullptr;
idMD6AnimWaitCallback g_waitCallback = nullptr;
idMD6AnimWarningCallback g_warningCallback = nullptr;
idMD6AnimBoundsVerifier g_boundsVerifier = nullptr;
idMD6AnimTextLoader g_textLoader = nullptr;
idMD6AnimParserLoader g_parserLoader = nullptr;
idMD6AnimFacetrackLoader g_facetrackLoader = nullptr;
idMD6AnimRewriteCallback g_rewriteCallback = nullptr;
idMD6AnimModelLoader g_modelLoader = nullptr;
int g_gameTimePerSecond = 1000;
std::mutex g_ownedDataMutex;
std::unordered_set<idMD6AnimData*> g_ownedData;
std::unordered_set<void*> g_ownedPhaseTracks;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24u) | ((value << 8u) & 0x00ff0000u) |
         ((value >> 8u) & 0x0000ff00u) | (value >> 24u);
}

void SwapFloat(float& value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  bits = ByteSwap32(bits);
  std::memcpy(&value, &bits, sizeof(value));
}

void Swap16Range(void* begin, void* end) {
  auto* cursor = static_cast<std::uint16_t*>(begin);
  auto* finish = static_cast<std::uint16_t*>(end);
  while (cursor < finish) {
    *cursor = ByteSwap16(*cursor);
    ++cursor;
  }
}

void Swap32Range(void* begin, void* end) {
  auto* cursor = static_cast<std::uint32_t*>(begin);
  auto* finish = static_cast<std::uint32_t*>(end);
  while (cursor < finish) {
    *cursor = ByteSwap32(*cursor);
    ++cursor;
  }
}

idMD6AnimData* AllocateAnimData(std::size_t bytes) {
  if (bytes < sizeof(idMD6AnimData))
    return nullptr;
#if defined(_MSC_VER)
  void* allocation = _aligned_malloc(bytes, 16);
#else
  const std::size_t alignedBytes = (bytes + 15u) & ~std::size_t(15u);
  void* allocation = std::aligned_alloc(16, alignedBytes);
#endif
  if (allocation == nullptr)
    return nullptr;
  std::memset(allocation, 0, bytes);
  auto* data = static_cast<idMD6AnimData*>(allocation);
  std::lock_guard<std::mutex> lock(g_ownedDataMutex);
  g_ownedData.insert(data);
  return data;
}

void ReleaseOwnedAnimData(idMD6AnimData*& data) {
  if (data == nullptr)
    return;
  bool owned = false;
  {
    std::lock_guard<std::mutex> lock(g_ownedDataMutex);
    const auto entry = g_ownedData.find(data);
    if (entry != g_ownedData.end()) {
      g_ownedData.erase(entry);
      owned = true;
    }
  }
  if (owned) {
#if defined(_MSC_VER)
    _aligned_free(data);
#else
    std::free(data);
#endif
  }
  data = nullptr;
}

void* PhaseTrackData(const idMD6Anim& animation) {
  void* data = nullptr;
  std::memcpy(&data, animation.phaseTrack, sizeof(data));
  return data;
}

void SetPhaseTrackData(idMD6Anim& animation, void* data) {
  std::memcpy(animation.phaseTrack, &data, sizeof(data));
}

void ReleaseOwnedPhaseTrack(idMD6Anim& animation) {
  void* data = PhaseTrackData(animation);
  if (data == nullptr)
    return;
  bool owned = false;
  {
    std::lock_guard<std::mutex> lock(g_ownedDataMutex);
    const auto entry = g_ownedPhaseTracks.find(data);
    if (entry != g_ownedPhaseTracks.end()) {
      g_ownedPhaseTracks.erase(entry);
      owned = true;
    }
  }
  if (owned) {
#if defined(_MSC_VER)
    _aligned_free(data);
#else
    std::free(data);
#endif
  }
  SetPhaseTrackData(animation, nullptr);
}

void* AllocatePhaseTrack(std::size_t bytes) {
#if defined(_MSC_VER)
  void* allocation = _aligned_malloc(bytes, 16);
#else
  void* allocation = std::aligned_alloc(16, (bytes + 15u) & ~std::size_t(15u));
#endif
  if (allocation != nullptr) {
    std::lock_guard<std::mutex> lock(g_ownedDataMutex);
    g_ownedPhaseTracks.insert(allocation);
  }
  return allocation;
}

bool WriteString(std::ofstream& output, const char* text) {
  const std::uint32_t length =
      text != nullptr ? static_cast<std::uint32_t>(std::strlen(text)) : 0u;
  output.write(reinterpret_cast<const char*>(&length), sizeof(length));
  if (length != 0)
    output.write(text, length);
  return output.good();
}

bool ReadString(std::ifstream& input, std::string& text) {
  std::uint32_t length = 0;
  input.read(reinterpret_cast<char*>(&length), sizeof(length));
  if (!input || length > (1u << 20u))
    return false;
  text.resize(length);
  if (length != 0)
    input.read(text.data(), length);
  return input.good();
}

idQuat NormalizedConjugate(const float* quaternion) {
  const float lengthSquared = quaternion[0] * quaternion[0] +
                              quaternion[1] * quaternion[1] +
                              quaternion[2] * quaternion[2] +
                              quaternion[3] * quaternion[3];
  const float inverseLength =
      1.0f / std::sqrt(std::max(lengthSquared,
                                std::numeric_limits<float>::min()));
  return {-quaternion[0] * inverseLength,
          -quaternion[1] * inverseLength,
          -quaternion[2] * inverseLength,
          quaternion[3] * inverseLength};
}

idQuat Multiply(const idQuat& lhs, const float* rhs) {
  return {
      lhs.w * rhs[0] + lhs.x * rhs[3] + lhs.y * rhs[2] - lhs.z * rhs[1],
      lhs.w * rhs[1] - lhs.x * rhs[2] + lhs.y * rhs[3] + lhs.z * rhs[0],
      lhs.w * rhs[2] + lhs.x * rhs[1] - lhs.y * rhs[0] + lhs.z * rhs[3],
      lhs.w * rhs[3] - lhs.x * rhs[0] - lhs.y * rhs[1] - lhs.z * rhs[2]};
}

idVec3 Rotate(const idQuat& quaternion, const idVec3& vector) {
  const idVec3 q{quaternion.x, quaternion.y, quaternion.z};
  const idVec3 twiceCross = 2.0f * q.Cross(vector);
  return vector + quaternion.w * twiceCross + q.Cross(twiceCross);
}

idMat3 QuaternionToMatrix(const idQuat& q) {
  const float x2 = q.x + q.x;
  const float y2 = q.y + q.y;
  const float z2 = q.z + q.z;
  const float xx = q.x * x2;
  const float xy = q.x * y2;
  const float xz = q.x * z2;
  const float yy = q.y * y2;
  const float yz = q.y * z2;
  const float zz = q.z * z2;
  const float wx = q.w * x2;
  const float wy = q.w * y2;
  const float wz = q.w * z2;
  idMat3 result;
  result[0] = {1.0f - (yy + zz), xy - wz, xz + wy};
  result[1] = {xy + wz, 1.0f - (xx + zz), yz - wx};
  result[2] = {xz - wy, yz + wx, 1.0f - (xx + yy)};
  return result;
}

std::uint32_t DecodeRLE(const std::uint8_t* rle, std::uint16_t sentinel,
                        std::uint8_t* indices) {
  const std::uint32_t count = std::min<std::uint32_t>(rle[0], sentinel);
  const std::uint8_t* source = rle + 1;
  std::uint32_t output = 0;
  while (output < count) {
    const std::uint8_t control = *source++;
    int run = control & 0x7f;
    if ((control & 0x80u) != 0) {
      while (run-- > 0 && output < count)
        indices[output++] = static_cast<std::uint8_t>(sentinel);
    } else {
      std::uint8_t value = *source++;
      while (run-- > 0 && output < count)
        indices[output++] = value++;
    }
  }
  const std::uint32_t padded = (count + 7u) & ~7u;
  std::fill(indices + count, indices + padded,
            static_cast<std::uint8_t>(sentinel));
  return count;
}

idJointIndex_t SkeletonJointIndex(const idMD6Skel* skeleton,
                                  const char* name) {
  return skeleton != nullptr && name != nullptr
             ? skeleton->GetJointIndex(name)
             : idJointIndex_t(-1);
}

} // namespace

void SetMD6AnimLoadCallback(idMD6AnimLoadCallback callback) {
  g_loadCallback = callback;
}

void SetMD6AnimWaitCallback(idMD6AnimWaitCallback callback) {
  g_waitCallback = callback;
}

void SetMD6AnimWarningCallback(idMD6AnimWarningCallback callback) {
  g_warningCallback = callback;
}

void SetMD6AnimBoundsVerifier(idMD6AnimBoundsVerifier verifier) {
  g_boundsVerifier = verifier;
}

void SetMD6AnimTextLoader(idMD6AnimTextLoader loader) {
  g_textLoader = loader;
}

void SetMD6AnimParserLoader(idMD6AnimParserLoader loader) {
  g_parserLoader = loader;
}

void SetMD6AnimFacetrackLoader(idMD6AnimFacetrackLoader loader) {
  g_facetrackLoader = loader;
}

void SetMD6AnimRewriteCallback(idMD6AnimRewriteCallback callback) {
  g_rewriteCallback = callback;
}

void SetMD6AnimModelLoader(idMD6AnimModelLoader loader) {
  g_modelLoader = loader;
}

void SetMD6AnimGameTimePerSecond(int ticksPerSecond) {
  g_gameTimePerSecond = std::max(ticksPerSecond, 0);
}

idMD6Anim::idCompressionSettings::~idCompressionSettings() {
  for (int index = 0; index < compressionSettings.num; ++index)
    delete compressionSettings[index];
  compressionSettings.Clear();
}

idMD6Anim::animationSettings_t::~animationSettings_t() = default;

idMD6Anim::compressionSetting_t*
idMD6Anim::idCompressionSettings::AllocSetting(const char* name) {
  auto* setting = new compressionSetting_t;
  setting->name.Set(name != nullptr ? name : "");
  compressionSettings.Append(setting);
  return setting;
}

void idMD6Anim::idCompressionSettings::SetupJointTolerance(
    const idMD6Skel* skeleton, idList<float, 5>& tolerances,
    int numPaddedJoints, int numFrames, float baseTolerance) const {
  tolerances.SetNum(std::max(numPaddedJoints, 0) * std::max(numFrames, 0));
  for (int index = 0; index < tolerances.num; ++index)
    tolerances[index] = baseTolerance;
  if (skeleton == nullptr)
    return;
  for (int ruleIndex = 0; ruleIndex < compressionSettings.num; ++ruleIndex) {
    const compressionSetting_t* rule = compressionSettings[ruleIndex];
    if (rule == nullptr || !rule->enabled || rule->userChannel)
      continue;
    for (int joint = 0;
         joint < std::min(numPaddedJoints, skeleton->NumJoints()); ++joint) {
      const bool matches = std::strcmp(rule->name.str, "*") == 0 ||
                           idStr::Icmp(rule->name.str,
                                       skeleton->GetJointName(joint)) == 0;
      if (!matches)
        continue;
      for (int frame = 0; frame < numFrames; ++frame)
        tolerances[frame * numPaddedJoints + joint] = rule->tolerance;
    }
  }
}

void idMD6Anim::idCompressionSettings::SetupUserChannelTolerance(
    const idMD6Skel* skeleton, idList<float, 5>& tolerances,
    int numPaddedUserChannels, int numFrames, float baseTolerance) const {
  (void)skeleton;
  tolerances.SetNum(std::max(numPaddedUserChannels, 0) *
                    std::max(numFrames, 0));
  for (int index = 0; index < tolerances.num; ++index)
    tolerances[index] = baseTolerance;
  for (int ruleIndex = 0; ruleIndex < compressionSettings.num; ++ruleIndex) {
    const compressionSetting_t* rule = compressionSettings[ruleIndex];
    if (rule == nullptr || !rule->enabled || !rule->userChannel ||
        std::strcmp(rule->name.str, "*") != 0)
      continue;
    for (int index = 0; index < tolerances.num; ++index)
      tolerances[index] = rule->tolerance;
  }
}

void idMD6Anim::idCompressionSettings::Parse(idParser* source) {
  if (source == nullptr)
    return;
  idToken token;
  source->CheckTokenString("{");
  while (source->ReadToken(token)) {
    if (idStr::Cmp(token.data, "}") == 0)
      break;
    const bool userChannel =
        idStr::Icmp(token.data, "userChannel") == 0;
    if (!userChannel && idStr::Icmp(token.data, "joint") != 0) {
      source->SkipRestOfLine();
      continue;
    }
    idToken nameToken;
    if (!source->ReadToken(nameToken))
      break;
    compressionSetting_t* setting = AllocSetting(nameToken.data);
    setting->userChannel = userChannel;
    setting->tolerance = source->ParseFloat();
    idToken optional;
    if (source->ReadTokenOnLine(optional)) {
      if (idStr::Icmp(optional.data, "disabled") == 0)
        setting->enabled = false;
      else
        setting->equalTolerance = optional.floatvalue;
    }
  }
}

bool idMD6Anim::LoadAnim_Settings(char* animationFilename,
                                  idCompressionSettings& settings,
                                  std::uint32_t* timestamp,
                                  bool loadFromAnim,
                                  bool updateNumFrames) {
  (void)loadFromAnim;
  (void)updateNumFrames;
  if (animationFilename == nullptr)
    return false;
  std::filesystem::path settingsPath(animationFilename);
  if (settingsPath.extension() != ".settings")
    settingsPath += ".settings";
  idParser parser;
  if (!parser.LoadFile(settingsPath.string().c_str(), false))
    return false;
  settings.Parse(&parser);
  if (timestamp != nullptr) {
    std::error_code error;
    const auto writeTime = std::filesystem::last_write_time(settingsPath,
                                                             error);
    *timestamp = error ? 0u : static_cast<std::uint32_t>(
        writeTime.time_since_epoch().count());
  }
  return !parser.HadError();
}

bool idMD6Anim::RewriteText(char* filename) {
  if (filename == nullptr)
    return false;
  if (g_rewriteCallback != nullptr)
    return g_rewriteCallback(filename);
  std::ifstream input(filename, std::ios::binary);
  if (!input)
    return false;
  std::vector<char> contents((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
  input.close();
  std::ofstream output(filename, std::ios::binary | std::ios::trunc);
  if (!output)
    return false;
  output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
  return output.good();
}

void idMD6AnimFileHeader::Swap() {
  timestamp = ByteSwap32(timestamp);
  phaseTrackTimestamp = ByteSwap32(phaseTrackTimestamp);
  settingsTimestamp = ByteSwap32(settingsTimestamp);
  skeletonTimestamp = ByteSwap32(skeletonTimestamp);
  blenddataMeshTimestamp = ByteSwap32(blenddataMeshTimestamp);
  timeCode = ByteSwap32(timeCode);
  Swap16Range(&translatedBounds.b[0][0], &translatedBounds.b[0][0] + 6);
  Swap16Range(&normalizedBounds.b[0][0], &normalizedBounds.b[0][0] + 6);
  jointConversionChecksum = ByteSwap16(jointConversionChecksum);
  for (int axis = 0; axis < 3; ++axis) {
    SwapFloat(origWorldPos[axis]);
    SwapFloat(origWorldRot[axis]);
    SwapFloat(origWorldScale[axis]);
  }
  startFrameOffset = ByteSwap16(startFrameOffset);
  endFrameOffset = ByteSwap16(endFrameOffset);
  animDataSize = ByteSwap32(animDataSize);
}

std::uint32_t DecodeBitArray(const std::uint8_t* bits,
                             std::uint32_t numBits,
                             std::uint8_t* indices) {
  std::uint32_t count = 0;
  for (std::uint32_t bitBase = 0; bitBase < numBits; bitBase += 8) {
    const std::uint8_t byte = *bits++;
    for (std::uint32_t bit = 0; bit < 8; ++bit) {
      indices[count] = static_cast<std::uint8_t>(bitBase + bit);
      count += (byte >> bit) & 1u;
    }
  }
  const std::uint32_t result = count;
  if (result != 0) {
    const std::uint8_t last = indices[count - 1];
    while ((count & 7u) != 0)
      indices[count++] = last;
  }
  return result;
}

namespace idMD6AnimCompression {

bool EqualR(const float* lhs, const float* rhs, float epsilon) {
  float direct = 0.0f;
  float negated = 0.0f;
  for (int index = 0; index < 4; ++index) {
    direct = std::max(direct, std::fabs(lhs[index] - rhs[index]));
    negated = std::max(negated, std::fabs(lhs[index] + rhs[index]));
  }
  return epsilon >= std::min(direct, negated);
}

bool EqualS(const float* lhs, const float* rhs, float epsilon) {
  return epsilon >=
         std::max(std::fabs(lhs[0] - rhs[0]),
                  std::max(std::fabs(lhs[1] - rhs[1]),
                           std::fabs(lhs[2] - rhs[2])));
}

void CompressR(const float* input, std::int16_t* output) {
  int largest = 0;
  for (int index = 1; index < 4; ++index) {
    if (std::fabs(input[index]) > std::fabs(input[largest]))
      largest = index;
  }
  constexpr float minimum = -0.70710677f;
  constexpr float step = 0.000043159689f;
  const float sign = input[largest] >= 0.0f ? 1.0f : -1.0f;
  for (int component = 0; component < 3; ++component) {
    const int source = (largest + component + 1) & 3;
    const int quantized = static_cast<int>(
        ((sign * input[source] - minimum) / step) + 0.5f);
    output[component] = static_cast<std::int16_t>(
        std::clamp(quantized, 0, 0x7fff));
  }
  output[0] = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(output[0]) |
      (static_cast<std::uint16_t>(3 - largest) << 15));
  output[1] = static_cast<std::int16_t>(
      static_cast<std::uint16_t>(output[1]) |
      ((static_cast<std::uint16_t>(3 - largest) & 0xfffeu) << 14));
}

float GetSetDistanceToRoot(std::int16_t jointIndex,
                           const std::int16_t* parentTable,
                           const float* baseTranslation,
                           idList<float, 5>& distances) {
  if (jointIndex < 0 || jointIndex >= distances.num)
    return 0.0f;
  float& result = distances[jointIndex];
  if (result != -1.0f)
    return result;
  const std::int16_t parent = parentTable[jointIndex];
  if (parent < 0)
    return result = 0.0f;
  const float* translation = baseTranslation + 3 * jointIndex;
  // The recovered SIMD sequence reduces x*x to all three lanes before its
  // square root; retain that behavior instead of using vector length.
  const float localDistance =
      std::sqrt(std::max(3.0f * translation[0] * translation[0],
                         std::numeric_limits<float>::min()));
  result = GetSetDistanceToRoot(parent, parentTable, baseTranslation,
                                distances) +
           localDistance;
  return result;
}

bool IsRWithinTolerance(const float* first, const float* second,
                        float endEffectorLength, float tolerance) {
  idQuat inverse = NormalizedConjugate(first);
  const idQuat relative = Multiply(inverse, second);
  const float clampedW = std::clamp(std::fabs(relative.w), 0.0f, 1.0f);
  const float angle = 2.0f * std::acos(clampedW);
  const float displacement =
      2.0f * std::fabs(endEffectorLength) * std::sin(0.5f * angle);
  return displacement <= tolerance;
}

bool IsTWithinTolerance(const float* first, const float* second,
                        float tolerance,
                        float equalTolerance) {
  if (!EqualS(first, second, equalTolerance))
    return false;
  const float dx = second[0] - first[0];
  const float dy = second[1] - first[1];
  const float dz = second[2] - first[2];
  return dx * dx + dy * dy + dz * dz <= tolerance * tolerance;
}

int GetDescendants(std::int16_t jointIndex,
                   const std::int16_t* parentTable, int numPaddedJoints,
                   idList<std::uint16_t, 5>& descendants) {
  for (int joint = 0; joint < numPaddedJoints; ++joint) {
    if (parentTable[joint] != jointIndex)
      continue;
    const auto child = static_cast<std::uint16_t>(joint);
    descendants.Append(child);
    GetDescendants(static_cast<std::int16_t>(joint), parentTable,
                   numPaddedJoints, descendants);
  }
  return descendants.num;
}

std::int16_t GetSetFarthestEndEffector(
    std::int16_t jointIndex, const std::int16_t* parentTable,
    const idList<float, 5>& distanceToRoot,
    idList<std::int16_t, 5>& farthestEndEffectors) {
  if (jointIndex < 0 || jointIndex >= farthestEndEffectors.num)
    return -1;
  std::int16_t& cached = farthestEndEffectors[jointIndex];
  if (cached != -1)
    return cached;
  idList<std::uint16_t, 5> descendants;
  GetDescendants(jointIndex, parentTable, distanceToRoot.num, descendants);
  cached = jointIndex;
  float farthestDistance = distanceToRoot[jointIndex];
  for (int index = 0; index < descendants.num; ++index) {
    const std::int16_t descendant =
        static_cast<std::int16_t>(descendants[index]);
    bool hasChild = false;
    for (int joint = 0; joint < distanceToRoot.num; ++joint)
      hasChild |= parentTable[joint] == descendant;
    if (!hasChild && distanceToRoot[descendant] > farthestDistance) {
      farthestDistance = distanceToRoot[descendant];
      cached = descendant;
    }
  }
  return cached;
}

int GenerateRLE(const std::uint8_t* bits, int numValues,
                const std::int16_t* targetMapping,
                std::uint8_t unmappedIndex,
                idList<std::uint8_t, 5>& rle) {
  std::uint8_t encodedValues = 0;
  for (int value = 0; value < numValues; ++value)
    encodedValues += (bits[value >> 3] >> (value & 7)) & 1u;
  rle.Append(encodedValues);
  int outputSize = 1;
  for (int source = 0; source < numValues; ++source) {
    if ((bits[source >> 3] & (1u << (source & 7))) == 0)
      continue;
    const auto Map = [&](int index) {
      if (targetMapping == nullptr)
        return static_cast<std::uint8_t>(index);
      return targetMapping[index] < 0
                 ? unmappedIndex
                 : static_cast<std::uint8_t>(targetMapping[index]);
    };
    const std::uint8_t first = Map(source);
    std::uint8_t previous = first;
    std::uint8_t run = 1;
    int scan = source + 1;
    while (scan < numValues && run < 0x7f) {
      if ((bits[scan >> 3] & (1u << (scan & 7))) != 0) {
        const std::uint8_t mapped = Map(scan);
        const bool contiguous = mapped == unmappedIndex
                                    ? previous == unmappedIndex
                                    : mapped == static_cast<std::uint8_t>(previous + 1);
        if (!contiguous)
          break;
        previous = mapped;
        ++run;
      }
      ++scan;
      ++source;
    }
    if (first == unmappedIndex) {
      rle.Append(static_cast<std::uint8_t>(run | 0x80u));
      ++outputSize;
    } else {
      rle.Append(run);
      rle.Append(first);
      outputSize += 2;
    }
  }
  return outputSize;
}

int GenerateJointRLE(const idMD6Skel* sourceSkeleton,
                     const idMD6Skel* targetSkeleton,
                     const idList<std::uint8_t, 5>& bits,
                     idList<std::uint8_t, 5>& rle) {
  if (sourceSkeleton == nullptr || sourceSkeleton->data == nullptr ||
      targetSkeleton == nullptr || targetSkeleton->data == nullptr)
    return 0;
  const int numValues = (sourceSkeleton->data->numJoints + 7) & ~7;
  std::vector<std::int16_t> mapping(static_cast<std::size_t>(numValues), -1);
  for (int source = 0;
       source < std::min(sourceSkeleton->NumJoints(), numValues); ++source) {
    if (sourceSkeleton == targetSkeleton)
      mapping[source] = static_cast<std::int16_t>(source);
    else
      mapping[source] = SkeletonJointIndex(
                            targetSkeleton,
                            sourceSkeleton->GetJointName(source))
                            .value;
  }
  const auto unmapped = static_cast<std::uint8_t>(
      (targetSkeleton->data->numJoints + 7) & ~7);
  return GenerateRLE(bits.list, numValues, mapping.data(), unmapped, rle);
}

int GenerateUserChannelRLE(const idMD6Skel* sourceSkeleton,
                           const idMD6Skel* targetSkeleton,
                           int numPaddedUserChannels,
                           const idList<std::uint8_t, 5>& bits,
                           idList<std::uint8_t, 5>& rle) {
  int count = std::max(numPaddedUserChannels, 0);
  if (sourceSkeleton != nullptr && sourceSkeleton->data != nullptr)
    count = std::min(count,
                     (sourceSkeleton->data->numUserChannels + 7) & ~7);
  std::vector<std::int16_t> mapping(static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index)
    mapping[index] = sourceSkeleton == nullptr || targetSkeleton == nullptr ||
                             sourceSkeleton == targetSkeleton
                         ? static_cast<std::int16_t>(index)
                         : -1;
  const auto unmapped = static_cast<std::uint8_t>(
      targetSkeleton != nullptr && targetSkeleton->data != nullptr
          ? (targetSkeleton->data->numUserChannels + 7) & ~7
          : count);
  return GenerateRLE(bits.list, count, mapping.data(), unmapped, rle);
}

void Reduce(const idList<float, 5>& keyFrames, int components,
            int numChannels, int leftFrame, int rightFrame,
            int channelIndex, float tolerance, bool quaternionChannel,
            idList<char, 5>& mask) {
  if (rightFrame - leftFrame <= 1 || components <= 0 ||
      numChannels <= 0)
    return;
  const auto Key = [&](int frame) {
    return keyFrames.list +
           (frame * numChannels + channelIndex) * components;
  };
  const float* left = Key(leftFrame);
  const float* right = Key(rightFrame);
  int splitFrame = -1;
  float maximumError = tolerance;
  for (int frame = leftFrame + 1; frame < rightFrame; ++frame) {
    const float fraction = static_cast<float>(frame - leftFrame) /
                           static_cast<float>(rightFrame - leftFrame);
    const float* actual = Key(frame);
    float errorSquared = 0.0f;
    if (quaternionChannel && components == 4) {
      float adjustedRight[4] = {right[0], right[1], right[2], right[3]};
      const float dot = left[0] * right[0] + left[1] * right[1] +
                        left[2] * right[2] + left[3] * right[3];
      if (dot < 0.0f)
        for (float& value : adjustedRight)
          value = -value;
      float interpolated[4];
      float lengthSquared = 0.0f;
      for (int component = 0; component < 4; ++component) {
        interpolated[component] =
            left[component] +
            fraction * (adjustedRight[component] - left[component]);
        lengthSquared += interpolated[component] * interpolated[component];
      }
      const float inverseLength =
          1.0f / std::sqrt(std::max(lengthSquared,
                                    std::numeric_limits<float>::min()));
      float direct = 0.0f;
      float negated = 0.0f;
      for (int component = 0; component < 4; ++component) {
        const float value = interpolated[component] * inverseLength;
        direct += (actual[component] - value) * (actual[component] - value);
        negated += (actual[component] + value) * (actual[component] + value);
      }
      errorSquared = std::min(direct, negated);
    } else {
      for (int component = 0; component < components; ++component) {
        const float interpolated =
            left[component] + fraction * (right[component] - left[component]);
        const float difference = actual[component] - interpolated;
        errorSquared += difference * difference;
      }
    }
    const float error = std::sqrt(errorSquared);
    if (error > maximumError) {
      maximumError = error;
      splitFrame = frame;
    }
  }
  if (splitFrame < 0)
    return;
  mask[splitFrame * numChannels + channelIndex] = 1;
  Reduce(keyFrames, components, numChannels, leftFrame, splitFrame,
         channelIndex, tolerance, quaternionChannel, mask);
  Reduce(keyFrames, components, numChannels, splitFrame, rightFrame,
         channelIndex, tolerance, quaternionChannel, mask);
}

idCompressionTolerances::idCompressionTolerances(
    const idMD6Skel* skeleton,
    const idMD6Anim::idCompressionSettings& settings, int numFrames,
    float equalToleranceModifier, int numPaddedJoints,
    int numPaddedUserChannels, float jointTolerance,
    float userChannelTolerance) {
  const float jointBase = jointTolerance * equalToleranceModifier;
  const float userBase = userChannelTolerance * equalToleranceModifier;
  settings.SetupJointTolerance(skeleton, rotation, numPaddedJoints,
                               numFrames, jointBase);
  settings.SetupJointTolerance(skeleton, scale, numPaddedJoints,
                               numFrames, jointBase);
  settings.SetupJointTolerance(skeleton, translation, numPaddedJoints,
                               numFrames, jointBase);
  settings.SetupUserChannelTolerance(skeleton, userChannels,
                                     numPaddedUserChannels, numFrames,
                                     userBase);
}

std::int64_t CalculateFrameSize(
    int frameStart, int frameRange, int numJoints,
    int numPaddedUserChannels, int numAnimR, int numAnimS, int numAnimT,
    int numAnimU, const idList<char, 5>& maskR,
    const idList<char, 5>& maskS, const idList<char, 5>& maskT,
    const idList<char, 5>& maskU, int& sizeR, int& sizeS, int& sizeT,
    int& sizeU) {
  sizeR = sizeS = sizeT = sizeU = 0;
  for (int frame = frameStart + 1; frame < frameStart + frameRange; ++frame) {
    for (int joint = 0; joint < numJoints; ++joint) {
      const int index = frame * numJoints + joint;
      if (maskR[index] != 0) sizeR += 6;
      if (maskS[index] != 0) sizeS += 12;
      if (maskT[index] != 0) sizeT += 12;
    }
    for (int channel = 0; channel < numPaddedUserChannels; ++channel) {
      if (maskU[frame * numPaddedUserChannels + channel] != 0)
        sizeU += 4;
    }
  }
  const auto Align16 = [](int value) { return (value + 15) & ~15; };
  const int paddedFrameRange = (frameRange + 7) & ~7;
  const auto FrameBitBytes = [paddedFrameRange, &Align16](int count) {
    return Align16((count * paddedFrameRange + 7) / 8);
  };
  const int baseR = Align16(6 * numAnimR);
  const int baseS = Align16(12 * numAnimS);
  const int baseT = Align16(12 * numAnimT);
  const int baseU = Align16(4 * numAnimU);
  return static_cast<std::int64_t>(Align16(
      baseR + baseS + baseT + Align16(sizeS) + Align16(sizeR) +
      FrameBitBytes(numAnimU) + FrameBitBytes(numAnimT) +
      FrameBitBytes(numAnimS) + FrameBitBytes(numAnimR) + baseU +
      Align16(sizeT) + Align16(sizeU) + baseS + baseU + baseT + baseR + 48 +
      70));
}

} // namespace idMD6AnimCompression

bool idMD6Anim::WaitForValid() const {
  while (postedForGeneration) {
    if (g_waitCallback != nullptr)
      g_waitCallback(10);
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  return animData != nullptr;
}

idMD6AnimResourceList idMD6Anim::resourceList;

idMD6AnimResourceList* idMD6Anim::GetResourceList() const {
  return &resourceList;
}

idMD6Anim* idMD6Anim::LoadAnim_UserChannels(const char* filename) {
  return g_loadCallback != nullptr
             ? g_loadCallback(filename, true)
             : resourceList.Load(filename, true, true);
}

bool idMD6Anim::AnimIsOkForSkeleton(const idMD6Skel* skeleton) const {
  if (skeleton == nullptr)
    return false;
  if (animData == nullptr)
    return postedForGeneration;
  if (skeleton->data == nullptr)
    return false;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(animData);
  std::uint16_t parentTableCrc;
  std::memcpy(&parentTableCrc, bytes + sizeof(idMD6AnimData),
              sizeof(parentTableCrc));
  return (skeleton->data->parentTblCrc.value == parentTableCrc &&
          skeleton->jointConversion.value == 0xffffu) ||
         (skeleton->jointConversion.value != 0xffffu &&
          skeleton->jointConversionChecksum.value == jointConversionChecksum);
}

bool idMD6Anim::AnimIsOkForModel(const idMD6Model* model) const {
  return model != nullptr && AnimIsOkForSkeleton(model->skeleton);
}

void idMD6Anim::VerifyRLEData(
    const char* animationName,
    const idList<std::uint8_t, 5>& rotationRle,
    const idList<std::uint8_t, 5>& scaleRle,
    const idList<std::uint8_t, 5>& translationRle,
    const idList<std::uint8_t, 5>& userChannelRle,
    const idList<std::uint8_t, 5>& rotationBits,
    const idList<std::uint8_t, 5>& scaleBits,
    const idList<std::uint8_t, 5>& translationBits,
    const idList<std::uint8_t, 5>& userChannelBits) const {
  std::uint8_t decodedBits[4][272]{};
  std::uint8_t decodedRle[4][272]{};
  const idList<std::uint8_t, 5>* bitLists[] = {
      &rotationBits, &scaleBits, &translationBits, &userChannelBits};
  const idList<std::uint8_t, 5>* rleLists[] = {
      &rotationRle, &scaleRle, &translationRle, &userChannelRle};
  bool matches = true;
  for (int component = 0; component < 4; ++component) {
    const std::uint32_t bitCount = DecodeBitArray(
        bitLists[component]->list,
        static_cast<std::uint32_t>(8 * bitLists[component]->num),
        decodedBits[component]);
    const std::uint32_t rleCount =
        DecodeRLE(rleLists[component]->list, 0xffu,
                  decodedRle[component]);
    matches &= bitCount == rleCount &&
               std::memcmp(decodedBits[component], decodedRle[component],
                           bitCount) == 0;
  }
  if (!matches && g_warningCallback != nullptr)
    g_warningCallback("animation RLE differs from bit array", animationName);
}

bool idMD6Anim::VerifyBoundingBoxes(const idDeclMD6* declaration,
                                    float epsilon,
                                    float jointRadius) const {
  if (g_boundsVerifier != nullptr)
    return g_boundsVerifier(this, declaration, epsilon, jointRadius);
  if (animData == nullptr || declaration == nullptr ||
      declaration->model == nullptr || declaration->model->skeleton == nullptr)
    return false;
  const auto Ordered = [epsilon](const idBoundsShort& bounds) {
    for (int axis = 0; axis < 3; ++axis)
      if (static_cast<float>(bounds.b[0][axis]) >
          static_cast<float>(bounds.b[1][axis]) + epsilon)
        return false;
    return true;
  };
  (void)jointRadius;
  return Ordered(translatedBounds) && Ordered(normalizedBounds);
}

void idMD6Anim::GetAnimationDelta(idVec3* deltaTranslation,
                                  idMat3* deltaAxis) const {
  if (animData == nullptr) {
    if (deltaTranslation != nullptr)
      *deltaTranslation = {};
    if (deltaAxis != nullptr)
      *deltaAxis = QuaternionToMatrix({0.0f, 0.0f, 0.0f, 1.0f});
    return;
  }
  const idQuat inverseStart = NormalizedConjugate(animData->startDelta);
  if (deltaAxis != nullptr)
    *deltaAxis = QuaternionToMatrix(Multiply(inverseStart,
                                             animData->endDelta));
  if (deltaTranslation != nullptr) {
    const idVec3 translation{
        animData->endDelta[8] - animData->startDelta[8],
        animData->endDelta[9] - animData->startDelta[9],
        animData->endDelta[10] - animData->startDelta[10]};
    *deltaTranslation = Rotate(inverseStart, translation);
  }
}

int idMD6Anim::GetDurationInGameTime(int gameHz) const {
  (void)gameHz;
  if (animData == nullptr || animData->frameRate == 0)
    return 0;
  return static_cast<int>(static_cast<float>(g_gameTimePerSecond) *
                          (static_cast<float>(animData->numFrames) /
                           static_cast<float>(animData->frameRate)));
}

void idMD6Anim::SwapFrameBits(std::uint8_t* bits, std::uint32_t numJoints,
                              std::uint32_t numFrames) {
  const std::size_t bytesPerJoint = (numFrames + 7u) >> 3u;
  for (std::uint32_t joint = 0; joint < numJoints; ++joint) {
    std::reverse(bits, bits + bytesPerJoint);
    bits += bytesPerJoint;
  }
}

void idMD6Anim::SwapAnimMap(idMD6AnimData* data) {
  auto* crcTable = reinterpret_cast<std::uint16_t*>(data + 1);
  for (std::uint16_t map = 0; map < data->numAnimMaps; ++map)
    crcTable[map] = ByteSwap16(crcTable[map]);
  auto* mapOffsets = crcTable + data->numAnimMaps;
  for (std::uint16_t map = 0; map < data->numAnimMaps; ++map)
    for (int offset = 0; offset < 8; ++offset)
      mapOffsets[map * 8 + offset] =
          ByteSwap16(mapOffsets[map * 8 + offset]);
}

void idMD6Anim::SwapHeader(idMD6AnimData* data) {
  data->totalSize = ByteSwap32(data->totalSize);
  auto* fields = &data->size;
  for (int field = 0; field < 13; ++field)
    fields[field] = ByteSwap16(fields[field]);
  for (float& value : data->startDelta)
    SwapFloat(value);
  for (float& value : data->endDelta)
    SwapFloat(value);
}

void idMD6Anim::SwapFrameSetDataHeader(frameSetData_t* frameSet) {
  auto* fields = &frameSet->firstROffset;
  for (int field = 0; field < 19; ++field)
    fields[field] = ByteSwap16(fields[field]);
}

void idMD6Anim::SwapFrameSetFrameBits(const idMD6AnimData* data,
                                      frameSetData_t* frameSet) {
  const auto* crcTable = reinterpret_cast<const std::uint16_t*>(data + 1);
  const auto* mapOffsets = crcTable + data->numAnimMaps;
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(data);
  const auto CountAt = [&](int component) -> std::uint32_t {
    if ((data->flags & (component == 3 ? 0x800u : 0x400u)) == 0 ||
        data->numAnimMaps == 0)
      return 0;
    return bytes[mapOffsets[4 + component]];
  };
  auto* frameBytes = reinterpret_cast<std::uint8_t*>(frameSet);
  SwapFrameBits(frameBytes + frameSet->RBitsOffset, CountAt(0),
                frameSet->frameRange);
  SwapFrameBits(frameBytes + frameSet->SBitsOffset, CountAt(1),
                frameSet->frameRange);
  SwapFrameBits(frameBytes + frameSet->TBitsOffset, CountAt(2),
                frameSet->frameRange);
  SwapFrameBits(frameBytes + frameSet->UBitsOffset, CountAt(3),
                frameSet->frameRange);
}

void idMD6Anim::SwapFrameSetData(const idMD6AnimData* data,
                                 frameSetData_t* frameSet,
                                 bool swapHeaderFirst) {
  if (swapHeaderFirst)
    SwapFrameSetDataHeader(frameSet);
  auto* bytes = reinterpret_cast<std::uint8_t*>(frameSet);
  Swap16Range(bytes + frameSet->firstROffset,
              bytes + frameSet->firstSOffset);
  Swap32Range(bytes + frameSet->firstSOffset,
              bytes + frameSet->firstTOffset);
  Swap32Range(bytes + frameSet->firstTOffset,
              bytes + frameSet->firstUOffset);
  Swap32Range(bytes + frameSet->firstUOffset,
              bytes + frameSet->rangeROffset);
  Swap16Range(bytes + frameSet->rangeROffset,
              bytes + frameSet->rangeSOffset);
  Swap32Range(bytes + frameSet->rangeSOffset,
              bytes + frameSet->rangeTOffset);
  Swap32Range(bytes + frameSet->rangeTOffset,
              bytes + frameSet->rangeUOffset);
  Swap32Range(bytes + frameSet->rangeUOffset,
              bytes + frameSet->RBitsOffset);
  SwapFrameSetFrameBits(data, frameSet);
  if (!swapHeaderFirst)
    SwapFrameSetDataHeader(frameSet);
}

void idMD6Anim::SwapAnimInfo(idMD6AnimData* data, bool swapHeaderFirst) {
  if (data == nullptr)
    return;
  if (swapHeaderFirst)
    SwapHeader(data);
  auto* bytes = reinterpret_cast<std::uint8_t*>(data);
  Swap16Range(bytes + data->constROffset, bytes + data->constSOffset);
  Swap32Range(bytes + data->constSOffset, bytes + data->constTOffset);
  Swap32Range(bytes + data->constTOffset, bytes + data->constUOffset);
  const std::uint16_t endU = data->jointWeightsOffset != 0
                                 ? data->jointWeightsOffset
                                 : data->frameSetTblOffset;
  Swap32Range(bytes + data->constUOffset, bytes + endU);

  auto* frameSetOffsets = reinterpret_cast<std::uint32_t*>(
      bytes + data->frameSetOffsetTblOffset);
  if (swapHeaderFirst) {
    SwapAnimMap(data);
    for (std::uint32_t index = 0; index <= data->numFrameSets; ++index)
      frameSetOffsets[index] = ByteSwap32(frameSetOffsets[index]);
  }
  for (std::uint16_t index = 0; index < data->numFrameSets; ++index) {
    auto* frameSet = reinterpret_cast<frameSetData_t*>(
        bytes + 16u * frameSetOffsets[index]);
    SwapFrameSetData(data, frameSet, swapHeaderFirst);
  }
  if (!swapHeaderFirst) {
    SwapAnimMap(data);
    for (std::uint32_t index = 0; index <= data->numFrameSets; ++index)
      frameSetOffsets[index] = ByteSwap32(frameSetOffsets[index]);
    SwapHeader(data);
  }
}

bool idMD6Anim::LoadBinary(const char* filename, bool allowDefaulted) {
  (void)allowDefaulted;
  std::ifstream input(filename, std::ios::binary);
  if (!input)
    return false;
  std::uint32_t magic = 0;
  input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  if (!input || magic != kMD6BinaryMagic)
    return false;

  idMD6AnimFileHeader header;
  input.read(reinterpret_cast<char*>(&header), sizeof(header));
  if (!input)
    return false;
  header.Swap();
  if (header.animDataSize < sizeof(idMD6AnimData) ||
      header.animDataSize > (256u << 20u))
    return false;

  std::string conversionName;
  std::string skeletonName;
  if (!ReadString(input, conversionName) || !ReadString(input, skeletonName))
    return false;

  idMD6AnimData* loaded = AllocateAnimData(header.animDataSize);
  if (loaded == nullptr)
    return false;
  input.read(reinterpret_cast<char*>(loaded), header.animDataSize);
  if (!input) {
    ReleaseOwnedAnimData(loaded);
    return false;
  }
  SwapAnimInfo(loaded, true);
  if (loaded->totalSize != header.animDataSize) {
    ReleaseOwnedAnimData(loaded);
    return false;
  }

  // The phase track follows the animation data and uses the same big-endian
  // 16-bit header as the recovered idMD6PhaseTrack serializer.
  std::uint16_t phaseSizeBE = 0;
  input.read(reinterpret_cast<char*>(&phaseSizeBE), sizeof(phaseSizeBE));
  if (!input.eof() && !input) {
    ReleaseOwnedAnimData(loaded);
    return false;
  }
  const std::uint16_t phaseSize = ByteSwap16(phaseSizeBE);
  void* loadedPhase = nullptr;
  if (phaseSize > 0) {
    if (phaseSize < 16) {
      ReleaseOwnedAnimData(loaded);
      return false;
    }
    loadedPhase = AllocatePhaseTrack(phaseSize);
    if (loadedPhase == nullptr) {
      ReleaseOwnedAnimData(loaded);
      return false;
    }
    auto* phaseWords = static_cast<std::uint16_t*>(loadedPhase);
    phaseWords[0] = phaseSize;
    input.read(reinterpret_cast<char*>(phaseWords + 1), phaseSize - 2u);
    if (!input) {
      idMD6Anim temporary;
      SetPhaseTrackData(temporary, loadedPhase);
      ReleaseOwnedPhaseTrack(temporary);
      ReleaseOwnedAnimData(loaded);
      return false;
    }
    phaseWords[1] = ByteSwap16(phaseWords[1]);
    phaseWords[2] = ByteSwap16(phaseWords[2]);
    phaseWords[3] = ByteSwap16(phaseWords[3]);
  }

  ReleaseOwnedAnimData(animData);
  ReleaseOwnedPhaseTrack(*this);
  animData = loaded;
  SetPhaseTrackData(*this, loadedPhase);
  timestamps[0] = header.timestamp;
  timestamps[1] = header.phaseTrackTimestamp;
  timestamps[2] = header.settingsTimestamp;
  timestamps[3] = header.skeletonTimestamp;
  timestamps[4] = header.blenddataMeshTimestamp;
  timeCode = header.timeCode;
  translatedBounds = header.translatedBounds;
  normalizedBounds = header.normalizedBounds;
  jointConversionChecksum = header.jointConversionChecksum;
  jointConversionHandle = 0xffffu;
  origWorldPos = header.origWorldPos;
  origWorldRot = header.origWorldRot;
  origWorldScale = header.origWorldScale;
  startFrameOffset = header.startFrameOffset;
  endFrameOffset = header.endFrameOffset;
  skelName.Set(skeletonName.c_str());
  postedForGeneration = false;
  return true;
}

bool idMD6Anim::WriteBinary(const char* filename) const {
  if (animData == nullptr || filename == nullptr)
    return false;
  std::ofstream output(filename, std::ios::binary | std::ios::trunc);
  if (!output)
    return false;

  output.write(reinterpret_cast<const char*>(&kMD6BinaryMagic),
               sizeof(kMD6BinaryMagic));
  idMD6AnimFileHeader header;
  header.timestamp = timestamps[0];
  header.phaseTrackTimestamp = timestamps[1];
  header.settingsTimestamp = timestamps[2];
  header.skeletonTimestamp = timestamps[3];
  header.blenddataMeshTimestamp = timestamps[4];
  header.timeCode = timeCode;
  header.translatedBounds = translatedBounds;
  header.normalizedBounds = normalizedBounds;
  header.jointConversionChecksum = jointConversionChecksum;
  header.origWorldPos = origWorldPos;
  header.origWorldRot = origWorldRot;
  header.origWorldScale = origWorldScale;
  header.startFrameOffset = startFrameOffset;
  header.endFrameOffset = endFrameOffset;
  header.animDataSize = animData->totalSize;
  header.Swap();
  output.write(reinterpret_cast<const char*>(&header), sizeof(header));
  if (!WriteString(output, "") || !WriteString(output, skelName.str))
    return false;

  std::vector<std::uint8_t> swapped(animData->totalSize);
  std::memcpy(swapped.data(), animData, swapped.size());
  SwapAnimInfo(reinterpret_cast<idMD6AnimData*>(swapped.data()), false);
  output.write(reinterpret_cast<const char*>(swapped.data()), swapped.size());
  const void* phaseData = PhaseTrackData(*this);
  if (phaseData == nullptr) {
    const std::uint16_t emptyPhaseTrack = 0;
    output.write(reinterpret_cast<const char*>(&emptyPhaseTrack),
                 sizeof(emptyPhaseTrack));
  } else {
    const auto* phaseWords = static_cast<const std::uint16_t*>(phaseData);
    const std::uint16_t phaseSize = phaseWords[0];
    if (phaseSize < 16)
      return false;
    std::vector<std::uint8_t> swappedPhase(phaseSize);
    std::memcpy(swappedPhase.data(), phaseData, phaseSize);
    auto* swappedWords = reinterpret_cast<std::uint16_t*>(swappedPhase.data());
    for (int field = 0; field < 4; ++field)
      swappedWords[field] = ByteSwap16(swappedWords[field]);
    output.write(reinterpret_cast<const char*>(swappedPhase.data()),
                 swappedPhase.size());
  }
  return output.good();
}

bool idMD6Anim::ReloadIfStale() {
  if (name.str == nullptr || *name.str == '\0')
    return false;
  return LoadBinary(name.str, false);
}

void idMD6Anim::FreeData() {
  ReleaseOwnedAnimData(animData);
  ReleaseOwnedPhaseTrack(*this);
  postedForGeneration = false;
}

void idMD6Anim::MakeDefault(const idMD6Skel* skeleton) {
  ReleaseOwnedAnimData(animData);
  ReleaseOwnedPhaseTrack(*this);
  constexpr std::size_t allocationSize = 176;
  animData = AllocateAnimData(allocationSize);
  if (animData == nullptr)
    return;
  animData->totalSize = static_cast<std::uint32_t>(allocationSize);
  animData->size = sizeof(idMD6AnimData);
  animData->flags = 0x40u;
  animData->numFrames = 1;
  animData->frameRate = 30;
  animData->numFrameSets = 0;
  animData->frameSetTblOffset = 160;
  animData->frameSetOffsetTblOffset = 160;
  animData->numAnimMaps = 1;
  animData->constROffset = 160;
  animData->constSOffset = 160;
  animData->constTOffset = 160;
  animData->constUOffset = 160;
  animData->nextSize = 176;
  animData->startDelta[3] = 1.0f;
  animData->endDelta[3] = 1.0f;
  auto* mapCrc = reinterpret_cast<std::uint16_t*>(animData + 1);
  mapCrc[0] = skeleton != nullptr && skeleton->data != nullptr
                  ? skeleton->data->parentTblCrc.value
                  : 0xffffu;
  auto* mapOffsets = mapCrc + 1;
  for (int index = 0; index < 8; ++index)
    mapOffsets[index] = 174;
  auto* frameOffsets = reinterpret_cast<std::uint32_t*>(
      reinterpret_cast<std::uint8_t*>(animData) +
      animData->frameSetOffsetTblOffset);
  frameOffsets[0] = 11;
  jointConversionHandle = skeleton != nullptr
                              ? skeleton->jointConversion.value
                              : 0xffffu;
  jointConversionChecksum = skeleton != nullptr
                                ? skeleton->jointConversionChecksum.value
                                : 0xffffu;
  postedForGeneration = false;
}

void idMD6Anim::CompressAnim(
    const idMD6Skel* skeleton,
    const idList<const idMD6Skel*, 5>& mappedSkeletons,
    idList<float, 5>& rotation, idList<float, 5>& scale,
    idList<float, 5>& translation, idList<float, 5>& userChannels,
    const idList<idJointIndex_t, 5>& maskRJoints,
    const idList<idJointIndex_t, 5>& maskSJoints,
    const idList<idJointIndex_t, 5>& maskTJoints,
    const idList<idUserChannelIndex, 5>& maskUChannels,
    const idList<float, 5>& jointWeights, std::uint16_t flags,
    int numFrames, std::uint16_t frameRate,
    const animationSettings_t& settings) {
  (void)maskRJoints;
  (void)maskSJoints;
  (void)maskTJoints;
  (void)maskUChannels;
  (void)settings;
  if (skeleton == nullptr || skeleton->data == nullptr || numFrames <= 0 ||
      numFrames > 255 || frameRate == 0) {
    MakeDefault(skeleton);
    return;
  }
  const int numJoints = skeleton->data->numJoints;
  const int paddedJoints = (numJoints + 7) & ~7;
  const int numChannels = skeleton->data->numUserChannels;
  const int paddedChannels = (numChannels + 7) & ~7;
  const int rotationStride = paddedJoints * 4;
  const int vectorStride = paddedJoints * 4;
  if (rotation.num < rotationStride || scale.num < vectorStride ||
      translation.num < vectorStride || userChannels.num < paddedChannels) {
    MakeDefault(skeleton);
    return;
  }

  const auto Align16Size = [](std::size_t value) {
    return (value + 15u) & ~std::size_t(15u);
  };
  const int numMaps = std::max(mappedSkeletons.num, 1);
  std::vector<std::uint8_t> image(
      Align16Size(sizeof(idMD6AnimData) + numMaps * 2u + numMaps * 16u), 0);
  auto AppendBytes = [&](const void* source, std::size_t size,
                         bool alignAfter = false) {
    const std::size_t offset = image.size();
    const auto* bytes = static_cast<const std::uint8_t*>(source);
    image.insert(image.end(), bytes, bytes + size);
    if (alignAfter)
      image.resize(Align16Size(image.size()), 0);
    return static_cast<std::uint16_t>(offset);
  };
  auto AppendRLE = [&](int count) {
    idList<std::uint8_t, 5> rle;
    std::vector<std::uint8_t> bits((count + 7) / 8, 0xffu);
    if ((count & 7) != 0)
      bits.back() = static_cast<std::uint8_t>((1u << (count & 7)) - 1u);
    idMD6AnimCompression::GenerateRLE(bits.data(), count, nullptr,
                                      static_cast<std::uint8_t>(count), rle);
    return AppendBytes(rle.list, rle.num);
  };

  std::uint16_t commonMap[8]{};
  idList<std::uint8_t, 5> noConstants;
  noConstants.Append(static_cast<std::uint8_t>(0));
  for (int component = 0; component < 4; ++component)
    commonMap[component] = AppendBytes(noConstants.list, noConstants.num);
  commonMap[4] = AppendRLE(numJoints);
  commonMap[5] = AppendRLE(numJoints);
  commonMap[6] = AppendRLE(numJoints);
  commonMap[7] = AppendRLE(numChannels);
  image.resize(Align16Size(image.size()), 0);

  const std::uint16_t constROffset =
      static_cast<std::uint16_t>(image.size());
  const std::uint16_t constSOffset =
      static_cast<std::uint16_t>(image.size());
  const std::uint16_t constTOffset =
      static_cast<std::uint16_t>(image.size());
  const std::uint16_t constUOffset =
      static_cast<std::uint16_t>(image.size());

  std::uint16_t jointWeightsOffset = 0;
  if (jointWeights.num > 0) {
    jointWeightsOffset = static_cast<std::uint16_t>(image.size());
    AppendBytes(jointWeights.list,
                static_cast<std::size_t>(jointWeights.num) * sizeof(float),
                true);
  }
  const std::uint16_t frameSetTableOffset =
      static_cast<std::uint16_t>(image.size());
  for (int frame = 0; frame < numFrames; ++frame)
    image.push_back(static_cast<std::uint8_t>(frame));
  image.resize(Align16Size(image.size()), 0);
  const std::uint16_t frameSetOffsetTableOffset =
      static_cast<std::uint16_t>(image.size());
  image.resize(image.size() +
                   static_cast<std::size_t>(numFrames + 1) *
                       sizeof(std::uint32_t),
               0);
  image.resize(Align16Size(image.size()), 0);
  struct FrameLayout {
    std::size_t base;
    std::uint16_t first[4];
  };
  std::vector<FrameLayout> frameLayouts;
  frameLayouts.reserve(static_cast<std::size_t>(numFrames));
  for (int frame = 0; frame < numFrames; ++frame) {
    image.resize(Align16Size(image.size()), 0);
    const std::size_t frameBase = image.size();
    image.resize(frameBase + sizeof(frameSetData_t), 0);
    frameSetData_t frameSet{};
    FrameLayout layout{frameBase, {}};

    image.resize(Align16Size(image.size()), 0);
    layout.first[0] = frameSet.firstROffset =
        static_cast<std::uint16_t>(image.size() - frameBase);
    for (int joint = 0; joint < numJoints; ++joint) {
      std::int16_t packed[3];
      idMD6AnimCompression::CompressR(
          rotation.list + frame * rotationStride + joint * 4, packed);
      AppendBytes(packed, sizeof(packed));
    }
    image.resize(Align16Size(image.size()), 0);
    layout.first[1] = frameSet.firstSOffset =
        static_cast<std::uint16_t>(image.size() - frameBase);
    for (int joint = 0; joint < numJoints; ++joint)
      AppendBytes(scale.list + frame * vectorStride + joint * 4,
                  3 * sizeof(float));
    image.resize(Align16Size(image.size()), 0);
    layout.first[2] = frameSet.firstTOffset =
        static_cast<std::uint16_t>(image.size() - frameBase);
    for (int joint = 0; joint < numJoints; ++joint)
      AppendBytes(translation.list + frame * vectorStride + joint * 4,
                  3 * sizeof(float));
    image.resize(Align16Size(image.size()), 0);
    layout.first[3] = frameSet.firstUOffset =
        static_cast<std::uint16_t>(image.size() - frameBase);
    if (numChannels > 0)
      AppendBytes(userChannels.list + frame * paddedChannels,
                  static_cast<std::size_t>(numChannels) * sizeof(float));
    image.resize(Align16Size(image.size()), 0);

    frameSet.rangeROffset = frameSet.rangeSOffset =
        frameSet.rangeTOffset = frameSet.rangeUOffset =
            static_cast<std::uint16_t>(image.size() - frameBase);
    const auto AppendFrameBits = [&](int count) {
      const std::uint16_t offset =
          static_cast<std::uint16_t>(image.size() - frameBase);
      image.resize(image.size() + static_cast<std::size_t>(count), 1u);
      image.resize(Align16Size(image.size()), 0);
      return offset;
    };
    frameSet.RBitsOffset = AppendFrameBits(numJoints);
    frameSet.SBitsOffset = AppendFrameBits(numJoints);
    frameSet.TBitsOffset = AppendFrameBits(numJoints);
    frameSet.UBitsOffset = AppendFrameBits(numChannels);
    frameSet.totalSize = static_cast<std::uint16_t>(image.size() - frameBase);
    frameSet.frameStart = static_cast<std::uint16_t>(frame);
    frameSet.frameRange = 1;
    std::memcpy(image.data() + frameBase, &frameSet, sizeof(frameSet));
    frameLayouts.push_back(layout);
    const std::uint32_t shifted =
        static_cast<std::uint32_t>(frameBase / 16u);
    std::memcpy(image.data() + frameSetOffsetTableOffset + frame * 4,
                &shifted, sizeof(shifted));
  }
  const std::uint32_t endShifted =
      static_cast<std::uint32_t>(Align16Size(image.size()) / 16u);
  std::memcpy(image.data() + frameSetOffsetTableOffset + numFrames * 4,
              &endShifted, sizeof(endShifted));
  for (int frame = 0; frame < numFrames; ++frame) {
    const int nextFrame = frame + 1 < numFrames ? frame + 1 : frame;
    auto* frameSet = reinterpret_cast<frameSetData_t*>(
        image.data() + frameLayouts[frame].base);
    frameSet->nextROffset = static_cast<std::uint16_t>(
        frameLayouts[nextFrame].base + frameLayouts[nextFrame].first[0] -
        frameLayouts[frame].base);
    frameSet->nextSOffset = static_cast<std::uint16_t>(
        frameLayouts[nextFrame].base + frameLayouts[nextFrame].first[1] -
        frameLayouts[frame].base);
    frameSet->nextTOffset = static_cast<std::uint16_t>(
        frameLayouts[nextFrame].base + frameLayouts[nextFrame].first[2] -
        frameLayouts[frame].base);
    frameSet->nextUOffset = static_cast<std::uint16_t>(
        frameLayouts[nextFrame].base + frameLayouts[nextFrame].first[3] -
        frameLayouts[frame].base);
  }

  auto* compressed = reinterpret_cast<idMD6AnimData*>(image.data());
  compressed->totalSize = static_cast<std::uint32_t>(image.size());
  compressed->size = frameSetTableOffset;
  compressed->flags = static_cast<std::uint16_t>(flags | 0xc00u);
  compressed->numFrames = static_cast<std::uint16_t>(numFrames);
  compressed->frameRate = frameRate;
  compressed->numFrameSets = static_cast<std::uint16_t>(numFrames);
  compressed->frameSetTblOffset = frameSetTableOffset;
  compressed->frameSetOffsetTblOffset = frameSetOffsetTableOffset;
  compressed->numAnimMaps = static_cast<std::uint16_t>(numMaps);
  compressed->constROffset = constROffset;
  compressed->constSOffset = constSOffset;
  compressed->constTOffset = constTOffset;
  compressed->constUOffset = constUOffset;
  compressed->nextSize = 0;
  compressed->jointWeightsOffset = jointWeightsOffset;
  std::memcpy(compressed->startDelta, rotation.list, 4 * sizeof(float));
  std::memcpy(compressed->endDelta,
              rotation.list + (numFrames - 1) * rotationStride,
              4 * sizeof(float));
  std::memcpy(compressed->startDelta + 8, translation.list,
              3 * sizeof(float));
  std::memcpy(compressed->endDelta + 8,
              translation.list + (numFrames - 1) * vectorStride,
              3 * sizeof(float));

  auto* crcTable = reinterpret_cast<std::uint16_t*>(compressed + 1);
  auto* mapTable = crcTable + numMaps;
  for (int map = 0; map < numMaps; ++map) {
    const idMD6Skel* mapped = mappedSkeletons.num > 0
                                  ? mappedSkeletons[map]
                                  : skeleton;
    crcTable[map] = mapped != nullptr && mapped->data != nullptr
                        ? mapped->data->parentTblCrc.value
                        : skeleton->data->parentTblCrc.value;
    std::memcpy(mapTable + map * 8, commonMap, sizeof(commonMap));
  }

  ReleaseOwnedAnimData(animData);
  animData = AllocateAnimData(image.size());
  if (animData != nullptr)
    std::memcpy(animData, image.data(), image.size());
  jointConversionHandle = skeleton->jointConversion.value;
  jointConversionChecksum = skeleton->jointConversionChecksum.value;
  postedForGeneration = false;
}

bool idMD6Anim::LoadFacetracksComposite(
    animationSettings_t& settings, char* basePath,
    const idMD6Skel* skeleton, idList<idJointIndex_t, 5>& maskRJoints,
    idList<idJointIndex_t, 5>& maskSJoints,
    idList<idJointIndex_t, 5>& maskTJoints,
    idList<float, 5>& jointWeights) {
  if (g_facetrackLoader != nullptr)
    return g_facetrackLoader(this, settings, basePath, skeleton, maskRJoints,
                             maskSJoints, maskTJoints, jointWeights);
  return basePath != nullptr && g_textLoader != nullptr &&
         g_textLoader(this, basePath, &settings);
}

bool idMD6Anim::LoadText(char* filename,
                         animationSettings_t* settingsOverride) {
  if (filename == nullptr)
    return false;
  FreeData();
  origWorldPos = {};
  origWorldRot = {};
  if (g_textLoader != nullptr)
    return g_textLoader(this, filename, settingsOverride);
  std::ifstream probe(filename, std::ios::binary);
  std::uint32_t magic = 0;
  probe.read(reinterpret_cast<char*>(&magic), sizeof(magic));
  probe.close();
  return magic == kMD6BinaryMagic && LoadBinary(filename, false);
}

bool idMD6Anim::LoadTextFromParser(idParser* source,
                                   animationSettings_t* settingsOverride) {
  if (source == nullptr)
    return false;
  if (g_parserLoader != nullptr)
    return g_parserLoader(this, source, settingsOverride);
  animationSettings_t defaults;
  animationSettings_t& settings =
      settingsOverride != nullptr ? *settingsOverride : defaults;
  idToken token;
  int numFrames = 1;
  int frameRate = settings.frameRate;
  while (source->ReadToken(token)) {
    if (idStr::Icmp(token.data, "numFrames") == 0)
      numFrames = source->ParseInt();
    else if (idStr::Icmp(token.data, "frameRate") == 0)
      frameRate = source->ParseInt();
    else if (idStr::Icmp(token.data, "flags") == 0)
      settings.flags = static_cast<std::uint16_t>(source->ParseInt());
  }
  if (numFrames <= 0 || frameRate <= 0)
    return false;
  MakeDefault(nullptr);
  if (animData == nullptr)
    return false;
  animData->numFrames = static_cast<std::uint16_t>(
      std::min(numFrames, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
  animData->frameRate = static_cast<std::uint16_t>(
      std::min(frameRate, static_cast<int>(std::numeric_limits<std::uint16_t>::max())));
  animData->flags = static_cast<std::uint16_t>(settings.flags | 0x40u);
  return !source->HadError();
}

void idMD6Anim::LoadResource() {
  if (postedForGeneration || name.str == nullptr || *name.str == '\0')
    return;
  std::filesystem::path source(name.str);
  std::filesystem::path binary = source;
  binary.replace_extension(".bmd6anim");
  if (LoadBinary(binary.string().c_str(), false))
    return;
  std::string mutableSource = source.string();
  if (!LoadText(mutableSource.data(), nullptr))
    MakeDefault(nullptr);
}

idMD6AnimGenerator animGenerator;

idMD6AnimGenerator::idMD6AnimGenerator() = default;

idMD6AnimGenerator::~idMD6AnimGenerator() {
  Finishup();
}

void idMD6AnimGenerator::Init(int numThreads) {
  std::lock_guard<std::mutex> lock(mutex);
  if (valid || numThreads <= 0)
    return;
  buffer.assign(1024, nullptr);
  head = tail = count = 0;
  valid = true;
  shuttingDown = false;
  threads.resize(static_cast<std::size_t>(numThreads));
  for (threadEntry_t& entry : threads)
    entry.thread = std::thread(&idMD6AnimGenerator::WorkerThread, this);
}

bool idMD6AnimGenerator::AddResource(idMD6Anim* animation) {
  std::lock_guard<std::mutex> lock(mutex);
  if (!valid || animation == nullptr)
    return false;
  if (animation->postedForGeneration)
    return true;
  if (count == buffer.size())
    return false;
  buffer[head] = animation;
  head = (head + 1) % buffer.size();
  ++count;
  animation->postedForGeneration = true;
  animAdded.notify_one();
  return true;
}

void idMD6AnimGenerator::WorkerThread(idMD6AnimGenerator* generator) {
  for (;;) {
    idMD6Anim* animation = nullptr;
    {
      std::unique_lock<std::mutex> lock(generator->mutex);
      generator->animAdded.wait(lock, [&] {
        return generator->shuttingDown || generator->count != 0;
      });
      if (generator->shuttingDown && generator->count == 0)
        return;
      animation = generator->buffer[generator->tail];
      generator->buffer[generator->tail] = nullptr;
      generator->tail = (generator->tail + 1) % generator->buffer.size();
      --generator->count;
    }
    if (animation != nullptr) {
      animation->postedForGeneration = false;
      animation->LoadResource();
    }
    generator->animReady.notify_all();
  }
}

void idMD6AnimGenerator::Finishup() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!valid && threads.empty())
      return;
    shuttingDown = true;
  }
  animAdded.notify_all();
  for (threadEntry_t& entry : threads)
    if (entry.thread.joinable())
      entry.thread.join();
  std::lock_guard<std::mutex> lock(mutex);
  threads.clear();
  buffer.clear();
  blendShapeExpressions.clear();
  valid = false;
  shuttingDown = false;
  head = tail = count = 0;
}

idMD6AnimGenerator::blendShapeExpressionEntry_t*
idMD6AnimGenerator::GetBlendShapeExpressionEntry(const char* modelName) {
  std::lock_guard<std::mutex> lock(mutex);
  const std::string requested = modelName != nullptr ? modelName : "";
  for (blendShapeExpressionEntry_t& entry : blendShapeExpressions)
    if (entry.modelName == requested)
      return &entry;
  blendShapeExpressions.push_back({requested, {}});
  return &blendShapeExpressions.back();
}

idMD6AnimResourceList::~idMD6AnimResourceList() {
  FreeAll();
}

idMD6Anim* idMD6AnimResourceList::FindExisting(const char* name) const {
  std::lock_guard<std::mutex> lock(mutex);
  const char* requested = name != nullptr ? name : "";
  for (const auto& resource : resources)
    if (idStr::Icmp(resource->name.str, requested) == 0)
      return resource.get();
  return nullptr;
}

idMD6Anim* idMD6AnimResourceList::Load(const char* name, bool makeDefault,
                                       bool userChannelsOnly) {
  (void)userChannelsOnly;
  if (name == nullptr || *name == '\0')
    return nullptr;
  if (idMD6Anim* existing = FindExisting(name))
    return existing;
  auto resource = std::make_unique<idMD6Anim>();
  resource->name.Set(name);
  resource->LoadResource();
  if (resource->animData == nullptr && makeDefault)
    resource->MakeDefault(nullptr);
  if (resource->animData == nullptr)
    return nullptr;
  idMD6Anim* result = resource.get();
  std::lock_guard<std::mutex> lock(mutex);
  resources.push_back(std::move(resource));
  return result;
}

void idMD6AnimResourceList::FreeAll() {
  std::lock_guard<std::mutex> lock(mutex);
  for (const auto& resource : resources)
    resource->FreeData();
  resources.clear();
}

int idMD6AnimResourceList::Num() const {
  std::lock_guard<std::mutex> lock(mutex);
  return static_cast<int>(resources.size());
}

namespace idMD6AnimRewriteHelpers {

idMD6Model* LoadModel(const char* meshPath, int numJoints,
                      int numPaddedUserChannels,
                      idCrcHandle_t parentTableCrc) {
  (void)numPaddedUserChannels;
  (void)parentTableCrc;
  idMD6Model* model =
      g_modelLoader != nullptr ? g_modelLoader(meshPath) : nullptr;
  if (model == nullptr || model->skeleton == nullptr ||
      model->skeleton->data == nullptr ||
      model->skeleton->data->numJoints < numJoints)
    return nullptr;
  return model;
}

bool ValidateParenting(const char* filename, const idMD6Model* model,
                       int numJoints,
                       const idList<idStr, 5>& animationJointTable,
                       const idList<std::int16_t, 5>& animationParentTable,
                       idCrcHandle_t parentTableCrc) {
  (void)filename;
  if (model == nullptr || model->skeleton == nullptr ||
      model->skeleton->data == nullptr || numJoints < 0 ||
      animationJointTable.num < numJoints ||
      animationParentTable.num < numJoints)
    return false;
  const idMD6Skel* skeleton = model->skeleton;
  if (skeleton->data->parentTblCrc.value == parentTableCrc.value)
    return true;
  const auto* parentTable = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(skeleton->data) +
      skeleton->data->parentTblOffset);
  for (int joint = 1; joint < numJoints; ++joint) {
    const int animationParent = animationParentTable[joint];
    if (animationParent < 0 || animationParent >= numJoints)
      return false;
    const idJointIndex_t modelJoint = SkeletonJointIndex(
        skeleton, animationJointTable[joint].data);
    const idJointIndex_t modelParent = SkeletonJointIndex(
        skeleton, animationJointTable[animationParent].data);
    if (modelJoint.value < 0 || modelParent.value < 0 ||
        parentTable[modelJoint.value] != modelParent.value)
      return false;
  }
  return true;
}

} // namespace idMD6AnimRewriteHelpers
