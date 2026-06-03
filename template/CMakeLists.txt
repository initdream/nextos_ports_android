# CMakeLists.txt — template de port nextos_ports_android
# Use com o toolchain NextOS/Elite (sysroot com SDL2 + blob Mali).
#
#   cmake -DCMAKE_TOOLCHAIN_FILE=<nextos>.conf -DUSE_GLES2=ON -B build
#   cmake --build build
#
# USE_GLES2=ON  -> linka GLESv2  (jogos GLES 2.0, ex. LEGO Star Wars)
# USE_GLES2=OFF -> linka GLES_CM (jogos GLES 1.x, ex. Syberia)   [default]

cmake_minimum_required(VERSION 3.10)
project(PORTNAME C)

set(CMAKE_C_STANDARD 11)
option(USE_GLES2 "Linkar GLESv2 (GLES2) em vez de GLES_CM (GLES1)" OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# core/ (reutilizável) + src/ (deste port)
file(GLOB CORE_SRC ${CMAKE_SOURCE_DIR}/src/so_util.c
                   ${CMAKE_SOURCE_DIR}/src/egl_shim.c
                   ${CMAKE_SOURCE_DIR}/src/opensles_shim.c
                   ${CMAKE_SOURCE_DIR}/src/util.c
                   ${CMAKE_SOURCE_DIR}/src/error.c)

add_executable(PORTNAME
    src/main.c
    src/imports.gen.c
    src/android_shim.c
    src/jni_shim.c
    ${CORE_SRC}
)

target_include_directories(PORTNAME PRIVATE ${SDL2_INCLUDE_DIRS} src)

if(USE_GLES2)
  set(GLES_LIB GLESv2)
else()
  # NextOS/Mali expõe o GLES1 como libGLESv1_CM (nome padrão Linux).
  # (No toolchain Trimui é "GLES_CM" — ajuste se portar pra lá.)
  set(GLES_LIB GLESv1_CM)
endif()

target_link_libraries(PORTNAME PRIVATE
    ${SDL2_LIBRARIES}
    ${GLES_LIB}
    EGL m dl pthread stdc++
)

target_compile_options(PORTNAME PRIVATE
    ${SDL2_CFLAGS_OTHER} -Wall -Wextra -Wno-unused-parameter)
