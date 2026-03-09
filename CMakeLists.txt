cmake_minimum_required(VERSION 3.14)
project(nexus32-sdk C CXX)

set(CMAKE_C_STANDARD 17)

# Assembler
add_executable(nxasm compiler/nxasm/main.c)
target_include_directories(nxasm PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compiler/nxasm)

# Linker
add_executable(nxld linker/nxld/main.c)
target_include_directories(nxld PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/linker/nxld)

# C compiler (stub then real)
add_executable(nxcc compiler/nxcc/main.c)
target_include_directories(nxcc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/compiler/nxcc)

# Standard library
set(LIB_SOURCES lib/src/sys.c lib/src/gfx.c lib/src/input.c)
add_library(libnx STATIC ${LIB_SOURCES})
target_include_directories(libnx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib/include)

# Tools (stubs)
add_executable(img2tex tools/img2tex/main.c)
add_executable(obj2mesh tools/obj2mesh/main.c)
add_executable(wav2smp tools/wav2smp/main.c)
add_executable(map2lvl tools/map2lvl/main.c)
add_executable(shaderc tools/shaderc/main.c)
add_executable(nxbuild tools/nxbuild/main.c)

install(TARGETS nxasm nxld nxcc libnx img2tex obj2mesh wav2smp map2lvl shaderc nxbuild
  RUNTIME DESTINATION bin
  ARCHIVE DESTINATION lib
)
