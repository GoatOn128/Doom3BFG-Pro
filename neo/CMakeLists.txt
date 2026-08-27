cmake_minimum_required(VERSION 3.25)

project(Darklight LANGUAGES C CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Darklight currently supports 32-bit Windows builds with MSVC only.")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Darklight is a 32-bit application. Configure CMake with -A Win32.")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(DARKLIGHT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(DARKLIGHT_OUTPUT_DIR "${DARKLIGHT_ROOT}" CACHE PATH
    "Directory in which the Darklight executable is written")

if(DEFINED ENV{DXSDK_DIR})
    file(TO_CMAKE_PATH "$ENV{DXSDK_DIR}" DXSDK_DIR)
else()
    set(DXSDK_DIR "C:/Program Files (x86)/Microsoft DirectX SDK (June 2010)")
endif()

if(NOT EXISTS "${DXSDK_DIR}/Include/XAudio2.h")
    message(FATAL_ERROR
        "The June 2010 DirectX SDK was not found at '${DXSDK_DIR}'. "
        "Install it or set the DXSDK_DIR environment variable.")
endif()

function(darklight_common_target target)
    target_include_directories(${target} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${DXSDK_DIR}/Include")
    target_compile_definitions(${target} PRIVATE
        _CRT_SECURE_NO_DEPRECATE
        _CRT_NONSTDC_NO_DEPRECATE
        _USE_32BIT_TIME_T)
    target_compile_options(${target} PRIVATE
        /MP
        /W3
        /wd4244
        /wd4267
        /wd4305
        /wd4996)
endfunction()

file(GLOB_RECURSE IDLIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/idlib/*.cpp")
add_library(idlib STATIC ${IDLIB_SOURCES})
darklight_common_target(idlib)
target_compile_definitions(idlib PRIVATE __IDLIB__)

file(GLOB EXTERNAL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/framework/zlib/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/*.cpp")
list(REMOVE_ITEM EXTERNAL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/framework/zlib/example.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/framework/zlib/minigzip.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jcapistd.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jctrans.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jload.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemansi.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemdos.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jmemname.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6/jpegtran.cpp")
add_library(external STATIC ${EXTERNAL_SOURCES})
darklight_common_target(external)
target_include_directories(external PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jpeg-6")

# Microsoft JPEG XR reference encoder.  Virtual-texture pages store the JPEG XR
# image-plane stream without the ordinary container/image header, so the engine
# wrapper uses the low-level streaming API directly.
set(JXRLIB_ENCODER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys/adapthuff.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys/image.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys/strcodec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys/strPredQuant.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys/strTransform.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys/perfTimerANSI.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/encode/encode.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/encode/segenc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/encode/strenc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/encode/strFwdTransform.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/encode/strPredQuantEnc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode/decode.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode/postprocess.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode/segdec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode/strdec.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode/strInvTransform.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode/strPredQuantDec.c")
add_library(jxrlib_encoder STATIC ${JXRLIB_ENCODER_SOURCES})
target_include_directories(jxrlib_encoder PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/common/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/sys"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/encode"
    "${CMAKE_CURRENT_SOURCE_DIR}/extern/jxrlib/image/decode")
target_compile_definitions(jxrlib_encoder PRIVATE
    __ANSI__
    DISABLE_PERF_MEASUREMENT
    _CRT_SECURE_NO_DEPRECATE)
target_compile_options(jxrlib_encoder PRIVATE
    /W0
    /wd4996)

file(GLOB_RECURSE GAME_D3XP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/d3xp/*.cpp")
list(REMOVE_ITEM GAME_D3XP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/d3xp/EndLevel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/d3xp/gamesys/Callbacks.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/d3xp/menus/MenuWidget_DevList.cpp")
# Game classes and events register themselves through static initializers and
# are looked up by name from DoomScript.  An ordinary static archive lets the
# linker discard translation units (such as AI_Vagary.cpp) that have no native
# symbol reference, so include every game object in the executable.
add_library(game_d3xp OBJECT ${GAME_D3XP_SOURCES})
darklight_common_target(game_d3xp)
target_compile_definitions(game_d3xp PRIVATE __DOOM__ _D3XP CTF)

file(GLOB DOOMCLASSIC_SOURCES CONFIGURE_DEPENDS
    "${DARKLIGHT_ROOT}/doomclassic/doom/*.cpp")
list(REMOVE_ITEM DOOMCLASSIC_SOURCES
    "${DARKLIGHT_ROOT}/doomclassic/doom/DoomLeaderboards.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/Networking.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/PlayerProfile.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/Precompiled.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/doomdef.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/doomstat.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/i_main.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/i_net.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/i_net_ps3.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/i_net_xbox.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/i_video_xbox.cpp"
    "${DARKLIGHT_ROOT}/doomclassic/doom/m_swap.cpp")
add_library(doomclassic STATIC ${DOOMCLASSIC_SOURCES})
darklight_common_target(doomclassic)
target_include_directories(doomclassic PRIVATE
    "${DARKLIGHT_ROOT}/doomclassic/Main")
target_compile_definitions(doomclassic PRIVATE _D3XP CTF)

file(GLOB TIMIDITY_SOURCES CONFIGURE_DEPENDS
    "${DARKLIGHT_ROOT}/doomclassic/timidity/*.cpp")
add_library(timidity STATIC ${TIMIDITY_SOURCES})
darklight_common_target(timidity)
target_include_directories(timidity PRIVATE
    "${DARKLIGHT_ROOT}/doomclassic/Main"
    "${DARKLIGHT_ROOT}/doomclassic/timidity")
target_compile_definitions(timidity PRIVATE _D3XP CTF)

file(GLOB DARKLIGHT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/aas/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/cm/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/framework/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/Color/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/DXT/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/OpenGL/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/feedbackanalysis/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pageminlod/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pagesort/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pageupdate/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/baredct/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barehdp/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barejxr/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sound/XAudio2/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/swf/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sys/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp")
# The reconstructed JXR codec uses std::clamp internally. Keep the legacy
# engine at C++14 and opt only these isolated translation units into C++17.
file(GLOB BAREJXR_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barejxr/*.cpp")
set_source_files_properties(${BAREJXR_SOURCES} PROPERTIES COMPILE_OPTIONS "/std:c++17")
list(REMOVE_ITEM DARKLIGHT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/framework/Session.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/renderer/BoundsTrack.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_gamma.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_stats.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/GameWindow.cpp")
list(APPEND DARKLIGHT_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/rc/doom.rc")
add_executable(Darklight WIN32 ${DARKLIGHT_SOURCES})
darklight_common_target(Darklight)
target_compile_definitions(Darklight PRIVATE __DOOM__)

target_link_directories(Darklight PRIVATE "${DXSDK_DIR}/Lib/x86")
target_link_libraries(Darklight PRIVATE
    idlib
    game_d3xp
    external
    jxrlib_encoder
    doomclassic
    timidity
    xinput
    dbghelp
    dinput8
    dsound
    dxguid
    DxErr
    glu32
    iphlpapi
    odbc32
    odbccp32
    opengl32
    winmm
    wsock32
    x3daudio)

target_link_options(Darklight PRIVATE
    /LARGEADDRESSAWARE
    /STACK:16777216,16777216)

set_target_properties(Darklight PROPERTIES
    OUTPUT_NAME "Darklight"
    RUNTIME_OUTPUT_DIRECTORY "${DARKLIGHT_OUTPUT_DIR}")

foreach(configuration Debug Release RelWithDebInfo MinSizeRel)
    string(TOUPPER "${configuration}" configuration_upper)
    set_target_properties(Darklight PROPERTIES
        "RUNTIME_OUTPUT_DIRECTORY_${configuration_upper}" "${DARKLIGHT_OUTPUT_DIR}")
endforeach()

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Darklight)
