#include "engine/models/skeletalanimation/md6phasetrack.h"

#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/lexer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

extern idMD6PhaseTrackSkelLookup md6PhaseTrackSkeletonLookup;

namespace {

constexpr memTag_t kPhaseTrackTag = static_cast<memTag_t>(29);
constexpr char kPhaseTrackAllocation[] =
    "w:\\build\\tech5\\engine\\models\\skeletalanimation\\MD6PhaseTrack.cpp(395) : TAG_MD6_PHASE_TRACK";
constexpr char kBinaryPhaseTrackAllocation[] =
    "w:\\build\\tech5\\engine\\models\\skeletalanimation\\MD6PhaseTrack.cpp(124) : TAG_MD6_PHASE_TRACK";
constexpr char kBinaryTemporaryAllocation[] =
    "w:\\build\\tech5\\engine\\models\\skeletalanimation\\MD6PhaseTrack.cpp(153) : TAG_TEMP";
constexpr int kPhaseTrackVersion = 1;

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8U) | (value >> 8U));
}

std::uint16_t Align16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value + 15U) & 0xFFF0U);
}

[[noreturn]] void ParseFailure(const char* message) {
  throw std::runtime_error(message != nullptr ? message : "parse failure");
}

void Require(bool result, const char* message) {
  if (!result)
    ParseFailure(message);
}

} // namespace

idMD6PhaseTrack::idMD6PhaseTrack() : data(nullptr) {}

idMD6PhaseTrack::~idMD6PhaseTrack() { Free(); }

phaseTrackData_t* idMD6PhaseTrack::CreatePhaseTrackData(
    const idMD6Skel*, const idMD6Anim* anim,
    const idList<std::uint8_t, 5>& phaseTable,
    const idList<float, 5>& distanceTable) const {
  phaseTrackData_t header{};
  header.numFrames = anim != nullptr && anim->animData != nullptr
                         ? anim->animData->numFrames
                         : 0;
  header.ofsPhaseTable = sizeof(phaseTrackData_t);
  header.totalSize = static_cast<std::uint16_t>(
      sizeof(phaseTrackData_t) +
      Align16(static_cast<std::uint16_t>(phaseTable.num)));
  if (distanceTable.num != 0) {
    header.ofsDistanceRemainingTable = header.totalSize;
    header.totalSize = static_cast<std::uint16_t>(
        header.totalSize +
        Align16(static_cast<std::uint16_t>(distanceTable.num * sizeof(float))));
  }

  auto* result = static_cast<phaseTrackData_t*>(
      idMem::Singleton()->AllocWithLocation(
          kPhaseTrackAllocation, header.totalSize, kPhaseTrackTag, false,
          ALIGN_16, HEAP_DEFAULTHEAP));
  std::memset(result, 0, header.totalSize);
  *result = header;
  if (phaseTable.num > 0) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(result) +
                    result->ofsPhaseTable,
                phaseTable.list, static_cast<std::size_t>(phaseTable.num));
  }
  if (distanceTable.num > 0) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(result) +
                    result->ofsDistanceRemainingTable,
                distanceTable.list,
                static_cast<std::size_t>(distanceTable.num) * sizeof(float));
  }
  return result;
}

bool idMD6PhaseTrack::GetPhase(int frame, std::uint8_t& phase) const {
  if (data != nullptr && frame >= 0 && frame < data->numFrames) {
    phase = data->GetPhaseTable()[frame];
    return true;
  }
  phase = 0;
  return false;
}

