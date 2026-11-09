#include "engine/renderer/declrenderprog.h"

#include "engine/renderer/declrenderprogattribs.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/hashing/md.h"
#include "shared/idlib/lib.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr unsigned int GL_FRAGMENT_SHADER = 0x8B30;
constexpr unsigned int GL_VERTEX_SHADER = 0x8B31;
constexpr unsigned int GL_COMPILE_STATUS = 0x8B81;
constexpr unsigned int GL_LINK_STATUS = 0x8B82;
constexpr unsigned int GL_INFO_LOG_LENGTH = 0x8B84;
constexpr unsigned int GL_PROGRAM_BINARY_RETRIEVABLE_HINT = 0x8257;
constexpr unsigned int GL_PROGRAM_BINARY_LENGTH = 0x8741;
constexpr unsigned int GL_INVALID_INDEX = ~0u;
constexpr int GLSL_BINARY_MAGIC = 0x01020300;

struct glslProgramBinary_t {
  int magic;
  unsigned int declMD5Checksum;
  bool useUniformArrays;
  unsigned char alignmentPadding[3];
  int binaryFormat;
  int binaryLength;
};

static_assert(sizeof(glslProgramBinary_t) == 20,
              "GLSL program-binary header ABI drift");

unsigned int CombinedProgramChecksum(const idDeclRenderProg& program) {
  return program.md5Checksum ^
         MD5_BlockChecksum(
             reinterpret_cast<const std::uint8_t*>(program.data.vertexCode.data),
             static_cast<std::size_t>(program.data.vertexCode.Length())) ^
         MD5_BlockChecksum(
             reinterpret_cast<const std::uint8_t*>(program.data.fragmentCode.data),
             static_cast<std::size_t>(program.data.fragmentCode.Length()));
}

void StoreUniform(void*& destination, int location) {
  destination = reinterpret_cast<void*>(
      static_cast<std::intptr_t>(location + 1));
}

idStr FixedGeneratedShaderName(const char* basePath, const char* extension,
                               const char* inputPath) {
  idStr fullPath(inputPath != nullptr ? inputPath : "");
  fullPath.SetFileExtension(extension);
  idStr fileName;
  fullPath.ExtractFileName(fileName);

  idStr result(basePath);
  if (fileName.Length() < 38) {
    result.AppendPath(fullPath.data);
    return result;
  }

  idStr filePath;
  fullPath.ExtractFilePath(filePath);
  result.AppendPath(filePath.data);
  fileName.StripFileExtension();
  for (int index = 0; index < fileName.Length(); ++index) {
    if (index != 0 && index % 30 == 0)
      result.Append('/');
    result.Append(fileName[index]);
  }
  result.SetFileExtension(extension);
  return result;
}

bool ReadProgramBinary(const idStr& path, std::vector<std::uint8_t>& bytes) {
  return fileSystem != nullptr && fileSystem->ReadFile(path.data, bytes);
}

void WriteProgramBinary(const idStr& path, const glslProgramBinary_t& header,
                        const std::uint8_t* binary, int binaryLength) {
  if (fileSystem == nullptr)
    return;
  idFileLocal file(fileSystem->OpenFileWrite(path.data));
  if (file.file == nullptr)
    return;
  file->Write(&header, sizeof(header));
  file->Write(binary, static_cast<std::size_t>(binaryLength));
}

}  // namespace

idCVar r_loadPrecompiledShaders(
    "r_loadPrecompiledShaders", "0", 65537,
    "If true, cache compiled shaders to generated directory.");
idCVar r_dumpGeneratedGLSL(
    "r_dumpGeneratedGLSL", "0", 1,
    "If true, will dump out shader text into base/generated.");
idCVar r_amdShaderLinkCrashWorkaroundTest(
    "r_amdShaderLinkCrashWorkaroundTest", "0", 1,
    "Insert a glFlush after glLinkProgram");

extern idCVar r_useUniformArrays;

