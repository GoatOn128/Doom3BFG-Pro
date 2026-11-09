#include "engine/renderer/declrenderparm.h"

#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/texturesampler.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/parser.h"

#include <cstring>

namespace {

constexpr int INVALID_PARM_INDEX = 2175;
constexpr int RENDER_PARM_PARSER_FLAGS = 156216;

bool ValuesEqual(const parmValue_t& left, const parmValue_t& right) {
  return std::memcmp(&left, &right, sizeof(parmValue_t)) == 0;
}

void PrintImage(const idImage* image) {
  if (image != nullptr)
    const_cast<idImage*>(image)->Print();
  else
    idLib::Printf("WARNING: NULL\n");
}

}  // namespace

idDeclInfoTemplate<idDeclRenderParm> idDeclRenderParm::resourceList(
    "renderParm", "idDeclRenderParm", "renderparm", false);

idDeclRenderParm::idDeclRenderParm()
    : parmIndex(INVALID_PARM_INDEX), parmType(PT_VECTOR), creator(PC_PARSED),
      cubeFilterTexture(false), declaredValue{}, edit(PE_NO_EDIT),
      editRange{0.0f, 1.0f} {
  resourceListPtr = &resourceList;
}

idDeclRenderParm::~idDeclRenderParm() {
  renderThreadParmState->FreeParmState(this);
}

const char* idDeclRenderParm::DefaultDefinition() const {
  return "{ vec 0 }\n";
}

idDeclInfo* idDeclRenderParm::GetDeclInfo() const {
  return &resourceList;
}

idImage* idDeclRenderParm::ParseImageLine(idParser& source) const {
  idStr imageName;
  idImage::ParsePastImageLineAndMakeCanonical(
      source, imageName, cubeFilterTexture ? "cubeFilter" : nullptr);
  return static_cast<idImage*>(
      idImage::resourceList.Load(imageName.data, true, false));
}

