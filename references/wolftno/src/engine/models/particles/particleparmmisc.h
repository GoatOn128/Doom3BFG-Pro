#pragma once

#include "engine/decls/decltable.h"
#include "engine/models/particles/jobs/dustgen.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/parser.h"
#include "shared/idlib/text/str.h"

extern idStr particleCalcTypeDesc[NUM_PARTICLE_EDIT_CALC_TYPES];

bool WriteParticleBool(idFile* file, const char* name, const bool& value,
                       const bool& defaultValue);
bool WriteParticleVec4(idFile* file, const char* name, const idVec4& value,
                       const idVec4& defaultValue);
void WriteParticleParm(idFile* file, const char* name,
                       const idParticleParm* parm,
                       const idParticleParm* defaultParm,
                       idList<const idDeclTable*, 5>& tableDecls,
                       const idStr& info, const idStr& parentInfo);
void SetParticleParmAsIntegrate(idParticleParm* parm, float from, float to,
                                float variance);
void SetParticleParmAsEval(idParticleParm* parm, float from, float to,
                           float variance);
void SetParticleParmAsConstant(idParticleParm* parm, float constantValue,
                               float variance);
void SetParticleParmAsMinMax(idParticleParm* parm, float minimum,
                             float maximum);
void ParseParticleParm(idParser& source, idParticleParm* parm,
                       idList<const idDeclTable*, 5>& tableDecls,
                       idList<idLookupTable, 5>& tables);
