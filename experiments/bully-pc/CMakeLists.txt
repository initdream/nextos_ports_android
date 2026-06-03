cmake_minimum_required(VERSION 3.10)
project(bully_pc C)
set(CMAKE_C_STANDARD 11)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
add_executable(bully-pc
    src/main_pc.c
    src/so_util_x64.c
    src/jni_shim.c
    src/egl_shim.c
    src/imports.c
    src/asset_archive.c
    src/util.c
    src/error.c
)
target_include_directories(bully-pc PRIVATE src ${SDL2_INCLUDE_DIRS})
target_compile_definitions(bully-pc PRIVATE GAMEDIR="${CMAKE_SOURCE_DIR}/gamefiles")
# -rdynamic: dlsym(RTLD_DEFAULT) enxerga libc/GLESv2/openal/z linkadas
target_link_options(bully-pc PRIVATE -rdynamic)
target_link_libraries(bully-pc PRIVATE ${SDL2_LIBRARIES} GLESv2 EGL openal z m dl pthread)
target_compile_options(bully-pc PRIVATE -Wall -Wno-unused-parameter -Wno-unused-function)
