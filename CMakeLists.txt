# badcompanyrecomp - Battlefield: Bad Company PC Recompilation
# Based on ReXGlue SDK v0.10.0+

cmake_minimum_required(VERSION 3.25)
project(badcompanyrecomp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/rexglue/win-amd64")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/rexglue.cmake")
    include(generated/rexglue.cmake)
endif()

# Sources
set(BADCOMPANYRECOMP_SOURCES
    src/main.cpp
)

if(WIN32)
    add_executable(badcompanyrecomp WIN32 ${BADCOMPANYRECOMP_SOURCES})
    target_link_libraries(badcompanyrecomp PRIVATE dbghelp)
else()
    add_executable(badcompanyrecomp ${BADCOMPANYRECOMP_SOURCES})
endif()

if(COMMAND rexglue_setup_target)
    rexglue_setup_target(badcompanyrecomp GPU_PLUGINS xenos)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/settings/gamecontrollerdb.txt")
    add_custom_command(TARGET badcompanyrecomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/settings/gamecontrollerdb.txt"
                "$<TARGET_FILE_DIR:badcompanyrecomp>/gamecontrollerdb.txt"
        COMMENT "Copying gamecontrollerdb.txt next to badcompanyrecomp.exe")
endif()
