#pragma once

#include "shared/idlib/filesystem/file.h"

#include <cstdint>

struct png_struct_def;
class idCVar;

extern idCVar image_screenshotQuality;
extern idCVar image_watermark;
extern idCVar image_pngCompressionLevel;
extern const char* imageExtensions[];

void user_read_data(png_struct_def* pngPointer, std::uint8_t* data,
                    std::uint64_t length);
void R_ReorganizeCubeImages(std::uint8_t** pictures, int size);
void R_WritePNGUserWrite(png_struct_def* pngPointer, std::uint8_t* data,
                         std::uint64_t size);
void R_WritePNGUserFlush(png_struct_def* pngPointer);
void* R_WritePNGUserMemAlloc(png_struct_def* pngPointer, std::uint64_t size);
void R_WritePNGUserMemFree(png_struct_def* pngPointer, void* address);
[[noreturn]] void R_WritePNGUserError(png_struct_def* pngPointer,
                                      const char* message);
void R_WritePNGUserWarning(png_struct_def* pngPointer, const char* message);

void R_LoadBMP(const char* name, std::uint8_t** picture,
               unsigned int* width, unsigned int* height,
               idFileProps* fileProps);
bool R_LoadTGA(const char* name, const std::uint8_t* buffer,
               std::uint64_t bufferSize, std::uint8_t** picture,
               int* width, int* height);
bool R_LoadTGA(const char* name, std::uint8_t** picture, int* width,
               int* height, idFileProps* fileProps);
bool R_ReadDCTFromBuffer(const std::uint8_t* buffer, std::uint64_t bufferSize,
                         std::uint8_t* output, int* width, int* height);
bool R_LoadDCT(const char* name, std::uint8_t** picture, int* width,
               int* height, idFileProps* fileProps);
bool R_LoadCubeImages(const char* imageName, std::uint8_t** pictures,
                      int* outputSize, idFileProps* fileProps);
bool R_LoadPNGFromFile(idFile* file, std::uint8_t** picture,
                       unsigned int* width, unsigned int* height,
                       idFileProps* fileProps);
bool R_LoadPNG(const char* name, std::uint8_t** picture,
               unsigned int* width, unsigned int* height,
               idFileProps* fileProps);
void R_LoadImageDimensions(const char* name, unsigned int* width,
                           unsigned int* height, idFileProps* fileProps);

void R_LoadImage(const char* name, std::uint8_t** picture,
                 unsigned int* width, unsigned int* height,
                 idFileProps* fileProps);

bool R_WritePNGToFile(idFile* file, std::uint8_t* data,
                      unsigned int width, unsigned int height,
                      bool flipVertical);
bool R_WritePNG(const char* filename, std::uint8_t* data,
                unsigned int width, unsigned int height, bool flipVertical,
                fsPath_t basePath);
bool R_WriteJPG(const char* filename, const std::uint8_t* data, int width,
                int height, fsPath_t basePath, bool useMarkers);

bool R_WriteTGA(const char* filename, const std::uint8_t* data, int width,
                int height, int depth, bool flipVertical,
                bool convertToBGR, fsPath_t basePath);
