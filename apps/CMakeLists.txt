cmake_minimum_required(VERSION 3.20)

project(axi-gpio
	VERSION 1.0.0
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(fmt REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GPIODCXX REQUIRED IMPORTED_TARGET libgpiodcxx>=2.0)


set(SOURCES
    Gpio/Src/Gpio.cpp
    main.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        Gpio/Inc/
)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
    PkgConfig::GPIODCXX
    Threads::Threads
    fmt::fmt
)

target_compile_options(${PROJECT_NAME} PUBLIC -Wall -Wextra -Wpedantic)


