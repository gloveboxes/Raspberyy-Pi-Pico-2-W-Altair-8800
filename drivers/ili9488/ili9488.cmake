if (NOT TARGET ili9488)
    add_library(ili9488 INTERFACE)

    target_sources(ili9488 INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/ili9488.c
    )

    target_link_libraries(ili9488 INTERFACE pico_stdlib hardware_spi hardware_gpio pico_sync)
    target_include_directories(ili9488 INTERFACE ${CMAKE_CURRENT_LIST_DIR})
endif()
