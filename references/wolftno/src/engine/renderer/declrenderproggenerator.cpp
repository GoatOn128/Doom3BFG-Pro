#include "engine/renderer/declrenderproggenerator.h"

#include "engine/framework/coderesource.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/parmblockparser.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/lexer.h"
#include "shared/idlib/text/parser.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

struct typeConversion_t {
  const char* typeCG;
  const char* typeGLSL;
};

struct builtinConversion_t {
  const char* nameCG;
  const char* nameGLSL;
};

struct progValidation_t {
  idStr type;
  idStr name;
  idStr semantic;
};

struct idCGBlock {
  idStr prefix;
  idStr name;
  idStr postfix;
  bool used = false;
};

struct inOutVariable_t {
  idStr type;
  idStr nameCg;
  idStr nameGLSL;
  bool declareInOut = false;
};

static_assert(sizeof(progValidation_t) == 120,
              "program-validation record ABI drift");
static_assert(sizeof(idCGBlock) == 128, "CG block ABI drift");
static_assert(sizeof(inOutVariable_t) == 128,
              "in/out-variable ABI drift");

constexpr typeConversion_t typeConversion[] = {
    {"void", "void"},
    {"fixed", "float"},
    {"float", "float"},
    {"float2", "vec2"},
    {"float3", "vec3"},
    {"float4", "vec4"},
    {"half", "float"},
    {"half2", "vec2"},
    {"half3", "vec3"},
    {"half4", "vec4"},
    {"int", "int"},
    {"int2", "ivec2"},
    {"int3", "ivec3"},
    {"int4", "ivec4"},
    {"bool", "bool"},
    {"bool2", "bvec2"},
    {"bool3", "bvec3"},
    {"bool4", "bvec4"},
    {"float2x2", "mat2x2"},
    {"float2x3", "mat2x3"},
    {"float2x4", "mat2x4"},
    {"float3x2", "mat3x2"},
    {"float3x3", "mat3x3"},
    {"float3x4", "mat3x4"},
    {"float4x2", "mat4x2"},
    {"float4x3", "mat4x3"},
    {"float4x4", "mat4x4"},
    {"sampler1D", "sampler1D"},
    {"sampler2D", "sampler2D"},
    {"sampler3D", "sampler3D"},
    {"samplerCUBE", "samplerCube"},
    {"sampler1DShadow", "sampler1DShadow"},
    {"sampler2DShadow", "sampler2DShadow"},
    {"sampler3DShadow", "sampler3DShadow"},
    {"samplerCubeShadow", "samplerCubeShadow"},
    {"sampler2DMS", "sampler2DMS"},
    {nullptr, nullptr},
};

constexpr builtinConversion_t builtinConversion[] = {
    {"frac", "fract"},       {"lerp", "mix"},
    {"rsqrt", "inversesqrt"}, {"ddx", "dFdx"},
    {"ddy", "dFdy"},        {nullptr, nullptr},
};

constexpr const char* types[] = {"int",  "float", "half",   "fixed",
                                 "bool", "cint",  "cfloat", "void"};
constexpr const char* typePosts[] = {
    "1",   "2",   "3",   "4",   "1x1", "1x2", "1x3",
    "1x4", "2x1", "2x2", "2x3", "2x4", "3x1", "3x2",
    "3x3", "3x4", "4x1", "4x2", "4x3", "4x4"};
constexpr const char* prefixes[] = {
    "static",          "const",           "uniform",
    "struct",          "sampler",         "sampler1D",
    "sampler2D",       "sampler3D",       "samplerCUBE",
    "sampler1DShadow", "sampler2DShadow", "sampler3DShadow",
    "samplerCubeShadow", "sampler2DMS"};

constexpr const char* vertexInsert =
    "#version 150\n\n"
    "float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }\n\n";

constexpr const char* fragmentInsert =
    "#version 150\n\n"
    "void clip( float v ) { if ( v < 0.0 ) { discard; } }\n"
    "void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }\n"
    "void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }\n"
    "void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }\n\n"
    "float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }\n"
    "vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }\n\n"
    "vec4 tex2D( sampler2D sampler, vec2 texcoord ) { return texture( sampler, texcoord.xy ); }\n"
    "vec4 tex2D( sampler2DShadow sampler, vec3 texcoord ) { return vec4( texture( sampler, texcoord.xyz ) ); }\n\n"
    "vec4 tex2D( sampler2D sampler, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( sampler, texcoord.xy, dx, dy ); }\n"
    "vec4 tex2D( sampler2DShadow sampler, vec3 texcoord, vec2 dx, vec2 dy ) { return vec4( textureGrad( sampler, texcoord.xyz, dx, dy ) ); }\n\n"
    "vec4 texCUBE( samplerCube sampler, vec3 texcoord ) { return texture( sampler, texcoord.xyz ); }\n"
    "vec4 texCUBE( samplerCubeShadow sampler, vec4 texcoord ) { return vec4( texture( sampler, texcoord.xyzw ) ); }\n\n"
    "vec4 tex1Dproj( sampler1D sampler, vec2 texcoord ) { return textureProj( sampler, texcoord ); }\n"
    "vec4 tex2Dproj( sampler2D sampler, vec3 texcoord ) { return textureProj( sampler, texcoord ); }\n"
    "vec4 tex3Dproj( sampler3D sampler, vec4 texcoord ) { return textureProj( sampler, texcoord ); }\n\n"
    "vec4 tex1Dbias( sampler1D sampler, vec4 texcoord ) { return texture( sampler, texcoord.x, texcoord.w ); }\n"
    "vec4 tex2Dbias( sampler2D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xy, texcoord.w ); }\n"
    "vec4 tex3Dbias( sampler3D sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }\n"
    "vec4 texCUBEbias( samplerCube sampler, vec4 texcoord ) { return texture( sampler, texcoord.xyz, texcoord.w ); }\n\n"
    "vec4 tex1Dlod( sampler1D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.x, texcoord.w ); }\n"
    "vec4 tex2Dlod( sampler2D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xy, texcoord.w ); }\n"
    "vec4 tex3Dlod( sampler3D sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }\n"
    "vec4 texCUBElod( samplerCube sampler, vec4 texcoord ) { return textureLod( sampler, texcoord.xyz, texcoord.w ); }\n\n";

void AppendFormat(idStr& destination, const char* format, ...) {
  idStr formatted;
  std::va_list arguments;
  va_start(arguments, format);
  formatted.VFormat(format, arguments);
  va_end(arguments);
  destination.Append(formatted);
}

bool HasWhiteSpace(const idToken& token) {
  return token.whiteSpaceStart_p != nullptr &&
         token.whiteSpaceEnd_p != nullptr &&
         token.whiteSpaceEnd_p > token.whiteSpaceStart_p;
}

const char* ParmName(int parmIndex) {
  if (renderThreadParmState == nullptr)
    return "<badParmIndex>";
  const char* name = renderThreadParmState->GetParmNameForIndex(parmIndex);
  return name != nullptr ? name : "<badParmIndex>";
}

const idDeclRenderParm* ParmForIndex(int parmIndex) {
  return renderThreadParmState != nullptr
             ? renderThreadParmState->GetRenderParmForIndex(parmIndex)
             : nullptr;
}

attribInfo_t* AttribTableForTarget(progTarget_t target) {
  switch (target) {
    case PT_PC:
      return attribsPC;
    case PT_PC_D3D:
    case PT_DURANGO:
      return attribsPCD3D;
    case PT_360:
      return attribs360;
    case PT_PS3:
      return attribsPS3;
    case PT_ORBIS:
      return attribsOrbis;
    default:
      idLib::FatalError("ParseShader: bad target");
  }
}

void CheckForAttribSemanticCollision(
    const idStaticList<attribInfo_t, 16>& attributes, idParser* source) {
  if (attributes.num < 2)
    return;
  const attribInfo_t& added = attributes[attributes.num - 1];
  for (int index = 0; index < attributes.num - 1; ++index) {
    if (idStr::Icmp(attributes[index].semantic, added.semantic) == 0) {
      source->Error("Semantic collision \"%s\" and \"%s\" both resolve to semantic \"%s\"",
                    attributes[index].name, added.name, added.semantic);
      return;
    }
  }
}

template <typename T>
int FindIndex(const idList<T>& values, const T& value) {
  for (int index = 0; index < values.num; ++index) {
    if (values[index] == value)
      return index;
  }
  return -1;
}

int FindParmIndex(const idList<short>& values, int parmIndex) {
  for (int index = 0; index < values.num; ++index) {
    if (values[index] == parmIndex)
      return index;
  }
  return -1;
}

void AppendTextureDeclarations(const renderProgParms_t* parms,
                               const renderProgData_t* data,
                               idStr& header) {
  for (int slot = 0; slot < data->textureParms.num; ++slot) {
    const int parmIndex = data->textureParms[slot];
    const idDeclRenderParm* parm = ParmForIndex(parmIndex);
    const bool separateSampler =
        parms->target == PT_PC_D3D || parms->target == PT_DURANGO ||
        parms->target == PT_ORBIS;
    const bool image = parm != nullptr &&
                       (parm->parmType == PT_IMAGE_2D_READ ||
                        parm->parmType == PT_IMAGE_2D_WRITE);
    if (separateSampler && !image) {
      AppendFormat(header, "%s samp_%s : register(t%i);\n",
                   TextureTypeForTextureParm(parms->target, parmIndex),
                   ParmName(parmIndex), slot);
      AppendFormat(header, "%s samp_state_%s : register(s%i);\n",
                   SamplerTypeForTextureParm(parms->target, parmIndex),
                   ParmName(parmIndex), slot);
    } else if (separateSampler) {
      AppendFormat(header, "%s samp_%s : register(u%i);\n",
                   TextureTypeForTextureParm(parms->target, parmIndex),
                   ParmName(parmIndex), slot);
    } else {
      AppendFormat(header, "uniform %s samp_%s : register(s%i);\n",
                   TextureTypeForTextureParm(parms->target, parmIndex),
                   ParmName(parmIndex), slot);
    }
  }
}

void AppendStructMember(idStr& code, const attribInfo_t& attribute) {
  AppendFormat(code, "\t%s %s : %s;\n", attribute.type, attribute.name,
               attribute.semantic);
}

