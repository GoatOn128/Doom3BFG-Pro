#include "hashindex.h"

#include <algorithm>
#include <cstdlib>

namespace {
int invalidIndexStorage = -1;
}

int* idHashIndex::INTERNAL_INVALID_INDEX = &invalidIndexStorage;

idHashIndex::idHashIndex(int initialHashSize, int initialIndexSize) : memTag(0) {
  InternalInit(initialHashSize, initialIndexSize);
}

idHashIndex::~idHashIndex() {
  InternalFree();
}

void idHashIndex::InternalInit(int initialHashSize, int initialIndexSize) {
  hashSize = initialHashSize;
  indexSize = initialIndexSize;
  hash = INTERNAL_INVALID_INDEX;
  indexChain = INTERNAL_INVALID_INDEX;
  granularity = 1024;
  hashMask = initialHashSize - 1;
  lookupMask = 0;
}

void idHashIndex::InternalFree() {
  if (hash != INTERNAL_INVALID_INDEX) {
    delete[] hash;
    hash = INTERNAL_INVALID_INDEX;
  }
  if (indexChain != INTERNAL_INVALID_INDEX) {
    delete[] indexChain;
    indexChain = INTERNAL_INVALID_INDEX;
  }
  lookupMask = 0;
}

void idHashIndex::InternalResizeIndex(int newIndexSize) {
  if (newIndexSize <= indexSize)
    return;
  int roundedSize = newIndexSize;
  const int remainder = roundedSize % granularity;
  if (remainder != 0)
    roundedSize += granularity - remainder;
  if (indexChain == INTERNAL_INVALID_INDEX) {
    indexSize = roundedSize;
    return;
  }
  int* replacement = new int[roundedSize];
  std::copy(indexChain, indexChain + indexSize, replacement);
  std::fill(replacement + indexSize, replacement + roundedSize, -1);
  delete[] indexChain;
  indexChain = replacement;
  indexSize = roundedSize;
}

int idHashIndex::GetSpread() const {
  if (hash == INTERNAL_INVALID_INDEX)
    return 100;
  int total = 0;
  int error = 0;
  int* counts = new int[hashSize]{};
  for (int bucket = 0; bucket < hashSize; ++bucket) {
    for (int index = hash[bucket]; index >= 0; index = indexChain[index]) {
      ++counts[bucket];
    }
    total += counts[bucket];
  }
  if (total <= 1) {
    delete[] counts;
    return 100;
  }
  const int average = total / hashSize;
  for (int bucket = 0; bucket < hashSize; ++bucket) {
    const int deviation = std::abs(counts[bucket] - average);
    if (deviation > 1)
      error += deviation - 1;
  }
  delete[] counts;
  return 100 - 100 * error / total;
}

void idHashIndex::InternalAllocate(int newHashSize, int newIndexSize) {
  InternalFree();
  hashSize = newHashSize;
  hash = new int[hashSize];
  std::fill(hash, hash + hashSize, -1);
  indexSize = newIndexSize;
  indexChain = new int[indexSize];
  std::fill(indexChain, indexChain + indexSize, -1);
  lookupMask = -1;
  hashMask = hashSize - 1;
}
void idHashIndex::RemoveIndex(int key, int index) {
  Remove(key, index);
  if (hash == INTERNAL_INVALID_INDEX)
    return;

  int maximumReferenced = index;
  for (int i = 0; i < hashSize; ++i) {
    if (hash[i] >= index) {
      maximumReferenced = std::max(maximumReferenced, hash[i]);
      --hash[i];
    }
  }
  for (int i = 0; i < indexSize; ++i) {
    if (indexChain[i] >= index) {
      maximumReferenced = std::max(maximumReferenced, indexChain[i]);
      --indexChain[i];
    }
  }
  for (int i = index; i < maximumReferenced; ++i)
    indexChain[i] = indexChain[i + 1];
  indexChain[maximumReferenced] = -1;
}
