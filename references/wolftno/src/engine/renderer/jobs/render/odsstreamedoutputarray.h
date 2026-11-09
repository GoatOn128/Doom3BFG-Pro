#pragma once

#include <cassert>
#include <cstddef>

template <typename T, int BufferSize> class alignas(16) idODSStreamedOutputArray {
public:
  idODSStreamedOutputArray(T* array, int* numElements, int maxElements)
      : localNum(0), outArray(array), outNum(numElements), outMax(maxElements) {
    static_assert(BufferSize > 0 && (BufferSize & (BufferSize - 1)) == 0,
                  "stream buffer size must be a power of two");
    static_assert(((BufferSize * sizeof(T)) & 15) == 0,
                  "stream buffer byte size must be 16-byte aligned");
  }

  ~idODSStreamedOutputArray() { *outNum = localNum; }

  int Num() const { return localNum; }
  void Append(T element) {
    assert(localNum < outMax);
    outArray[localNum++] = element;
  }
  T& Alloc() {
    assert(localNum < outMax);
    return outArray[localNum++];
  }

private:
  int localNum;
  std::uint32_t alignmentPadding;
  T* outArray;
  int* outNum;
  int outMax;
  std::uint32_t tailPadding;
};

static_assert(sizeof(idODSStreamedOutputArray<int, 128>) == 0x20,
              "streamed output array ABI drift");
