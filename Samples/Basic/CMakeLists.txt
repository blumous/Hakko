add_executable(HakkoSample WIN32
        main.cpp
        WinAdapter.cpp
        GameService.cpp
        )

target_include_directories(HakkoSample PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(HakkoSample PRIVATE Hakko user32.lib gdi32.lib)

hakko_deploy(HakkoSample)

add_custom_command(TARGET HakkoSample POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:HakkoSample>/res"
)
