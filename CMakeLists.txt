cmake_minimum_required(VERSION 3.16)

project(
    NCS_Charging_Platform
    LANGUAGES CXX
)

set(
    CMAKE_CXX_STANDARD
    17
)

set(
    CMAKE_CXX_STANDARD_REQUIRED
    ON
)

set(
    CMAKE_CXX_EXTENSIONS
    OFF
)


# ============================================================
# UTF-8 source code support
# ============================================================

if(MSVC)
    add_compile_options(
        /utf-8
    )
endif()


# ============================================================
# Platform information
# ============================================================

if(WIN32)
    message(
        STATUS
        "NCS platform: Windows"
    )
elseif(UNIX AND NOT APPLE)
    message(
        STATUS
        "NCS platform: Linux"
    )
elseif(APPLE)
    message(
        STATUS
        "NCS platform: macOS"
    )
endif()


# ============================================================
# Project modules
# ============================================================

add_subdirectory(core)
add_subdirectory(client_user)
add_subdirectory(client_admin)
add_subdirectory(charger_simulator)
