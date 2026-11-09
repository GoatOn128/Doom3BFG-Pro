#include "engine/renderer/parmblockparser.h"

#include "engine/decls/decltable.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/texturesampler.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/parser.h"
#include "shared/idlib/text/token.h"

#include <cstdint>
#include <cstring>

namespace {

constexpr short INVALID_PARM_INDEX = 2175;
constexpr int MAX_PARMBLOCK_OPS = 256;
constexpr int MAX_EXPRESSION_TEMPS = 32;

}  // namespace

const idDeclRenderParm* idParmBlockParser::tempParms[MAX_EXPRESSION_TEMPS]{};

bool idParmBlockParser::CleanupTemporaries() {
  bool opIsRedundant[MAX_PARMBLOCK_OPS];
  bool anyRedundant = false;
  for (int index = ops->num - 1; index >= 0; --index) {
    bool usedByLaterOperation = false;
    bool overwrittenByLaterOperation = false;
    opIsRedundant[index] = false;
    for (int later = index + 1; later < ops->num; ++later) {
      if (opIsRedundant[later])
        continue;
      if ((*ops)[later].parmIndexA == (*ops)[index].parmIndexDest ||
          (*ops)[later].parmIndexB == (*ops)[index].parmIndexDest) {
        usedByLaterOperation = true;
      }
      const int currentMask = ((*ops)[index].type >> 9) & 15;
      const int laterMask = ((*ops)[later].type >> 9) & 15;
      if ((*ops)[later].parmIndexDest == (*ops)[index].parmIndexDest &&
          (currentMask & ~laterMask & 15) == 0) {
        overwrittenByLaterOperation = true;
        break;
      }
    }
    if (!usedByLaterOperation &&
        (overwrittenByLaterOperation ||
         (*ops)[index].GetDest()->creator == PC_TEMPORARY)) {
      opIsRedundant[index] = true;
      anyRedundant = true;
    }
  }

  if (anyRedundant) {
    int retained = 0;
    for (int index = 0; index < ops->num; ++index) {
      if (!opIsRedundant[index]) {
        (*ops)[retained] = (*ops)[index];
        (*constants)[retained] = (*constants)[index];
        ++retained;
      }
    }
    ops->SetNum(retained);
    constants->SetNum(retained);
  }

  bool releasedTemporary = false;
  for (int tempIndex = 0; tempIndex < MAX_EXPRESSION_TEMPS; ++tempIndex) {
    if (static_cast<std::size_t>(tempIndex) >= usedTempParms.bits ||
        (usedTempParms.buffer[tempIndex >> 3] &
         (1u << (tempIndex & 7))) == 0) {
      continue;
    }
    bool stillUsed = false;
    for (int opIndex = 0; opIndex < ops->num; ++opIndex) {
      if ((*ops)[opIndex].GetDest() == tempParms[tempIndex]) {
        stillUsed = true;
        break;
      }
    }
    if (!stillUsed) {
      usedTempParms.buffer[tempIndex >> 3] &=
          static_cast<unsigned char>(~(1u << (tempIndex & 7)));
      releasedTemporary = true;
    }
  }
  return releasedTemporary;
}

const idDeclRenderParm* idParmBlockParser::GetExpressionTemporaryParm() {
  int index = 0;
  while (index < static_cast<int>(usedTempParms.bits) &&
         (usedTempParms.buffer[index >> 3] & (1u << (index & 7))) != 0) {
    if (++index >= MAX_EXPRESSION_TEMPS) {
      index = -1;
      break;
    }
  }
  if (index < 0) {
    if (CleanupTemporaries()) {
      index = 0;
      while (index < static_cast<int>(usedTempParms.bits) &&
             (usedTempParms.buffer[index >> 3] &
              (1u << (index & 7))) != 0) {
        if (++index >= MAX_EXPRESSION_TEMPS) {
          index = -1;
          break;
        }
      }
    }
    if (index < 0) {
      idLib::FatalError(
          "idParmBlockParser::GetExpressionTemporaryParm: "
          "MAX_EXPRESSION_TEMPS");
    }
  }
  if (index < static_cast<int>(usedTempParms.bits)) {
    usedTempParms.buffer[index >> 3] |=
        static_cast<unsigned char>(1u << (index & 7));
  }
  return tempParms[index];
}

