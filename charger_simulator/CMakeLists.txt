find_package(
    Qt6 REQUIRED
    COMPONENTS
        Core
        Network
)

# ============================================================
# Qt Meta-Object Compiler
# Required for Q_OBJECT / signals / slots
# ============================================================

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)


qt_add_executable(
    charger_simulator

    main.cpp

    chargerserver.cpp
    chargerserver.h

    chargerworker.cpp
    chargerworker.h
)


target_link_libraries(
    charger_simulator

    PRIVATE
        Qt6::Core
        Qt6::Network
)