void NormalizeStateToken(idToken& token) {
  token.ToUpper();
  token.StripLeading("GL_");
}

bool ReadStateToken(idParser* source, idToken& token) {
  if (source->ReadToken(token))
    return true;
  source->Warning("EOF in program.");
  return false;
}

void ParseInOutStruct(idLexer* source, int attribType,
                      idList<inOutVariable_t>* variables) {
  source->ExpectTokenString("{");
  while (!source->CheckTokenString("}")) {
    inOutVariable_t variable;
    idToken token;
    if (!source->ReadToken(token))
      break;
    variable.type = token;
    if (!source->ReadToken(token))
      break;
    variable.nameCg = token;
    if (!source->CheckTokenString(":")) {
      source->SkipUntilString(";");
      continue;
    }
    source->ReadToken(token);
    variable.nameGLSL = token;
    source->ExpectTokenString(";");

    for (int index = 0; typeConversion[index].typeCG != nullptr; ++index) {
      if (variable.type.Cmp(typeConversion[index].typeCG) == 0) {
        variable.type = typeConversion[index].typeGLSL;
        break;
      }
    }

    bool isBuiltIn = false;
    for (int index = 0; attribsPC[index].semantic != nullptr; ++index) {
      if ((attribsPC[index].flags & attribType) != 0 &&
          variable.nameGLSL.Cmp(attribsPC[index].semantic) == 0) {
        variable.nameGLSL = attribsPC[index].glsl;
        isBuiltIn = (attribsPC[index].flags & AT_PS_INOUT) != 0;
        break;
      }
    }

    variable.declareInOut = !isBuiltIn;
    for (int index = 0; index < variables->num; ++index) {
      if (variable.nameGLSL == (*variables)[index].nameGLSL) {
        variable.declareInOut = false;
        break;
      }
    }
    variables->Append(variable);
  }
  source->ExpectTokenString(";");
}

}  // namespace