expOp_t* idParmBlockParser::GetExpressionOp(
    idParser& source, const parmValue_t& constant) {
  if (ops->num == MAX_PARMBLOCK_OPS) {
    source.Warning("GetExpressionOp: hit MAX_PARMBLOCK_OPS");
    return &ops->list[ops->num];
  }
  expOp_t* operation = ops->Alloc();
  *operation = {};
  operation->parmIndexDest = INVALID_PARM_INDEX;
  operation->parmIndexA = INVALID_PARM_INDEX;
  operation->parmIndexB = INVALID_PARM_INDEX;
  *constants->Alloc() = constant;
  return operation;
}

const idDeclRenderParm* idParmBlockParser::GetConstantVector(
    idParser& source, const float* const value) {
  parmValue_t constant;
  std::memcpy(constant.value, value, sizeof(constant.value));
  const idDeclRenderParm* temporary = GetExpressionTemporaryParm();
  expOp_t* operation = GetExpressionOp(source, constant);
  operation->type = static_cast<unsigned short>(
      (operation->type & 0xFFE0u) | (15 << 9));
  operation->parmIndexDest = static_cast<short>(temporary->parmIndex);
  operation->type = static_cast<unsigned short>(
      (temporary->parmType << 5) | (operation->type & 0xFE1Fu));
  return temporary;
}

const idDeclRenderParm* idParmBlockParser::EmitOp(
    idParser& source, const idDeclRenderParm* parmA,
    const idDeclRenderParm* parmB, const idDeclTable* table,
    expOpType_t opType) {
  int tableIndex = renderThreadParmState->GetTableIndex(table);
  const idDeclRenderParm* temporary = GetExpressionTemporaryParm();
  expOp_t* operation = GetExpressionOp(source, defaultConstant);
  operation->type = static_cast<unsigned short>(
      (operation->type & 0xFFE0u) | opType | (15 << 9));
  operation->parmIndexDest = static_cast<short>(temporary->parmIndex);
  operation->parmIndexA = static_cast<short>(
      tableIndex == INVALID_PARM_INDEX ? parmA->parmIndex : tableIndex);
  operation->parmIndexB = static_cast<short>(parmB->parmIndex);
  operation->type = static_cast<unsigned short>(
      (temporary->parmType << 5) | (operation->type & 0xFE1Fu));
  return temporary;
}

const idDeclRenderParm* idParmBlockParser::ParseEmitOp(
    idParser& source, const idDeclRenderParm* parmA, expOpType_t opType,
    int priority) {
  const idDeclRenderParm* parmB = ParseExpressionPriority(source, priority);
  return EmitOp(source, parmA, parmB, nullptr, opType);
}

const idDeclRenderParm* idParmBlockParser::ParseIntrinsic(
    idParser& source, expOpType_t opType) {
  source.ExpectTokenString("(");
  const idDeclRenderParm* parmA = ParseExpressionPriority(source, 4);
  source.ExpectTokenString(",");
  const idDeclRenderParm* parmB = ParseExpressionPriority(source, 4);
  source.ExpectTokenString(")");
  return EmitOp(source, parmA, parmB, nullptr, opType);
}

