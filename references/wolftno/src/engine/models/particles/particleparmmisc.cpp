#include "engine/models/particles/particleparmmisc.h"

#include "shared/idlib/lib.h"

#include <cstdlib>
#include <cstring>
#include <new>

idStr particleCalcTypeDesc[NUM_PARTICLE_EDIT_CALC_TYPES] = {
    "none",
    "constant",
    "minmax",
    "curve",
    "curve_scale_bias",
    "curve_use_variance_mod_constant",
    "curve_mod_curve",
    "curve_add_curve",
    "parametricEval",
    "parametricIntegrate",
    "parametricIntegrateMinMax",
};

template <>
bool idList<idLookupTable, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2)
      delete[] list;
    list = nullptr;
    size = 0;
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0)
    return true;
  idLookupTable* replacement = static_cast<idLookupTable*>(
      ::operator new[](sizeof(idLookupTable) * newSize, std::nothrow));
  if (replacement == nullptr)
    return false;
  const int copyCount = num < newSize ? num : newSize;
  if (copyCount > 0)
    std::memmove(replacement, list, sizeof(*list) * copyCount);
  delete[] list;
  list = replacement;
  size = newSize;
  num = copyCount;
  listStatic = 0;
  return true;
}

template <>
int idList<idLookupTable, 5>::Append(const idLookupTable& value) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize))
      return -1;
  }
  if (num == size) {
    int newSize;
    if (granularity > 0) {
      newSize = size + granularity;
      newSize -= newSize % granularity;
    } else {
      newSize = size > 0 ? size * 2 : 1;
      if (newSize < size)
        newSize = 0x7fffffff;
    }
    if (!Resize(newSize))
      return -1;
  }
  if (num >= size)
    return num - 1;
  std::memmove(&list[num], &value, sizeof(value));
  return num++;
}

namespace {

void WriteParametric(idFile* file, const idParticleParm* parm,
                     const char*) {
  file->WriteFloatString("\"%.3f\" ", parm->val0);
  if (parm->val0 != parm->val1)
    file->WriteFloatString(" to \"%.3f\" ", parm->val1);
  if (parm->variance != 0.0f)
    file->WriteFloatString(" variance \"%.3f\"", parm->variance);
  file->WriteFloatString("\n");
}

int AddTable(const char* tableName,
             idList<const idDeclTable*, 5>& tableDecls) {
  const idDeclTable* table = static_cast<const idDeclTable*>(
      idDeclTable::resourceList.FindWithInheritance(tableName, true, true));
  if (table->resourceError != nullptr)
    idLib::Warning("Table decl error: '%s' - using default.",
                   table->name.str);
  for (int index = 0; index < tableDecls.Num(); ++index) {
    if (tableDecls[index] == table)
      return index;
  }
  if (tableDecls.num == tableDecls.size) {
    int newSize;
    if (tableDecls.granularity > 0) {
      newSize = tableDecls.size + tableDecls.granularity;
      newSize -= newSize % tableDecls.granularity;
    } else {
      newSize = tableDecls.size > 0 ? tableDecls.size * 2 : 1;
    }
    const idDeclTable** replacement = new const idDeclTable*[newSize];
    if (tableDecls.num > 0) {
      std::memmove(replacement, tableDecls.list,
                   sizeof(*replacement) * tableDecls.num);
    }
    delete[] tableDecls.list;
    tableDecls.list = replacement;
    tableDecls.size = newSize;
  }
  tableDecls.list[tableDecls.num] = table;
  return tableDecls.num++;
}

void ParseParametric(idParser& source, idParticleParm* parm,
                     idList<const idDeclTable*, 5>& tableDecls,
                     idList<idLookupTable, 5>& tables) {
  parm->tableIdx = -1;
  parm->table2Idx = -1;
  parm->val0 = 0.0f;
  parm->val1 = 0.0f;
  parm->variance = 0.0f;
  idToken token;
  if (!source.ReadToken(token)) {
    source.Error("not enough parameters");
    return;
  }
  if (!token.IsNumeric()) {
    source.UnreadToken(token);
    ParseParticleParm(source, parm, tableDecls, tables);
    return;
  }

  const float initial = std::strtof(token.data, nullptr);
  float destination = initial;
  float variance = 0.0f;
  while (!source.HadError() && source.ReadToken(token)) {
    if (token.Icmp("to") == 0) {
      if (!source.ReadToken(token)) {
        source.Error("Missing 'to' parameter for parametric parm");
        break;
      }
      destination = std::strtof(token.data, nullptr);
    } else if (token.Icmp("variance") == 0) {
      if (!source.ReadToken(token)) {
        source.Error("Missing 'variance' parameter for parametric parm");
        break;
      }
      variance = std::strtof(token.data, nullptr);
    } else {
      source.UnreadToken(token);
      break;
    }
  }
  parm->val0 = initial;
  parm->val1 = destination;
  parm->variance = variance;
}

} // namespace

