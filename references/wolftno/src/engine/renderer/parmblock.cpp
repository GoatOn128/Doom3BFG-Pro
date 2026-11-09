#include "engine/renderer/parmblock.h"

#include "engine/decls/decltable.h"
#include "engine/framework/coderesource.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/texturesampler.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/lib.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr short INVALID_PARM_INDEX = 2175;
int c_redundantOperationsRemoved = 0;
const char* const suffix[] = {"x", "y", "z", "w"};

idCodeResource<idDeclRenderParm> rp_transSortMap("transSortMap");
idCodeResource<idDeclRenderParm> rp_transAtlasMap("transAtlasMap");
idCodeResource<idDeclRenderParm> rp_landDefinitionFile("landDefinitionFile");

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00FF0000u) |
         ((value >> 8) & 0x0000FF00u) | (value >> 24);
}

}  // namespace

template <>
std::size_t idFile::WriteBigArray<int>(const int* values, int count) {
  std::size_t bytes = 0;
  for (int index = 0; index < count; ++index) {
    const std::uint32_t value =
        ByteSwap32(static_cast<std::uint32_t>(values[index]));
    bytes += Write(&value, sizeof(value));
  }
  return bytes;
}

template <>
std::size_t idFile::ReadBigArray<int>(int* values, int count) {
  const std::size_t bytes =
      Read(values, sizeof(*values) * static_cast<std::size_t>(count));
  for (int index = 0; index < count; ++index) {
    values[index] = static_cast<int>(
        ByteSwap32(static_cast<std::uint32_t>(values[index])));
  }
  return bytes;
}

const idDeclRenderParm* expOp_t::GetDest() const {
  return parmIndexDest >= INVALID_PARM_INDEX
             ? nullptr
             : renderThreadParmState->GetRenderParmForIndex(parmIndexDest);
}

const idDeclRenderParm* expOp_t::GetA() const {
  return parmIndexA >= INVALID_PARM_INDEX
             ? nullptr
             : renderThreadParmState->GetRenderParmForIndex(parmIndexA);
}

bool idParmBlock::SetsRenderParm(const idDeclRenderParm* parm) const {
  for (int index = 0; index < ops.num; ++index) {
    if (ops[index].parmIndexDest == parm->parmIndex)
      return true;
  }
  return false;
}

void idParmBlock::WriteConstant(idStr& string, parmType_t parmType,
                                const parmValue_t& parmValue) {
  if (parmType == PT_VECTOR) {
    idStr formatted;
    if (parmValue.value[0] == parmValue.value[1] &&
        parmValue.value[1] == parmValue.value[2] &&
        parmValue.value[2] == parmValue.value[3]) {
      formatted.Format("%f", parmValue.value[0]);
    } else {
      formatted.Format("{ %f, %f, %f, %f }", parmValue.value[0],
                       parmValue.value[1], parmValue.value[2],
                       parmValue.value[3]);
    }
    string += formatted;
    return;
  }

  if (parmType >= PT_TEXTURE_2D && parmType <= PT_IMAGE_2D_WRITE) {
    string += parmValue.image->name.str;
  } else if (parmType == PT_SAMPLER) {
    string += textureSamplerManager->GetSamplerName(parmValue.sampler);
  } else if (parmType == PT_PROGRAM) {
    string += parmValue.program->name.str;
  } else if (parmType == PT_STRING) {
    string += "\"";
    string += parmValue.string;
    string += "\"";
  }
}

void idParmBlock::WriteTerm(idStr& string, const idDeclRenderParm* term) {
  if (term->creator == PC_CONSTANT) {
    WriteConstant(
        string, term->parmType,
        reinterpret_cast<const parmValue_t&>(
            renderThreadParmState->GetParmVec4(term)));
  } else {
    string += term->name.str;
  }
}

