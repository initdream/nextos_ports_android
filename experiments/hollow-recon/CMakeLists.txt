cmake_minimum_required(VERSION 3.10)
project(hollow_recon C)
set(CMAKE_C_STANDARD 11)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
add_executable(hollow-recon
    src/main_recon.c
    src/so_util.c
    src/jni_shim.c
    src/imports_unity.gen.c
    src/jni_idx_stubs.gen.c
    src/recon_egl.c
    src/pthread_shim.c
    src/egl_shim.c
    src/util.c
    src/error.c
)
target_include_directories(hollow-recon PRIVATE src ${SDL2_INCLUDE_DIRS})
target_link_options(hollow-recon PRIVATE -Wl,--no-as-needed -rdynamic -no-pie)
target_compile_options(hollow-recon PRIVATE -fno-pie)
target_link_libraries(hollow-recon PRIVATE ${SDL2_LIBRARIES} GLESv2 EGL m dl pthread z)
target_compile_options(hollow-recon PRIVATE -Wall -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable)
