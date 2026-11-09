#include "engine/models/skeletalanimation/md6animevent.h"

#include "engine/decls/decl.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/typeinfo/typeinfo.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

extern idMD6AnimEventLookupCallback md6AnimEventLookupCallback;
extern idMD6AnimAliasCallback md6AnimAliasCallback;

namespace {

constexpr const char* MD6_JOINT_CACHE_LOCATION =
    "w:\\build\\tech5\\engine\\models\\skeletalanimation\\"
    "MD6AnimEvent.cpp(138) : TAG_MD6_JOINTCACHE";
constexpr const char* VAR_ARGS_LOCATION =
    "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag";
constexpr memTag_t TAG_MD6_JOINTCACHE = static_cast<memTag_t>(0x16);
constexpr memTag_t TAG_AI_FSM = static_cast<memTag_t>(0x5C);
constexpr int ENUM_FLAG_BITFIELD = 0x800000;

std::uint32_t currentAnimEventId = 0;

idAnimEventId NextAnimEventId() {
  if (currentAnimEventId == std::numeric_limits<std::uint32_t>::max())
    currentAnimEventId = 1;
  else
    ++currentAnimEventId;
  return idAnimEventId{currentAnimEventId};
}

const char* DeclName(const idDeclMD6* md6) {
  return md6 != nullptr && md6->name.str != nullptr ? md6->name.str
                                                     : "< NULL >";
}

const char* FormatTypeName(char format) {
  switch (format) {
  case 'b': return "bool";
  case 'i': return "int";
  case 'f': return "float";
  case 'v': return "vec3";
  case 'a': return "angles";
  case 's': return "string";
  case 'd': return "decl";
  default: return "";
  }
}

const char* ArgumentTypeName(const idAnimEventCommandInfo& event,
                             int argumentIndex) {
  if (event.argumentTypeNames != nullptr && argumentIndex >= 0 &&
      argumentIndex < event.numArgs &&
      event.argumentTypeNames[argumentIndex] != nullptr)
    return event.argumentTypeNames[argumentIndex];
  const char format = event.formatSpec != nullptr && argumentIndex >= 0
                          ? event.formatSpec[argumentIndex]
                          : '\0';
  return FormatTypeName(format);
}

void ExpectFormat(idParser& source, int argumentIndex, char actual,
                  char expected, const char* expectedName,
                  const char* actualName) {
  if (actual != expected) {
    source.Error(
        "Invalid argument for event arg %d: found %s, expected %s.",
        argumentIndex, actualName, expectedName != nullptr ? expectedName : "");
  }
}

int EnumValueForToken(const idEnumInfo& enumInfo, const idToken& token,
                      const idAnimEventCommandInfo& event,
                      const idDeclMD6* md6) {
  if (token.type == TT_NUMBER)
    return static_cast<int>(token.intvalue);
  const idEnumValueInfo* value =
      declTypeInfoTools != nullptr
          ? declTypeInfoTools->FindEnumValueInfo(enumInfo, token.data, true)
          : nullptr;
  if (value != nullptr)
    return value->value;
  idLib::Warning(
      "Error unknown event enum value: '%s::%s' while parsing md6 anim "
      "event %s block in %s.",
      enumInfo.name.data, token.data, event.name != nullptr ? event.name : "",
      DeclName(md6));
  return 0;
}

} // namespace