idStr* idParmBlock::WriteString() const {
  idStr* string = new idStr("{\n");
  for (int index = 0; index < ops.num; ++index) {
    const expOp_t& operation = ops[index];
    const idDeclRenderParm* destination = operation.GetDest();
    const idDeclRenderParm* parmA = operation.GetA();
    const idDeclRenderParm* parmB =
        operation.parmIndexB >= INVALID_PARM_INDEX
            ? nullptr
            : renderThreadParmState->GetRenderParmForIndex(
                  operation.parmIndexB);

    *string += destination->name.str;
    const int writeMask = (operation.type >> 9) & 0x0F;
    if (writeMask != 0 && writeMask != 0x0F) {
      *string += ".";
      if ((writeMask & 1) != 0)
        *string += "x";
      if ((writeMask & 2) != 0)
        *string += "y";
      if ((writeMask & 4) != 0)
        *string += "z";
      if ((writeMask & 8) != 0)
        *string += "w";
    }
    *string += "\t";

    const int type = operation.type & 0x1F;
    if (type == OP_TYPE_TABLE) {
      const idDeclTable* table =
          renderThreadParmState->GetTable(operation.parmIndexA);
      *string += table != nullptr ? table->name.str : "NULL";
      *string += "[";
      *string += parmB->name.str;
      *string += "]\n";
      continue;
    }
    if (type == OP_TYPE_UNMASKED_MOVE_CONSTANT ||
        type == OP_TYPE_MOVE_CONSTANT) {
      WriteConstant(*string, destination->parmType, constants[index]);
      *string += "\n";
      continue;
    }
    if (type == OP_TYPE_SWIZZLE) {
      WriteTerm(*string, parmA);
      *string += ".";
      for (int component = 0; component < 4; ++component) {
        const int swizzle = constants[index].swizzle[component];
        if (static_cast<unsigned int>(swizzle) > 3u) {
          idLib::Error(
              "idParmBlock::WriteString: bad swizzle index %d for term %s",
              swizzle, parmA->name.str);
        }
        *string += suffix[swizzle];
      }
      *string += "\n";
      continue;
    }
    if (type == OP_TYPE_MOVE || type == OP_TYPE_UNMASKED_MOVE) {
      WriteTerm(*string, parmA);
      *string += "\n";
      continue;
    }
    if (type == OP_TYPE_DOT3 || type == OP_TYPE_DOT4) {
      *string += type == OP_TYPE_DOT3 ? " dot3" : " dot4";
      *string += "( ";
      WriteTerm(*string, parmA);
      *string += ", ";
      WriteTerm(*string, parmB);
      *string += " )\n";
      continue;
    }

    const char* operatorText = "";
    switch (type) {
      case OP_TYPE_ADD: operatorText = " + "; break;
      case OP_TYPE_SUBTRACT: operatorText = " - "; break;
      case OP_TYPE_MULTIPLY: operatorText = " * "; break;
      case OP_TYPE_DIVIDE: operatorText = " / "; break;
      case OP_TYPE_MOD: operatorText = " % "; break;
      case OP_TYPE_GT: operatorText = " > "; break;
      case OP_TYPE_GE: operatorText = " >= "; break;
      case OP_TYPE_LT: operatorText = " < "; break;
      case OP_TYPE_LE: operatorText = " <= "; break;
      case OP_TYPE_EQ: operatorText = " == "; break;
      case OP_TYPE_NE: operatorText = " != "; break;
      case OP_TYPE_AND: operatorText = " && "; break;
      case OP_TYPE_OR: operatorText = " || "; break;
      default: break;
    }
    WriteTerm(*string, parmA);
    *string += operatorText;
    WriteTerm(*string, parmB);
    *string += "\n";
  }
  *string += "}\n";
  return string;
}

void idParmBlock::Print() const {
  idStr* string = WriteString();
  idLib::Printf("%s", string->data);
  delete string;
}

bool idParmBlock::operator==(const idParmBlock& other) const {
  if (ops.num != other.ops.num || constants.num != other.constants.num)
    return false;
  for (int index = 0; index < ops.num; ++index) {
    const expOp_t& left = ops[index];
    const expOp_t& right = other.ops[index];
    if (left.type != right.type ||
        left.parmIndexDest != right.parmIndexDest ||
        left.parmIndexA != right.parmIndexA ||
        left.parmIndexB != right.parmIndexB) {
      return false;
    }
  }
  return constants.num == 0 ||
         std::memcmp(constants.list, other.constants.list,
                     static_cast<std::size_t>(constants.num) *
                         sizeof(parmValue_t)) == 0;
}

void idParmBlock::Clear() {
  ops.ClearFree();
  constants.ClearFree();
}

