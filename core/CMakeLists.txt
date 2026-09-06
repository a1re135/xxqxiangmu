find_package(Qt6 REQUIRED COMPONENTS Core Sql Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_library(ncs_core STATIC
    database/databasemanager.cpp
    database/databasemanager.h

    database/station_repository.cpp
    database/station_repository.h

    service/station_service.cpp
    service/station_service.h

    service/geocoder.cpp
    service/geocoder.h

    util/geo_util.cpp
    util/geo_util.h

    util/logger.cpp
    util/logger.h

    util/app_paths.h
)

target_link_libraries(ncs_core
    PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
)

target_include_directories(ncs_core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

qt_add_resources(ncs_core "database_resources"
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        "${CMAKE_SOURCE_DIR}/db/schema.sql"
)