void idDeclRenderProg::FreeData() {
  if (vertexProgram != nullptr) {
    _glewDeleteProgram(
        static_cast<unsigned int>(reinterpret_cast<std::uintptr_t>(vertexProgram)));
    vertexProgram = nullptr;
    fragmentProgram = nullptr;
  }
  Clear();
}

unsigned int CompileGLSL(idStr* programGLSL, unsigned int* checksum,
                         const bool isVertexProgram, const char* name,
                         const char* fileName) {
  const char* programType = isVertexProgram ? "vertex" : "fragment";
  *checksum = MD5_BlockChecksum(
      reinterpret_cast<const std::uint8_t*>(programGLSL->data),
      static_cast<std::size_t>(programGLSL->Length()));

  const char* source = programGLSL->data;
  const unsigned int shader = _glewCreateShader(
      isVertexProgram ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
  if (shader == 0)
    return 0;

  _glewShaderSource(shader, 1, &source, nullptr);
  _glewCompileShader(shader);

  int infoLogLength = 0;
  _glewGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);
  if (infoLogLength > 1) {
    std::vector<char> infoLog(static_cast<std::size_t>(infoLogLength), '\0');
    int charsWritten = 0;
    _glewGetShaderInfoLog(shader, infoLogLength, &charsWritten, infoLog.data());
    if (std::strstr(infoLog.data(),
                    "successfully compiled to run on hardware") == nullptr &&
        std::strstr(infoLog.data(), "No errors.") == nullptr) {
      idLib::Printf("While compiling %s program %s from %s\n", programType,
                    name, fileName);
      idLib::Printf("-----------------\n");
      const char* lineStart = source;
      int lineNumber = 1;
      while (*lineStart != '\0') {
        const char* lineEnd = std::strchr(lineStart, '\n');
        if (lineEnd == nullptr) {
          idLib::Printf("%3d: %s\n", lineNumber, lineStart);
          break;
        }
        idLib::Printf("%3d: %.*s\n", lineNumber,
                      static_cast<int>(lineEnd - lineStart), lineStart);
        lineStart = lineEnd + 1;
        ++lineNumber;
      }
      idLib::Printf("-----------------\n");
      idLib::Printf("%s\n", infoLog.data());
    }
  }

  int compiled = 0;
  _glewGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (compiled != 0)
    return shader;
  _glewDeleteShader(shader);
  return 0;
}