void idParticleParm::SetCalcTypeFromEditType(
    particleEditCalcType_t editType) {
  switch (editType) {
  case PARTICLE_EDIT_CALC_NONE:
    calcType = PARTICLE_CALC_NONE;
    break;
  case PARTICLE_EDIT_CALC_CONSTANT:
  case PARTICLE_EDIT_CALC_MINMAX:
    calcType = PARTICLE_CALC_GENERIC;
    break;
  case PARTICLE_EDIT_CALC_CURVE:
  case PARTICLE_EDIT_CALC_CURVE_SCALE_BIAS:
  case PARTICLE_EDIT_CALC_CURVE_VARIANCE_MOD_CONSTANT:
    calcType = PARTICLE_CALC_CURVE_GENERIC;
    break;
  case PARTICLE_EDIT_CALC_CURVE_MOD_CURVE:
    calcType = PARTICLE_CALC_CURVE_MOD_CURVE;
    break;
  case PARTICLE_EDIT_CALC_CURVE_ADD_CURVE:
    calcType = PARTICLE_CALC_CURVE_ADD_CURVE;
    break;
  case PARTICLE_EDIT_CALC_PARAMETRIC_EVAL:
    calcType = PARTICLE_CALC_PARAMETRIC_EVAL;
    break;
  case PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE:
    calcType = PARTICLE_CALC_PARAMETRIC_INTEGRATE;
    break;
  case PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE_MINMAX:
    calcType = PARTICLE_CALC_PARAMETRIC_INTEGRATE_MINMAX;
    break;
  default:
    break;
  }
}

particleEditCalcType_t idParticleParm::GetEditCalcType() const {
  switch (calcType) {
  case PARTICLE_CALC_CURVE_GENERIC:
    if (variance != 0.0f)
      return PARTICLE_EDIT_CALC_CURVE_VARIANCE_MOD_CONSTANT;
    return val0 == 1.0f && val1 == 0.0f
               ? PARTICLE_EDIT_CALC_CURVE
               : PARTICLE_EDIT_CALC_CURVE_SCALE_BIAS;
  case PARTICLE_CALC_GENERIC:
    return val0 == val1 ? PARTICLE_EDIT_CALC_CONSTANT
                        : PARTICLE_EDIT_CALC_MINMAX;
  case PARTICLE_CALC_CURVE_MOD_CURVE:
    return PARTICLE_EDIT_CALC_CURVE_MOD_CURVE;
  case PARTICLE_CALC_CURVE_ADD_CURVE:
    return PARTICLE_EDIT_CALC_CURVE_ADD_CURVE;
  case PARTICLE_CALC_PARAMETRIC_EVAL:
    return PARTICLE_EDIT_CALC_PARAMETRIC_EVAL;
  case PARTICLE_CALC_PARAMETRIC_INTEGRATE:
    return PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE;
  case PARTICLE_CALC_PARAMETRIC_INTEGRATE_MINMAX:
    return PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE_MINMAX;
  default:
    return PARTICLE_EDIT_CALC_NONE;
  }
}

bool WriteParticleBool(idFile* file, const char* name, const bool& value,
                       const bool& defaultValue) {
  if (value == defaultValue)
    return false;
  idStr paddedName(name);
  paddedName.PadLength(25);
  file->WriteFloatString("\t\t%s\t%i\n", paddedName.data,
                         static_cast<unsigned>(value));
  return true;
}

bool WriteParticleVec4(idFile* file, const char* name, const idVec4& value,
                       const idVec4& defaultValue) {
  if (value.x == defaultValue.x && value.y == defaultValue.y &&
      value.z == defaultValue.z && value.w == defaultValue.w)
    return false;
  idStr paddedName(name);
  paddedName.PadLength(25);
  file->WriteFloatString("\t\t%s\t%.3f %.3f %.3f %.3f\n",
                         paddedName.data, value.x, value.y, value.z, value.w);
  return true;
}

