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

# Standard library — assembled from .asm sources using nxasm, archived into libnx.a.
set(LIBNX_ASM_SOURCES crt0 sys gfx input mem math asset audio save debug)
set(LIBNX_NXO_OUTPUTS)
set(LIBNX_OUTPUT "${CMAKE_CURRENT_SOURCE_DIR}/lib/libnx.a")

foreach(src ${LIBNX_ASM_SOURCES})
  set(asm_file "${CMAKE_CURRENT_SOURCE_DIR}/lib/src/${src}.asm")
  set(nxo_file "${CMAKE_CURRENT_BINARY_DIR}/libnx_nxo/${src}.nxo")
  add_custom_command(
    OUTPUT "${nxo_file}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/libnx_nxo"
    COMMAND "$<TARGET_FILE:nxasm>" -o "${nxo_file}" "${asm_file}"
    DEPENDS nxasm "${asm_file}"
    COMMENT "Assembling ${src}.asm -> ${src}.nxo"
  )
  list(APPEND LIBNX_NXO_OUTPUTS "${nxo_file}")
endforeach()

add_custom_command(
  OUTPUT "${LIBNX_OUTPUT}"
  COMMAND ${CMAKE_AR} rcs "${LIBNX_OUTPUT}" ${LIBNX_NXO_OUTPUTS}
  DEPENDS ${LIBNX_NXO_OUTPUTS}
  COMMENT "Archiving libnx.a"
)
add_custom_target(libnx ALL DEPENDS "${LIBNX_OUTPUT}")

# Tools (stubs)
add_executable(img2tex tools/img2tex/main.c)
add_executable(obj2mesh tools/obj2mesh/main.c)
add_executable(wav2smp tools/wav2smp/main.c)
add_executable(map2lvl tools/map2lvl/main.c)
add_executable(shaderc tools/shaderc/main.c)
add_executable(nxbuild tools/nxbuild/main.c)

install(TARGETS nxasm nxld nxcc img2tex obj2mesh wav2smp map2lvl shaderc nxbuild
  RUNTIME DESTINATION bin
)
install(FILES "${LIBNX_OUTPUT}" DESTINATION lib)
install(DIRECTORY lib/include/ DESTINATION include)
