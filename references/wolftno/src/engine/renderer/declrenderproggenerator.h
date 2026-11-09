#pragma once

#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/declrenderprogattribs.h"

class idParser;

extern const char* programTargetNameString[PT_NUM_TARGETS];

enum programType_t : int {
  PROGRAM_TYPE_VERTEX = 0,
  PROGRAM_TYPE_FRAGMENT = 1,
};

const char* TextureTypeForTextureParm(progTarget_t target, int parmIndex);
const char* SamplerTypeForTextureParm(progTarget_t target, int parmIndex);
void GenerateHeader(const renderProgParms_t* parms, programType_t programType,
                    const idStr* globalInclude,
                    const renderProgData_t* data,
                    shaderConstructInfo_t* shaderInfo);
void GenerateCode(const renderProgParms_t* parms, renderProgData_t* data,
                  shaderConstructInfo_t* vertexInfo,
                  shaderConstructInfo_t* fragmentInfo);
bool ParseState(idParser* source, renderProgData_t* data);
bool ParseFlags(idParser* source, renderProgData_t* data);
void ValidateInOutParms360(const renderProgData_t* data, const char* name);
bool ParseShader(const renderProgParms_t* parms, programType_t programType,
                 idParser* source, const idStr* globalInclude,
                 renderProgData_t* data, shaderConstructInfo_t* shaderInfo);
idStr ConvertCG2GLSL(const idStr& input, const char* name,
                     bool isVertexProgram);
idStr StripDeadCode(const idStr& input, const char* name);

renderProgData_t TranslateRenderProg(const renderProgParms_t* parms,
                                     idParser* source,
                                     unsigned int checksum,
                                     shaderConstructInfo_t* vertexInfo,
                                     shaderConstructInfo_t* fragmentInfo);