void WriteParticleParm(idFile* file, const char* name,
                       const idParticleParm* parm,
                       const idParticleParm* defaultParm,
                       idList<const idDeclTable*, 5>& tableDecls,
                       const idStr& info, const idStr& parentInfo) {
  if (parm->tableIdx == defaultParm->tableIdx &&
      parm->table2Idx == defaultParm->table2Idx &&
      parm->val0 == defaultParm->val0 && parm->val1 == defaultParm->val1 &&
      parm->variance == defaultParm->variance &&
      parm->calcType == defaultParm->calcType &&
      info.Icmp(parentInfo.data) == 0)
    return;

  idStr paddedName(name);
  paddedName.PadLength(25);
  file->WriteFloatString("\t\t%s\t", paddedName.data);
  if (!info.IsEmpty())
    file->WriteFloatString("%s ", info.data);
  const particleEditCalcType_t editType = parm->GetEditCalcType();
  file->WriteFloatString("%s ", particleCalcTypeDesc[editType].data);
  switch (editType) {
  case PARTICLE_EDIT_CALC_CONSTANT:
    file->WriteFloatString("\"%.3f\"", parm->val0);
    if (parm->variance != 0.0f)
      file->WriteFloatString(" \"%.3f\"", parm->variance);
    file->WriteFloatString("\n");
    break;
  case PARTICLE_EDIT_CALC_MINMAX:
    file->WriteFloatString("\"%.3f\" \"%.3f\"\n", parm->val0,
                           parm->val1);
    break;
  case PARTICLE_EDIT_CALC_CURVE:
  case PARTICLE_EDIT_CALC_CURVE_SCALE_BIAS:
  case PARTICLE_EDIT_CALC_CURVE_VARIANCE_MOD_CONSTANT:
    if (parm->tableIdx < 0 || parm->tableIdx >= tableDecls.Num()) {
      idLib::Error(
          "TableIndex out of range (%d of %d) when writing entry '%s' for particle '%s'",
          parm->tableIdx, tableDecls.Num(), name, file->GetName());
      return;
    }
    if (editType == PARTICLE_EDIT_CALC_CURVE) {
      file->WriteFloatString("%s\n", tableDecls[parm->tableIdx]->name.str);
    } else if (editType == PARTICLE_EDIT_CALC_CURVE_SCALE_BIAS) {
      file->WriteFloatString("%s  \"%.3f\" \"%.3f\"\n",
                             tableDecls[parm->tableIdx]->name.str, parm->val0,
                             parm->val1);
    } else {
      file->WriteFloatString("%s  \"%.3f\" \"%.3f\"\n",
                             tableDecls[parm->tableIdx]->name.str,
                             parm->variance, parm->val0);
    }
    break;
  case PARTICLE_EDIT_CALC_CURVE_MOD_CURVE:
  case PARTICLE_EDIT_CALC_CURVE_ADD_CURVE:
    if (parm->tableIdx < 0 || parm->tableIdx >= tableDecls.Num()) {
      idLib::Error(
          "TableIndex1 out of range (%d of %d) when writing entry '%s' for particle '%s'",
          parm->tableIdx, tableDecls.Num(), name, file->GetName());
      return;
    }
    if (parm->table2Idx < 0 || parm->table2Idx >= tableDecls.Num()) {
      idLib::Error(
          "TableIndex2 out of range (%d of %d) when writing entry '%s' for particle '%s'",
          parm->table2Idx, tableDecls.Num(), name, file->GetName());
      return;
    }
    file->WriteFloatString("%s  %s\n",
                           tableDecls[parm->tableIdx]->name.str,
                           tableDecls[parm->table2Idx]->name.str);
    break;
  case PARTICLE_EDIT_CALC_PARAMETRIC_EVAL:
  case PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE:
  case PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE_MINMAX:
    WriteParametric(file, parm, name);
    break;
  default:
    break;
  }
}

void SetParticleParmAsIntegrate(idParticleParm* parm, float from, float to,
                                float variance) {
  parm->val0 = from;
  parm->val1 = to;
  parm->variance = variance;
  parm->calcType = PARTICLE_CALC_PARAMETRIC_INTEGRATE;
}

void SetParticleParmAsEval(idParticleParm* parm, float from, float to,
                           float variance) {
  parm->val0 = from;
  parm->val1 = to;
  parm->variance = variance;
  parm->calcType = PARTICLE_CALC_PARAMETRIC_EVAL;
}

void SetParticleParmAsConstant(idParticleParm* parm, float constantValue,
                               float variance) {
  parm->val0 = constantValue;
  parm->val1 = constantValue;
  parm->variance = variance;
  parm->calcType = PARTICLE_CALC_GENERIC;
}

void SetParticleParmAsMinMax(idParticleParm* parm, float minimum,
                             float maximum) {
  parm->val0 = minimum;
  parm->val1 = maximum;
  parm->calcType = PARTICLE_CALC_GENERIC;
  parm->variance = 0.0f;
}

