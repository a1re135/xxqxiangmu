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

find_package(
    Qt${QT_VERSION_MAJOR}
    QUIET
    COMPONENTS WebEngineWidgets
)

find_package(
    Qt${QT_VERSION_MAJOR}
    QUIET
    COMPONENTS Multimedia MultimediaWidgets
)

set(PROJECT_SOURCES
    main.cpp

    mainwindow.cpp
    mainwindow.h
    mainwindow.ui

    loginwindow.cpp
    loginwindow.h
    loginwindow.ui

    personalhomepage.cpp
    personalhomepage.h
    personalhomepage.ui

    ui/station_list_page.cpp
    ui/station_list_page.h

    ui/station_card_widget.cpp
    ui/station_card_widget.h

    navigationdialog.cpp
    navigationdialog.h
    navigation_resources.qrc

    ordersettlementdialog.cpp
    ordersettlementdialog.h

    ordershistorydialog.cpp
    ordershistorydialog.h
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

if(
    QT_VERSION_MAJOR EQUAL 6
    AND TARGET Qt6::Multimedia
    AND TARGET Qt6::MultimediaWidgets
)
    target_sources(
        client_user
        PRIVATE
            avatarcapturedialog.cpp
            avatarcapturedialog.h
    )

    target_link_libraries(
        client_user
        PRIVATE
            Qt6::Multimedia
            Qt6::MultimediaWidgets
    )

    target_compile_definitions(
        client_user
        PRIVATE
            NCS_HAS_CAMERA=1
    )

    message(STATUS "NCS camera support: ENABLED")

else()

    target_compile_definitions(
        client_user
        PRIVATE
            NCS_HAS_CAMERA=0
    )

    message(STATUS "NCS camera support: DISABLED")

endif()

if(TARGET Qt${QT_VERSION_MAJOR}::WebEngineWidgets)
    target_link_libraries(
        client_user
        PRIVATE
        Qt${QT_VERSION_MAJOR}::WebEngineWidgets
    )

    target_compile_definitions(
        client_user
        PRIVATE
        NCS_HAS_WEBENGINE=1
    )
else()
    target_compile_definitions(
        client_user
        PRIVATE
        NCS_HAS_WEBENGINE=0
    )
endif()

set_target_properties(client_user PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

add_custom_command(TARGET client_user POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_SOURCE_DIR}/config"
            "$<TARGET_FILE_DIR:client_user>/config"
    COMMENT "Copy charging page configuration"
    VERBATIM
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(client_user)
endif()
