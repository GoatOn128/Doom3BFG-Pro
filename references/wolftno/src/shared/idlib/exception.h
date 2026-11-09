#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>

class idException {
public:
  static constexpr int MAX_ERROR_LEN=2048;

  idException()=default;
  explicit idException(const char* message) {
    const char* source=message!=nullptr ? message : "";
    const std::size_t length=std::min<std::size_t>(
        std::strlen(source),MAX_ERROR_LEN-1);
    std::memcpy(error,source,length);
    error[length]='\0';
  }
  const char* GetError() const { return error; }
  char* GetErrorBuffer() { return error; }
  int GetErrorBufferSize() const { return MAX_ERROR_LEN; }

  inline static char error[MAX_ERROR_LEN]{};
};

class idFatalException {
public:
  static constexpr int MAX_ERROR_LEN=2048;

  idFatalException()=default;
  explicit idFatalException(const char* message) {
    const char* source=message!=nullptr ? message : "";
    const std::size_t length=std::min<std::size_t>(
        std::strlen(source),MAX_ERROR_LEN-1);
    std::memcpy(idException::error,source,length);
    idException::error[length]='\0';
  }
  const char* GetError() const { return idException::error; }
  char* GetErrorBuffer() { return idException::error; }
  int GetErrorBufferSize() const { return MAX_ERROR_LEN; }
};

class idNetworkLoadException {
public:
  idNetworkLoadException()=default;
  explicit idNetworkLoadException(const char* message) {
    const char* source=message!=nullptr ? message : "";
    const std::size_t length=std::min<std::size_t>(
        std::strlen(source),idException::MAX_ERROR_LEN-1);
    std::memcpy(idException::error,source,length);
    idException::error[length]='\0';
  }
};

static_assert(sizeof(idException)==1,"idException ABI drift");
static_assert(sizeof(idFatalException)==1,"idFatalException ABI drift");
static_assert(sizeof(idNetworkLoadException)==1,
              "idNetworkLoadException ABI drift");
