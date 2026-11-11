cmake_minimum_required(VERSION 3.25)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

project(Darklight DESCRIPTION "Darklight" LANGUAGES C CXX )

if(WIN32)
	enable_language(RC)
endif()

include(CheckIPOSupported)
include(FindWindowsSDK)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type for single-configuration generators" FORCE)
endif()

if(UNIX)
	set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Build options
if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	set(_darklight_idstudio_default ON)
else()
	set(_darklight_idstudio_default OFF)
endif()

option(DARKLIGHT_BUILD_EXECUTABLE "Build the Darklight application" ON)
option(DARKLIGHT_BUILD_IDSTUDIO "Build the MFC-based idStudio integration (Windows/MSVC only)" ${_darklight_idstudio_default})
option(DARKLIGHT_LEGACY_32BIT_TIME_T "Use the old 32-bit time_t ABI on 32-bit Windows builds only" OFF)
option(DARKLIGHT_ENABLE_LTO "Enable link-time optimization when the compiler supports it" OFF)
option(DARKLIGHT_WARNINGS_AS_ERRORS "Treat project warnings as errors" OFF)
option(DARKLIGHT_MSVC_STATIC_RUNTIME "Link the static MSVC runtime (/MT)" ON)

if(MSVC)
	if(DARKLIGHT_MSVC_STATIC_RUNTIME)
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	else()
		set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL$<$<CONFIG:Debug>:Debug>")
	endif()
endif()

# The CMake project lives in neo/, so ../bin is the bin/ folder at the
# repository root, next to the base/ directory the game runs from.
set(DARKLIGHT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/../bin" CACHE PATH "Directory used for executable output")
set(DARKLIGHT_DXSDK_DIR "" CACHE PATH "DirectX SDK (June 2010) root directory; may also be set with DXSDK_DIR")

if(DARKLIGHT_BUILD_IDSTUDIO AND NOT (WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "MSVC"))
	message(FATAL_ERROR
		"DARKLIGHT_BUILD_IDSTUDIO requires Windows and the MSVC toolchain. "
		"Configure with -DDARKLIGHT_BUILD_IDSTUDIO=OFF for other platforms.")
endif()

if(DARKLIGHT_LEGACY_32BIT_TIME_T)
	if(NOT WIN32 OR CMAKE_SIZEOF_VOID_P EQUAL 8)
		message(FATAL_ERROR "DARKLIGHT_LEGACY_32BIT_TIME_T is only valid for 32-bit Windows builds.")
	endif()
	message(WARNING "The legacy 32-bit time_t ABI is enabled. Prefer the default 64-bit time_t wherever possible.")
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(DARKLIGHT_ARCH "x64")
else()
	set(DARKLIGHT_ARCH "x86")
endif()

if(WIN32)
	set(DARKLIGHT_PLATFORM "windows")
elseif(APPLE)
	set(DARKLIGHT_PLATFORM "macos")
elseif(UNIX)
	set(DARKLIGHT_PLATFORM "linux")
else()
	set(DARKLIGHT_PLATFORM "unknown")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" DARKLIGHT_PROCESSOR)

if(DARKLIGHT_PROCESSOR MATCHES "^(aarch64|arm64)$"
	OR CMAKE_GENERATOR_PLATFORM MATCHES "^[Aa][Rr][Mm]64$")
	set(DARKLIGHT_CPU "arm64")

elseif(DARKLIGHT_PROCESSOR MATCHES "^(arm|armv[0-9].*)$")
	set(DARKLIGHT_CPU "arm")

elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(DARKLIGHT_CPU "x64")

elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set(DARKLIGHT_CPU "x86")

else()
	message(FATAL_ERROR "Unable to determine target architecture.")
endif()

if(WIN32)
	set(DARKLIGHT_BUILD_OS_ID 0)

	set(DARKLIGHT_PATHSEPARATOR_STR "\\\\")
	set(DARKLIGHT_PATHSEPARATOR_CHAR "\\\\")
	set(DARKLIGHT_NEWLINE "\\r\\n")
elseif(APPLE)
	set(DARKLIGHT_BUILD_OS_ID 2)

	set(DARKLIGHT_PATHSEPARATOR_STR "/")
	set(DARKLIGHT_PATHSEPARATOR_CHAR "/")
	set(DARKLIGHT_NEWLINE "\\n")
elseif(UNIX)
	set(DARKLIGHT_BUILD_OS_ID 1)

	set(DARKLIGHT_PATHSEPARATOR_STR "/")
	set(DARKLIGHT_PATHSEPARATOR_CHAR "/")
	set(DARKLIGHT_NEWLINE "\\n")
else()
	message(FATAL_ERROR "Unsupported target operating system.")
endif()

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/platform.h.in"
	"${CMAKE_BINARY_DIR}/platform.h"
	@ONLY)