void ParseParticleParm(idParser& source, idParticleParm* parm,
                       idList<const idDeclTable*, 5>& tableDecls,
                       idList<idLookupTable, 5>& tables) {
  parm->tableIdx = -1;
  parm->table2Idx = -1;
  parm->val0 = 0.0f;
  parm->val1 = 0.0f;
  parm->variance = 0.0f;
  idToken token;
  if (!source.ReadToken(token)) {
    source.Error("not enough parameters");
    return;
  }

  particleEditCalcType_t editType = PARTICLE_EDIT_CALC_NONE;
  for (; editType < NUM_PARTICLE_EDIT_CALC_TYPES;
       editType = static_cast<particleEditCalcType_t>(editType + 1)) {
    if (token.Icmp(particleCalcTypeDesc[editType].data) == 0)
      break;
  }
  if (editType >= NUM_PARTICLE_EDIT_CALC_TYPES) {
    if (token.Icmp("constant_use_variance") == 0)
      editType = PARTICLE_EDIT_CALC_CONSTANT;
    else {
      editType = PARTICLE_EDIT_CALC_NONE;
      source.Error("bad particle parm calculation type: %s\n", token.data);
    }
  }
  parm->SetCalcTypeFromEditType(editType);

  auto readFloat = [&source, &token](float& value) {
    if (!source.ReadToken(token))
      return false;
    value = std::strtof(token.data, nullptr);
    return true;
  };
  switch (editType) {
  case PARTICLE_EDIT_CALC_CONSTANT:
    source.ReadToken(token);
    parm->val0 = parm->val1 = std::strtof(token.data, nullptr);
    if (source.ReadTokenOnLine(token))
      parm->variance = std::strtof(token.data, nullptr);
    break;
  case PARTICLE_EDIT_CALC_MINMAX:
    if (!readFloat(parm->val0) || !readFloat(parm->val1)) {
      source.Error("particle minmax: not enough parameters");
      return;
    }
    parm->variance = 0.0f;
    break;
  case PARTICLE_EDIT_CALC_CURVE:
    if (!source.ReadToken(token)) {
      source.Error("particle curve: not enough parameters");
      return;
    }
    parm->tableIdx = static_cast<std::int16_t>(AddTable(token.data, tableDecls));
    parm->val0 = 1.0f;
    parm->val1 = 0.0f;
    break;
  case PARTICLE_EDIT_CALC_CURVE_SCALE_BIAS:
    if (!source.ReadToken(token)) {
      source.Error("particle curve modulate constant: not enough parameters");
      return;
    }
    parm->tableIdx = static_cast<std::int16_t>(AddTable(token.data, tableDecls));
    if (!readFloat(parm->val0)) {
      source.Error("particle curve scale bias: not enough parameters");
      return;
    }
    if (!readFloat(parm->val1)) {
      source.Error("particle curve modulate constant: not enough parameters");
      return;
    }
    break;
  case PARTICLE_EDIT_CALC_CURVE_VARIANCE_MOD_CONSTANT:
    if (!source.ReadToken(token)) {
      source.Error("particle curve use variance: not enough parameters");
      return;
    }
    parm->tableIdx = static_cast<std::int16_t>(AddTable(token.data, tableDecls));
    if (!readFloat(parm->variance) || !readFloat(parm->val0)) {
      source.Error("particle curve use variance: not enough parameters");
      return;
    }
    break;
  case PARTICLE_EDIT_CALC_CURVE_MOD_CURVE:
  case PARTICLE_EDIT_CALC_CURVE_ADD_CURVE:
    if (!source.ReadToken(token)) {
      source.Error("particle curve modulate/add curve: not enough parameters");
      return;
    }
    parm->tableIdx = static_cast<std::int16_t>(AddTable(token.data, tableDecls));
    if (!source.ReadToken(token)) {
      source.Error("particle curve modulate/add curve: not enough parameters");
      return;
    }
    parm->table2Idx = static_cast<std::int16_t>(AddTable(token.data, tableDecls));
    break;
  case PARTICLE_EDIT_CALC_PARAMETRIC_EVAL:
  case PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE:
  case PARTICLE_EDIT_CALC_PARAMETRIC_INTEGRATE_MINMAX:
    ParseParametric(source, parm, tableDecls, tables);
    break;
  default:
    source.Error("Particle Cacl Type %d not supported\n", parm->calcType);
    break;
  }
  for (int index = tables.Num(); index < tableDecls.Num(); ++index)
    tables.Append(tableDecls[index]->LookupTable());
}
