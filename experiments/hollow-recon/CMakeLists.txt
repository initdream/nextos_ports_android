cmake_minimum_required(VERSION 3.10)
project(hollow_recon C)
set(CMAKE_C_STANDARD 11)
add_executable(hollow-recon
    src/main_recon.c
    src/so_util.c
    src/jni_shim.c
    src/imports_unity.gen.c
    src/jni_idx_stubs.gen.c
    src/util.c
    src/error.c
)
target_include_directories(hollow-recon PRIVATE src)
# --no-as-needed: força libm/dl/pthread carregadas (resolvemos via dlsym, sem ref estática)
# -rdynamic: expõe símbolos p/ dlsym(RTLD_DEFAULT)
target_link_options(hollow-recon PRIVATE -Wl,--no-as-needed -rdynamic)
target_link_libraries(hollow-recon PRIVATE m dl pthread)
target_compile_options(hollow-recon PRIVATE -Wall -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable)
