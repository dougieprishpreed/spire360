# spire360's Xbox 360 build on the open xSDK (github.com/xenosworks/xsdk) and
# vulkan4xenon. tools/xsdk/build.sh writes the shaders and configures this
# with xsdk's toolchain file; see xsdk/README.md.
cmake_minimum_required(VERSION 3.22)
project(spire360 C CXX)

if(NOT XSDK)
    message(FATAL_ERROR "Configure with -DCMAKE_TOOLCHAIN_FILE=<xsdk>/cmake/xsdk.cmake")
endif()

set(VULKAN4XENON "" CACHE PATH "vulkan4xenon checkout, with build/libvulkan4xenon.a built")
set(SPIRE_SHADERS "" CACHE PATH "the shaders' folder: spire_shaders.c and vk_shader_table.h (build.sh)")
set(SPIRE_LOGIC_C "" CACHE PATH "the translated game logic (out/logic/c, tools/build_logic.py)")
set(SPIRE_GIT_COMMIT "" CACHE STRING "Source commit for the log's first line")
set(SPIRE_GIT_DATE "" CACHE STRING "Commit date")
set(SPIRE_TITLE_ID "0x53333630" CACHE STRING "The XEX's title ID ('S360')")

get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

# char is signed, as with the XDK; no strict aliasing, as with the XDK's
# compiler; no fused multiply-adds, which would change the logic's floats
# against the PC's (the seeded traces).
set(COMMON_FLAGS -O2 -g -fsigned-char -fno-strict-aliasing -ffp-contract=off -ffunction-sections -fdata-sections)

# The translated logic and its runtime (w2c2's C, the wasm runtime on xsdk's
# POSIX calls), started by logic_glue.c.
file(GLOB LOGIC_SOURCES "${SPIRE_LOGIC_C}/*.c")
add_library(spire_logic STATIC ${LOGIC_SOURCES}
    "${ROOT}/runtime/src/srt_core.c" "${ROOT}/runtime/src/srt_wasi.c" "${ROOT}/runtime/src/srt_wasi_p2.c"
    "${ROOT}/runtime/src/srt_plat_posix.c" "${ROOT}/xdk/spire360/logic_glue.c")
target_include_directories(spire_logic PRIVATE
    "${ROOT}/runtime/include" "${ROOT}/runtime/src" "${ROOT}/third_party/w2c2/w2c2" "${SPIRE_LOGIC_C}")
target_compile_definitions(spire_logic PRIVATE W2C2_HOST_MEMORY)
target_compile_options(spire_logic PRIVATE ${COMMON_FLAGS} -std=gnu99 -w)

file(GLOB ENGINE_SOURCES "${ROOT}/engine/*/*.cpp")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(d3d9_renderer|sw_renderer)\\.cpp$")
add_executable(spire360 ${ENGINE_SOURCES} "${ROOT}/xsdk/main.cpp" "${ROOT}/xsdk/big_alloc.c" "${SPIRE_SHADERS}/spire_shaders.c")
target_include_directories(spire360 PRIVATE "${ROOT}/runtime/include" "${SPIRE_SHADERS}")
target_include_directories(spire360 SYSTEM PRIVATE
    "${VULKAN4XENON}/include" "${VULKAN4XENON}/third_party/vulkan-headers/include")
target_compile_definitions(spire360 PRIVATE SPIRE_WITH_SRT)
target_compile_options(spire360 PRIVATE ${COMMON_FLAGS})
set_source_files_properties(${ENGINE_SOURCES} "${ROOT}/xsdk/main.cpp" PROPERTIES COMPILE_OPTIONS
    "-std=gnu++98;-Wall;-Wextra;-Wno-unused-parameter;-Wno-missing-field-initializers")
set_source_files_properties("${SPIRE_SHADERS}/spire_shaders.c" PROPERTIES COMPILE_OPTIONS "-std=gnu99;-w")
set_source_files_properties("${ROOT}/xsdk/big_alloc.c" PROPERTIES COMPILE_OPTIONS "-std=gnu99;-Wall;-Wextra")
if(SPIRE_GIT_COMMIT)
    target_compile_definitions(spire360 PRIVATE SPIRE_GIT_COMMIT="${SPIRE_GIT_COMMIT}" SPIRE_GIT_DATE="${SPIRE_GIT_DATE}")
endif()

target_link_libraries(spire360 PRIVATE spire_logic "${VULKAN4XENON}/build/libvulkan4xenon.a")
set_property(TARGET spire360 APPEND PROPERTY LINK_DEPENDS "${VULKAN4XENON}/build/libvulkan4xenon.a")
# xSDK's import stubs have no .note.GNU-stack; the console has no such thing.
target_link_options(spire360 PRIVATE "-Wl,--gc-sections,--no-warn-execstack,-Map,${CMAKE_CURRENT_BINARY_DIR}/spire360.map")
# Big blocks from the kernel's pages, given back whole (big_alloc.c).
target_link_options(spire360 PRIVATE "-Wl,--wrap=malloc,--wrap=free,--wrap=realloc,--wrap=calloc")
xsdk_add_xex(spire360 TITLE_ID ${SPIRE_TITLE_ID} NAME "Spire360")