bool idParmBlock::IsEqualSimple(const idParmBlock& other) const {
  if (ops.num != other.ops.num)
    return false;
  for (int index = 0; index < other.ops.num; ++index) {
    const int type = other.ops[index].type & 0x1F;
    if (type != OP_TYPE_UNMASKED_MOVE_CONSTANT &&
        type != OP_TYPE_MOVE_CONSTANT) {
      return false;
    }
  }
  for (int leftIndex = 0; leftIndex < ops.num; ++leftIndex) {
    const int type = ops[leftIndex].type & 0x1F;
    if (type != OP_TYPE_UNMASKED_MOVE_CONSTANT &&
        type != OP_TYPE_MOVE_CONSTANT) {
      return false;
    }
    int rightIndex = 0;
    while (rightIndex < other.ops.num &&
           other.ops[rightIndex].parmIndexDest !=
               ops[leftIndex].parmIndexDest) {
      ++rightIndex;
    }
    if (rightIndex == other.ops.num ||
        ((other.ops[rightIndex].type ^ ops[leftIndex].type) & 0x1E00) !=
            0 ||
        std::memcmp(&constants[leftIndex], &other.constants[rightIndex],
                    sizeof(parmValue_t)) != 0) {
      return false;
    }
  }
  return true;
}

void idParmBlock::GetRenderParmFromOps_r(
    parmValue_t& parmValue, int numOps, int parmIndex,
    const idParmBlock* parentBlock) const {
  const idParmBlock* block = this;
  const idParmBlock* parent = parentBlock;
  int operationIndex = numOps - 1;
  while (true) {
    while (operationIndex >= 0 &&
           block->ops[operationIndex].parmIndexDest != parmIndex) {
      --operationIndex;
    }
    if (operationIndex >= 0)
      break;
    if (parent == nullptr) {
      const idDeclRenderParm* parm =
          renderThreadParmState->GetRenderParmForIndex(parmIndex);
      idParmState::CopyParmUnaligned(parmValue, parm->declaredValue);
      return;
    }
    block = parent;
    operationIndex = block->ops.num - 1;
    parent = nullptr;
  }

  parmValue_t parmA{};
  parmValue_t parmB{};
  const expOp_t& operation = block->ops[operationIndex];
  if (operation.parmIndexA != INVALID_PARM_INDEX) {
    block->GetRenderParmFromOps_r(parmA, operationIndex,
                                  operation.parmIndexA, parent);
  }
  if (operation.parmIndexB != INVALID_PARM_INDEX) {
    block->GetRenderParmFromOps_r(parmB, operationIndex,
                                  operation.parmIndexB, parent);
  }
  idParmState::ApplyOp(operation, block->constants[operationIndex], parmA,
                       parmB, parmValue);
}

float idParmBlock::GetFloat(const idDeclRenderParm* parm,
                            const idParmBlock* parentBlock) const {
  if (parm->parmType != PT_VECTOR) {
    idLib::Error("idParmBlock::GetFloat: %s is not a vector parm",
                 parm->name.str);
  }
  parmValue_t value;
  GetRenderParmFromOps_r(value, ops.num, parm->parmIndex, parentBlock);
  return value.value[0];
}

int idParmBlock::GetInteger(const idDeclRenderParm* parm,
                            const idParmBlock* parentBlock) const {
  if (parm->parmType != PT_VECTOR) {
    idLib::Error("idParmBlock::GetInteger: %s is not a vector parm",
                 parm->name.str);
  }
  parmValue_t value;
  GetRenderParmFromOps_r(value, ops.num, parm->parmIndex, parentBlock);
  return static_cast<int>(value.value[0]);
}

idVec4 idParmBlock::GetVector(const idDeclRenderParm* parm,
                              const idParmBlock* parentBlock) const {
  if (parm->parmType != PT_VECTOR) {
    idLib::Error("idParmBlock::GetVector: %s is not a vector parm",
                 parm->name.str);
  }
  parmValue_t value;
  GetRenderParmFromOps_r(value, ops.num, parm->parmIndex, parentBlock);
  return {value.value[0], value.value[1], value.value[2], value.value[3]};
}

