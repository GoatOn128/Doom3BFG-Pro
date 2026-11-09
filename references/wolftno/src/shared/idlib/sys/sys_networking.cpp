#include "shared/idlib/networking/networking.h"

#include <cstring>

bool idTCP::WriteDataBlock(char* buffer, int size, int timeoutMS) {
  char sizeBuffer[4];
  std::memcpy(sizeBuffer, &size, 4);
  return WriteBlocking(sizeBuffer, 4, timeoutMS) == 4 &&
         WriteBlocking(buffer, size, timeoutMS) == size;
}

std::int64_t idTCP::ReadDataBlock(char* buffer, int bufferSize, int timeoutMS) {
  int size = 0;
  if (ReadBlocking(reinterpret_cast<char*>(&size), 4, timeoutMS) != 4)
    return -1;
  if (size < 0 || size > bufferSize)
    return -1;
  return ReadBlocking(buffer, size, timeoutMS) == size ? size : -1;
}