const idDeclRenderParm* idParmBlockParser::ParseTerm(idParser& source,
                                                     idToken* fakeToken) {
  idToken token;
  if (fakeToken != nullptr) {
    token = fakeToken->data;
    token.type = fakeToken->type;
    token.subtype = fakeToken->subtype;
    token.line = fakeToken->line;
    token.linesCrossed = fakeToken->linesCrossed;
    token.flags = fakeToken->flags;
    token.intvalue = fakeToken->intvalue;
    token.floatvalue = fakeToken->floatvalue;
    token.whiteSpaceStart_p = fakeToken->whiteSpaceStart_p;
    token.whiteSpaceEnd_p = fakeToken->whiteSpaceEnd_p;
    token.next = fakeToken->next;
  } else {
    source.ReadToken(token);
  }

  if (token == "dot3")
    return ParseIntrinsic(source, OP_TYPE_DOT3);
  if (token == "dot4")
    return ParseIntrinsic(source, OP_TYPE_DOT4);
  if (token == "(") {
    const idDeclRenderParm* result = ParseExpressionPriority(source, 4);
    source.ExpectTokenString(")");
    return result;
  }
  if (token == "{" || token == "-" || token.type == TT_NUMBER) {
    source.UnreadToken(token);
    parmValue_t constantValue;
    idDeclRenderParm::ParseVectorConstant(source, constantValue.value);
    return GetConstantVector(source, constantValue.value);
  }

  const char* dot = std::strstr(token.data, ".");
  if (dot != nullptr) {
    const int baseLength = static_cast<int>(dot - token.data);
    idStr base = token.Left(baseLength);
    idToken baseToken;
    baseToken = base.data;
    baseToken.type = TT_NAME;
    const idDeclRenderParm* parmA = ParseTerm(source, &baseToken);
    const idDeclRenderParm* temporary = GetExpressionTemporaryParm();
    parmValue_t swizzle{};
    swizzle.swizzle[0] = 0;
    swizzle.swizzle[1] = 1;
    swizzle.swizzle[2] = 2;
    swizzle.swizzle[3] = 3;
    int count = 0;
    while (dot[count + 1] != '\0' && count < 4) {
      switch (idStr::ToLower(static_cast<unsigned char>(dot[count + 1]))) {
        case 'x':
        case 'r': swizzle.swizzle[count] = 0; break;
        case 'y':
        case 'g': swizzle.swizzle[count] = 1; break;
        case 'z':
        case 'b': swizzle.swizzle[count] = 2; break;
        case 'w':
        case 'a': swizzle.swizzle[count] = 3; break;
        default:
          source.Warning("bad swizzle '%s'\n", dot);
          goto finishSwizzle;
      }
      ++count;
    }
finishSwizzle:
    for (int component = count; component < 4; ++component)
      swizzle.swizzle[component] = swizzle.swizzle[0];
    expOp_t* operation = GetExpressionOp(source, swizzle);
    operation->type = static_cast<unsigned short>(
        (operation->type & 0xFFE4u) | OP_TYPE_SWIZZLE | (15 << 9));
    operation->parmIndexDest = static_cast<short>(temporary->parmIndex);
    operation->parmIndexA = static_cast<short>(parmA->parmIndex);
    operation->parmIndexB = INVALID_PARM_INDEX;
    operation->type = static_cast<unsigned short>(
        (temporary->parmType << 5) | (operation->type & 0xFE1Fu));
    return temporary;
  }

  if (source.PeekTokenString("[")) {
    const idDeclTable* table = static_cast<const idDeclTable*>(
        idDeclTable::resourceList.FindWithInheritance(token.data, false,
                                                      true));
    if (table != nullptr) {
      source.ExpectTokenString("[");
      const idDeclRenderParm* index = ParseExpressionPriority(source, 4);
      source.ExpectTokenString("]");
      return EmitOp(source, nullptr, index, table, OP_TYPE_TABLE);
    }
  } else {
    const idDeclRenderParm* parm = static_cast<const idDeclRenderParm*>(
        idDeclRenderParm::resourceList.FindWithInheritance(token.data, false,
                                                           true));
    if (parm != nullptr)
      return parm;
  }

  source.Warning("Bad term '%s'", token.data);
  return GetExpressionTemporaryParm();
}

