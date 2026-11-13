file(GLOB_RECURSE IDLIB_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
if(NOT IDLIB_SOURCES)
    message(FATAL_ERROR "No idlib source files were found.")
endif()

source_group(TREE ${CMAKE_SOURCE_DIR} FILES ${IDLIB_SOURCES})

add_library(idlib STATIC ${IDLIB_SOURCES})
darklight_configure_target(idlib)
darklight_set_folder(idlib "libs")
target_compile_definitions(idlib PRIVATE __IDLIB__)

# The Windows stack tracer resolves symbols through dbghelp.
if(WIN32)
    target_link_libraries(idlib PRIVATE dbghelp)
endif()