message(STATUS "Platform: ${DARKLIGHT_PLATFORM}")
message(STATUS "Architecture: ${DARKLIGHT_CPU}")
message(STATUS "OS ID: ${DARKLIGHT_BUILD_OS_ID}")

# Shared target helpers
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER  "")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

function(darklight_set_output_directories target)
	set_target_properties(${target} PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${DARKLIGHT_OUTPUT_DIR}"
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
		ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

	foreach(_configuration Debug Release RelWithDebInfo MinSizeRel)
		string(TOUPPER "${_configuration}" _configuration_upper)
		set_target_properties(${target} PROPERTIES
			"RUNTIME_OUTPUT_DIRECTORY_${_configuration_upper}" "${DARKLIGHT_OUTPUT_DIR}"
			"LIBRARY_OUTPUT_DIRECTORY_${_configuration_upper}" "${CMAKE_BINARY_DIR}/lib"
			"ARCHIVE_OUTPUT_DIRECTORY_${_configuration_upper}" "${CMAKE_BINARY_DIR}/lib")
	endforeach()
endfunction()

function(darklight_configure_target target)
	target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}" "${CMAKE_SOURCE_DIR}/idlib" "${CMAKE_SOURCE_DIR}/extern/glad/include")

	if(WIN32)
		target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	endif()

	if(DARKLIGHT_LEGACY_32BIT_TIME_T)
		target_compile_definitions(${target} PRIVATE _USE_32BIT_TIME_T)
	endif()

	target_compile_definitions(${target} PRIVATE __DOOM__ ${PLATFORM_DEFINES})

	if(MSVC)
		target_compile_options(${target} PRIVATE
			/MP
			/W3
			/wd4244
			/wd4267
			/wd4305
			/wd4996
		)
		target_compile_definitions(${target} PRIVATE
			_CRT_SECURE_NO_DEPRECATE
			_CRT_NONSTDC_NO_DEPRECATE
		)
	elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
		target_compile_options(${target} PRIVATE
			-Wall
			-Wextra
			-Wno-unused-parameter
			-Wno-missing-field-initializers
		)
	endif()

	if(DARKLIGHT_WARNINGS_AS_ERRORS)
		if(MSVC)
			target_compile_options(${target} PRIVATE /WX)
		else()
			target_compile_options(${target} PRIVATE -Werror)
		endif()
	endif()
	
	if (NOT ARGN)
		target_precompile_headers(${target} PRIVATE "${CMAKE_SOURCE_DIR}/idlib/precompiled.h")
	endif()

	darklight_set_output_directories(${target})
endfunction()

function(darklight_set_folder target folder)
	set_target_properties(${target} PROPERTIES FOLDER "${folder}")
endfunction()

function(darklight_collect_sources output_variable)
	file(GLOB_RECURSE _collected CONFIGURE_DEPENDS ${ARGN})
	list(REMOVE_DUPLICATES _collected)
	set(${output_variable} "${_collected}" PARENT_SCOPE)
endfunction()

# Third-party libraries
add_subdirectory(extern)

# Engine libraries
add_subdirectory(idlib)
add_subdirectory(game)