void idDeclRenderParm::List() const {
  idLib::Printf("%s ", name.str);
  const parmValue_t* value = reinterpret_cast<const parmValue_t*>(
      &renderThreadParmState->GetParmVec4(this));

  for (int pass = 0; pass < 2; ++pass) {
    switch (parmType) {
      case PT_VECTOR:
        idLib::Printf("Vector %f %f %f %f\n", value->value[0],
                      value->value[1], value->value[2], value->value[3]);
        break;
      case PT_TEXTURE_2D:
        idLib::Printf("Tex2D ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_3D:
        idLib::Printf("Tex3D ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_CUBE:
        idLib::Printf("TexCube ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_SHADOW_2D:
        idLib::Printf("TexShadow2D ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_SHADOW_3D:
        idLib::Printf("TexShadow3D ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_SHADOW_CUBE:
        idLib::Printf("TexShadowCube ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_DIM_SHADOW_2D:
        idLib::Printf("TexDimShadow2D ");
        PrintImage(value->image);
        break;
      case PT_TEXTURE_MULTISAMPLE_2D:
        idLib::Printf("TexMultisample2D ");
        PrintImage(value->image);
        break;
      case PT_IMAGE_2D_READ:
      case PT_IMAGE_2D_WRITE:
        idLib::Printf("Image2D ");
        PrintImage(value->image);
        break;
      case PT_SAMPLER:
        if (value->sampler != nullptr) {
          idLib::Printf("Sampler %s\n",
                        textureSamplerManager->GetSamplerName(value->sampler));
        } else {
          idLib::Printf("Sampler NULL\n");
        }
        break;
      case PT_PROGRAM:
        if (value->program != nullptr)
          idLib::Printf("Program %s\n", value->program->name.str);
        else
          idLib::Printf("Program WARNING: NULL\n");
        break;
      case PT_STRING:
        if (value->string != nullptr)
          idLib::Printf("String \"%s\"\n", value->string);
        else
          idLib::Printf("String WARNING: NULL\n");
        break;
      default:
        idLib::Printf("BAD TYPE!\n");
        break;
    }

    if (ValuesEqual(*value, declaredValue))
      break;
    value = &declaredValue;
    idLib::Printf("  Declared:\n");
  }
}

bool idDeclRenderParm::ParseVectorConstant(idParser& source, float* vector) {
  idToken token;
  source.ReadToken(token);

  if (token == "{") {
    std::memset(vector, 0, sizeof(float) * 4);
    int count = 0;
    while (count < 4) {
      source.ReadToken(token);
      bool negative = false;
      if (token == "-") {
        negative = true;
        source.ReadToken(token);
      }
      if (token.type != TT_NUMBER) {
        if (negative)
          source.Warning("Bad negative number '%s'", token.data);
        else
          source.Warning("Non numeric term in vector constant");
        return false;
      }
      token.NumberValue();
      vector[count++] = negative ? -token.floatvalue : token.floatvalue;

      source.ReadToken(token);
      if (token == "}")
        break;
      if (count == 4) {
        source.Warning("Over four elements in a vector constant");
        return false;
      }
      if (token != ",") {
        source.Warning("Missing comma in vector constant");
        return false;
      }
    }
    if (count == 0) {
      source.Warning("Vector constant with no elements");
      return false;
    }
    if (count == 1)
      vector[1] = vector[2] = vector[3] = vector[0];
    return true;
  }

  bool negative = false;
  if (token == "-") {
    negative = true;
    source.ReadToken(token);
  }
  if (token.type != TT_NUMBER) {
    std::memset(vector, 0, sizeof(float) * 4);
    if (negative)
      source.Warning("Bad negative number '%s'", token.data);
    else
      source.Warning("Non numeric constant");
    return false;
  }
  token.NumberValue();
  const float scalar = negative ? -token.floatvalue : token.floatvalue;
  vector[0] = vector[1] = vector[2] = vector[3] = scalar;
  return true;
}

void idDeclRenderParm::ParseValue(idParser& source, parmValue_t& value) {
  switch (parmType) {
    case PT_VECTOR:
      ParseVectorConstant(source, value.value);
      return;
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
      value.image = ParseImageLine(source);
      return;
    case PT_SAMPLER: {
      idToken token;
      if (!source.ExpectAnyToken(token) || token.Icmp("0") == 0) {
        value.sampler = nullptr;
      } else {
        value.sampler = textureSamplerManager->FindSamplerByName(token.data,
                                                                 false);
      }
      return;
    }
    case PT_PROGRAM: {
      idToken token;
      if (!source.ExpectAnyToken(token) || token.Icmp("0") == 0) {
        value.program = nullptr;
      } else {
        value.program = static_cast<const idDeclRenderProg*>(
            idDeclRenderProg::resourceList.FindWithInheritance(
                token.data, true, true));
      }
      return;
    }
    case PT_STRING: {
      idToken token;
      idStr stringValue;
      if (source.ExpectAnyToken(token)) {
        while (token != "}") {
          if (!stringValue.IsEmpty())
            stringValue += " ";
          stringValue += token.data;
          if (!source.ExpectAnyToken(token))
            break;
        }
        if (token == "}")
          source.UnreadToken(token);
      }
      idAtomicString atom(stringValue.data);
      value.string = atom.str;
      return;
    }
    default:
      source.Warning("Bad renderParm type: %s\n", "");
      return;
  }
}

void idDeclRenderParm::ParseStringToValue(const char* text,
                                          parmValue_t& value) {
  idParser source(RENDER_PARM_PARSER_FLAGS);
  source.LoadMemory(text, idStr::Length(text), "value string");
  ParseValue(source, value);
}

void idDeclRenderParm::Parse(idParser& source) {
  idToken token;
  if (source.PeekTokenType(TT_PUNCTUATION, 47, token))
    return;
  if (!source.ExpectAnyToken(token))
    return;

  if (token.Icmp("Tex") == 0 || token.Icmp("Tex2D") == 0) {
    parmType = PT_TEXTURE_2D;
  } else if (token.Icmp("Tex3D") == 0) {
    parmType = PT_TEXTURE_3D;
  } else if (token.Icmp("TexCube") == 0) {
    parmType = PT_TEXTURE_CUBE;
  } else if (token.Icmp("TexShadow2D") == 0) {
    parmType = PT_TEXTURE_SHADOW_2D;
  } else if (token.Icmp("TexShadow3D") == 0) {
    parmType = PT_TEXTURE_SHADOW_3D;
  } else if (token.Icmp("TexShadowCube") == 0) {
    parmType = PT_TEXTURE_SHADOW_CUBE;
  } else if (token.Icmp("TexDimShadow2D") == 0) {
    parmType = PT_TEXTURE_DIM_SHADOW_2D;
  } else if (token.Icmp("TexMultisample2D") == 0) {
    parmType = PT_TEXTURE_MULTISAMPLE_2D;
  } else if (token.Icmp("Image2D_R") == 0) {
    parmType = PT_IMAGE_2D_READ;
  } else if (token.Icmp("Image2D_W") == 0) {
    parmType = PT_IMAGE_2D_WRITE;
  } else if (token.Icmp("environment") == 0) {
    parmType = PT_TEXTURE_CUBE;
    cubeFilterTexture = true;
  } else if (token.Icmp("Sampler") == 0) {
    parmType = PT_SAMPLER;
  } else if (token.Icmp("Program") == 0) {
    parmType = PT_PROGRAM;
  } else if (token.Icmp("Vec") == 0) {
    parmType = PT_VECTOR;
  } else if (token.Icmp("String") == 0) {
    parmType = PT_STRING;
  } else {
    source.Warning("Bad renderParm type: %s\n", token.data);
    return;
  }

  if (parmIndex >= INVALID_PARM_INDEX)
    parmIndex = renderThreadParmState->AllocParmState(this);
  ParseValue(source, declaredValue);
  renderThreadParmState->SetParmValue(this, declaredValue);

  if (!source.ExpectAnyToken(token))
    return;
  if (token == "}")
    return;
  if (token.Icmp("Bool") == 0) {
    edit = PE_BOOL;
    source.ExpectTokenString("}");
  } else if (token.Icmp("Color") == 0) {
    edit = PE_COLOR;
    if (source.ExpectAnyToken(token)) {
      if (token == "}") {
        editRange[0] = 0.0f;
        editRange[1] = 1.0f;
      } else {
        source.UnreadToken(token);
        editRange[0] = source.ParseFloat();
        source.ExpectTokenString(",");
        editRange[1] = source.ParseFloat();
      }
    }
  } else if (token.Icmp("Range") == 0) {
    edit = PE_RANGE;
    editRange[0] = source.ParseFloat();
    source.ExpectTokenString(",");
    editRange[1] = source.ParseFloat();
    source.ExpectTokenString("}");
  } else {
    source.Warning("Unknown edit specifier '%s'\n", token.data);
  }
}