bool idMD6PhaseTrack::GetFrame(std::uint8_t phase,
                               std::int16_t& outFrame) const {
  if (data == nullptr) {
    outFrame = 0;
    return false;
  }

  idArray<std::uint16_t, 256> phaseToFrameLookupData;
  std::memset(&phaseToFrameLookupData, 0xFF,
              sizeof(phaseToFrameLookupData));
  const std::uint16_t numFrames = data->numFrames;
  const std::uint8_t* phases = data->GetPhaseTable();
  for (std::uint16_t frame = 0; frame < numFrames; ++frame) {
    const std::uint8_t framePhase = phases[frame];
    if (phaseToFrameLookupData.ptr[framePhase] == 0xFFFFU)
      phaseToFrameLookupData.ptr[framePhase] = frame;
  }

  std::uint16_t previousPhase = 0xFFFFU;
  for (std::uint16_t currentPhase = 0; currentPhase < 256;
       ++currentPhase) {
    const std::uint16_t currentFrame =
        phaseToFrameLookupData.ptr[currentPhase];
    if (currentFrame == 0xFFFFU)
      continue;
    if (previousPhase != 0xFFFFU) {
      const int phaseDifference = currentPhase - previousPhase;
      if (phaseDifference > 1) {
        const int previousFrame =
            phaseToFrameLookupData.ptr[previousPhase];
        const int wrappedFrame = numFrames + currentFrame;
        const int interpolationEnd =
            std::abs(static_cast<int>(currentFrame) - previousFrame) <=
                    std::abs(wrappedFrame - previousFrame)
                ? currentFrame
                : wrappedFrame;
        const int frameDifference = interpolationEnd - previousFrame;
        for (int offset = 1; offset < phaseDifference; ++offset) {
          int frame =
              previousFrame + frameDifference * offset / phaseDifference;
          while (frame >= numFrames)
            frame -= numFrames;
          while (frame < 0)
            frame += numFrames;
          phaseToFrameLookupData.ptr[previousPhase + offset] =
              static_cast<std::uint16_t>(frame);
        }
      }
    }
    previousPhase = currentPhase;
  }
  outFrame = static_cast<std::int16_t>(phaseToFrameLookupData.ptr[phase]);
  return true;
}

bool idMD6PhaseTrack::IsValid(const idMD6Anim* anim) const {
  if (data == nullptr || data->GetPhaseTable() == nullptr)
    return false;
  const std::uint16_t animationFrames =
      anim != nullptr && anim->animData != nullptr ? anim->animData->numFrames
                                                   : 0;
  return data->numFrames == animationFrames;
}

bool idMD6PhaseTrack::LoadBinary(idFile* file) {
  Free();
  std::uint16_t totalSize = 0;
  file->Read(&totalSize, sizeof(totalSize));
  totalSize = ByteSwap16(totalSize);
  if (totalSize == 0)
    return true;
  data = static_cast<phaseTrackData_t*>(
      idMem::Singleton()->AllocWithLocation(
          kBinaryPhaseTrackAllocation, totalSize, kPhaseTrackTag, false,
          ALIGN_16, HEAP_DEFAULTHEAP));
  if (data == nullptr)
    return false;
  data->totalSize = totalSize;
  file->Read(&data->numFrames, totalSize - sizeof(totalSize));
  data->Swap();
  data->totalSize = totalSize;
  return true;
}

bool idMD6PhaseTrack::WriteBinary(idFile* file) {
  if (data == nullptr) {
    const std::uint16_t empty = 0;
    file->Write(&empty, sizeof(empty));
    return true;
  }
  auto* copy = static_cast<phaseTrackData_t*>(
      idMem::Singleton()->AllocWithLocation(
          kBinaryTemporaryAllocation, data->totalSize, TAG_TEMP, false,
          ALIGN_16, HEAP_DEFAULTHEAP));
  std::memcpy(copy, data, data->totalSize);
  copy->Swap();
  file->Write(copy, data->totalSize);
  idMem::Singleton()->Free(copy, ALIGN_16);
  return true;
}

