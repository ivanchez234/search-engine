add_executable(searcher WIN32
    main.cpp
    mainwindow.h
    mainwindow.cpp
    searchmodels.h
    searchmodels.cpp
    searchworker.h
    searchworker.cpp
)

target_link_libraries(searcher PRIVATE
    se_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

add_custom_command(TARGET searcher POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/searcher.ini"
        "$<TARGET_FILE_DIR:searcher>/searcher.ini"
    COMMENT "Копирование searcher.ini в каталог сборки"
)