const idImage* idParmBlock::GetImage(
    const idDeclRenderParm* parm, const idParmBlock* parentBlock) const {
  if (parm->parmType < PT_TEXTURE_2D ||
      parm->parmType > PT_IMAGE_2D_WRITE) {
    idLib::Error("idParmBlock::GetImage: %s is not a texture parm",
                 parm->name.str);
  }
  parmValue_t value;
  GetRenderParmFromOps_r(value, ops.num, parm->parmIndex, parentBlock);
  return value.image;
}

const idDeclRenderProg* idParmBlock::GetProgram(
    const idDeclRenderParm* parm, const idParmBlock* parentBlock) const {
  if (parm->parmType != PT_PROGRAM) {
    idLib::Error("idParmBlock::GetProgram: %s is not a program parm",
                 parm->name.str);
  }
  parmValue_t value;
  GetRenderParmFromOps_r(value, ops.num, parm->parmIndex, parentBlock);
  return value.program;
}

const char* idParmBlock::GetString(
    const idDeclRenderParm* parm, const idParmBlock* parentBlock) const {
  if (parm->parmType != PT_STRING) {
    idLib::Error("idParmBlock::GetString: %s is not a string parm",
                 parm->name.str);
  }
  parmValue_t value;
  GetRenderParmFromOps_r(value, ops.num, parm->parmIndex, parentBlock);
  return value.string;
}

idParmBlock::idParmBlock() = default;

idParmBlock::~idParmBlock() noexcept(false) {
  Clear();
}

idParmBlock::idParmBlock(const idParmBlock& other) {
  CopyFrom(other);
}

idParmBlock& idParmBlock::operator=(const idParmBlock& other) {
  if (this != &other)
    CopyFrom(other);
  return *this;
}

void idParmBlock::Save(idFile* file) const {
  const unsigned char usingTempOps = 0;
  file->Write(&usingTempOps, sizeof(usingTempOps));
  const std::uint32_t bigCount =
      ByteSwap32(static_cast<std::uint32_t>(ops.num));
  file->Write(&bigCount, sizeof(bigCount));

  for (int index = 0; index < ops.num; ++index) {
    const expOp_t& operation = ops[index];
    const idDeclRenderParm* destination = operation.GetDest();
    const unsigned char type = operation.type & 0x1F;
    const unsigned char writeMask = (operation.type >> 9) & 0x0F;
    file->Write(&type, sizeof(type));
    file->Write(&writeMask, sizeof(writeMask));
    file->WriteString(destination->name.str);

    if (type == OP_TYPE_UNMASKED_MOVE_CONSTANT ||
        type == OP_TYPE_MOVE_CONSTANT) {
      switch (destination->parmType) {
        case PT_VECTOR:
          file->WriteBigArray<int>(
              reinterpret_cast<const int*>(constants[index].value), 4);
          break;
        case PT_TEXTURE_2D:
        case PT_TEXTURE_3D:
        case PT_TEXTURE_CUBE:
        case PT_TEXTURE_SHADOW_2D:
        case PT_TEXTURE_SHADOW_3D:
        case PT_TEXTURE_SHADOW_CUBE:
        case PT_TEXTURE_DIM_SHADOW_2D:
        case PT_TEXTURE_MULTISAMPLE_2D:
        case PT_IMAGE_2D_READ:
        case PT_IMAGE_2D_WRITE:
          file->WriteString(constants[index].image->name.str);
          break;
        case PT_SAMPLER:
          file->WriteString(constants[index].sampler == nullptr
                                ? ""
                                : textureSamplerManager->GetSamplerName(
                                      constants[index].sampler));
          break;
        case PT_PROGRAM:
          file->WriteString(constants[index].program == nullptr
                                ? ""
                                : constants[index].program->name.str);
          break;
        case PT_STRING:
          file->WriteString(constants[index].string);
          break;
        default:
          break;
      }
      continue;
    }

    if (type == OP_TYPE_TABLE) {
      const idDeclTable* table =
          renderThreadParmState->GetTable(operation.parmIndexA);
      file->WriteString(table != nullptr ? table->name.str : "");
    } else {
      const idDeclRenderParm* parmA = operation.GetA();
      if (parmA == nullptr) {
        file->WriteString("");
      } else if ((destination->parmType >= PT_TEXTURE_2D &&
                  destination->parmType <= PT_IMAGE_2D_WRITE) ||
                 destination->parmType == PT_PROGRAM) {
        file->WriteString(parmA->declaredValue.image->name.str);
      } else {
        file->WriteString(parmA->name.str);
      }
      if (type == OP_TYPE_SWIZZLE) {
        file->WriteBigArray<int>(constants[index].swizzle, 4);
      }
    }

    const idDeclRenderParm* parmB =
        operation.parmIndexB >= INVALID_PARM_INDEX
            ? nullptr
            : renderThreadParmState->GetRenderParmForIndex(
                  operation.parmIndexB);
    file->WriteString(parmB != nullptr ? parmB->name.str : "");
  }
}