# Engine sources
file(GLOB DARKLIGHT_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/aas2file/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/aas2file/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/gamelib/aas2/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/gamelib/aas2/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/aas2build/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/tools/compilers/aas2build/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/cm/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/cm/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/framework/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/framework/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/Color/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/Color/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/DXT/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/DXT/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/OpenGL/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/OpenGL/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/approximatelighting/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/approximatelighting/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/render/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/render/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/feedbackanalysis/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/feedbackanalysis/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pageminlod/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pageminlod/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pagesort/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pagesort/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pageupdate/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/pageupdate/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/baredct/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/baredct/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barehdp/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barehdp/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barejxr/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barejxr/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sound/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/swf/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/swf/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/sys/*.h"
	"${CMAKE_CURRENT_SOURCE_DIR}/ui/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/ui/*.h")

list(REMOVE_ITEM DARKLIGHT_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/framework/Session.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/renderer/BoundsTrack.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/ui/GameWindow.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/ui/GameWindow.h")

# Add only the system layer for the selected platform. These directories are
# optional so a partially ported platform can be brought up incrementally.
if(WIN32)
	darklight_collect_sources(DARKLIGHT_PLATFORM_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/*.h")

	list(REMOVE_ITEM DARKLIGHT_PLATFORM_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_snd.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_stats.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_stats.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/win_gamma.cpp")
elseif(APPLE)
	darklight_collect_sources(DARKLIGHT_PLATFORM_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/posix/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/posix/*.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/osx/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/osx/*.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/macos/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/macos/*.h")
elseif(UNIX)
	darklight_collect_sources(DARKLIGHT_PLATFORM_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/posix/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/posix/*.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/linux/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sys/linux/*.h")
endif()
list(APPEND DARKLIGHT_SOURCES ${DARKLIGHT_PLATFORM_SOURCES})

# Select the audio backend without pulling Windows headers into other builds.
set(DARKLIGHT_USE_OPENAL_BACKEND OFF)
if(WIN32)
	file(GLOB AUDIO_BACKEND_SOURCES CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/XAudio2/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/XAudio2/*.h")

	list(APPEND DARKLIGHT_SOURCES ${AUDIO_BACKEND_SOURCES})
elseif(APPLE)
	darklight_collect_sources(DARKLIGHT_OPENAL_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/OpenAL/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/OpenAL/*.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/AL/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/AL/*.h")

	darklight_collect_sources(DARKLIGHT_COREAUDIO_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/CoreAudio/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/CoreAudio/*.h")

	list(APPEND DARKLIGHT_SOURCES
		${DARKLIGHT_OPENAL_SOURCES}
		${DARKLIGHT_COREAUDIO_SOURCES})

	if(DARKLIGHT_OPENAL_SOURCES)
		set(DARKLIGHT_USE_OPENAL_BACKEND ON)
	endif()
elseif(UNIX)
	darklight_collect_sources(DARKLIGHT_OPENAL_SOURCES
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/OpenAL/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/OpenAL/*.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/AL/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/sound/AL/*.h")

	list(APPEND DARKLIGHT_SOURCES ${DARKLIGHT_OPENAL_SOURCES})

	if(DARKLIGHT_OPENAL_SOURCES)
		set(DARKLIGHT_USE_OPENAL_BACKEND ON)
	endif()
endif()

# idStudio uses MFC, MFC is limited to MSVC only.
if(DARKLIGHT_BUILD_IDSTUDIO)
	file(GLOB IDSTUDIO_SOURCES CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/comafx/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/comafx/*.h"
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/idstudio/*.cpp"
		"${CMAKE_CURRENT_SOURCE_DIR}/tools/idstudio/*.h")
	if(NOT IDSTUDIO_SOURCES)
		message(FATAL_ERROR "DARKLIGHT_BUILD_IDSTUDIO is enabled, but no idStudio sources were found.")
	endif()
	list(APPEND DARKLIGHT_SOURCES ${IDSTUDIO_SOURCES})
endif()

# barejxr uses std::clamp until we make the engine compiles under C++17
# This would be compile has C++17 for now
file(GLOB BAREJXR_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/renderer/jobs/transcode/image/barejxr/*.cpp")
if(BAREJXR_SOURCES)
	if(MSVC)
		set_source_files_properties(${BAREJXR_SOURCES} PROPERTIES COMPILE_OPTIONS "/std:c++17")
	else()
		set_source_files_properties(${BAREJXR_SOURCES} PROPERTIES COMPILE_OPTIONS "-std=gnu++17")
	endif()
	set_source_files_properties(${BAREJXR_SOURCES} PROPERTIES SKIP_PRECOMPILE_HEADERS TRUE)
endif()

if(WIN32)
	list(APPEND DARKLIGHT_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/rc/doom.rc")
		
	if(DARKLIGHT_BUILD_IDSTUDIO)
		list(APPEND DARKLIGHT_SOURCES
			"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/rc/Common.rc"
			"${CMAKE_CURRENT_SOURCE_DIR}/sys/win32/rc/Radiant.rc")	
	endif()
endif()

list(REMOVE_DUPLICATES DARKLIGHT_SOURCES)

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${DARKLIGHT_SOURCES})

if(DARKLIGHT_BUILD_EXECUTABLE)
	if(NOT DARKLIGHT_SOURCES)
		message(FATAL_ERROR "No application source files were found.")
	endif()

	# Do not enable MFC for the whole engine.
	if(DARKLIGHT_BUILD_IDSTUDIO)
		set(_darklight_saved_mfc_flag "${CMAKE_MFC_FLAG}")
		set(CMAKE_MFC_FLAG 1)
	endif()

	if(WIN32)
		add_executable(Darklight WIN32 ${DARKLIGHT_SOURCES})
	else()
		add_executable(Darklight ${DARKLIGHT_SOURCES})
	endif()

	if(DARKLIGHT_BUILD_IDSTUDIO)
		set(CMAKE_MFC_FLAG "${_darklight_saved_mfc_flag}")
		set_property(TARGET Darklight PROPERTY VS_GLOBAL_UseOfMfc "Static")
	endif()

	darklight_configure_target(Darklight)
	darklight_set_folder(Darklight "exes")

	target_include_directories(Darklight PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/extern/jpeg-6")

	target_link_libraries(Darklight PRIVATE
		idlib
		game
		external
		jxrlib_encoder)

	find_package(Threads REQUIRED)
	target_link_libraries(Darklight PRIVATE Threads::Threads)

	if(CMAKE_DL_LIBS)
		target_link_libraries(Darklight PRIVATE ${CMAKE_DL_LIBS})
	endif()

	find_package(OpenGL REQUIRED)
	target_link_libraries(Darklight PRIVATE OpenGL::GL)

	if(DARKLIGHT_USE_OPENAL_BACKEND)
		find_package(OpenAL REQUIRED)
		target_link_libraries(Darklight PRIVATE OpenAL::OpenAL)
	endif()

	if(WIN32)	# Windows libraries
		target_compile_definitions(Darklight PRIVATE
			WIN32_LEAN_AND_MEAN
			NOMINMAX)

		if(DARKLIGHT_BUILD_IDSTUDIO)
			target_compile_definitions(Darklight PRIVATE IDSTUDIO)
		endif()

		target_link_libraries(Darklight PRIVATE
			dbghelp
			iphlpapi
			comctl32
			comdlg32
			shell32
			shlwapi
			ole32
			oleaut32
			uuid
			odbc32
			odbccp32
			winmm
			ws2_32
			user32
			gdi32
			advapi32)

			# Find Windows SDK
			find_package(WindowsSDK REQUIRED)
			get_windowssdk_library_dirs("${WINDOWSSDK_LATEST_DIR}" WINSDK_LIB_DIRS)

			set(WINSDK_LIB_DIR "")
			foreach(_dir ${WINSDK_LIB_DIRS})
				if(EXISTS "${_dir}/dinput8.lib")
					set(WINSDK_LIB_DIR "${_dir}")
					break()
				endif()
			endforeach()
	
			target_link_directories(Darklight PRIVATE ${WINSDK_LIB_DIR})

			target_link_libraries(Darklight PRIVATE
				xinput
				dinput8
				dsound
				dxguid
				xaudio2)

			if(MSVC)
				target_link_options(Darklight PRIVATE /LARGEADDRESSAWARE /MANIFEST:NO "/STACK:16777216,16777216")
			elseif(MINGW AND DARKLIGHT_ARCH STREQUAL "x86")
				target_link_options(Darklight PRIVATE -Wl,--large-address-aware)
			endif()
	elseif(APPLE) # macOS system libraries
		foreach(_framework Cocoa IOKit CoreFoundation CoreAudio AudioToolbox)
			find_library(_darklight_${_framework}_framework ${_framework})
			if(_darklight_${_framework}_framework)
				target_link_libraries(Darklight PRIVATE "${_darklight_${_framework}_framework}")
			endif()
		endforeach()
	elseif(UNIX) # Linux and *nix system libraries
		target_link_libraries(Darklight PRIVATE m)

		find_package(X11 QUIET)
		if(X11_FOUND)
			if(TARGET X11::X11)
				target_link_libraries(Darklight PRIVATE X11::X11)
			else()
				target_include_directories(Darklight PRIVATE ${X11_INCLUDE_DIR})
				target_link_libraries(Darklight PRIVATE ${X11_LIBRARIES})
			endif()

			foreach(_x11_target
				X11::Xrandr
				X11::Xi
				X11::Xinerama
				X11::Xcursor
				X11::Xxf86vm
				X11::Xext
				X11::Xrender)
				if(TARGET ${_x11_target})
					target_link_libraries(Darklight PRIVATE ${_x11_target})
				endif()
			endforeach()
		endif()
	endif()

	set_target_properties(Darklight PROPERTIES
		OUTPUT_NAME Darklight
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/..")

	install(TARGETS Darklight
		RUNTIME DESTINATION bin
		BUNDLE DESTINATION .)
endif()

# Link-time optimization
if(DARKLIGHT_ENABLE_LTO)
	check_ipo_supported(RESULT _darklight_ipo_supported OUTPUT _darklight_ipo_error)
	if(_darklight_ipo_supported)
		foreach(_target idlib game jxrlib_encoder Darklight)
			if(TARGET ${_target})
				set_property(TARGET ${_target} PROPERTY
					INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
			endif()
		endforeach()
	else()
		message(WARNING "Link-time optimization is not available with this toolchain: ${_darklight_ipo_error}")
	endif()
endif()

message(STATUS "Platform: ${DARKLIGHT_PLATFORM}")
message(STATUS "Architecture: ${DARKLIGHT_ARCH}")
message(STATUS "C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "idStudio Integration: ${DARKLIGHT_BUILD_IDSTUDIO}")
message(STATUS "Executable output: ${DARKLIGHT_OUTPUT_DIR}")