const idDeclRenderParm* idParmBlockParser::ParseExpressionPriority(
    idParser& source, int priority) {
  if (priority == 0)
    return ParseTerm(source, nullptr);
  const idDeclRenderParm* parmA =
      ParseExpressionPriority(source, priority - 1);
  if (source.HadWarning())
    return GetExpressionTemporaryParm();

  idToken token;
  if (!source.ReadToken(token))
    return parmA;
  switch (priority) {
    case 1:
      if (token == "*")
        return ParseEmitOp(source, parmA, OP_TYPE_MULTIPLY, 1);
      if (token == "/")
        return ParseEmitOp(source, parmA, OP_TYPE_DIVIDE, 1);
      if (token == "%")
        return ParseEmitOp(source, parmA, OP_TYPE_MOD, 1);
      break;
    case 2:
      if (token == "+")
        return ParseEmitOp(source, parmA, OP_TYPE_ADD, 2);
      if (token == "-")
        return ParseEmitOp(source, parmA, OP_TYPE_SUBTRACT, 2);
      break;
    case 3:
      if (token == ">")
        return ParseEmitOp(source, parmA, OP_TYPE_GT, 3);
      if (token == ">=")
        return ParseEmitOp(source, parmA, OP_TYPE_GE, 3);
      if (token == "<")
        return ParseEmitOp(source, parmA, OP_TYPE_LT, 3);
      if (token == "<=")
        return ParseEmitOp(source, parmA, OP_TYPE_LE, 3);
      if (token == "==")
        return ParseEmitOp(source, parmA, OP_TYPE_EQ, 3);
      if (token == "!=")
        return ParseEmitOp(source, parmA, OP_TYPE_NE, 3);
      break;
    case 4:
      if (token == "&&")
        return ParseEmitOp(source, parmA, OP_TYPE_AND, 4);
      if (token == "||")
        return ParseEmitOp(source, parmA, OP_TYPE_OR, 4);
      break;
    default:
      break;
  }
  if (token != ";")
    source.UnreadToken(token);
  return parmA;
}

