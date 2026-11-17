cmake_minimum_required(VERSION 3.22)
project(recroom_patch_github C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# github / LAN build — no flags needed, everything is fixed here:
#   discovery  https://ns.rec.net  ->  http://192.168.1.91:8805
#   Photon     Server=192.168.1.91  Port=8805  UseNameServer=false
# ---------------------------------------------------------------------------
set(PATCH_PHOTON_SERVER "192.168.1.91"             CACHE STRING "Photon host")
set(PATCH_PHOTON_PORT   "8805"                     CACHE STRING "Photon port")
set(PATCH_DISCOVERY_URL "http://192.168.1.91:8805" CACHE STRING "Discovery URL")

# Dobby: build it yourself (see README) and drop it in libs/
set(DOBBY_LIB     "${CMAKE_SOURCE_DIR}/libs/libdobby.a" CACHE FILEPATH "libdobby.a")
set(DOBBY_INCLUDE "${CMAKE_SOURCE_DIR}/libs/include"    CACHE PATH     "dir with dobby.h")

add_library(recroom_patch SHARED src/hook.c)

target_compile_definitions(recroom_patch PRIVATE PATCH_VARIANT_GITHUB=1)

target_compile_definitions(recroom_patch PRIVATE
    PATCH_PHOTON_SERVER="${PATCH_PHOTON_SERVER}"
    PATCH_PHOTON_PORT=${PATCH_PHOTON_PORT}
    PATCH_DISCOVERY_URL="${PATCH_DISCOVERY_URL}"
)

target_compile_options(recroom_patch PRIVATE
    -Wall -Wextra -Wno-cast-function-type -fvisibility=hidden
)

target_include_directories(recroom_patch PRIVATE ${DOBBY_INCLUDE})

target_link_libraries(recroom_patch PRIVATE
    ${DOBBY_LIB}
    log
    dl
)