void idDeclRenderProg::UploadTargetCode() {
  if (idLib::headless)
    return;

  int textureUnit = 0;
  unsigned int program = 0;
  idStr glslBinaryFilename("generated/glslBinaries/");
  glslBinaryFilename.Append(name.str);

  const bool useBinaryCache =
      r_loadPrecompiledShaders.valueInteger != 0 &&
      idLib::production != PROD_BUILDING &&
      glConfig.getProgramBinaryAvailable;

  if (useBinaryCache) {
    std::vector<std::uint8_t> fileData;
    if (ReadProgramBinary(glslBinaryFilename, fileData) &&
        fileData.size() > sizeof(glslProgramBinary_t)) {
      glslProgramBinary_t header{};
      std::memcpy(&header, fileData.data(), sizeof(header));
      const bool validLength =
          header.binaryLength >= 0 &&
          static_cast<std::size_t>(header.binaryLength) <=
              fileData.size() - sizeof(header);
      if (header.magic == GLSL_BINARY_MAGIC &&
          header.declMD5Checksum == CombinedProgramChecksum(*this) &&
          header.useUniformArrays ==
              (r_useUniformArrays.valueInteger != 0) &&
          validLength) {
        program = _glewCreateProgram();
        _glewProgramBinary(program, static_cast<unsigned int>(header.binaryFormat),
                           fileData.data() + sizeof(header),
                           header.binaryLength);
        int linked = 0;
        _glewGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked != 0) {
          idLib::Printf("Loaded binary shader program %s\n", name.str);
        } else {
          _glewDeleteProgram(program);
          program = 0;
          idLib::Warning(
              "Failed to load binary shader program %s, rebuilding.",
              name.str);
        }
      }
    }
  }

  if (program == 0) {
    if (r_dumpGeneratedGLSL.valueInteger != 0 && fileSystem != nullptr) {
      const idStr vertexName =
          FixedGeneratedShaderName("generated/glsl", "vglsl", name.str);
      fileSystem->WriteFile(vertexName.data, data.vertexCode.data,
                            static_cast<std::size_t>(data.vertexCode.Length()));
      const idStr fragmentName =
          FixedGeneratedShaderName("generated/glsl", "fglsl", name.str);
      fileSystem->WriteFile(fragmentName.data, data.fragmentCode.data,
                            static_cast<std::size_t>(data.fragmentCode.Length()));
    }

    const unsigned int vertexShader =
        CompileGLSL(&data.vertexCode, &vertexProgramChecksum, true, name.str,
                    GetFileName());
    const unsigned int fragmentShader =
        CompileGLSL(&data.fragmentCode, &fragmentProgramChecksum, false,
                    name.str, GetFileName());

    if (vertexShader != 0 && fragmentShader != 0) {
      program = _glewCreateProgram();
      _glewAttachShader(program, vertexShader);
      _glewAttachShader(program, fragmentShader);

      for (int index = 0; attribsPC[index].glsl != nullptr; ++index) {
        if ((attribsPC[index].flags & AT_VS_IN) != 0) {
          _glewBindAttribLocation(program,
                                  static_cast<unsigned int>(attribsPC[index].bind),
                                  attribsPC[index].glsl);
        }
      }
      for (int index = 0; attribsPC[index].glsl != nullptr; ++index) {
        if ((attribsPC[index].flags & AT_PS_OUT) != 0) {
          _glewBindFragDataLocation(
              program, static_cast<unsigned int>(attribsPC[index].bind),
              attribsPC[index].glsl);
        }
      }

      if (useBinaryCache) {
        _glewProgramParameteri(program, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, 1);
      }
      _glewLinkProgram(program);
      if (r_amdShaderLinkCrashWorkaroundTest.valueInteger != 0)
        glFlush();

      _glewDetachShader(program, vertexShader);
      _glewDetachShader(program, fragmentShader);
      _glewDeleteShader(vertexShader);
      _glewDeleteShader(fragmentShader);

      int infoLogLength = 0;
      _glewGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);
      if (infoLogLength > 1) {
        std::vector<char> infoLog(static_cast<std::size_t>(infoLogLength), '\0');
        int charsWritten = 0;
        _glewGetProgramInfoLog(program, infoLogLength, &charsWritten,
                               infoLog.data());
        if (std::strstr(infoLog.data(),
                        "Vertex shader(s) linked, fragment shader(s) linked.") ==
                nullptr &&
            std::strstr(infoLog.data(), "No errors.") == nullptr) {
          idLib::Printf("While linking %s from %s\n", name.str,
                        GetFileName());
          idLib::Printf("%s\n", infoLog.data());
        }
      }

      int linked = 0;
      _glewGetProgramiv(program, GL_LINK_STATUS, &linked);
      if (linked == 0) {
        _glewDeleteProgram(program);
        idLib::Error("Failed to link render prog %s from %s", name.str,
                     GetFileName());
      }

      if (useBinaryCache) {
        int binaryLength = 0;
        _glewGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binaryLength);
        if (binaryLength <= 0) {
          idLib::Warning("Shader binary length <= 0");
        } else {
          std::vector<std::uint8_t> binaryData(
              static_cast<std::size_t>(binaryLength));
          int storedLength = 0;
          unsigned int binaryFormat = 0;
          _glewGetProgramBinary(program, binaryLength, &storedLength,
                                &binaryFormat, binaryData.data());
          glslProgramBinary_t header{};
          header.magic = GLSL_BINARY_MAGIC;
          header.declMD5Checksum = CombinedProgramChecksum(*this);
          header.useUniformArrays = r_useUniformArrays.valueInteger != 0;
          header.binaryFormat = static_cast<int>(binaryFormat);
          header.binaryLength = binaryLength;
          idLib::Printf("Writing shader binary: %s\n",
                        glslBinaryFilename.data);
          WriteProgramBinary(glslBinaryFilename, header, binaryData.data(),
                             binaryLength);
        }
      }
    } else {
      if (vertexShader != 0)
        _glewDeleteShader(vertexShader);
      if (fragmentShader != 0)
        _glewDeleteShader(fragmentShader);
      idLib::Error("Failed to compile render prog %s from %s", name.str,
                   GetFileName());
    }
  }

  vertexProgram = reinterpret_cast<void*>(static_cast<std::uintptr_t>(program));
  fragmentProgram = reinterpret_cast<void*>(static_cast<std::uintptr_t>(program));

  if (r_useUniformArrays.valueInteger != 0) {
    if (data.vertexParms.num > 0) {
      const int location = _glewGetUniformLocation(program, "_va_");
      StoreUniform(vertexUniforms.ptr[0], location);
      if (location == -1)
        idLib::Warning("%s: Could not get vertex uniform _va_", name.str);
    }
  } else {
    for (int index = 0; index < data.vertexParms.num; ++index) {
      idStr uniformName("rp_name_");
      uniformName.Append(
          renderThreadParmState->GetParmNameForIndex(data.vertexParms[index]));
      const int location =
          _glewGetUniformLocation(program, uniformName.data);
      StoreUniform(vertexUniforms.ptr[index], location);
      if (location == -1) {
        idLib::Warning("%s: Could not get vertex program uniform %s", name.str,
                       uniformName.data);
      }
    }
  }

  StoreUniform(vertexUniforms.ptr[data.vertexParms.num],
               _glewGetUniformLocation(program, "skinOffsets"));
  StoreUniform(vertexUniforms.ptr[data.vertexParms.num + 1],
               _glewGetUniformLocation(program, "hardwareSkinning"));
  const unsigned int uniformBlockIndex =
      _glewGetUniformBlockIndex(program, "matrices_ubo");
  if (uniformBlockIndex == GL_INVALID_INDEX) {
    vertexUniforms.ptr[data.vertexParms.num + 2] = nullptr;
  } else {
    vertexUniforms.ptr[data.vertexParms.num + 2] =
        reinterpret_cast<void*>(static_cast<std::uintptr_t>(uniformBlockIndex + 1));
    _glewUniformBlockBinding(program, uniformBlockIndex, 0);
  }

  if (r_useUniformArrays.valueInteger != 0) {
    if (data.fragmentParms.num > 0) {
      const int location = _glewGetUniformLocation(program, "_fa_");
      StoreUniform(fragmentUniforms.ptr[0], location);
      if (location == -1)
        idLib::Warning("%s: Could not get fragment uniform _fa_", name.str);
    }
  } else {
    for (int index = 0; index < data.fragmentParms.num; ++index) {
      idStr uniformName("rp_name_");
      uniformName.Append(renderThreadParmState->GetParmNameForIndex(
          data.fragmentParms[index]));
      const int location =
          _glewGetUniformLocation(program, uniformName.data);
      StoreUniform(fragmentUniforms.ptr[index], location);
      if (location == -1) {
        idLib::Warning("%s: Could not get fragment program uniform %s",
                       name.str, uniformName.data);
      }
    }
  }

  _glewUseProgram(program);
  for (int index = 0; index < data.textureParms.num; ++index) {
    idStr samplerName("samp_");
    samplerName.Append(
        renderThreadParmState->GetParmNameForIndex(data.textureParms[index]));
    const int location = _glewGetUniformLocation(program, samplerName.data);
    if (location == -1) {
      data.textureParms.RemoveIndex(index);
      data.samplerParms.RemoveIndex(index);
      --index;
      --textureUnit;
    } else {
      _glewUniform1i(location, textureUnit);
    }
    ++textureUnit;
  }
}
