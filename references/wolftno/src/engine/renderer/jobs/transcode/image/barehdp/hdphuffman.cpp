#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>
#include <limits>

namespace {

constexpr std::int16_t kHuffLookup4[40] = {
    19, 19, 19, 19, 27, 27, 27, 27, 10, 10, 10, 10, 10, 10,
    10, 10, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
    1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,
};

constexpr std::int16_t kHuffLookup5[2][42] = {
    {28, 28, 36, 36, 19, 19, 19, 19, 10, 10, 10, 10, 10, 10,
     10, 10, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {11, 11, 11, 11, 19, 19, 19, 19, 27, 27, 27, 27, 35, 35,
     35, 35, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
};

constexpr std::int16_t kHuffLookup6[4][44] = {
    {13, 29, 44, 44, 19, 19, 19, 19, 34, 34, 34, 34, 34, 34,
     34, 34, 1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0},
    {12, 12, 28, 28, 43, 43, 43, 43, 2,  2,  2,  2,  2,  2,
     2,  2,  18, 18, 18, 18, 18, 18, 18, 18, 34, 34, 34, 34,
     34, 34, 34, 34, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0},
    {4,  4,  12, 12, 43, 43, 43, 43, 18, 18, 18, 18, 18, 18,
     18, 18, 26, 26, 26, 26, 26, 26, 26, 26, 34, 34, 34, 34,
     34, 34, 34, 34, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0},
    {5,  13, 36, 36, 43, 43, 43, 43, 18, 18, 18, 18, 18, 18,
     18, 18, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
     25, 25, 25, 25, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0},
};

constexpr std::int16_t kHuffLookup7[2][46] = {
    {45, 53, 36, 36, 27, 27, 27, 27, 2,  2,  2,  2,  2,  2,
     2,  2,  10, 10, 10, 10, 10, 10, 10, 10, 18, 18, 18, 18,
     18, 18, 18, 18, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0},
    {-32736, 37, 28, 28, 19, 19, 19, 19, 10, 10, 10, 10,
     10,     10, 10, 10, 1,  1,  1,  1,  1,  1,  1,  1,
     1,      1,  1,  1,  1,  1,  1,  1,  5,  6,  0,  0,
     0,      0,  0,  0,  0,  0,  0,  0,  0,  0},
};

constexpr std::int16_t kHuffLookup8[2][48] = {
    {53, 21, 28, 28, 11, 11, 11, 11, 43, 43, 43, 43, 59, 59,
     59, 59, 2,  2,  2,  2,  2,  2,  2,  2,  34, 34, 34, 34,
     34, 34, 34, 34, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0},
    {52, 52, 20, 20, 3,  3,  3,  3,  11, 11, 11, 11, 27, 27,
     27, 27, 35, 35, 35, 35, 43, 43, 43, 43, 58, 58, 58, 58,
     58, 58, 58, 58, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0},
};

constexpr std::int16_t kHuffLookup9[2][50] = {
    {13, 29, 37, 61, 20, 20, 68, 68, 3,  3,  3,  3,  51, 51,
     51, 51, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41,
     41, 41, 41, 41, 0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     0,  0,  0,  0,  0,  0,  0,  0},
    {-32736, 53, 28, 28, 11, 11, 11, 11, 19, 19, 19, 19,
     43,     43, 43, 43, 1,  1,  1,  1,  1,  1,  1,  1,
     1,      1,  1,  1,  1,  1,  1,  1,  -32734, 4, 7, 8,
     0,      0,  0,  0,  0,  0,  0,  0,  0,      0, 0, 0,
     0,      0},
};

constexpr std::int16_t kHuffLookup12[5][56] = {
    {-32736, 5,  76, 76, 37, 53, 69, 85, 43, 43, 43, 43, 91, 91,
     91,     91, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57, 57,
     57,     57, 57, 57, -32734, 1, 2, 3, 0, 0, 0, 0, 0, 0,
     0,      0,  0,  0,  0,      0, 0, 0, 0, 0, 0, 0, 0, 0},
    {-32736, 85, 13, 53, 4,  4,  36, 36, 43, 43, 43, 43, 67, 67,
     67,     67, 75, 75, 75, 75, 91, 91, 91, 91, 58, 58, 58, 58,
     58,     58, 58, 58, 2,  3,  0,  0,  0,  0,  0,  0,  0,  0,
     0,      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0},
    {-32736, 37, 92, 92, 11, 11, 11, 11, 43, 43, 43, 43, 59, 59,
     59,     59, 67, 67, 67, 67, 75, 75, 75, 75, 2,  2,  2,  2,
     2,      2,  2,  2,  -32734, -32732, 2, 3, 6, 10, 0, 0, 0, 0,
     0,      0,  0,  0,  0,      0,      0, 0, 0, 0,  0, 0,
     0,      0},
    {-32736, 29, 37, 69, 3,  3,  3,  3,  43, 43, 43, 43, 59, 59,
     59,     59, 75, 75, 75, 75, 91, 91, 91, 91, 10, 10, 10, 10,
     10,     10, 10, 10, -32734, 10, 2,  6,  0,  0,  0,  0,  0,  0,
     0,      0,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,  0},
    {-32736, 93, 28, 28, 60, 60, 76, 76, 3,  3,  3,  3,  43, 43,
     43,     43, 9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,  9,
     9,      9,  9,  9,  -32734, -32732, -32730, 2, 4, 8, 6, 10, 0, 0,
     0,      0,  0,  0,  0,      0,      0,      0, 0, 0, 0, 0,  0, 0},
};

constexpr int kCode4[] = {4, 1, 1, 1, 2, 0, 3, 1, 3};
constexpr int kCode5[] = {
    5, 1, 1, 1, 2, 1, 3, 0, 4, 1, 4,
    5, 1, 1, 0, 3, 1, 3, 2, 3, 3, 3,
};
constexpr int kDelta5[] = {0, -1, 0, 1, 1};
constexpr int kCode6[] = {
    6, 1, 1, 0, 5, 1, 3, 1, 5, 1, 2, 1, 4,
    6, 1, 2, 0, 4, 2, 2, 1, 4, 3, 2, 1, 3,
    6, 0, 4, 1, 4, 1, 2, 2, 2, 3, 2, 1, 3,
    6, 0, 5, 1, 5, 1, 2, 1, 1, 1, 4, 1, 3,
};
constexpr int kDelta6[] = {
    -1, 1, 1, 1, 0, 1, -2, 0, 0, 2, 0, 0, -1, -1, 0, 1, -2, 0,
};
constexpr int kCode7[] = {
    7, 1, 2, 2, 2, 3, 2, 1, 3, 1, 4, 0, 5, 1, 5,
    7, 1, 1, 1, 2, 1, 3, 1, 4, 1, 5, 0, 6, 1, 6,
};
constexpr int kDelta7[] = {1, 0, -1, -1, -1, -1, -1};
constexpr int kCode8[] = {
    8, 2, 2, 1, 3, 1, 5, 1, 4, 3, 2, 2, 3, 0, 5, 3, 3,
    8, 1, 3, 2, 3, 1, 4, 3, 3, 4, 3, 5, 3, 0, 4, 3, 2,
};
constexpr int kDelta8[] = {-1, 0, 1, 1, -1, 0, 1, 1};
constexpr int kCode9[] = {
    9, 2, 3, 0, 5, 2, 4, 1, 5, 2, 5, 1, 1, 3, 3, 3, 5, 3, 4,
    9, 1, 1, 1, 3, 2, 3, 1, 4, 1, 6, 3, 3, 1, 5, 0, 7, 1, 7,
};
constexpr int kDelta9[] = {2, 2, 1, 1, -1, -2, -2, -2, -3};
constexpr int kCode12[] = {
    12, 1, 5, 1, 6, 0, 7, 1, 7, 4, 5, 2, 3, 5, 5, 1, 1, 6, 5, 1, 4, 7, 5, 3, 3,
    12, 2, 4, 2, 5, 0, 6, 1, 6, 3, 4, 2, 3, 3, 5, 3, 2, 3, 3, 4, 3, 1, 5, 5, 3,
    12, 3, 2, 1, 3, 0, 7, 1, 7, 1, 5, 2, 3, 2, 7, 3, 3, 4, 3, 5, 3, 3, 7, 1, 4,
    12, 1, 3, 3, 2, 0, 7, 1, 5, 2, 5, 2, 3, 1, 7, 3, 3, 3, 5, 4, 3, 1, 6, 5, 3,
    12, 2, 3, 1, 1, 1, 7, 1, 4, 2, 7, 3, 3, 0, 8, 2, 4, 3, 7, 3, 4, 1, 8, 1, 5,
};
constexpr int kDelta12[] = {
    1, 1, 1, 1, 1, 0, 0, -1, 2, 1, 0, 0,
    2, 2, -1, -1, -1, 0, -2, -1, 0, 0, -2, -1,
    -1, 1, 0, 2, 0, 0, 0, 0, -2, 0, 1, 1,
    0, 1, 0, 1, -2, 0, -1, -1, -2, -1, -2, -2,
};

constexpr int kMaxTables[13] = {0, 0, 0, 0, 1, 2, 4,
                                2, 2, 2, 0, 0, 5};
constexpr int kSecondDiscriminant[13] = {0, 0, 0, 0, 0, 0, 1,
                                        0, 0, 0, 0, 0, 1};

}  // namespace

void InitAdaptiveHuffman(hdpCodecParms_t*,
                         hdpAdaptiveHuffman_t* adaptiveHuffman,
                         int numSymbols) {
  *adaptiveHuffman = {};
  adaptiveHuffman->numSymbols = numSymbols;
}

void ShutdownAdaptiveHuffman(hdpCodecParms_t*,
                             hdpAdaptiveHuffman_t* adaptiveHuffman) {
  adaptiveHuffman->encTable = nullptr;
  adaptiveHuffman->decTable = nullptr;
}

void AdaptiveHuffmanDiscriminant(hdpAdaptiveHuffman_t* adaptiveHuffman) {
  const int numSymbols = adaptiveHuffman->numSymbols;
  int discriminant = adaptiveHuffman->discriminant;
  int discriminant1 = adaptiveHuffman->discriminant1;
  int tableIndex = adaptiveHuffman->tableIndex;

  if (adaptiveHuffman->isInitialized == 0) {
    adaptiveHuffman->isInitialized = 1;
    tableIndex = kSecondDiscriminant[numSymbols];
    discriminant = 0;
    discriminant1 = 0;
  }

  if (discriminant < adaptiveHuffman->lowerBound) {
    --tableIndex;
    discriminant = 0;
    discriminant1 = 0;
  } else {
    const int upperDiscriminant =
        kSecondDiscriminant[numSymbols] == 0 ? discriminant : discriminant1;
    if (upperDiscriminant > adaptiveHuffman->upperBound) {
      ++tableIndex;
      discriminant = 0;
      discriminant1 = 0;
    } else {
      discriminant = std::clamp(discriminant, -64, 64);
      discriminant1 = std::clamp(discriminant1, -64, 64);
    }
  }

  adaptiveHuffman->discriminant = discriminant;
  adaptiveHuffman->discriminant1 = discriminant1;
  adaptiveHuffman->tableIndex = tableIndex;
  adaptiveHuffman->lowerBound =
      tableIndex == 0 ? std::numeric_limits<int>::min() : -8;
  adaptiveHuffman->upperBound =
      tableIndex == kMaxTables[numSymbols] - 1 ? 0x40000000 : 8;

  const int tableStride = 2 * numSymbols + 1;
  switch (numSymbols) {
    case 4:
      adaptiveHuffman->encTable = kCode4;
      adaptiveHuffman->decTable = kHuffLookup4;
      adaptiveHuffman->deltaTable = nullptr;
      adaptiveHuffman->deltaTable1 = nullptr;
      break;
    case 5:
      adaptiveHuffman->encTable = kCode5 + tableIndex * tableStride;
      adaptiveHuffman->decTable = kHuffLookup5[tableIndex];
      adaptiveHuffman->deltaTable = kDelta5;
      adaptiveHuffman->deltaTable1 = nullptr;
      break;
    case 6:
      adaptiveHuffman->encTable = kCode6 + tableIndex * tableStride;
      adaptiveHuffman->decTable = kHuffLookup6[tableIndex];
      adaptiveHuffman->deltaTable =
          kDelta6 + numSymbols * (tableIndex - 1 + (tableIndex == 0));
      adaptiveHuffman->deltaTable1 =
          kDelta6 + numSymbols *
                        (tableIndex -
                         (tableIndex + 1 == kMaxTables[numSymbols]));
      break;
    case 7:
      adaptiveHuffman->encTable = kCode7 + tableIndex * tableStride;
      adaptiveHuffman->decTable = kHuffLookup7[tableIndex];
      adaptiveHuffman->deltaTable = kDelta7;
      adaptiveHuffman->deltaTable1 = nullptr;
      break;
    case 8:
      adaptiveHuffman->encTable = kCode8 + tableIndex * tableStride;
      adaptiveHuffman->decTable = kHuffLookup8[0];
      adaptiveHuffman->deltaTable = kDelta8;
      adaptiveHuffman->deltaTable1 = nullptr;
      break;
    case 9:
      adaptiveHuffman->encTable = kCode9 + tableIndex * tableStride;
      adaptiveHuffman->decTable = kHuffLookup9[tableIndex];
      adaptiveHuffman->deltaTable = kDelta9;
      adaptiveHuffman->deltaTable1 = nullptr;
      break;
    case 12:
      adaptiveHuffman->encTable = kCode12 + tableIndex * tableStride;
      adaptiveHuffman->decTable = kHuffLookup12[tableIndex];
      adaptiveHuffman->deltaTable =
          kDelta12 + numSymbols * (tableIndex - 1 + (tableIndex == 0));
      adaptiveHuffman->deltaTable1 =
          kDelta12 + numSymbols *
                         (tableIndex -
                          (tableIndex + 1 == kMaxTables[numSymbols]));
      break;
    default:
      break;
  }
}