bool idMD6PhaseTrack::LoadText(const char* fileName, const idMD6Anim* anim) {
  Free();
  idLexer source(LEXFL_THROWFATAL | LEXFL_ALLOWPATHNAMES |
                 LEXFL_NOSTRINGESCAPECHARS | LEXFL_NOSTRINGCONCAT);
  if (!source.LoadFile(fileName, false)) {
    idLib::Warning("Failed to open file '%s' for reading", fileName);
    return false;
  }

  idList<std::uint8_t, 5> phaseTable;
  idList<float, 5> distanceTable;
  idStr dependentAnimName;
  idStr dependentSkelName;
  idToken token;
  try {
    Require(source.ExpectTokenString("phaseTrack"), "phaseTrack expected");
    Require(source.ExpectTokenString("version"), "version expected");
    Require(source.ExpectTokenType(TT_NUMBER, TT_INTEGER, token),
            "version number expected");
    if (static_cast<int>(token.intvalue) != kPhaseTrackVersion)
      ParseFailure("wrong phase-track version");
    Require(source.ExpectTokenString("init"), "init expected");
    Require(source.ExpectTokenString("{"), "init block expected");

    int numFrames = anim != nullptr && anim->animData != nullptr
                        ? anim->animData->numFrames
                        : 0;
    while (source.ReadToken(token) && token != "}") {
      if (idStr::Icmp(token.data, "dependentAnimName") == 0) {
        Require(source.ExpectTokenType(TT_STRING, 0, token),
                "dependent animation name expected");
        dependentAnimName = token;
      } else if (idStr::Icmp(token.data, "dependentSkelName") == 0) {
        Require(source.ExpectTokenType(TT_STRING, 0, token),
                "dependent skeleton name expected");
        dependentSkelName = token;
      } else if (idStr::Icmp(token.data, "dependentModelName") == 0 ||
                 idStr::Icmp(token.data, "leftAnkleJoint") == 0 ||
                 idStr::Icmp(token.data, "rightAnkleJoint") == 0 ||
                 idStr::Icmp(token.data, "originJoint") == 0) {
        Require(source.ExpectTokenType(TT_STRING, 0, token),
                "quoted name expected");
      } else if (idStr::Icmp(token.data, "dependentAnimTimestamp") == 0) {
        source.ParseUnsignedInt();
      } else if (idStr::Icmp(token.data, "numFrames") == 0) {
        const unsigned parsedFrames = source.ParseInt();
        if (parsedFrames >= 0x10000U)
          ParseFailure("numFrames is out of range");
        numFrames = static_cast<int>(parsedFrames);
      } else {
        ParseFailure("unknown init token");
      }
    }
    if (numFrames == 0 || dependentAnimName.IsEmpty() ||
        dependentSkelName.IsEmpty()) {
      ParseFailure("incomplete init block");
    }
    const int animationFrames =
        anim != nullptr && anim->animData != nullptr ? anim->animData->numFrames
                                                     : 0;
    if (numFrames != animationFrames)
      ParseFailure("phase-track frame count mismatch");
    if (anim == nullptr || idStr::Icmp(dependentAnimName.data,
                                      anim->name.str) != 0) {
      ParseFailure("dependent animation mismatch");
    }

    idToken tableToken;
    idToken phaseValueToken;
    idToken distanceValueToken;
    Require(source.ExpectTokenString("phase"), "phase table expected");
    Require(source.ExpectTokenString("{"), "phase block expected");
    phaseTable.SetNum(numFrames);
    for (int frame = 0; frame < numFrames; ++frame)
      phaseTable[frame] = static_cast<std::uint8_t>(source.ParseInt());
    Require(source.ExpectTokenString("}"), "phase block end expected");
    if (source.CheckTokenString("distanceRemaining")) {
      Require(source.ExpectTokenString("{"), "distance block expected");
      distanceTable.SetNum(numFrames);
      for (int frame = 0; frame < numFrames; ++frame)
        distanceTable[frame] = source.ParseFloat();
      Require(source.ExpectTokenString("}"), "distance block end expected");
    }
    (void)tableToken;
    (void)phaseValueToken;
    (void)distanceValueToken;
  } catch (const std::exception& exception) {
    idLib::Warning("Phase track file '%s': %s", fileName, exception.what());
    return false;
  }

  const idMD6Skel* skeleton =
      md6PhaseTrackSkeletonLookup != nullptr
          ? md6PhaseTrackSkeletonLookup(dependentSkelName.data, false)
          : nullptr;
  if (skeleton == nullptr) {
    idLib::Warning("Phase track species invalid skeleton '%s'",
                   dependentSkelName.data);
    return false;
  }
  data = CreatePhaseTrackData(skeleton, anim, phaseTable, distanceTable);
  return data != nullptr;
}