idParmBlockParser::idParmBlockParser(idParser& source, idParmBlock& block)
    : usedTempParms(), ops(&block.ops), constants(&block.constants),
      defaultConstant{} {
  if (tempParms[0] == nullptr) {
    for (int index = 0; index < MAX_EXPRESSION_TEMPS; ++index) {
      idStr name;
      name.Format("temp%i", index + 1);
      idDeclRenderParm* temporary = static_cast<idDeclRenderParm*>(
          idDeclRenderParm::resourceList.FindWithInheritance(name.data, true,
                                                             true));
      temporary->creator = PC_TEMPORARY;
      tempParms[index] = temporary;
    }
  }

  usedTempParms.memTag = 9;
  usedTempParms.Alloc(MAX_EXPRESSION_TEMPS);
  if (usedTempParms.buffer != nullptr) {
    std::memset(usedTempParms.buffer, 0,
                (usedTempParms.bits + 7u) >> 3u);
  }

  idToken token;
  if (!source.ExpectAnyToken(token))
    return;
  while (true) {
    if (token == "}")
      return;
    if (token == "{") {
      source.Warning("Can't have an open brace inside a parmBlock.");
      return;
    }

    int writeMask = 15;
    const char* dot = std::strstr(token.data, ".");
    if (dot != nullptr) {
      idStr base = token.Left(static_cast<int>(dot - token.data));
      int nextComponent = 0;
      writeMask = 0;
      const char* mask = dot + 1;
      if (*mask == '\0') {
        source.Warning("bad write mask\n");
        return;
      }
      for (; *mask != '\0'; ++mask) {
        const int character = idStr::ToLower(
            static_cast<unsigned char>(*mask));
        int component = -1;
        for (int candidate = nextComponent; candidate < 4; ++candidate) {
          constexpr char RGBA[] = "rgba";
          constexpr char XYZW[] = "xyzw";
          if (character == RGBA[candidate] || character == XYZW[candidate]) {
            component = candidate;
            break;
          }
        }
        if (component < 0) {
          source.Warning("bad write mask\n");
          return;
        }
        writeMask |= 1 << component;
        nextComponent = component + 1;
      }
      token = base.data;
    }

    idDeclRenderParm* destination = static_cast<idDeclRenderParm*>(
        idDeclRenderParm::resourceList.FindWithInheritance(token.data, true,
                                                           true));
    if (destination->resourceError != nullptr) {
      source.Warning("'%s' is an unknown renderParm.", token.data);
      return;
    }
    source.CheckTokenString("=");

    if (destination->parmType >= PT_TEXTURE_2D &&
        destination->parmType <= PT_IMAGE_2D_WRITE) {
      parmValue_t constant{};
      constant.image = destination->ParseImageLine(source);
      if (source.HadWarning())
        return;
      expOp_t* operation = GetExpressionOp(source, constant);
      operation->type = static_cast<unsigned short>(
          (operation->type & 0xFFE0u) | (15 << 9));
      operation->parmIndexDest = static_cast<short>(destination->parmIndex);
      operation->parmIndexA = INVALID_PARM_INDEX;
      operation->parmIndexB = INVALID_PARM_INDEX;
      operation->type = static_cast<unsigned short>(
          (destination->parmType << 5) | (operation->type & 0xFE1Fu));
      source.CheckTokenString(";");
    } else if (destination->parmType == PT_SAMPLER) {
      if (!source.ExpectAnyToken(token))
        return;
      parmValue_t constant{};
      constant.sampler =
          textureSamplerManager->FindSamplerByName(token.data, false);
      if (constant.sampler == nullptr) {
        source.Warning("'%s' is an unknown sampler.", token.data);
        return;
      }
      expOp_t* operation = GetExpressionOp(source, constant);
      operation->type = static_cast<unsigned short>(
          (operation->type & 0xFFE0u) | (15 << 9));
      operation->parmIndexDest = static_cast<short>(destination->parmIndex);
      operation->parmIndexA = INVALID_PARM_INDEX;
      operation->parmIndexB = INVALID_PARM_INDEX;
      operation->type = static_cast<unsigned short>(
          (destination->parmType << 5) | (operation->type & 0xFE1Fu));
      source.CheckTokenString(";");
    } else if (destination->parmType == PT_PROGRAM) {
      if (!source.ExpectAnyToken(token))
        return;
      parmValue_t constant{};
      constant.program = static_cast<const idDeclRenderProg*>(
          idDeclRenderProg::resourceList.FindWithInheritance(token.data,
                                                             false, true));
      if (constant.program == nullptr) {
        source.Warning("'%s' is an unknown renderProg.", token.data);
        return;
      }
      expOp_t* operation = GetExpressionOp(source, constant);
      operation->type = static_cast<unsigned short>(
          (operation->type & 0xFFE0u) | (15 << 9));
      operation->parmIndexDest = static_cast<short>(destination->parmIndex);
      operation->parmIndexA = INVALID_PARM_INDEX;
      operation->parmIndexB = INVALID_PARM_INDEX;
      operation->type = static_cast<unsigned short>(
          (destination->parmType << 5) | (operation->type & 0xFE1Fu));
      source.CheckTokenString(";");
    } else if (destination->parmType == PT_STRING) {
      idStr text;
      idAtomicString atomic(source.ParseRestOfLine(text).data);
      parmValue_t constant{};
      constant.string = atomic.str;
      expOp_t* operation = GetExpressionOp(source, constant);
      operation->type = static_cast<unsigned short>(
          (operation->type & 0xFFE0u) | (15 << 9));
      operation->parmIndexDest = static_cast<short>(destination->parmIndex);
      operation->parmIndexA = INVALID_PARM_INDEX;
      operation->parmIndexB = INVALID_PARM_INDEX;
      operation->type = static_cast<unsigned short>(
          (destination->parmType << 5) | (operation->type & 0xFE1Fu));
      source.CheckTokenString(";");
    } else {
      const idDeclRenderParm* result = ParseExpressionPriority(source, 4);
      if (result->creator == PC_TEMPORARY && ops->num > 0 &&
          (*ops)[ops->num - 1].parmIndexDest == result->parmIndex) {
        expOp_t& operation = (*ops)[ops->num - 1];
        operation.type = static_cast<unsigned short>(
            (operation.type & 0xE1FFu) | (writeMask << 9));
        operation.parmIndexDest = static_cast<short>(destination->parmIndex);
      } else {
        expOp_t* operation = GetExpressionOp(source, defaultConstant);
        operation->type = static_cast<unsigned short>(
            (operation->type & (writeMask == 15 ? 0xFFE1u : 0xFFE3u)) |
            (writeMask == 15 ? OP_TYPE_UNMASKED_MOVE : OP_TYPE_MOVE));
        operation->type = static_cast<unsigned short>(
            (operation->type & 0xE1FFu) | (writeMask << 9));
        operation->parmIndexDest = static_cast<short>(destination->parmIndex);
        operation->parmIndexA = static_cast<short>(result->parmIndex);
        operation->parmIndexB = INVALID_PARM_INDEX;
        operation->type = static_cast<unsigned short>(
            (destination->parmType << 5) | (operation->type & 0xFE1Fu));
      }
      if (source.HadWarning())
        return;
    }

    if (!source.ExpectAnyToken(token))
      return;
  }
}