void idParmBlock::RemoveRedundantOperations() {
  bool opIsRedundant[256];
  bool anyRedundant = false;
  for (int index = ops.num - 1; index >= 0; --index) {
    bool usedByLaterOperation = false;
    bool overwrittenByLaterOperation = false;
    opIsRedundant[index] = false;
    for (int later = index + 1; later < ops.num; ++later) {
      if (opIsRedundant[later])
        continue;
      if (ops[later].parmIndexA == ops[index].parmIndexDest ||
          ops[later].parmIndexB == ops[index].parmIndexDest) {
        usedByLaterOperation = true;
      }
      const int currentMask = (ops[index].type >> 9) & 0x0F;
      const int laterMask = (ops[later].type >> 9) & 0x0F;
      if (ops[later].parmIndexDest == ops[index].parmIndexDest &&
          (currentMask & ~laterMask & 0x0F) == 0) {
        overwrittenByLaterOperation = true;
        break;
      }
    }
    if (!usedByLaterOperation &&
        (overwrittenByLaterOperation ||
         ops[index].GetDest()->creator == PC_TEMPORARY)) {
      opIsRedundant[index] = true;
      anyRedundant = true;
    }
  }

  if (!anyRedundant)
    return;
  int retained = 0;
  for (int index = 0; index < ops.num; ++index) {
    if (opIsRedundant[index]) {
      ++c_redundantOperationsRemoved;
    } else {
      ops[retained] = ops[index];
      constants[retained] = constants[index];
      ++retained;
    }
  }
  ops.SetNum(retained);
  constants.SetNum(retained);
}

void idParmBlock::CopyFrom(const idParmBlock& other) {
  ops.SetNum(other.ops.num);
  constants.SetNum(other.constants.num);
  for (int index = 0; index < other.ops.num; ++index) {
    ops[index] = other.ops[index];
    constants[index] = other.constants[index];
  }
}

void idParmBlock::AddOp(const expOp_t& operation,
                        const parmValue_t& constant) {
  ops.Append(operation);
  constants.Append(constant);
}

void idParmBlock::ClearParm(const idDeclRenderParm* parm) {
  bool removedComplexOperation = false;
  for (int index = 0; index < ops.num; ++index) {
    if (ops[index].parmIndexDest != parm->parmIndex)
      continue;
    const int type = ops[index].type & 0x1F;
    removedComplexOperation |= type >= OP_TYPE_SWIZZLE;
    ops.RemoveIndex(index);
    constants.RemoveIndex(index);
    --index;
  }
  if (removedComplexOperation)
    RemoveRedundantOperations();
}

void idParmBlock::Append(const idParmBlock& other) {
  for (int index = 0; index < other.ops.num; ++index)
    AddOp(other.ops[index], other.constants[index]);
  RemoveRedundantOperations();
}

