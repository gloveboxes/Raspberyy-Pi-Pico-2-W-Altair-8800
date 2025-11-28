cmake_minimum_required(VERSION 3.13)

# 💡 Select board BEFORE importing SDK (safer)
set(PICO_BOARD pico2_w)

# Import Pico SDK
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(altair C CXX ASM)
pico_sdk_init()

set(WIFI_SSID "" CACHE STRING "Wi-Fi network SSID for CYW43 station mode")
set(WIFI_PASSWORD "" CACHE STRING "Wi-Fi network password for CYW43 station mode")
option(ENABLE_INKY_DISPLAY "Enable Pimoroni Inky Pack welcome display" ON)

set(ALTAIR_SOURCES
    main.c
    Altair8800/intel8080.c
    Altair8800/memory.c
    Altair8800/pico_disk.c
    io_ports.c
    PortDrivers/time_io.c
    PortDrivers/utility_io.c
    wifi.c
    ws.c
    websocket_console.c
)

set(ALTAIR_LIBS)

if(ENABLE_INKY_DISPLAY)
    set(PIMORONI_PICO_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/pimoroni-pico CACHE PATH "Pimoroni Pico library path")
    include(${PIMORONI_PICO_PATH}/pimoroni_pico_import.cmake)
    include(${PIMORONI_PICO_PATH}/common/pimoroni_bus.cmake)
    include(${PIMORONI_PICO_PATH}/libraries/pico_graphics/pico_graphics.cmake)
    include(${PIMORONI_PICO_PATH}/drivers/uc8151/uc8151.cmake)

    list(APPEND ALTAIR_SOURCES inky_display.cpp)
    list(APPEND ALTAIR_LIBS uc8151)
endif()

add_executable(altair ${ALTAIR_SOURCES})

# Make sure all CYW43 headers are visible and include Altair8800 directory
target_include_directories(altair PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/Altair8800
    ${CMAKE_CURRENT_LIST_DIR}/PortDrivers
    ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_arch/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_cyw43_driver/include
    ${PICO_SDK_PATH}/lib/cyw43-driver/src        # 👈 this one is for cyw43.h
)

target_compile_definitions(altair PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    ENABLE_INKY_DISPLAY=$<BOOL:${ENABLE_INKY_DISPLAY}>
)

# USB stdio on, UART stdio off
pico_enable_stdio_usb(altair 1)
pico_enable_stdio_uart(altair 0)

# Link stdlib + CYW43 (for onboard LED)
target_link_libraries(altair
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
    pico_multicore
    ${ALTAIR_LIBS}
)

# Generate .uf2, .elf, etc.
pico_add_extra_outputs(altair)