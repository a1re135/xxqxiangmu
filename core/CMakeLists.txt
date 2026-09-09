find_package(Qt6 REQUIRED COMPONENTS
    Core
    Sql
    Network
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

add_library(ncs_core STATIC

    logging/ncslogger.cpp
    logging/ncslogger.h

    database/databasemanager.cpp
    database/databasemanager.h

    database/user_repository.cpp
    database/user_repository.h

    database/station_repository.cpp
    database/station_repository.h

    service/userservice.cpp
    service/userservice.h

    service/adminauthservice.cpp
    service/adminauthservice.h

    service/statsservice.cpp
    service/statsservice.h

    service/predictionservice.cpp
    service/predictionservice.h

    service/chargerservice.cpp
    service/chargerservice.h

    service/stationservice.cpp
    service/stationservice.h
    
    service/adminstationservice.cpp
    service/adminstationservice.h

    service/adminuserservice.cpp
    service/adminuserservice.h

    service/stationservice.cpp
    service/stationservice.h

    service/geocoder.cpp
    service/geocoder.h

    service/charge_service.cpp
    service/charge_service.h

    service/charge_service_query.cpp
    service/charge_service_snapshot.cpp
    service/charge_service_settlement.cpp

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
