cmake_minimum_required(VERSION 3.16)
project(nxdbg VERSION 1.0.0 LANGUAGES C CXX)

# Опции проекта
option(BUILD_SERVICE "Build Switch service" ON)
option(BUILD_DEBUGGER "Build Python debugger" ON)
option(BUILD_TESTS "Build tests" OFF)

# Настройка компилятора для Switch
if(BUILD_SERVICE)
    if(NOT DEFINED ENV{DEVKITPRO})
        message(FATAL_ERROR "Please set DEVKITPRO in your environment")
    endif()
    
    set(CMAKE_SYSTEM_NAME "Switch")
    set(CMAKE_SYSTEM_PROCESSOR "aarch64")
    
    # Пути к тулчейну
    set(DEVKITPRO $ENV{DEVKITPRO})
    set(DEVKITA64 "${DEVKITPRO}/devkitA64")
    set(LIBNX "${DEVKITPRO}/libnx")
    
    # Компиляторы
    set(CMAKE_C_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc")
    set(CMAKE_CXX_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-g++")
    set(CMAKE_ASM_COMPILER "${DEVKITA64}/bin/aarch64-none-elf-gcc")
    
    # Флаги компиляции
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g3 -Wall -O0 -ffunction-sections -march=armv8-a -mtune=cortex-a57 -mtp=soft -fPIE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -std=gnu++17")
    
    # Включаем поддержку ассемблера
    enable_language(ASM)
    
    # Добавляем подпроект service
    add_subdirectory(service)
endif()

# Настройка Python окружения для отладчика
if(BUILD_DEBUGGER)
    find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
    
    # Добавляем цель для установки зависимостей Python
    add_custom_target(debugger_deps
        COMMAND ${Python3_EXECUTABLE} -m pip install -r ${CMAKE_SOURCE_DIR}/debugger/requirements.txt
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

# Настройка тестов
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG=1)
endif()