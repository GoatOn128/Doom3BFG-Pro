#pragma once

#include <cstdint>

bool JxrLibCompressBareYCoCg( const std::uint8_t * rgba,
	std::uint8_t * output, int outputCapacity, int width, int height,
	int inputByteStride, int quality, int * outputBytes );

bool JxrLibCompressBareMono( const std::uint8_t * gray,
	std::uint8_t * output, int outputCapacity, int width, int height,
	int inputByteStride, int quality, int * outputBytes );

bool JxrLibDecompressBareRGB( const std::uint8_t * input, int inputBytes,
	std::uint8_t * rgba, int width, int height, int outputByteStride );
