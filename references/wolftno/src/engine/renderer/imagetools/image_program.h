#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/text/parser.h"

#include <cstdint>

struct idImageByteArray {
  std::uint8_t* data;
  int width;
  int height;

  void Extrude(int x, int y, int w, int h);
};

void AppendToken(idToken* token);
void MatchAndAppendToken(idParser* parser, const char* match);

void R_PatchAlpha(std::uint8_t* data1, int width1, int height1,
                  std::uint8_t* data2, int width2, int height2);
void R_ImageAdd(std::uint8_t* data1, int width1, int height1,
                std::uint8_t* data2, int width2, int height2);
void R_HeightmapToNormalMap(std::uint8_t* data, int width,
                            unsigned int height, float scale);
void R_ImageScale(std::uint8_t* data, int width, int height, float* scale);
void R_DivideAlpha(std::uint8_t* data, int width, int height);
void R_AddNormalMaps(std::uint8_t* data1, unsigned int width1,
                     unsigned int height1, std::uint8_t* data2, int width2,
                     int height2);
void R_SmoothNormalMap(std::uint8_t* data, int width, int height);
std::uint8_t* R_AtlasBorder(std::uint8_t* picture, int* width, int* height);
std::uint8_t* R_Resize(std::uint8_t* picture, int sourceWidth,
                       int sourceHeight, int newWidth, int newHeight);
std::uint8_t* R_ImageCompileWrinkleMask(
    int* width, int* height, idList<std::uint8_t*, 5>* pictures);
std::uint8_t* R_ImageShuffle(int* sourceImages, char* sourceChannels,
                             std::uint8_t* constants, int* width, int* height,
                             idList<std::uint8_t*, 5>* pictures);
void R_OutlineAlpha(std::uint8_t* data, int width, int height,
                    std::uint8_t* alphaData, int count);
int ParseIntAndAppendToken(idParser* parser);
bool R_ParseImageProgram_r(idParser* parser, std::uint8_t** picture,
                           int* width, int* height,
                           idFileProps* fileProps,
                           const char* imageNameBase);
bool R_LoadImageProgram(idParser* parser, std::uint8_t** picture,
                        int* width, int* height, idFileProps* fileProps);
idStr* R_ParsePastImageProgram(idStr* result, idParser* parser);
bool R_LoadImageProgram(const char* name, std::uint8_t** picture,
                        int* width, int* height, idFileProps* fileProps);

static_assert(sizeof(idImageByteArray) == 16, "image byte-array ABI drift");