bool idParmBlock::Load(idFile* file) {
  unsigned char usingTempOps;
  if (file->Read(&usingTempOps, sizeof(usingTempOps)) == 0)
    return false;
  std::uint32_t bigCount = 0;
  if (file->Read(&bigCount, sizeof(bigCount)) == 0)
    return false;
  const int count = static_cast<int>(ByteSwap32(bigCount));
  ops.SetNum(count);
  constants.SetNum(count);

  for (int index = 0; index < ops.num; ++index) {
    unsigned char type;
    unsigned char writeMask;
    if (file->Read(&type, sizeof(type)) == 0 ||
        file->Read(&writeMask, sizeof(writeMask)) == 0) {
      return false;
    }
    ops[index].type = static_cast<unsigned short>(
        (ops[index].type & 0xFFE0u) | type);
    ops[index].type = static_cast<unsigned short>(
        (ops[index].type & 0xE1FFu) | (writeMask << 9));

    idStr string;
    if (file->ReadString(string) == 0)
      return false;
    const idDeclRenderParm* destination =
        static_cast<const idDeclRenderParm*>(
            idDeclRenderParm::resourceList.FindWithInheritance(string.data,
                                                               true, true));
    ops[index].parmIndexDest = static_cast<short>(destination->parmIndex);
    ops[index].type = static_cast<unsigned short>(
        (ops[index].type & 0xFE1Fu) | (destination->parmType << 5));

    const int operationType = ops[index].type & 0x1F;
    if (operationType == OP_TYPE_UNMASKED_MOVE_CONSTANT ||
        operationType == OP_TYPE_MOVE_CONSTANT) {
      switch (destination->parmType) {
        case PT_VECTOR:
          if (file->ReadBigArray<int>(
                  reinterpret_cast<int*>(constants[index].value), 4) == 0) {
            return false;
          }
          break;
        case PT_TEXTURE_2D:
        case PT_TEXTURE_3D:
        case PT_TEXTURE_CUBE:
        case PT_TEXTURE_SHADOW_2D:
        case PT_TEXTURE_SHADOW_3D:
        case PT_TEXTURE_SHADOW_CUBE:
        case PT_TEXTURE_DIM_SHADOW_2D:
        case PT_TEXTURE_MULTISAMPLE_2D:
        case PT_IMAGE_2D_READ:
        case PT_IMAGE_2D_WRITE:
          if (file->ReadString(string) == 0)
            return false;
          constants[index].image =
              globalImages->ImageFromFile(string.data, nullptr, true);
          if (constants[index].image == nullptr) {
            idLib::Warning("idParmBlock::Load: NULL Texture '%s'",
                           string.data);
          }
          break;
        case PT_SAMPLER:
          if (file->ReadString(string) == 0)
            return false;
          constants[index].sampler =
              string.IsEmpty()
                  ? nullptr
                  : textureSamplerManager->FindSamplerByName(string.data,
                                                             false);
          if (!string.IsEmpty() && constants[index].sampler == nullptr) {
            idLib::Warning("idParmBlock::Load: NULL Sampler '%s'",
                           string.data);
          }
          [[fallthrough]];
        case PT_PROGRAM:
          if (file->ReadString(string) == 0)
            return false;
          constants[index].program =
              string.IsEmpty()
                  ? nullptr
                  : static_cast<const idDeclRenderProg*>(
                        idDeclRenderProg::resourceList.FindWithInheritance(
                            string.data, true, true));
          if (!string.IsEmpty() && constants[index].program == nullptr) {
            idLib::Warning("idParmBlock::Load: NULL Program '%s'",
                           string.data);
          }
          break;
        case PT_STRING: {
          if (file->ReadString(string) == 0)
            return false;
          idAtomicString atomic(string.data);
          constants[index].string = atomic.str;
          break;
        }
        default:
          break;
      }
      ops[index].parmIndexA = INVALID_PARM_INDEX;
      ops[index].parmIndexB = INVALID_PARM_INDEX;
      continue;
    }

    if (operationType == OP_TYPE_TABLE) {
      if (file->ReadString(string) == 0)
        return false;
      const idDeclTable* table =
          string.IsEmpty()
              ? nullptr
              : static_cast<const idDeclTable*>(
                    idDeclTable::resourceList.FindWithInheritance(
                        string.data, true, true));
      ops[index].parmIndexA = static_cast<short>(
          renderThreadParmState->GetTableIndex(table));
    } else {
      if (file->ReadString(string) == 0)
        return false;
      const idDeclRenderParm* parmA = nullptr;
      if (!string.IsEmpty() &&
          !(destination->parmType >= PT_TEXTURE_2D &&
            destination->parmType <= PT_IMAGE_2D_WRITE) &&
          destination->parmType != PT_PROGRAM) {
        parmA = static_cast<const idDeclRenderParm*>(
            idDeclRenderParm::resourceList.FindWithInheritance(string.data,
                                                               true, true));
      }
      ops[index].parmIndexA = static_cast<short>(
          parmA != nullptr ? parmA->parmIndex : INVALID_PARM_INDEX);
      if (operationType == OP_TYPE_SWIZZLE &&
          file->ReadBigArray<int>(constants[index].swizzle, 4) == 0) {
        return false;
      }
    }

    if (file->ReadString(string) == 0)
      return false;
    const idDeclRenderParm* parmB =
        string.IsEmpty()
            ? nullptr
            : static_cast<const idDeclRenderParm*>(
                  idDeclRenderParm::resourceList.FindWithInheritance(
                      string.data, true, true));
    ops[index].parmIndexB = static_cast<short>(
        parmB != nullptr ? parmB->parmIndex : INVALID_PARM_INDEX);
  }
  return true;
}

