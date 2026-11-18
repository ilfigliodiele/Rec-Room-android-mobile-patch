cmake_minimum_required(VERSION 3.22)
project(recroom_patch_github C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# public/HTTPS build - no flags needed, everything is fixed here:
#   https://ns.rec.net/...  ->  https://recrum.xyz/...
#   plus the Mono/UnityTLS chain-verification bypass (see src/hook.c).
#
# The old LAN/nip.io rewrite (http://<service>.192.168.1.91.nip.io:8806) is
# disabled: BestHTTP rejects non-HTTPS URIs with "Invalid URI scheme".
# ---------------------------------------------------------------------------
set(PATCH_SERVER_DOMAIN "recrum.xyz" CACHE STRING "Discovery host")

# Photon: this deployment runs its own NameServer and app ids (see .env). The client
# still gets the app ids from /player/connection-info; only the NameServer host is
# forced here, because the stock client would use Photon's public one.
set(PATCH_PHOTON_NAMESERVER "photon.recflare.net" CACHE STRING "Photon NameServer host")
set(PATCH_PHOTON_NAMESERVER_PORT 5058 CACHE STRING "Photon NameServer port")

# Dobby: build it yourself (see README) and drop it in libs/
set(DOBBY_LIB     "${CMAKE_SOURCE_DIR}/libs/libdobby.a" CACHE FILEPATH "libdobby.a")
set(DOBBY_INCLUDE "${CMAKE_SOURCE_DIR}/libs/include"    CACHE PATH     "dir with dobby.h")

add_library(recroom_patch SHARED src/hook.c)

target_compile_definitions(recroom_patch PRIVATE PATCH_VARIANT_GITHUB=1)

target_compile_definitions(recroom_patch PRIVATE
    PATCH_SERVER_DOMAIN="${PATCH_SERVER_DOMAIN}"
    PATCH_PHOTON_NAMESERVER="${PATCH_PHOTON_NAMESERVER}"
    PATCH_PHOTON_NAMESERVER_PORT=${PATCH_PHOTON_NAMESERVER_PORT}
)

target_compile_options(recroom_patch PRIVATE
    -Wall -Wextra -Wno-cast-function-type -fvisibility=hidden
    -fno-omit-frame-pointer
)

target_include_directories(recroom_patch PRIVATE ${DOBBY_INCLUDE})

target_link_libraries(recroom_patch PRIVATE
    ${DOBBY_LIB}
    log
    dl
    c++_static
    c++abi
)
