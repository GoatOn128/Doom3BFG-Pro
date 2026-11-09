file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
list(REMOVE_ITEM GAME_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/EndLevel.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/EndLevel.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/gamesys/Callbacks.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menus/MenuWidget_DevList.cpp")
if(NOT GAME_SOURCES)
    message(FATAL_ERROR "No game source files were found.")
endif()

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${GAME_SOURCES})

add_library(game OBJECT ${GAME_SOURCES})
darklight_configure_target(game)
darklight_set_folder(game "libs")
target_compile_definitions(game PRIVATE _D3XP CTF)