void idParmBlock::SetParm(const idDeclRenderParm* parm,
                          const idDeclRenderParm* sourceParm) {
  int index = ops.num - 1;
  while (index >= 0 && ops[index].parmIndexDest != parm->parmIndex)
    --index;
  bool replacedComplexOperation = false;
  if (index < 0) {
    if (ops.listStatic != 0 && ops.num == ops.size) {
      idLib::FatalError("idParmBlock::SetParm: MAX_STATIC_PARM_OPS exceeded");
    }
    index = ops.num;
    expOp_t operation{};
    parmValue_t constant{};
    ops.Append(operation);
    constants.Append(constant);
  } else {
    replacedComplexOperation = (ops[index].type & 0x1F) >= OP_TYPE_SWIZZLE;
  }

  ops[index].type = static_cast<unsigned short>(
      (parm->parmType << 5) | (ops[index].type & 0xFE01u) |
      OP_TYPE_UNMASKED_MOVE);
  ops[index].parmIndexDest = static_cast<short>(parm->parmIndex);
  ops[index].parmIndexA = static_cast<short>(sourceParm->parmIndex);
  ops[index].parmIndexB = INVALID_PARM_INDEX;
  if (replacedComplexOperation)
    RemoveRedundantOperations();
}

void idParmBlock::SetParm(const idDeclRenderParm* parm,
                          const parmValue_t& parmValue) {
  int index = ops.num - 1;
  while (index >= 0 && ops[index].parmIndexDest != parm->parmIndex)
    --index;
  bool replacedComplexOperation = false;
  if (index < 0) {
    if (ops.listStatic != 0 && ops.num == ops.size) {
      idLib::FatalError("idParmBlock::SetParm: MAX_STATIC_PARM_OPS exceeded");
    }
    index = ops.num;
    expOp_t operation{};
    parmValue_t constant{};
    ops.Append(operation);
    constants.Append(constant);
  } else {
    replacedComplexOperation = (ops[index].type & 0x1F) >= OP_TYPE_SWIZZLE;
  }

  ops[index].type = static_cast<unsigned short>(
      (parm->parmType << 5) | (ops[index].type & 0xFE00u));
  ops[index].parmIndexDest = static_cast<short>(parm->parmIndex);
  ops[index].parmIndexA = INVALID_PARM_INDEX;
  ops[index].parmIndexB = INVALID_PARM_INDEX;
  constants[index] = parmValue;
  if (replacedComplexOperation)
    RemoveRedundantOperations();
}

void idParmBlock::SetParm(const idDeclRenderParm* parm,
                          const idVec4& value) {
  SetVec4(parm, value);
}

void idParmBlock::SetParm(const idDeclRenderParm* parm, float value) {
  SetFloat(parm, value);
}

void idParmBlock::SetFloat(const idDeclRenderParm* parm, float value) {
  SetVec4(parm, {value, value, value, value});
}

void idParmBlock::SetVec4(const idDeclRenderParm* parm,
                          const idVec4& value) {
  parmValue_t parmValue;
  parmValue.value[0] = value.x;
  parmValue.value[1] = value.y;
  parmValue.value[2] = value.z;
  parmValue.value[3] = value.w;
  SetParm(parm, parmValue);
}

void idParmBlock::SetImage(const idDeclRenderParm* parm,
                           const idImage* image) {
  parmValue_t parmValue;
  parmValue.image = image;
  SetParm(parm, parmValue);
}