template <int MaxArgs> idVarArgs<MaxArgs>::~idVarArgs() {
  if (args != nullptr) {
    idMem::Singleton()->Free(args, ALIGN_16);
    args = nullptr;
  }
  argSize = 0;
  buffSize = 0;
  numArgs = 0;
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::Grow(const std::size_t newArgSize) {
  if (newArgSize == 0)
    return;
  std::size_t required = static_cast<std::size_t>(argSize) + newArgSize;
  if (required <= buffSize)
    return;
  required = std::max(required, static_cast<std::size_t>(buffSize) * 2);
  auto* replacement = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          VAR_ARGS_LOCATION, required, TAG_AI_FSM, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (args != nullptr) {
    std::memmove(replacement, args, argSize);
    idMem::Singleton()->Free(args, ALIGN_16);
  }
  args = replacement;
  buffSize = static_cast<std::uint16_t>(required);
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::Copy(const idVarArgs& other) {
  if (this == &other)
    return;
  if (args != nullptr) {
    idMem::Singleton()->Free(args, ALIGN_16);
    args = nullptr;
  }
  argSize = 0;
  buffSize = 0;
  numArgs = 0;
  numArgs = other.numArgs;
  argSize = other.argSize;
  buffSize = other.buffSize;
  std::memcpy(argOffsets, other.argOffsets, sizeof(argOffsets));
  std::memcpy(argTypes, other.argTypes, sizeof(argTypes));
  std::memcpy(argExTypes, other.argExTypes, sizeof(argExTypes));
  if (buffSize != 0) {
    args = static_cast<std::uint8_t*>(idMem::Singleton()->AllocWithLocation(
        VAR_ARGS_LOCATION, buffSize, TAG_AI_FSM, false, ALIGN_16,
        HEAP_DEFAULTHEAP));
    if (argSize != 0)
      std::memmove(args, other.args, argSize);
  }
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::AddArg(const void* value, std::uint8_t type,
                                std::uint8_t exType) {
  argTypes[numArgs] = type;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  const int index = numArgs;
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  std::memcpy(args + argOffsets[index], &value, sizeof(value));
  ++numArgs;
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::AddArg(const char* value, std::uint8_t exType) {
  if (value == nullptr)
    value = "";
  argTypes[numArgs] = ARGTYPE_STRING;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  const std::size_t length = std::strlen(value) + 1;
  Grow(length);
  const int index = numArgs;
  argSize = static_cast<std::uint16_t>(argSize + length);
  std::memmove(args + argOffsets[index], value, length);
  ++numArgs;
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::AddArg(float value, std::uint8_t exType) {
  argTypes[numArgs] = ARGTYPE_FLOAT;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  const int index = numArgs;
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  std::memcpy(args + argOffsets[index], &value, sizeof(value));
  ++numArgs;
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::AddArg(int value, std::uint8_t exType) {
  argTypes[numArgs] = ARGTYPE_INT;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  const int index = numArgs;
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  std::memcpy(args + argOffsets[index], &value, sizeof(value));
  ++numArgs;
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::AddArg(bool value, std::uint8_t exType) {
  argTypes[numArgs] = ARGTYPE_BOOL;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  const int index = numArgs;
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  args[argOffsets[index]] = value ? 1 : 0;
  ++numArgs;
}

template <int MaxArgs>
void idVarArgs<MaxArgs>::AddHandleArg(int value, std::uint8_t type,
                                      std::uint8_t exType) {
  argTypes[numArgs] = type;
  argExTypes[numArgs] = exType;
  argOffsets[numArgs] = argSize;
  Grow(sizeof(value));
  const int index = numArgs;
  argSize = static_cast<std::uint16_t>(argSize + sizeof(value));
  std::memcpy(args + argOffsets[index], &value, sizeof(value));
  ++numArgs;
}

template idVarArgs<5>::~idVarArgs();
template void idVarArgs<5>::Grow(std::size_t);
template void idVarArgs<5>::Copy(const idVarArgs<5>&);
template void idVarArgs<5>::AddArg(const void*, std::uint8_t, std::uint8_t);
template void idVarArgs<5>::AddArg(const char*, std::uint8_t);
template void idVarArgs<5>::AddArg(float, std::uint8_t);
template void idVarArgs<5>::AddArg(int, std::uint8_t);
template void idVarArgs<5>::AddArg(bool, std::uint8_t);
template void idVarArgs<5>::AddHandleArg(int, std::uint8_t, std::uint8_t);

const char* idMD6AnimEvent::animEventArgTypeNames[4] = {
    "", "jointIndex_t", "jointTag", "enum"};

idMD6AnimEvent::idMD6AnimEvent()
    : args(0), cachedJoint(nullptr), eventNum(-1), frameNum(0),
      eventId(NextAnimEventId()), locked(false), row(0), pad{} {}

idMD6AnimEvent::~idMD6AnimEvent() noexcept(false) {
  if (cachedJoint != nullptr) {
    idMem::Singleton()->Free(cachedJoint, ALIGN_16);
    cachedJoint = nullptr;
  }
}

void idMD6AnimEvent::SetCachedJoint(const idJointIndex_t& jointIndex,
                                    int frame, const idVec3& translation,
                                    const idQuat& rotation) {
  if (cachedJoint == nullptr) {
    cachedJoint = static_cast<idCachedJoint*>(
        idMem::Singleton()->AllocWithLocation(
            MD6_JOINT_CACHE_LOCATION, sizeof(idCachedJoint),
            TAG_MD6_JOINTCACHE, false, ALIGN_16, HEAP_DEFAULTHEAP));
  }
  if (cachedJoint != nullptr) {
    cachedJoint->jointIndex = jointIndex;
    cachedJoint->frameNum = static_cast<std::int16_t>(frame);
    cachedJoint->trans = translation;
    cachedJoint->rot = rotation;
  }
}

void idMD6AnimEvent::Copy(const idMD6AnimEvent& other) {
  if (this == &other)
    return;
  if (other.cachedJoint != nullptr) {
    if (cachedJoint == nullptr) {
      cachedJoint = static_cast<idCachedJoint*>(
          idMem::Singleton()->AllocWithLocation(
              MD6_JOINT_CACHE_LOCATION, sizeof(idCachedJoint),
              TAG_MD6_JOINTCACHE, false, ALIGN_16, HEAP_DEFAULTHEAP));
    }
    if (cachedJoint != nullptr)
      *cachedJoint = *other.cachedJoint;
  } else if (cachedJoint != nullptr) {
    idMem::Singleton()->Free(cachedJoint, ALIGN_16);
    cachedJoint = nullptr;
  }
  args.Copy(other.args);
  eventNum = other.eventNum;
  frameNum = other.frameNum;
  eventId = other.eventId;
  locked = other.locked;
  row = other.row;
}

bool idMD6AnimEvent::Parse(const idDeclMD6* md6, idParser& source,
                           int& loadErrors) {
  idToken token;
  if (!source.ReadToken(token))
    return false;
  idStr name(token);
  const idAnimEventCommandInfo* event =
      md6AnimEventLookupCallback != nullptr
          ? md6AnimEventLookupCallback(token.data)
          : nullptr;
  eventNum = static_cast<std::int16_t>(event != nullptr ? event->eventNum : -1);
  if (eventNum < 0 || event == nullptr) {
    ++loadErrors;
    idLib::Warning("Unmatched frame command name '%s' in md6Def '%s'",
                   token.data, DeclName(md6));
    source.SkipBracedSection(true);
    return false;
  }

  int argumentIndex = 1;
  const int line = source.GetCurrentSourceLine();
  source.ExpectTokenType(TT_PUNCTUATION, 46, token);
  try {
    while (source.ReadToken(token) &&
           (token.type != TT_PUNCTUATION || token.subtype != 47)) {
      if (token.Icmp("frame") == 0) {
        frameNum = static_cast<std::int16_t>(source.ParseInt());
        continue;
      }
      if (token.Icmp("row") == 0) {
        row = static_cast<std::uint8_t>(source.ParseInt());
        continue;
      }
      if (token.Icmp("locked") == 0) {
        locked = source.ParseInt() != 0;
        continue;
      }

      const char expectedFormat =
          event->formatSpec != nullptr ? event->formatSpec[argumentIndex] : '\0';
      idStr argTypeName(ArgumentTypeName(*event, argumentIndex));

      if (token.Icmp("bool") == 0) {
        ExpectFormat(source, argumentIndex, expectedFormat, 'b',
                     argTypeName.data, "int");
        args.AddArg(source.ParseBool(), 0);
      } else if (token.Icmp("int") == 0) {
        ExpectFormat(source, argumentIndex, expectedFormat, 'i',
                     argTypeName.data, "int");
        int value = 0;
        std::uint8_t exType = MD6_EVENTARG_NONE;
        const idEnumInfo* enumInfo =
            idStr::Icmp(argTypeName.data, "int") != 0 &&
                    declTypeInfoTools != nullptr
                ? declTypeInfoTools->FindEnumInfo(argTypeName.data)
                : nullptr;
        if (enumInfo == nullptr) {
          value = source.ParseInt();
        } else {
          exType = MD6_EVENTARG_ENUM;
          idToken enumValue;
          if (source.ReadToken(enumValue)) {
            value |= EnumValueForToken(*enumInfo, enumValue, *event, md6);
            while ((enumInfo->flags & ENUM_FLAG_BITFIELD) != 0 &&
                   source.ReadTokenOnLine(enumValue)) {
              value |= EnumValueForToken(*enumInfo, enumValue, *event, md6);
            }
          }
        }
        args.AddArg(value, exType);
      } else if (token.Icmp("float") == 0) {
        ExpectFormat(source, argumentIndex, expectedFormat, 'f',
                     argTypeName.data, "float");
        args.AddArg(source.ParseFloat(), 0);
      } else if (token.Icmp("vec3") == 0) {
        ExpectFormat(source, argumentIndex, expectedFormat, 'v',
                     argTypeName.data, "vector");
        idVec3 value;
        source.Parse1DMatrix(3, &value.x);
        args.argTypes[args.numArgs] = ARGTYPE_VEC3;
        args.argExTypes[args.numArgs] = 0;
        args.argOffsets[args.numArgs] = args.argSize;
        args.Grow(sizeof(value));
        args.argSize = static_cast<std::uint16_t>(args.argSize + sizeof(value));
        std::memcpy(args.args + args.argOffsets[args.numArgs], &value,
                    sizeof(value));
        ++args.numArgs;
      } else if (token.Icmp("angles") == 0) {
        ExpectFormat(source, argumentIndex, expectedFormat, 'a',
                     argTypeName.data, "angles");
        idAngles value;
        source.Parse1DMatrix(3, &value.pitch);
        args.argTypes[args.numArgs] = ARGTYPE_ANGLES;
        args.argExTypes[args.numArgs] = 0;
        args.argOffsets[args.numArgs] = args.argSize;
        args.Grow(sizeof(value));
        args.argSize = static_cast<std::uint16_t>(args.argSize + sizeof(value));
        std::memcpy(args.args + args.argOffsets[args.numArgs], &value,
                    sizeof(value));
        ++args.numArgs;
      } else if (token.Icmp("string") == 0) {
        ExpectFormat(source, argumentIndex, expectedFormat, 's',
                     argTypeName.data, "string");
        source.ReadToken(token);
        args.AddArg(token.data, 0);
      } else if (token.Icmp("animalias") == 0) {
        source.ReadToken(token);
        const std::uint16_t alias =
            md6AnimAliasCallback != nullptr
                ? md6AnimAliasCallback(token.data)
                : std::numeric_limits<std::uint16_t>::max();
        args.AddHandleArg(alias, ARGTYPE_HANDLE, 0);
      } else if (token.Icmp("joint") == 0 ||
                 token.Icmp("jointName") == 0) {
        if (expectedFormat != 's' && expectedFormat != '2' &&
            expectedFormat != '5' && expectedFormat != '1') {
          source.Error(
              "Invalid argument for event arg %d: found jointName, expected "
              "%s.",
              argumentIndex, argTypeName.data);
        }
        source.ReadToken(token);
        idJointIndex_t jointIndex;
        if (md6 != nullptr && md6->model != nullptr &&
            md6->model->skeleton != nullptr) {
          jointIndex = md6->model->skeleton->GetJointIndex(token.data);
        }
        if (jointIndex.value == -1) {
          idLib::Warning("frame command '%s' references unknown joint '%s'",
                         name.data, token.data);
        }
        args.AddArg(static_cast<int>(jointIndex.value),
                    MD6_EVENTARG_JOINT_INDEX);
      } else {
        idDeclInfo* declType =
            declManager != nullptr ? declManager->GetDeclType(token.data)
                                   : nullptr;
        if (declType != nullptr) {
          ExpectFormat(source, argumentIndex, expectedFormat, 'd',
                       argTypeName.data, "decl");
          source.ReadToken(token);
          const idDecl* decl = nullptr;
          if (token != "NULL" && token.data != nullptr)
            decl = declType->FindWithInheritance(token.data, false, true);
          args.AddArg(decl, ARGTYPE_POINTER, 0);
          if (decl == nullptr && token != "NULL") {
            idLib::Warning(
                "frame command '%s' references unknown decl '%s', type '%s'",
                name.data, token.data,
                declType->name != nullptr ? declType->name : "");
          }
        } else {
          const idEnumInfo* enumInfo =
              declTypeInfoTools != nullptr
                  ? declTypeInfoTools->FindEnumInfo(token.data)
                  : nullptr;
          if (enumInfo != nullptr) {
            ExpectFormat(source, argumentIndex, expectedFormat, 'i',
                         argTypeName.data, "enum");
            idToken enumValue;
            int value = 0;
            if (source.ReadToken(enumValue)) {
              do {
                value |= EnumValueForToken(*enumInfo, enumValue, *event, md6);
              } while ((enumInfo->flags & ENUM_FLAG_BITFIELD) != 0 &&
                       source.ReadTokenOnLine(enumValue));
            }
            args.AddArg(value, MD6_EVENTARG_ENUM);
          } else {
            idStr tempStr;
            source.ParseRestOfLine(tempStr);
            idLib::Warning(
                "Error unknown event arg: '%s' while parsing md6 anim event "
                "%s block in %s.",
                token.data, event->name != nullptr ? event->name : "",
                DeclName(md6));
          }
        }
      }
      ++argumentIndex;
    }

    if (event->numArgs - 1 != args.numArgs) {
      idLib::Warning("event '%s', %s(line %d), has %d args, expecting %d",
                     event->name != nullptr ? event->name : "", "< NULL >",
                     line, args.numArgs, event->numArgs - 1);
    }
  } catch (...) {
    ++loadErrors;
    idLib::Warning("Error while parsing md6 anim event %s",
                   event->name != nullptr ? event->name : "");
    source.SkipBracedSection(false);
  }
  return true;
}
