#include "engine/guis/swf/swf.h"

#include <cstdlib>
#include <cstring>
#include <zlib.h>

namespace {
void* SWFZAlloc(void*, unsigned int items, unsigned int size) {
  return std::malloc(static_cast<std::size_t>(items) * size);
}
void SWFZFree(void*, void* pointer) { std::free(pointer); }
} // namespace

bool idSWF::Inflate(const std::uint8_t* input, unsigned int inputSize, std::uint8_t* output,
                    unsigned int outputSize) {
  z_stream stream{};
  stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(input));
  stream.avail_in = inputSize;
  stream.next_out = reinterpret_cast<Bytef*>(output);
  stream.avail_out = outputSize;
  stream.zalloc = SWFZAlloc;
  stream.zfree = SWFZFree;
  inflateInit(&stream);
  const bool success = inflate(&stream, Z_FINISH) == Z_STREAM_END;
  inflateEnd(&stream);
  return success;
}