#define NON_PC_INTERPOLANTS(HALF_TYPE)                                         \
  {"float4", "color", "COLOR", "", 0, AT_VS_OUT | AT_PS_IN, 0},          \
      {"float4", "color0", "COLOR0", "", 0, AT_VS_OUT | AT_PS_IN, 0},    \
      {"float4", "color1", "COLOR1", "", 0, AT_VS_OUT | AT_PS_IN, 0},    \
      {HALF_TYPE, "hcolor", "COLOR", "", 0, AT_PS_IN, 0},                  \
      {HALF_TYPE, "hcolor0", "COLOR0", "", 0, AT_PS_IN, 0},               \
      {HALF_TYPE, "hcolor1", "COLOR1", "", 0, AT_PS_IN, 0},               \
      {"float4", "texcoord0", "TEXCOORD0", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord1", "TEXCOORD1", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord2", "TEXCOORD2", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord3", "TEXCOORD3", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord4", "TEXCOORD4", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord5", "TEXCOORD5", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord6", "TEXCOORD6", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord7", "TEXCOORD7", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord8", "TEXCOORD8", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {"float4", "texcoord9", "TEXCOORD9", "", 0, AT_VS_OUT | AT_PS_IN, 0}, \
      {HALF_TYPE, "htexcoord0", "TEXCOORD0", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord1", "TEXCOORD1", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord2", "TEXCOORD2", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord3", "TEXCOORD3", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord4", "TEXCOORD4", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord5", "TEXCOORD5", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord6", "TEXCOORD6", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord7", "TEXCOORD7", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord8", "TEXCOORD8", "", 0, AT_PS_IN, 0},          \
      {HALF_TYPE, "htexcoord9", "TEXCOORD9", "", 0, AT_PS_IN, 0},          \
      {"float", "fog", "FOG", "", 0, AT_VS_OUT, 0},                     \
      {"float4", "fog", "FOG", "", 0, AT_PS_IN, 0}

attribInfo_t attribsPC[] = {
    {"float4", "position", "POSITION", "in_Position", 0, AT_VS_IN, 1},
    {"float4", "position1", "POSITION1", "in_Position1", 5, AT_VS_IN, 4096},
    {"float2", "texcoord", "TEXCOORD0", "in_TexCoord", 1, AT_VS_IN, 2},
    {"float2", "texcoord0", "TEXCOORD0", "in_TexCoord", 1, AT_VS_IN, 2},
    {"float2", "texcoord1", "TEXCOORD1", "in_TexCoord1", 4, AT_VS_IN, 1024},
    {"float4", "normal", "ATTR2", "in_Normal", 2, AT_VS_IN, 4},
    {"float4", "tangent", "ATTR9", "in_Tangent", 9, AT_VS_IN, 16},
    {"float4", "color", "COLOR", "in_Color", 3, AT_VS_IN, 8},
    {"float4", "morph", "ATTR10", "in_Morph", 10, AT_VS_IN, 128},
    {"float4", "position", "POSITION", "gl_Position", 0, AT_VS_OUT | AT_PS_INOUT, 0},
    {"float", "clip0", "CLP0", "gl_ClipDistance[0]", 0, AT_VS_OUT, 0},
    {"float", "clip1", "CLP1", "gl_ClipDistance[1]", 0, AT_VS_OUT, 0},
    {"float", "clip2", "CLP2", "gl_ClipDistance[2]", 0, AT_VS_OUT, 0},
    {"float", "clip3", "CLP3", "gl_ClipDistance[3]", 0, AT_VS_OUT, 0},
    {"float", "clip4", "CLP4", "gl_ClipDistance[4]", 0, AT_VS_OUT, 0},
    {"float", "clip5", "CLP5", "gl_ClipDistance[5]", 0, AT_VS_OUT, 0},
    {"float4", "position", "WPOS", "gl_FragCoord", 0, AT_PS_IN, 0},
    {"half4", "hposition", "WPOS", "gl_FragCoord", 0, AT_PS_IN, 0},
    {"bool", "facing", "FACE", "gl_FrontFacing", 0, AT_PS_IN | AT_PS_INOUT, 0},
    {"float4", "color", "COLOR", "out_FragColor0", 0, AT_PS_OUT, 0},
    {"half4", "hcolor", "COLOR", "out_FragColor0", 0, AT_PS_OUT, 0},
    {"float4", "color0", "COLOR0", "out_FragColor0", 0, AT_PS_OUT, 0},
    {"float4", "color1", "COLOR1", "out_FragColor1", 1, AT_PS_OUT, 0},
    {"float4", "color2", "COLOR2", "out_FragColor2", 2, AT_PS_OUT, 0},
    {"float4", "color3", "COLOR3", "out_FragColor3", 3, AT_PS_OUT, 0},
    {"float", "depth", "DEPTH", "out_FragDepth", 4, AT_PS_OUT, 0},
    {"float4", "color", "COLOR", "vofi_Color", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "color0", "COLOR0", "vofi_Color", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "color1", "COLOR1", "vofi_Color", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"half4", "hcolor", "COLOR", "vofi_Color", 0, AT_PS_IN, 0},
    {"half4", "hcolor0", "COLOR0", "vofi_Color", 0, AT_PS_IN, 0},
    {"half4", "hcolor1", "COLOR1", "vofi_Color", 0, AT_PS_IN, 0},
    {"float4", "texcoord0", "TEXCOORD0", "vofi_TexCoord0", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord1", "TEXCOORD1", "vofi_TexCoord1", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord2", "TEXCOORD2", "vofi_TexCoord2", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord3", "TEXCOORD3", "vofi_TexCoord3", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord4", "TEXCOORD4", "vofi_TexCoord4", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord5", "TEXCOORD5", "vofi_TexCoord5", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord6", "TEXCOORD6", "vofi_TexCoord6", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord7", "TEXCOORD7", "vofi_TexCoord7", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord8", "TEXCOORD8", "vofi_TexCoord8", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"float4", "texcoord9", "TEXCOORD9", "vofi_TexCoord9", 0, AT_VS_OUT | AT_PS_IN, 0},
    {"half4", "htexcoord0", "TEXCOORD0", "vofi_TexCoord0", 0, AT_PS_IN, 0},
    {"half4", "htexcoord1", "TEXCOORD1", "vofi_TexCoord1", 0, AT_PS_IN, 0},
    {"half4", "htexcoord2", "TEXCOORD2", "vofi_TexCoord2", 0, AT_PS_IN, 0},
    {"half4", "htexcoord3", "TEXCOORD3", "vofi_TexCoord3", 0, AT_PS_IN, 0},
    {"half4", "htexcoord4", "TEXCOORD4", "vofi_TexCoord4", 0, AT_PS_IN, 0},
    {"half4", "htexcoord5", "TEXCOORD5", "vofi_TexCoord5", 0, AT_PS_IN, 0},
    {"half4", "htexcoord6", "TEXCOORD6", "vofi_TexCoord6", 0, AT_PS_IN, 0},
    {"half4", "htexcoord7", "TEXCOORD7", "vofi_TexCoord7", 0, AT_PS_IN, 0},
    {"half4", "htexcoord8", "TEXCOORD8", "vofi_TexCoord8", 0, AT_PS_IN, 0},
    {"half4", "htexcoord9", "TEXCOORD9", "vofi_TexCoord9", 0, AT_PS_IN, 0},
    {"float", "fog", "FOG", "vofi_Fog", 0, AT_VS_OUT, 0},
    {"float4", "fog", "FOG", "vofi_Fog", 0, AT_PS_IN, 0},
    {nullptr, nullptr, nullptr, nullptr, 0, 0, 0},
};

attribInfo_t attribsPCD3D[] = {
    {"float4", "position", "POSITION", "", 0, AT_VS_IN, 1},
    {"float2", "texcoord", "TEXCOORD0", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord0", "TEXCOORD0", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord1", "TEXCOORD1", "", 0, AT_VS_IN, 1024},
    {"float4", "normal", "NORMAL", "", 0, AT_VS_IN, 4},
    {"float4", "color", "COLOR0", "", 0, AT_VS_IN, 8},
    {"float4", "tangent", "TANGENT", "", 0, AT_VS_IN, 16},
    {"float4", "morph", "COLOR1", "", 0, AT_VS_IN, 128},
    {"int", "index", "INDEX", "", 0, AT_VS_IN, 0},
    {"float4", "position", "SV_Position", "", 0, AT_VS_OUT, 0},
    {"float4", "position", "SV_Position", "", 0, AT_PS_IN, 0},
    {"float4", "hposition", "SV_Position", "", 0, AT_PS_IN, 0},
    {"bool", "facing", "SV_IsFrontFace", "", 0, AT_PS_IN | AT_PS_INOUT, 0},
    {"float4", "color", "SV_Target", "", 0, AT_PS_OUT, 0},
    {"half4", "hcolor", "SV_Target", "", 0, AT_PS_OUT, 0},
    {"float4", "color0", "SV_Target0", "", 0, AT_PS_OUT, 0},
    {"float4", "color1", "SV_Target1", "", 0, AT_PS_OUT, 0},
    {"float4", "color2", "SV_Target2", "", 0, AT_PS_OUT, 0},
    {"float4", "color3", "SV_Target3", "", 0, AT_PS_OUT, 0},
    {"float", "depth", "SV_Depth", "", 0, AT_PS_OUT, 0},
    NON_PC_INTERPOLANTS("half4"),
    {nullptr, nullptr, nullptr, nullptr, 0, 0, 0},
};

attribInfo_t attribs360[] = {
    {"float4", "position", "POSITION", "", 0, AT_VS_IN, 1},
    {"float4", "position1", "POSITION1", "", 0, AT_VS_IN, 4096},
    {"float2", "texcoord", "TEXCOORD0", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord0", "TEXCOORD0", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord1", "TEXCOORD1", "", 0, AT_VS_IN, 1024},
    {"float4", "normal", "NORMAL", "", 0, AT_VS_IN, 4},
    {"float4", "color", "COLOR0", "", 0, AT_VS_IN, 8},
    {"float4", "tangent", "TANGENT", "", 0, AT_VS_IN, 16},
    {"float4", "morph", "COLOR1", "", 0, AT_VS_IN, 128},
    {"int", "index", "INDEX", "", 0, AT_VS_IN, 0},
    {"float4", "position", "POSITION", "", 0, AT_VS_OUT, 0},
    {"float4", "position", "VPOS", "", 0, AT_PS_IN, 0},
    {"float4", "hposition", "VPOS", "", 0, AT_PS_IN, 0},
    {"float", "facing", "VFACE", "", 0, AT_PS_IN, 0},
    {"float4", "color", "COLOR", "", 0, AT_PS_OUT, 0},
    {"half4", "hcolor", "COLOR", "", 0, AT_PS_OUT, 0},
    {"float4", "color0", "COLOR0", "", 0, AT_PS_OUT, 0},
    {"float4", "color1", "COLOR1", "", 0, AT_PS_OUT, 0},
    {"float4", "color2", "COLOR2", "", 0, AT_PS_OUT, 0},
    {"float4", "color3", "COLOR3", "", 0, AT_PS_OUT, 0},
    {"float", "depth", "DEPTH", "", 0, AT_PS_OUT, 0},
    NON_PC_INTERPOLANTS("half4"),
    {nullptr, nullptr, nullptr, nullptr, 0, 0, 0},
};

attribInfo_t attribsPS3[] = {
    {"float4", "position", "ATTR0", "", 0, AT_VS_IN, 1},
    {"float4", "position1", "ATTR12", "", 0, AT_VS_IN, 4096},
    {"float2", "texcoord", "ATTR1", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord0", "ATTR1", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord1", "ATTR10", "", 0, AT_VS_IN, 1024},
    {"float4", "normal", "ATTR2", "", 0, AT_VS_IN, 4},
    {"float4", "color", "ATTR3", "", 0, AT_VS_IN, 8},
    {"float4", "tangent", "ATTR4", "", 0, AT_VS_IN, 16},
    {"float4", "morph", "ATTR7", "", 0, AT_VS_IN, 128},
    {"float4", "position", "POSITION", "", 0, AT_VS_OUT, 0},
    {"float4", "position", "WPOS", "", 0, AT_PS_IN, 0},
    {"half4", "hposition", "WPOS", "", 0, AT_PS_IN, 0},
    {"float", "facing", "FACE", "", 0, AT_PS_IN, 0},
    {"float4", "color", "COLOR", "", 0, AT_PS_OUT, 0},
    {"half4", "hcolor", "COLOR", "", 0, AT_PS_OUT, 0},
    {"float4", "color0", "COLOR0", "", 0, AT_PS_OUT, 0},
    {"float4", "color1", "COLOR1", "", 0, AT_PS_OUT, 0},
    {"float4", "color2", "COLOR2", "", 0, AT_PS_OUT, 0},
    {"float4", "color3", "COLOR3", "", 0, AT_PS_OUT, 0},
    {"float", "depth", "DEPTH", "", 0, AT_PS_OUT, 0},
    NON_PC_INTERPOLANTS("half4"),
    {nullptr, nullptr, nullptr, nullptr, 0, 0, 0},
};

attribInfo_t attribsOrbis[] = {
    {"float4", "position", "POSITION", "", 0, AT_VS_IN, 1},
    {"float2", "texcoord", "TEXCOORD0", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord0", "TEXCOORD0", "", 0, AT_VS_IN, 2},
    {"float2", "texcoord1", "TEXCOORD1", "", 0, AT_VS_IN, 1024},
    {"float4", "normal", "NORMAL", "", 0, AT_VS_IN, 4},
    {"float4", "color", "COLOR0", "", 0, AT_VS_IN, 8},
    {"float4", "tangent", "TANGENT", "", 0, AT_VS_IN, 16},
    {"float4", "morph", "COLOR1", "", 0, AT_VS_IN, 128},
    {"int", "index", "INDEX", "", 0, AT_VS_IN, 0},
    {"float4", "position", "S_POSITION", "", 0, AT_VS_OUT, 0},
    {"float4", "position", "S_POSITION", "", 0, AT_PS_IN, 0},
    {"float4", "hposition", "S_POSITION", "", 0, AT_PS_IN, 0},
    {"bool", "facing", "S_FRONT_FACE", "", 0, AT_PS_IN, 0},
    {"float4", "color", "S_TARGET_OUTPUT", "", 0, AT_PS_OUT, 0},
    {"float4", "hcolor", "S_TARGET_OUTPUT", "", 0, AT_PS_OUT, 0},
    {"float4", "color0", "S_TARGET_OUTPUT0", "", 0, AT_PS_OUT, 0},
    {"float4", "color1", "S_TARGET_OUTPUT1", "", 0, AT_PS_OUT, 0},
    {"float4", "color2", "S_TARGET_OUTPUT2", "", 0, AT_PS_OUT, 0},
    {"float4", "color3", "S_TARGET_OUTPUT3", "", 0, AT_PS_OUT, 0},
    {"float", "depth", "S_DEPTH_OUTPUT", "", 0, AT_PS_OUT, 0},
    NON_PC_INTERPOLANTS("float4"),
    {nullptr, nullptr, nullptr, nullptr, 0, 0, 0},
};

#undef NON_PC_INTERPOLANTS

const char* TextureTypeForTextureParm(progTarget_t target, int parmIndex) {
  const idDeclRenderParm* parm = ParmForIndex(parmIndex);
  if (parm == nullptr)
    return "<badParmIndex>";

  if (target == PT_PC_D3D || target == PT_DURANGO) {
    switch (parm->parmType) {
      case PT_TEXTURE_2D:
      case PT_TEXTURE_SHADOW_2D:
      case PT_TEXTURE_DIM_SHADOW_2D:
      case PT_TEXTURE_MULTISAMPLE_2D:
        return "Texture2D";
      case PT_TEXTURE_3D:
      case PT_TEXTURE_SHADOW_3D:
        return "Texture3D";
      case PT_TEXTURE_CUBE:
      case PT_TEXTURE_SHADOW_CUBE:
        return "TextureCube";
      case PT_IMAGE_2D_READ:
        return "RWTexture2D<float>";
      case PT_IMAGE_2D_WRITE:
        return "RWTexture2D<float4>";
      default:
        return "<badParmType>";
    }
  }

  if (target == PT_ORBIS) {
    switch (parm->parmType) {
      case PT_TEXTURE_2D:
      case PT_TEXTURE_SHADOW_2D:
      case PT_TEXTURE_DIM_SHADOW_2D:
      case PT_IMAGE_2D_READ:
      case PT_IMAGE_2D_WRITE:
        return "Texture2D";
      case PT_TEXTURE_3D:
      case PT_TEXTURE_SHADOW_3D:
        return "Texture3D";
      case PT_TEXTURE_CUBE:
      case PT_TEXTURE_SHADOW_CUBE:
        return "TextureCube";
      case PT_TEXTURE_MULTISAMPLE_2D:
        return "MS_Texture2D<float4>";
      default:
        return "<badParmType>";
    }
  }

  if (target == PT_PC) {
    switch (parm->parmType) {
      case PT_TEXTURE_2D:
        return "sampler2D";
      case PT_TEXTURE_3D:
        return "sampler3D";
      case PT_TEXTURE_CUBE:
        return "samplerCUBE";
      case PT_TEXTURE_SHADOW_2D:
      case PT_TEXTURE_DIM_SHADOW_2D:
        return "sampler2DShadow";
      case PT_TEXTURE_SHADOW_3D:
        return "sampler3DShadow";
      case PT_TEXTURE_SHADOW_CUBE:
        return "samplerCubeShadow";
      case PT_TEXTURE_MULTISAMPLE_2D:
        return "sampler2DMS";
      case PT_IMAGE_2D_READ:
      case PT_IMAGE_2D_WRITE:
        return "image2D";
      default:
        return "<badParmType>";
    }
  }

  switch (parm->parmType) {
    case PT_TEXTURE_3D:
    case PT_TEXTURE_SHADOW_3D:
      return "sampler3D";
    case PT_TEXTURE_CUBE:
    case PT_TEXTURE_SHADOW_CUBE:
      return "samplerCUBE";
    case PT_TEXTURE_2D:
    case PT_TEXTURE_SHADOW_2D:
    case PT_TEXTURE_DIM_SHADOW_2D:
    case PT_TEXTURE_MULTISAMPLE_2D:
    case PT_IMAGE_2D_READ:
    case PT_IMAGE_2D_WRITE:
      return "sampler2D";
    default:
      return "<badParmType>";
  }
}

const char* SamplerTypeForTextureParm(progTarget_t target, int parmIndex) {
  const idDeclRenderParm* parm = ParmForIndex(parmIndex);
  if (parm == nullptr)
    return "<badParmIndex>";
  if (target != PT_ORBIS && target != PT_DURANGO && target != PT_PC_D3D)
    return "SamplerState";
  switch (parm->parmType) {
    case PT_TEXTURE_SHADOW_2D:
    case PT_TEXTURE_SHADOW_3D:
    case PT_TEXTURE_SHADOW_CUBE:
    case PT_TEXTURE_DIM_SHADOW_2D:
      return "SamplerComparisonState";
    case PT_TEXTURE_2D:
    case PT_TEXTURE_3D:
    case PT_TEXTURE_CUBE:
    case PT_TEXTURE_MULTISAMPLE_2D:
    case PT_IMAGE_2D_READ:
    case PT_IMAGE_2D_WRITE:
      return "SamplerState";
    default:
      return "<badParmType>";
  }
}

void GenerateHeader(const renderProgParms_t* parms, programType_t programType,
                    const idStr* globalInclude,
                    const renderProgData_t* data,
                    shaderConstructInfo_t* shaderInfo) {
  idStr& header = shaderInfo->headerString;
  header.Clear();
  if (programType == PROGRAM_TYPE_FRAGMENT && (data->flags & 5) != 0 &&
      parms->target == PT_DURANGO) {
    header.Append("#define __XBOX_PS_USE_RE_Z 1\n");
  }
  header += *globalInclude;

  if (programType == PROGRAM_TYPE_VERTEX) {
    if (parms->target == PT_PC) {
      if (parms->useUniformArrays) {
        AppendFormat(header, "uniform float4 _va_[%d];\n",
                     data->vertexParms.num);
        header.Append("uniform float4 skinOffsets;\n");
        if (data->hasHardwareSkinning) {
          header.Append("uniform float4 hardwareSkinning;\n");
          header.Append(
              "uniform matrices_ubo { float4 matrices[768]; };\n");
        }
      } else {
        for (int index = 0; index < data->vertexParms.num; ++index) {
          AppendFormat(header,
                       "uniform float4 rp_name_%s : register(c%i);\n",
                       ParmName(data->vertexParms[index]), index);
        }
        AppendFormat(header, "uniform float4 skinOffsets : register(c%i);\n",
                     data->vertexParms.num);
        if (data->hasHardwareSkinning) {
          AppendFormat(
              header,
              "uniform float4 hardwareSkinning : register(c%i);\n",
              data->vertexParms.num + 1);
          AppendFormat(header,
                       "uniform float4 matrices[768] : register(c%i);\n",
                       data->vertexParms.num + 2);
        }
      }
    } else if (parms->target == PT_PC_D3D || parms->target == PT_DURANGO ||
               parms->target == PT_ORBIS) {
      const char* keyword =
          parms->target == PT_ORBIS ? "ConstantBuffer" : "cbuffer";
      AppendFormat(header, "%s constantBufferV : register(c0) {\n", keyword);
      for (int index = 0; index < data->vertexParms.num; ++index) {
        AppendFormat(header, "\tfloat4 rp_name_%s;\n",
                     ParmName(data->vertexParms[index]));
      }
      header.Append("};\n");
      AppendFormat(header, "%s skinningBufferV : register(c1) {\n", keyword);
      header.Append("\tfloat4 skinOffsets;\n");
      header.Append("\tfloat4 hardwareSkinning;\n");
      header.Append("};\n");
      if (data->hasHardwareSkinning) {
        AppendFormat(header, "%s jointBufferV : register(c2) {\n", keyword);
        AppendFormat(header, "\tfloat4 matrices[%d];\n",
                     parms->target == PT_DURANGO ? 255 : 768);
        header.Append("};\n");
      }
    } else if (parms->target == PT_PS3 || parms->target == PT_360) {
      for (int index = 0; index < data->vertexParms.num; ++index) {
        AppendFormat(header,
                     "uniform float4 rp_name_%s : register(c%i);\n",
                     ParmName(data->vertexParms[index]), index);
      }
      AppendFormat(header, "uniform float4 skinOffsets : register(c%i);\n",
                   data->vertexParms.num);
      if (parms->target == PT_PS3 && data->hasHardwareSkinning)
        header.Append("uniform float4 matrices[420];\n");
    }
    if (data->hasVertexTexture)
      AppendTextureDeclarations(parms, data, header);
    return;
  }

  if (parms->target == PT_PC) {
    if (parms->useUniformArrays) {
      AppendFormat(header, "uniform float4 _fa_[%d];\n",
                   data->fragmentParms.num);
    } else {
      for (int index = 0; index < data->fragmentParms.num; ++index) {
        AppendFormat(header,
                     "uniform float4 rp_name_%s : register(c%i);\n",
                     ParmName(data->fragmentParms[index]), index);
      }
    }
  } else if (parms->target == PT_PC_D3D || parms->target == PT_DURANGO ||
             parms->target == PT_ORBIS) {
    const char* keyword =
        parms->target == PT_ORBIS ? "ConstantBuffer" : "cbuffer";
    AppendFormat(header, "%s constantBufferP : register(c0) {\n", keyword);
    for (int index = 0; index < data->fragmentParms.num; ++index) {
      AppendFormat(header, "\tfloat4 rp_name_%s;\n",
                   ParmName(data->fragmentParms[index]));
    }
    header.Append("};\n");
  } else if (parms->target == PT_PS3 || parms->target == PT_360) {
    for (int index = 0; index < data->fragmentParms.num; ++index) {
      AppendFormat(header,
                   "uniform float4 rp_name_%s : register(c%i);\n",
                   ParmName(data->fragmentParms[index]), index);
    }
  }
  AppendTextureDeclarations(parms, data, header);
}

void GenerateCode(const renderProgParms_t* parms, renderProgData_t* data,
                  shaderConstructInfo_t* vertexInfo,
                  shaderConstructInfo_t* fragmentInfo) {
  if (vertexInfo->parsed) {
    idStr& code = data->vertexCode;
    code = vertexInfo->headerString;
    code.Append("struct VS_IN {\n");
    for (int index = 0; index < vertexInfo->inAttribs.num; ++index)
      AppendStructMember(code, vertexInfo->inAttribs[index]);
    if (parms->target == PT_PS3 && vertexInfo->inAttribs.num == 0)
      code.Append("\tfloat4 foo;\n");
    code.Append("};\nstruct VS_OUT {\n");

    idList<int> used;
    for (int fragmentIndex = 0;
         fragmentIndex < fragmentInfo->inAttribs.num; ++fragmentIndex) {
      for (int vertexIndex = 0;
           vertexIndex < vertexInfo->outAttribs.num; ++vertexIndex) {
        if (idStr::Icmp(fragmentInfo->inAttribs[fragmentIndex].semantic,
                        vertexInfo->outAttribs[vertexIndex].semantic) == 0) {
          AppendStructMember(code, vertexInfo->outAttribs[vertexIndex]);
          used.Append(vertexIndex);
          break;
        }
      }
    }
    for (int vertexIndex = 0; vertexIndex < vertexInfo->outAttribs.num;
         ++vertexIndex) {
      if (FindIndex(used, vertexIndex) == -1)
        AppendStructMember(code, vertexInfo->outAttribs[vertexIndex]);
    }
    if (parms->target == PT_PS3 && vertexInfo->outAttribs.num == 0)
      code.Append("\tfloat4 foo;\n");
    code.Append("};\nvoid main( VS_IN vertex, out VS_OUT result ) {\n");
    if (parms->target == PT_360)
      code += vertexInfo->outputAttribInitString;
    code.Append(vertexInfo->sourceString);
    code.Append("\n}\n");
  }

  if (fragmentInfo->parsed) {
    idStr& code = data->fragmentCode;
    code = fragmentInfo->headerString;
    code.Append("struct PS_IN {\n");
    bool delayedSystemInputs = false;
    for (int index = 0; index < fragmentInfo->inAttribs.num; ++index) {
      const attribInfo_t& attribute = fragmentInfo->inAttribs[index];
      if ((parms->target == PT_PC_D3D || parms->target == PT_DURANGO) &&
          (attribute.flags & AT_PS_INOUT) != 0) {
        delayedSystemInputs = true;
      } else {
        AppendStructMember(code, attribute);
      }
    }
    if (delayedSystemInputs) {
      for (int index = 0; index < fragmentInfo->inAttribs.num; ++index) {
        if ((fragmentInfo->inAttribs[index].flags & AT_PS_INOUT) != 0)
          AppendStructMember(code, fragmentInfo->inAttribs[index]);
      }
    }
    if (parms->target == PT_PS3 && fragmentInfo->inAttribs.num == 0)
      code.Append("\tfloat4 foo;\n");
    code.Append("};\nstruct PS_OUT {\n");
    for (int index = 0; index < fragmentInfo->outAttribs.num; ++index)
      AppendStructMember(code, fragmentInfo->outAttribs[index]);
    if (parms->target == PT_PS3 && fragmentInfo->outAttribs.num == 0)
      code.Append("\tfloat4 foo;\n");
    code.Append("};\n");
    if (parms->target == PT_360 && data->registerCount360 != 0)
      AppendFormat(code, "[maxtempreg(%d)]\n", data->registerCount360);
    if (parms->target == PT_ORBIS && (data->flags & 5) != 0)
      code.Append("[RE_Z]\n");
    code.Append("void main( PS_IN fragment, out PS_OUT result ) {\n");
    code.Append(fragmentInfo->sourceString);
    code.Append("\n}\n");
  }
}

void shaderConstructInfo_t::Clear() {
  parsed = false;
  headerString.Clear();
  sourceString.Clear();
  outputAttribInitString.Clear();
  inAttribs.ClearFree();
  outAttribs.ClearFree();
}

bool ParseState(idParser* source, renderProgData_t* data) {
  idToken token;
  if (!source->ExpectTokenString("{"))
    return false;
  while (source->ReadToken(token)) {
    if (token == "}")
      return true;

    if (token.Icmp("blend") == 0) {
      if (!ReadStateToken(source, token))
        return false;
      NormalizeStateToken(token);
      if (token.Icmp("ONE") == 0) {}
      else if (token.Icmp("ZERO") == 0) data->glState |= 0x1;
      else if (token.Icmp("DST_COLOR") == 0) data->glState |= 0x2;
      else if (token.Icmp("ONE_MINUS_DST_COLOR") == 0) data->glState |= 0x3;
      else if (token.Icmp("SRC_ALPHA") == 0) data->glState |= 0x4;
      else if (token.Icmp("ONE_MINUS_SRC_ALPHA") == 0) data->glState |= 0x5;
      else if (token.Icmp("DST_ALPHA") == 0) data->glState |= 0x6;
      else if (token.Icmp("ONE_MINUS_DST_ALPHA") == 0) data->glState |= 0x7;
      else {
        source->Warning("bad blend mode %s", token.data);
        return false;
      }
      if (!ReadStateToken(source, token))
        return false;
      NormalizeStateToken(token);
      if (token.Icmp("ZERO") == 0) {}
      else if (token.Icmp("ONE") == 0) data->glState |= 0x8;
      else if (token.Icmp("SRC_COLOR") == 0) data->glState |= 0x10;
      else if (token.Icmp("ONE_MINUS_SRC_COLOR") == 0) data->glState |= 0x18;
      else if (token.Icmp("SRC_ALPHA") == 0) data->glState |= 0x20;
      else if (token.Icmp("ONE_MINUS_SRC_ALPHA") == 0) data->glState |= 0x28;
      else if (token.Icmp("DST_ALPHA") == 0) data->glState |= 0x30;
      else if (token.Icmp("ONE_MINUS_DST_ALPHA") == 0) data->glState |= 0x38;
      else {
        source->Warning("bad blend mode %s", token.data);
        return false;
      }
    } else if (token.Icmp("blendOp") == 0) {
      if (!ReadStateToken(source, token)) return false;
      NormalizeStateToken(token);
      if (token.Icmp("ADD") == 0) {}
      else if (token.Icmp("MIN") == 0) data->glState |= 0x80000;
      else if (token.Icmp("MAX") == 0) data->glState |= 0xC0000;
      else if (token.Icmp("SUB") == 0) data->glState |= 0x40000;
      else { source->Warning("bad blend op %s", token.data); return false; }
    } else if (token.Icmp("depthFunc") == 0) {
      if (!ReadStateToken(source, token)) return false;
      NormalizeStateToken(token);
      if (token.Icmp("LEQUAL") == 0) {}
      else if (token.Icmp("GREATER") == 0) data->glState |= 0x2000;
      else if (token.Icmp("ALWAYS") == 0) data->glState |= 0x4000;
      else if (token.Icmp("EQUAL") == 0) data->glState |= 0x6000;
      else { source->Warning("bad depthFunc %s", token.data); return false; }
    } else if (token.Icmp("wireframe") == 0) {
      data->glState |= 0x800;
    } else if (token.Icmp("alphaCoverage") == 0) {
      data->glState |= 0x20000;
      data->hasAlphaToCoverage = true;
    } else if (token.Icmp("depthMask") == 0) {
      data->glState |= 0x40;
    } else if (token.Icmp("colorMask") == 0) {
      data->glState |= 0x380;
    } else if (token.Icmp("alphaMask") == 0) {
      data->glState |= 0x400;
    } else if (token.Icmp("twoSided") == 0) {
      data->glState |= 0x8000;
    } else if (token.Icmp("backSided") == 0) {
      data->glState |= 0x10000;
    } else if (token.Icmp("polygonOffset") == 0) {
      data->glState |= 0x1000;
    } else if (token.Icmp("stencilOp") == 0) {
      for (int shift = 0; shift < 9; shift += 3) {
        if (!ReadStateToken(source, token)) return false;
        NormalizeStateToken(token);
        std::uint64_t operation = 0;
        if (token.Icmp("KEEP") == 0) operation = 0;
        else if (token.Icmp("ZERO") == 0) operation = 0x8000000000ull;
        else if (token.Icmp("REPLACE") == 0) operation = 0x10000000000ull;
        else if (token.Icmp("INCR") == 0) operation = 0x18000000000ull;
        else if (token.Icmp("DECR") == 0) operation = 0x20000000000ull;
        else if (token.Icmp("INVERT") == 0) operation = 0x28000000000ull;
        else if (token.Icmp("INCR_WRAP") == 0) operation = 0x30000000000ull;
        else if (token.Icmp("DECR_WRAP") == 0) operation = 0x38000000000ull;
        else { source->Warning("bad stencilOp %s", token.data); return false; }
        data->glState |= operation << shift;
      }
    } else if (token.Icmp("stencilFunc") == 0) {
      if (!ReadStateToken(source, token)) return false;
      NormalizeStateToken(token);
      if (token.Icmp("ALWAYS") == 0) {}
      else if (token.Icmp("LESS") == 0) data->glState |= 0x1000000000ull;
      else if (token.Icmp("LEQUAL") == 0) data->glState |= 0x2000000000ull;
      else if (token.Icmp("GREATER") == 0) data->glState |= 0x3000000000ull;
      else if (token.Icmp("GEQUAL") == 0) data->glState |= 0x4000000000ull;
      else if (token.Icmp("EQUAL") == 0) data->glState |= 0x5000000000ull;
      else if (token.Icmp("NOTEQUAL") == 0) data->glState |= 0x6000000000ull;
      else if (token.Icmp("NEVER") == 0) data->glState |= 0x7000000000ull;
      else { source->Warning("bad stencilFunc %s", token.data); return false; }
      if (source->ReadTokenOnLine(token)) {
        if (token.type == TT_NUMBER)
          token.NumberValue();
        data->glState |= static_cast<std::uint64_t>(
                             token.type == TT_NUMBER ? token.intvalue : 0) << 20;
      }
      if (source->ReadTokenOnLine(token)) {
        if (token.type == TT_NUMBER)
          token.NumberValue();
        data->glState |= static_cast<std::uint64_t>(
                             token.type == TT_NUMBER ? token.intvalue : 0) << 28;
      }
    } else if (token.Icmp("registerCountPS3") == 0) {
      data->registerCountPS3 = static_cast<std::uint8_t>(source->ParseInt());
    } else if (token.Icmp("registerCount360") == 0) {
      data->registerCount360 = static_cast<std::uint8_t>(source->ParseInt());
    } else {
      source->Warning("Unknown drawing state option '%s'\n", token.data);
    }
  }
  source->Warning("Unexpected EOF in state block");
  return false;
}

bool ParseFlags(idParser* source, renderProgData_t* data) {
  idToken token;
  if (!source->ExpectTokenString("{"))
    return false;
  while (source->ReadToken(token)) {
    if (token == "}")
      return true;
    if (token.Icmp("perforated") == 0)
      data->flags |= 1;
    else if (token.Icmp("modelfade") == 0)
      data->flags |= 4;
    else if (token.Icmp("sky") == 0)
      data->flags |= 2;
    else
      source->Warning("Unknown flag option '%s'\n", token.data);
  }
  source->Warning("Unexpected EOF in flags block");
  return false;
}

namespace {

idCodeResource<idDeclRenderParm> rpVertexXYZScale("vertexXYZScale");
idCodeResource<idDeclRenderParm> rpVertexXYZBias("vertexXYZBias");
idCodeResource<idDeclRenderParm> rpVertexStScaleBias("vertexStScaleBias");
idCodeResource<idDeclRenderParm> rpDefaultSampler("defaultSampler");

const idDeclRenderParm* FindRenderParm(const char* name) {
  return name != nullptr
             ? static_cast<const idDeclRenderParm*>(
                   idDeclRenderParm::resourceList.FindWithInheritance(
                       name, false, true))
             : nullptr;
}

const idDeclRenderParm* DefaultSamplerParm() {
  const idDeclRenderParm* parm = rpDefaultSampler.Get();
  return parm != nullptr ? parm : FindRenderParm("defaultSampler");
}

void ParseValidationStruct(const idStr& code, const char* structName,
                           idList<progValidation_t>& values) {
  idLexer lexer(LEXFL_NOFATALERRORS);
  if (!lexer.LoadMemory(code.data, static_cast<std::size_t>(code.Length()),
                        structName))
    return;
  idToken token;
  while (lexer.ReadToken(token)) {
    if (token == "struct" && lexer.CheckTokenString(structName))
      break;
  }
  if (!lexer.ExpectTokenString("{"))
    return;
  while (!lexer.CheckTokenString("}")) {
    progValidation_t value;
    if (!lexer.ReadToken(token))
      return;
    value.type = token;
    if (!lexer.ReadToken(token))
      return;
    value.name = token;
    if (!lexer.CheckTokenString(":")) {
      lexer.SkipUntilString(";");
      continue;
    }
    if (!lexer.ReadToken(token))
      return;
    value.semantic = token;
    lexer.ExpectTokenString(";");
    values.Append(value);
  }
}

bool SemanticKind(const idStr& semantic, const char* prefix, int length) {
  return semantic.Icmpn(prefix, length) == 0;
}

idStr CroppedSemantic(const idStr& semantic, const char* prefix, int length) {
  if (!SemanticKind(semantic, prefix, length))
    return idStr();
  const int cropLength = length + 1;
  return semantic.Length() > cropLength ? semantic.Left(cropLength) : semantic;
}

int SemanticSlot(const idStr& semantic, int prefixLength) {
  if (semantic.Length() <= prefixLength)
    return 0;
  const char digit = semantic[prefixLength];
  return digit >= '0' && digit <= '9' ? digit - '0' : 0;
}

void WarnDuplicateSemantics(const idList<progValidation_t>& values,
                            const char* shaderKind, const char* name) {
  std::array<int, 10> texcoords{};
  std::array<int, 10> colors{};
  for (int index = 0; index < values.num; ++index) {
    const idStr& semantic = values[index].semantic;
    if (SemanticKind(semantic, "TEXCOORD", 8)) {
      const idStr cropped = CroppedSemantic(semantic, "TEXCOORD", 8);
      const int slot = SemanticSlot(cropped, 8);
      if (++texcoords[slot] > 1) {
        idLib::Warning(
            "X360 %s Shader Patch Issue: %s - %s %s more than once! - Check mixed usage of half and float version of texcoord",
            shaderKind, name != nullptr ? name : "", cropped.data,
            idStr::Icmp(shaderKind, "Vertex") == 0 ? "output" : "input");
      }
    } else if (SemanticKind(semantic, "COLOR", 5)) {
      const idStr cropped = CroppedSemantic(semantic, "COLOR", 5);
      const int slot = SemanticSlot(cropped, 5);
      if (++colors[slot] > 1) {
        idLib::Warning(
            "X360 %s Shader Patch Issue: %s - %s %s more than once! - Check mixed usage of half and float version of color",
            shaderKind, name != nullptr ? name : "", cropped.data,
            idStr::Icmp(shaderKind, "Vertex") == 0 ? "output" : "input");
      }
    }
  }
}

int CountSemanticKind(const idList<progValidation_t>& values,
                      const char* prefix, int length) {
  int count = 0;
  for (int index = 0; index < values.num; ++index)
    count += SemanticKind(values[index].semantic, prefix, length) ? 1 : 0;
  return count;
}

bool ContainsCroppedSemantic(const idList<progValidation_t>& values,
                             const idStr& semantic,
                             const char* prefix, int length) {
  for (int index = 0; index < values.num; ++index) {
    if (CroppedSemantic(values[index].semantic, prefix, length) == semantic)
      return true;
  }
  return false;
}

void CompareSemanticKind(const idList<progValidation_t>& vertexValues,
                         const idList<progValidation_t>& fragmentValues,
                         const char* prefix, int length, const char* name) {
  const int vertexCount = CountSemanticKind(vertexValues, prefix, length);
  const int fragmentCount = CountSemanticKind(fragmentValues, prefix, length);
  if (fragmentCount > vertexCount) {
    for (int index = 0; index < fragmentValues.num; ++index) {
      idStr semantic =
          CroppedSemantic(fragmentValues[index].semantic, prefix, length);
      if (!semantic.IsEmpty() &&
          !ContainsCroppedSemantic(vertexValues, semantic, prefix, length)) {
        idLib::Warning(
            "X360 Shader Patch Issue: %s - Fragment shader requires %s - Fix Vertex Shader to output %s",
            name != nullptr ? name : "", semantic.data, semantic.data);
      }
    }
  } else if (vertexCount > fragmentCount) {
    for (int index = 0; index < vertexValues.num; ++index) {
      idStr semantic =
          CroppedSemantic(vertexValues[index].semantic, prefix, length);
      if (!semantic.IsEmpty() &&
          !ContainsCroppedSemantic(fragmentValues, semantic, prefix, length)) {
        idLib::Warning(
            "X360 Shader Patch Issue: %s - Vertex shader outputs %s but the fragment shader does not use it - Fix Vertex Shader to not output %s",
            name != nullptr ? name : "", semantic.data, semantic.data);
      }
    }
  }
}

bool AppendShaderAttribute(shaderConstructInfo_t* shaderInfo,
                           attribInfo_t* table, int tableCount,
                           std::vector<bool>& used, int requiredFlag,
                           const idToken& attributeName, bool output,
                           idParser* source, renderProgData_t* data) {
  for (int index = 0; index < tableCount; ++index) {
    if ((table[index].flags & requiredFlag) == 0 || used[index] ||
        attributeName.Cmp(table[index].name) != 0)
      continue;
    idStaticList<attribInfo_t, 16>& destination =
        output ? shaderInfo->outAttribs : shaderInfo->inAttribs;
    destination.Append(table[index]);
    used[index] = true;
    CheckForAttribSemanticCollision(destination, source);
    if (requiredFlag == AT_VS_IN)
      data->vertexMask |= table[index].vertexMask;
    if (requiredFlag == AT_PS_OUT)
      ++data->fragmentOutputs;
    if (requiredFlag == AT_VS_OUT) {
      AppendFormat(shaderInfo->outputAttribInitString,
                   "\tresult.%s = 0;\n", table[index].name);
    }
    return true;
  }
  return false;
}

}  // namespace

void ValidateInOutParms360(const renderProgData_t* data, const char* name) {
  idList<progValidation_t> vertexOutputs;
  idList<progValidation_t> fragmentInputs;
  ParseValidationStruct(data->vertexCode, "VS_OUT", vertexOutputs);
  ParseValidationStruct(data->fragmentCode, "PS_IN", fragmentInputs);

  idList<progValidation_t> filteredVertex;
  for (int index = 0; index < vertexOutputs.num; ++index) {
    if (vertexOutputs[index].semantic.Icmp("POSITION") != 0)
      filteredVertex.Append(vertexOutputs[index]);
  }
  idList<progValidation_t> filteredFragment;
  for (int index = 0; index < fragmentInputs.num; ++index) {
    const idStr& semantic = fragmentInputs[index].semantic;
    if (SemanticKind(semantic, "TEXCOORD", 8) ||
        SemanticKind(semantic, "COLOR", 5)) {
      filteredFragment.Append(fragmentInputs[index]);
    }
  }

  WarnDuplicateSemantics(filteredVertex, "Vertex", name);
  WarnDuplicateSemantics(filteredFragment, "Fragment", name);
  CompareSemanticKind(filteredVertex, filteredFragment, "TEXCOORD", 8, name);
  CompareSemanticKind(filteredVertex, filteredFragment, "COLOR", 5, name);
}

bool ParseShader(const renderProgParms_t* parms, programType_t programType,
                 idParser* source, const idStr* globalInclude,
                 renderProgData_t* data, shaderConstructInfo_t* shaderInfo) {
  if (programType == PROGRAM_TYPE_VERTEX) {
    data->has16BitScaleBias = false;
    data->hasHardwareSkinning = false;
    data->hasVertexTexture = false;
    data->vertexMask = 0;
    data->vertexParms.ClearFree();
  } else {
    data->hasClip = false;
    data->hasDepthOutput = false;
    data->hasBranchStripping = false;
    data->fragmentOutputs = 0;
    data->fragmentParms.ClearFree();
    if (!data->hasVertexTexture) {
      data->textureParms.ClearFree();
      data->samplerParms.ClearFree();
    }
  }

  attribInfo_t* table = AttribTableForTarget(parms->target);
  int tableCount = 0;
  while (table[tableCount].name != nullptr)
    ++tableCount;
  std::vector<bool> inputUsed(static_cast<std::size_t>(tableCount), false);
  std::vector<bool> outputUsed(static_cast<std::size_t>(tableCount), false);

  idList<short>& uniformParms = programType == PROGRAM_TYPE_VERTEX
                                    ? static_cast<idList<short>&>(data->vertexParms)
                                    : static_cast<idList<short>&>(data->fragmentParms);
  const char* programName =
      programType == PROGRAM_TYPE_VERTEX ? "vertex" : "fragment";
  bool sawXYZScale = false;
  bool sawXYZBias = false;
  bool sawStScaleBias = false;
  bool usesPosition = false;
  bool usesTexcoord0 = false;

  idToken token;
  if (!source->ExpectTokenString("{", token))
    return false;
  int braceDepth = 1;
  while (source->ReadToken(token)) {
    if (token == "$") {
      if (!source->ReadToken(token))
        break;
      idStr renderParmName(token);
      const idDeclRenderParm* renderParm = FindRenderParm(renderParmName.data);
      if (renderParm == nullptr) {
        source->Warning("render parm '%s' not found for %s program",
                        renderParmName.data, programName);
        return false;
      }

      if (renderParm->parmType == PT_VECTOR) {
        int uniformIndex = FindParmIndex(uniformParms, renderParm->parmIndex);
        if (uniformIndex == -1) {
          if (uniformParms.IsFull()) {
            source->Warning("more than %d render parms used in %s program",
                            uniformParms.num, programName);
            return false;
          }
          uniformParms.Append(static_cast<short>(renderParm->parmIndex));
          uniformIndex = uniformParms.num - 1;
        }
        if (programType == PROGRAM_TYPE_VERTEX) {
          sawXYZScale |=
              idStr::Icmp(renderParm->name.str, "vertexXYZScale") == 0;
          sawXYZBias |=
              idStr::Icmp(renderParm->name.str, "vertexXYZBias") == 0;
          sawStScaleBias |=
              idStr::Icmp(renderParm->name.str, "vertexStScaleBias") == 0;
        }
        if (parms->target == PT_PC && parms->useUniformArrays) {
          AppendFormat(shaderInfo->sourceString,
                       programType == PROGRAM_TYPE_VERTEX
                           ? " _va_[%d /* %s */]"
                           : " _fa_[%d /* %s */]",
                       uniformIndex, renderParm->name.str);
        } else {
          AppendFormat(shaderInfo->sourceString, " rp_name_%s",
                       renderParm->name.str);
        }
      } else if (renderParm->parmType >= PT_TEXTURE_2D &&
                 renderParm->parmType <= PT_IMAGE_2D_WRITE) {
        const idDeclRenderParm* samplerParm = DefaultSamplerParm();
        if (source->CheckTokenString(".")) {
          idToken samplerName;
          source->ReadToken(samplerName);
          samplerParm = FindRenderParm(samplerName.data);
          if (samplerParm == nullptr) {
            source->Warning("render parm '%s' not found for %s program",
                            samplerName.data, programName);
            return false;
          }
          if (samplerParm->parmType != PT_SAMPLER) {
            source->Warning(
                "render parm '%s' does not reference a sampler for %s program",
                samplerName.data, programName);
            return false;
          }
        }
        if (samplerParm == nullptr) {
          source->Warning("render parm 'defaultSampler' not found for %s program",
                          programName);
          return false;
        }

        int textureIndex =
            FindParmIndex(data->textureParms, renderParm->parmIndex);
        if (textureIndex == -1) {
          if (data->textureParms.IsFull()) {
            source->Warning("more than %d textures used in %s program",
                            data->textureParms.num, programName);
            return false;
          }
          data->textureParms.Append(static_cast<short>(renderParm->parmIndex));
          data->samplerParms.Append(static_cast<short>(samplerParm->parmIndex));
        } else if (data->samplerParms[textureIndex] != samplerParm->parmIndex) {
          source->Warning(
              "texture '%s' previously sampled with a different sampler in %s program",
              renderParmName.data, programName);
          return false;
        }

        if (programType == PROGRAM_TYPE_VERTEX) {
          data->hasVertexTexture = true;
          source->Warning("sampling a texture in the vertex program");
        }
        AppendFormat(shaderInfo->sourceString, " samp_%s",
                     renderParm->name.str);
        if (renderParm->parmType != PT_IMAGE_2D_READ &&
            renderParm->parmType != PT_IMAGE_2D_WRITE &&
            (parms->target == PT_PC_D3D || parms->target == PT_DURANGO ||
             parms->target == PT_ORBIS)) {
          AppendFormat(shaderInfo->sourceString, ", samp_state_%s",
                       renderParm->name.str);
        }
      } else {
        source->Warning("invalid render parm '%s' used in %s program",
                        renderParmName.data, programName);
        return false;
      }
      continue;
    }

    if (token.type == TT_NAME) {
      if (programType == PROGRAM_TYPE_VERTEX && token == "vertex" &&
          source->CheckTokenString(".")) {
        idToken attributeName;
        source->ReadToken(attributeName);
        token += ".";
        token += attributeName;
        AppendShaderAttribute(shaderInfo, table, tableCount, inputUsed,
                              AT_VS_IN, attributeName, false, source, data);
        usesPosition |= attributeName.Cmp("position") == 0;
        usesTexcoord0 |= attributeName.Cmp("texcoord0") == 0;
      } else if (programType == PROGRAM_TYPE_VERTEX && token == "result" &&
                 source->CheckTokenString(".")) {
        idToken attributeName;
        source->ReadToken(attributeName);
        token += ".";
        token += attributeName;
        AppendShaderAttribute(shaderInfo, table, tableCount, outputUsed,
                              AT_VS_OUT, attributeName, true, source, data);
      } else if (programType == PROGRAM_TYPE_VERTEX &&
                 token.Cmpn("hardwareSkinning", 16) == 0) {
        data->hasHardwareSkinning = true;
      } else if (programType == PROGRAM_TYPE_FRAGMENT && token == "fragment" &&
                 source->CheckTokenString(".")) {
        idToken attributeName;
        source->ReadToken(attributeName);
        token += ".";
        token += attributeName;
        AppendShaderAttribute(shaderInfo, table, tableCount, inputUsed,
                              AT_PS_IN, attributeName, false, source, data);
      } else if (programType == PROGRAM_TYPE_FRAGMENT && token == "result" &&
                 source->CheckTokenString(".")) {
        idToken attributeName;
        source->ReadToken(attributeName);
        token += ".";
        token += attributeName;
        AppendShaderAttribute(shaderInfo, table, tableCount, outputUsed,
                              AT_PS_OUT, attributeName, true, source, data);
        if (attributeName.Cmp("depth") == 0)
          data->hasDepthOutput = true;
      } else if (programType == PROGRAM_TYPE_FRAGMENT && token == "clip") {
        data->hasClip = true;
      } else if (programType == PROGRAM_TYPE_FRAGMENT && token == "branch" &&
                 parms->target == PT_PS3 && source->PeekTokenString("]")) {
        data->hasBranchStripping = true;
      }
    }

    if (token == "{") {
      ++braceDepth;
    } else if (token == "}" && --braceDepth <= 0) {
      break;
    }

    if (token.linesCrossed > 0)
      shaderInfo->sourceString += '\n';
    if (HasWhiteSpace(token))
      shaderInfo->sourceString += ' ';
    if (token.CmpPrefix("rp_name_") == 0) {
      source->Warning(
          "%s:, \"rp_name_\" is not allowed as prefix (used internally by compiler)",
          token.data);
    }
    shaderInfo->sourceString += token;
  }

  if (programType == PROGRAM_TYPE_VERTEX) {
    data->has16BitScaleBias = true;
    if (usesPosition && (!sawXYZScale || !sawXYZBias))
      data->has16BitScaleBias = false;
    if (usesTexcoord0 && !sawStScaleBias)
      data->has16BitScaleBias = false;
  }

  if (shaderInfo->sourceString.IsEmpty()) {
    (programType == PROGRAM_TYPE_VERTEX ? data->vertexCode
                                        : data->fragmentCode)
        .Empty();
  } else {
    shaderInfo->parsed = true;
    GenerateHeader(parms, programType, globalInclude, data, shaderInfo);
  }
  return true;
}

idStr ConvertCG2GLSL(const idStr& input, const char* name,
                     bool isVertexProgram) {
  idStr result(isVertexProgram ? vertexInsert : fragmentInsert);
  if (!isVertexProgram)
    result.ReAllocate(input.Length() * 2 + idStr::Length(fragmentInsert), true);

  idList<inOutVariable_t> variablesIn;
  idList<inOutVariable_t> variablesOut;
  idLexer source(LEXFL_NOFATALERRORS);
  source.LoadMemory(input.data, static_cast<std::size_t>(input.Length()), name);
  bool inMain = false;
  idStr newline("\n");

  auto appendLeadingWhiteSpace = [&](const idToken& token) {
    if (token.linesCrossed > 0)
      result += newline;
    else if (HasWhiteSpace(token))
      result += ' ';
  };

  idToken token;
  while (source.ReadToken(token)) {
    if (token == "struct") {
      if (source.CheckTokenString("VS_IN")) {
        ParseInOutStruct(&source, AT_VS_IN, &variablesIn);
        result += "\n\n";
        for (int index = 0; index < variablesIn.num; ++index) {
          if (variablesIn[index].declareInOut)
            AppendFormat(result, "in %s %s;\n", variablesIn[index].type.data,
                         variablesIn[index].nameGLSL.data);
        }
        continue;
      }
      if (source.CheckTokenString("VS_OUT")) {
        ParseInOutStruct(&source, AT_VS_OUT, &variablesOut);
        result += '\n';
        for (int index = 0; index < variablesOut.num; ++index) {
          if (variablesOut[index].declareInOut)
            AppendFormat(result, "out %s %s;\n", variablesOut[index].type.data,
                         variablesOut[index].nameGLSL.data);
        }
        continue;
      }
      if (source.CheckTokenString("PS_IN")) {
        ParseInOutStruct(&source, AT_PS_IN, &variablesIn);
        result += "\n\n";
        for (int index = 0; index < variablesIn.num; ++index) {
          if (variablesIn[index].declareInOut)
            AppendFormat(result, "in %s %s;\n", variablesIn[index].type.data,
                         variablesIn[index].nameGLSL.data);
        }
        continue;
      }
      if (source.CheckTokenString("PS_OUT")) {
        ParseInOutStruct(&source, AT_PS_OUT, &variablesOut);
        result += '\n';
        for (int index = 0; index < variablesOut.num; ++index) {
          if (variablesOut[index].declareInOut)
            AppendFormat(result, "out %s %s;\n", variablesOut[index].type.data,
                         variablesOut[index].nameGLSL.data);
        }
        continue;
      }
    }

    if (token == "static") {
      appendLeadingWhiteSpace(token);
      source.SkipWhiteSpace(true);
      continue;
    }

    if (token == ":" && source.CheckTokenString("register")) {
      source.SkipUntilString(";");
      result += ';';
      continue;
    }

    if (token == "void" && source.CheckTokenString("main")) {
      source.ExpectTokenString("(");
      while (source.ReadToken(token) && token != ")") {}
      result += "\nvoid main()";
      inMain = true;
      continue;
    }

    if (token == "const" && inMain) {
      appendLeadingWhiteSpace(token);
      source.SkipWhiteSpace(true);
      continue;
    }

    if (token == "{") {
      appendLeadingWhiteSpace(token);
      result += '{';
      if (newline.Length() < 127)
        newline += '\t';
      continue;
    }
    if (token == "}") {
      if (!newline.IsEmpty())
        newline.CapLength(std::max(0, newline.Length() - 1));
      appendLeadingWhiteSpace(token);
      result += '}';
      continue;
    }

    bool converted = false;
    for (int index = 0; typeConversion[index].typeCG != nullptr; ++index) {
      if (token.Cmp(typeConversion[index].typeCG) == 0) {
        appendLeadingWhiteSpace(token);
        result += typeConversion[index].typeGLSL;
        converted = true;
        break;
      }
    }
    if (converted)
      continue;

    if (source.CheckTokenString(".")) {
      if (token == "vertex" || token == "fragment") {
        idToken member;
        source.ReadToken(member);
        appendLeadingWhiteSpace(token);
        bool found = false;
        for (int index = 0; index < variablesIn.num; ++index) {
          if (member.Cmp(variablesIn[index].nameCg.data) == 0) {
            result += variablesIn[index].nameGLSL;
            found = true;
            break;
          }
        }
        if (!found) {
          source.Error("invalid input parameter %s.%s", token.data,
                       member.data);
          result += token;
          result += '.';
          result += member;
        }
        continue;
      }
      if (token == "result") {
        idToken member;
        source.ReadToken(member);
        appendLeadingWhiteSpace(token);
        bool found = false;
        for (int index = 0; index < variablesOut.num; ++index) {
          if (member.Cmp(variablesOut[index].nameCg.data) == 0) {
            result += variablesOut[index].nameGLSL;
            found = true;
            break;
          }
        }
        if (!found) {
          source.Error("invalid output parameter %s.%s", token.data,
                       member.data);
          result += token;
          result += '.';
          result += member;
        }
        continue;
      }
      appendLeadingWhiteSpace(token);
      result += token;
      result += '.';
      continue;
    }

    for (int index = 0; builtinConversion[index].nameCG != nullptr; ++index) {
      if (token.Cmp(builtinConversion[index].nameCG) == 0) {
        appendLeadingWhiteSpace(token);
        result += builtinConversion[index].nameGLSL;
        converted = true;
        break;
      }
    }
    if (converted)
      continue;

    appendLeadingWhiteSpace(token);
    result += token;
  }
  return result;
}

namespace {

bool IsCGDeclarationPrefix(const idToken& token) {
  for (const char* prefix : prefixes) {
    if (token == prefix)
      return true;
  }
  for (const char* type : types) {
    if (token == type)
      return true;
    const int typeLength = idStr::Length(type);
    if (token.Cmpn(type, typeLength) != 0)
      continue;
    for (const char* postfix : typePosts) {
      if (idStr::Cmp(token.data + typeLength, postfix) == 0)
        return true;
    }
  }
  return false;
}

}  // namespace

idStr StripDeadCode(const idStr& input, const char* name) {
  idLexer source(LEXFL_NOFATALERRORS);
  source.LoadMemory(input.data, static_cast<std::size_t>(input.Length()), name);
  idList<idCGBlock> blocks;
  blocks.EnsureCapacity(100);
  idToken token;

  while (!source.EndOfFile()) {
    idCGBlock* block = blocks.Alloc();
    if (block == nullptr)
      break;

    while (source.ReadToken(token)) {
      if (!IsCGDeclarationPrefix(token)) {
        source.UnreadToken();
        break;
      }
      if (!block->prefix.IsEmpty() && HasWhiteSpace(token))
        block->prefix += ' ';
      block->prefix += token;
    }
    if (!source.ReadToken(token)) {
      blocks.SetNum(blocks.num - 1);
      break;
    }
    block->name = token;

    if (source.PeekTokenString("=") || source.PeekTokenString(":") ||
        source.PeekTokenString("[")) {
      source.ReadToken(token);
      block->postfix = token;
      while (source.ReadToken(token)) {
        if (token == ";") {
          block->postfix += ';';
          break;
        }
        if (HasWhiteSpace(token))
          block->postfix += ' ';
        block->postfix += token;
      }
    } else if (source.PeekTokenString("(")) {
      idStr parameters;
      idStr body;
      source.ParseBracedSection(parameters, -1, true, '(', ')');
      if (source.CheckTokenString(";")) {
        block->postfix = parameters + ";";
      } else {
        source.ParseBracedSection(body, -1, true, '{', '}');
        block->postfix = parameters + " " + body;
      }
    } else if (source.PeekTokenString("{")) {
      source.ParseBracedSection(block->postfix, -1, true, '{', '}');
      if (source.CheckTokenString(";"))
        block->postfix += ';';
    } else if (source.CheckTokenString(";")) {
      block->postfix = ';';
    } else {
      source.Warning("Could not strip dead code -- unknown token %s", token.data);
      return input;
    }
  }

  idList<int> stack;
  for (int index = 0; index < blocks.num; ++index) {
    blocks[index].used = blocks[index].name == "main" ||
                         blocks[index].name.Right(4) == "_ubo";
    if (blocks[index].used)
      stack.Append(index);
  }

  while (stack.num > 0) {
    const int blockIndex = stack[stack.num - 1];
    stack.SetNum(stack.num - 1);
    idLexer references(LEXFL_NOFATALERRORS);
    references.LoadMemory(blocks[blockIndex].postfix.data,
                          static_cast<std::size_t>(
                              blocks[blockIndex].postfix.Length()),
                          name);
    while (references.ReadToken(token)) {
      for (int candidate = 0; candidate < blocks.num; ++candidate) {
        if (!blocks[candidate].used && token == blocks[candidate].name) {
          blocks[candidate].used = true;
          stack.Append(candidate);
        }
      }
    }
  }

  idStr output;
  for (int index = 0; index < blocks.num; ++index) {
    if (!blocks[index].used)
      continue;
    output += blocks[index].prefix;
    output += ' ';
    output += blocks[index].name;
    output += ' ';
    output += blocks[index].postfix;
    output += '\n';
  }
  return output;
}

renderProgData_t TranslateRenderProg(const renderProgParms_t* parms,
                                     idParser* source,
                                     unsigned int checksum,
                                     shaderConstructInfo_t* vertexInfo,
                                     shaderConstructInfo_t* fragmentInfo) {
  idToken token;
  idStr globalInclude;
  source->PopDefineScope();

  switch (parms->target) {
    case PT_PC:
      source->AddDefine("PC 1");
      source->AddDefine("GLSL 1");
      break;
    case PT_PC_D3D:
      source->AddDefine("PC_D3D 1");
      break;
    case PT_360:
      source->AddDefine("XBOX 1");
      break;
    case PT_DURANGO:
      source->AddDefine("DURANGO 1");
      break;
    case PT_PS3:
      source->AddDefine("PS3 1");
      break;
    case PT_ORBIS:
      source->AddDefine("ORBIS 1");
      break;
    default:
      break;
  }
  if (parms->useDeferredSlowmapLighting)
    source->AddDefine("DEFERRED_LIGHTING 1");
  if (parms->feedbackBGRA)
    source->AddDefine("FEEDBACK_BGRA 1");
  if (parms->useNormalGBuffer &&
      (parms->target == PT_PC || parms->target == PT_ORBIS)) {
    source->AddDefine("NORMAL_GBUFFER 1 ");
  }

  const int includeLevel = source->GetIncludeLevel();
  source->AddInclude("/decls/renderprogs/global.inc");
  if (source->ReadToken(token)) {
    while (source->GetIncludeLevel() > includeLevel) {
      int whiteSpace = HasWhiteSpace(token)
                           ? static_cast<int>(token.whiteSpaceEnd_p -
                                              token.whiteSpaceStart_p)
                           : 0;
      if (token.linesCrossed > 0) {
        globalInclude += '\n';
        --whiteSpace;
      }
      while (whiteSpace-- > 0)
        globalInclude += ' ';
      globalInclude += token;
      if (!source->ReadToken(token))
        break;
    }
    if (source->GetIncludeLevel() <= includeLevel)
      source->UnreadToken(token);
  }
  globalInclude += '\n';

  renderProgData_t data;
  bool parsedShader = false;
  while (source->ReadToken(token)) {
    if (token.Icmp("inherit") == 0) {
      if (data.glState != 0 || data.flags != 0 || data.parmBlock.ops.num != 0 ||
          parsedShader) {
        source->Warning("'inherit' MUST come before all other commands");
        return data;
      }
      if (!source->ReadToken(token)) {
        source->Warning("Unexpected EOF after 'inherit'");
        return data;
      }
      idDeclRenderProg* parent = static_cast<idDeclRenderProg*>(
          idDeclRenderProg::resourceList.FindWithInheritance(token.data, false,
                                                              true));
      if (parent == nullptr) {
        source->Warning("Unknown RenderProg %s", token.data);
        return data;
      }
      renderProgParms_t inheritedParms = *parms;
      data = parent->GetRenderProgData(&inheritedParms, vertexInfo,
                                       fragmentInfo);
      data.inheritsFrom = parent->name.str;
      data.parentCheckSum = data.compoundCheckSum;
    } else if (token.Icmp("parms") == 0) {
      source->ExpectTokenString("{");
      idParmBlockParser parser(*source, data.parmBlock);
      const idDeclRenderParm* stageSort = FindRenderParm("stageSort");
      data.stageSort =
          std::floor(data.parmBlock.GetFloat(stageSort, nullptr));
    } else if (token.Icmp("state") == 0) {
      data.glState = 0;
      data.hasAlphaToCoverage = false;
      ParseState(source, &data);
    } else if (token.Icmp("flags") == 0) {
      data.flags = 0;
      ParseFlags(source, &data);
    } else if (token.Icmp("interaction") == 0) {
      data.hasInteractions = true;
    } else if (token.Icmp("hlsl_vp") == 0 || token.Icmp("hlsl_fp") == 0) {
      const int oldFlags = source->GetFlags();
      source->SetFlags(oldFlags & ~LEXFL_ALLOWPATHNAMES);
      source->PushDefineScope();
      const programType_t programType =
          token.Icmp("hlsl_vp") == 0 ? PROGRAM_TYPE_VERTEX
                                     : PROGRAM_TYPE_FRAGMENT;
      shaderConstructInfo_t* shaderInfo =
          programType == PROGRAM_TYPE_VERTEX ? vertexInfo : fragmentInfo;
      shaderInfo->Clear();
      const bool parsed = ParseShader(parms, programType, source,
                                      &globalInclude, &data, shaderInfo);
      source->PopDefineScope();
      source->SetFlags(oldFlags);
      if (!parsed)
        return data;
      parsedShader = true;
    } else if (token == "}") {
      break;
    } else {
      source->Warning("Unknown program type: %s", token.data);
      return data;
    }
  }

  GenerateCode(parms, &data, vertexInfo, fragmentInfo);
  const char* sourceName = source->GetCurrentSourceName();
  if (parms->target == PT_PC) {
    if (vertexInfo->parsed) {
      idStr stripped = StripDeadCode(data.vertexCode, sourceName);
      data.vertexCode = ConvertCG2GLSL(stripped, sourceName, true);
    }
    if (fragmentInfo->parsed) {
      idStr stripped = StripDeadCode(data.fragmentCode, sourceName);
      data.fragmentCode = ConvertCG2GLSL(stripped, sourceName, false);
    }
  }
  if (parms->validateInOutParms360)
    ValidateInOutParms360(&data, sourceName);

  for (int index = 0; index < source->GetNumDependencies(-1); ++index) {
    const char* dependency = source->GetDependency(index).fileName.data;
    bool found = false;
    for (int existing = 0; existing < data.dependencies.num; ++existing) {
      if (data.dependencies[existing] == dependency) {
        found = true;
        break;
      }
    }
    if (!found)
      data.dependencies.Append(idStr(dependency));
  }

  data.dependencyTimestamps.SetNum(data.dependencies.num);
  for (int index = 0; index < data.dependencies.num; ++index) {
    data.dependencyTimestamps[index] =
        fileSystem != nullptr
            ? fileSystem->GetTimestamp(data.dependencies[index].data)
            : 0;
    checksum ^= data.dependencyTimestamps[index];
  }
  data.compoundCheckSum = data.parentCheckSum ^ checksum;
  return data;
}
