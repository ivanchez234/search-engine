add_executable(indexer
    main.cpp
    fileindexer.h
    fileindexer.cpp
)

target_link_libraries(indexer PRIVATE
    se_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Concurrent
)

add_custom_command(TARGET indexer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/indexer.ini"
        "$<TARGET_FILE_DIR:indexer>/indexer.ini"
    COMMENT "Копирование indexer.ini в каталог сборки"
)
