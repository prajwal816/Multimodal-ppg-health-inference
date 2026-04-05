cmake_minimum_required(VERSION 3.16)
project(ppg_embedded_pipeline VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PPG_BUILD_TESTS "Build C++ unit tests" ON)

find_package(Threads REQUIRED)

add_library(ppg_common INTERFACE)
target_include_directories(ppg_common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/cpp
)

add_library(ppg_gpio STATIC
    src/cpp/gpio/gpio_interrupt.cpp
)
target_link_libraries(ppg_gpio PUBLIC ppg_common Threads::Threads)
if(UNIX AND NOT APPLE)
    target_compile_definitions(ppg_gpio PRIVATE PPG_PLATFORM_LINUX=1)
elseif(WIN32)
    target_compile_definitions(ppg_gpio PRIVATE PPG_PLATFORM_WINDOWS=1)
endif()

add_library(ppg_acquisition STATIC
    src/cpp/acquisition/sensor_simulator.cpp
    src/cpp/acquisition/i2c_device.cpp
    src/cpp/acquisition/spi_device.cpp
)
target_link_libraries(ppg_acquisition PUBLIC ppg_common ppg_gpio)

add_library(ppg_sync STATIC
    src/cpp/sync/multimodal_sync.cpp
)
target_link_libraries(ppg_sync PUBLIC ppg_common Threads::Threads)

add_library(ppg_signal STATIC
    src/cpp/signal_processing/bandpass_filter.cpp
    src/cpp/signal_processing/ppg_pipeline.cpp
)
target_link_libraries(ppg_signal PUBLIC ppg_common)

add_executable(ppg_realtime_demo
    src/cpp/main.cpp
)
target_link_libraries(ppg_realtime_demo PRIVATE
    ppg_acquisition
    ppg_sync
    ppg_signal
    ppg_gpio
    Threads::Threads
)

if(PPG_BUILD_TESTS)
    enable_testing()
    add_executable(ppg_test_sync tests/cpp/test_sync.cpp)
    target_link_libraries(ppg_test_sync PRIVATE ppg_sync Threads::Threads)
    add_test(NAME ppg_sync_cpp COMMAND ppg_test_sync)
endif()
