cmake_minimum_required(VERSION 3.16)

project(client_admin VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Charts)

set(PROJECT_SOURCES
    main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    mainwindowhelpers.h
    revenuepage.cpp
    chargerstatuspage.cpp
    chargermanagepage.cpp
    chargerfilters.cpp
    chargerops.cpp
    chargeradd.cpp
    chargerdelete.cpp
    stationmanagepage.cpp
    usermanagepage.cpp
    userorders.cpp

    adminloginwindow.cpp
    adminloginwindow.h
    adminloginwindow.ui
)

qt_add_executable(client_admin
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

target_link_libraries(client_admin
    PRIVATE
        Qt6::Widgets
        Qt6::Charts
        ncs_core
)

set_target_properties(client_admin PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

qt_finalize_executable(client_admin)
