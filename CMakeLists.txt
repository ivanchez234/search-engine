cmake_minimum_required(VERSION 3.16)

project(SearchEngine
    VERSION 1.0.0
    DESCRIPTION "Поисковая система: индексатор и поисковик (Qt + PostgreSQL)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    add_compile_options(/utf-8 /W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Sql Concurrent Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Sql Concurrent Widgets)

message(STATUS "Сборка с Qt ${QT_VERSION}")

add_subdirectory(common)
add_subdirectory(indexer)
add_subdirectory(searcher)
