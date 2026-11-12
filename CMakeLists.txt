#
# CMakeLists.txt -- build rules of the anonfm reader.
#
# The reader needs wolfSSL. The build finds wolfSSL through pkg-config,
# through a hint path, or through the default search paths of the system.
#
cmake_minimum_required(VERSION 3.13)
project(anonfm_reader C)

set(CMAKE_C_STANDARD 90)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /utf-8 /D_CRT_SECURE_NO_WARNINGS)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# A hint for a local wolfSSL installation, for example:
#   cmake -DWOLFSSL_ROOT=~/wolfssl-install ..
set(WOLFSSL_ROOT "" CACHE PATH "Root of a wolfSSL installation")

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(WOLFSSL QUIET wolfssl)
endif()

if(NOT WOLFSSL_FOUND)
    find_path(WOLFSSL_INCLUDE_DIR
              NAMES wolfssl/ssl.h
              HINTS ${WOLFSSL_ROOT}
                    ${WOLFSSL_ROOT}/include
                    ENV WOLFSSL_ROOT)
    find_library(WOLFSSL_LIBRARY
                 NAMES wolfssl libwolfssl
                 HINTS ${WOLFSSL_ROOT}
                       ${WOLFSSL_ROOT}/lib
                       ${WOLFSSL_ROOT}/lib/Release
                       ${WOLFSSL_ROOT}/lib/Debug
                       ENV WOLFSSL_ROOT)
    if(WOLFSSL_INCLUDE_DIR AND WOLFSSL_LIBRARY)
        set(WOLFSSL_INCLUDE_DIRS ${WOLFSSL_INCLUDE_DIR})
        set(WOLFSSL_LIBRARIES ${WOLFSSL_LIBRARY})
        set(WOLFSSL_FOUND TRUE)
    endif()
endif()

if(NOT WOLFSSL_FOUND)
    message(FATAL_ERROR
            "wolfSSL not found. Install libwolfssl-dev or set"
            " -DWOLFSSL_ROOT=<path>.")
endif()

add_executable(anonfm
               src/main.c
               src/net.c
               src/answer.c
               src/color.c
               src/util.c)

target_include_directories(anonfm PRIVATE ${WOLFSSL_INCLUDE_DIRS})
target_link_libraries(anonfm PRIVATE ${WOLFSSL_LIBRARIES})

if(WOLFSSL_LIBRARY_DIRS)
    target_link_directories(anonfm PRIVATE ${WOLFSSL_LIBRARY_DIRS})
endif()

if(WIN32)
    target_link_libraries(anonfm PRIVATE ws2_32 crypt32)
    if(NOT MSVC)
        # Strict C89 with MinGW: the headers of wolfSSL use the C99
        # inline keyword. NO_INLINE removes that from the headers.
        target_compile_definitions(anonfm PRIVATE NO_INLINE)
    endif()
endif()

if(UNIX)
    target_link_libraries(anonfm PRIVATE m)
endif()
