cmake_minimum_required(VERSION 3.5)

project(client_user VERSION 0.1 LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

set(PROJECT_SOURCES
    main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    loginwindow.cpp
    loginwindow.h
    loginwindow.ui

    ui/station_list_page.cpp
    ui/station_list_page.h

    ui/station_card_widget.cpp
    ui/station_card_widget.h

    personalhomepage.cpp
    personalhomepage.h
    personalhomepage.ui
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(client_user
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(client_user SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(client_user
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(client_user
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ncs_core
)

set_target_properties(client_user PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(client_user)
endif()
