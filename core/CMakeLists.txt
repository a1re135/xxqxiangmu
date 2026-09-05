find_package(Qt6 REQUIRED COMPONENTS Core Sql)

add_library(ncs_core STATIC
    database/databasemanager.cpp
    database/databasemanager.h
)

target_link_libraries(ncs_core
    PUBLIC
    Qt6::Core
    Qt6::Sql
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
