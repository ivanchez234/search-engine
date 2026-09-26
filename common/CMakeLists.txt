add_library(se_core STATIC
    appconfig.h
    appconfig.cpp
    database.h
    database.cpp
    searchtypes.h
    textprocessor.h
    textprocessor.cpp
)

target_include_directories(se_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(se_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)
