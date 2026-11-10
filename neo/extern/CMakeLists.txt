file(GLOB GLAD_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/*.c")
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${GLAD_SOURCES})

if(GLAD_SOURCES)
	add_library(darklight_glad STATIC ${GLAD_SOURCES})
	darklight_configure_target(darklight_glad true)
	darklight_set_folder(darklight_glad "third-party")
	target_include_directories(darklight_glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/glad/include")
endif()

file(GLOB ZLIB_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/zlib/*.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/zlib/*.h")
list(REMOVE_ITEM ZLIB_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/zlib/example.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/zlib/minigzip.c")

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${ZLIB_SOURCES})

if(ZLIB_SOURCES)
	add_library(darklight_zlib STATIC ${ZLIB_SOURCES})
	darklight_configure_target(darklight_zlib true)
	darklight_set_folder(darklight_zlib "third-party")
	target_include_directories(darklight_zlib PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/zlib")
endif()

file(GLOB JPEG_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/*.h")
list(REMOVE_ITEM JPEG_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jcapistd.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jctrans.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jload.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jmemansi.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jmemdos.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jmemname.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6/jpegtran.cpp")
source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${JPEG_SOURCES})

if(JPEG_SOURCES)
	add_library(darklight_jpeg STATIC ${JPEG_SOURCES})
	darklight_configure_target(darklight_jpeg true)
	darklight_set_folder(darklight_jpeg "third-party")
	target_include_directories(darklight_jpeg PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6")
endif()

set(JXRLIB_ENCODER_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys/adapthuff.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys/image.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys/strcodec.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys/strPredQuant.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys/strTransform.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys/perfTimerANSI.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/encode/encode.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/encode/segenc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/encode/strenc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/encode/strFwdTransform.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/encode/strPredQuantEnc.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode/decode.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode/postprocess.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode/segdec.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode/strdec.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode/strInvTransform.c"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode/strPredQuantDec.c")

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${JXRLIB_ENCODER_SOURCES})

foreach(_source IN LISTS JXRLIB_ENCODER_SOURCES)
	if(NOT EXISTS "${_source}")
		message(FATAL_ERROR "A required JPEG XR source file is missing: ${_source}")
	endif()
endforeach()

add_library(jxrlib_encoder STATIC ${JXRLIB_ENCODER_SOURCES})
target_include_directories(jxrlib_encoder PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/common/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/sys"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/encode"
	"${CMAKE_CURRENT_SOURCE_DIR}/jxrlib/image/decode")

target_compile_definitions(jxrlib_encoder PRIVATE __ANSI__ DISABLE_PERF_MEASUREMENT)

if(MSVC)
	target_compile_options(jxrlib_encoder PRIVATE /W0)
else()
	target_compile_options(jxrlib_encoder PRIVATE -w)
endif()

darklight_configure_target(jxrlib_encoder true)
darklight_set_folder(jxrlib_encoder "third-party")

add_library(external INTERFACE)
darklight_set_folder(external "third-party")
if(TARGET darklight_zlib)
	target_link_libraries(external INTERFACE darklight_zlib)
endif()
if(TARGET darklight_jpeg)
	target_link_libraries(external INTERFACE darklight_jpeg)
endif()
if(TARGET darklight_glad)
	target_link_libraries(external INTERFACE darklight_glad)
endif()