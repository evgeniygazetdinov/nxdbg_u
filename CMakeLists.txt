cmake_minimum_required(VERSION 3.16)
project(nxdbg VERSION 1.0.0 LANGUAGES C CXX ASM)

# Проверяем наличие DEVKITPRO
if(NOT DEFINED ENV{DEVKITPRO})
    message(FATAL_ERROR "Please set DEVKITPRO in your environment")
endif()

# Настройка компилятора для Switch
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

# Флаги линковки
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -specs=${DEVKITPRO}/libnx/switch.specs -g3 -march=armv8-a -mtune=cortex-a57 -fPIE -L${DEVKITPRO}/libnx/lib")

# # Включаем поддержку C++ и ассемблера
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка сборки сервиса
set(SERVICE_SOURCES
    service/source/main.cpp
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME}_elf ${SERVICE_SOURCES})

target_include_directories(${PROJECT_NAME}_elf
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBNX}/include
)

target_link_directories(${PROJECT_NAME}_elf
    PRIVATE
        ${LIBNX}/lib
)

target_link_libraries(${PROJECT_NAME}_elf
    PRIVATE
        nx
)

# Добавляем флаги компиляции для C++
# target_compile_options(${PROJECT_NAME}_elf PRIVATE
#     -ffunction-sections
#     -fdata-sections
#     -march=armv8-a+crc+crypto
#     -mtune=cortex-a57
#     -mtp=soft
#     -fPIE
#     -fno-rtti
#     -fno-exceptions
# )

set_target_properties(${PROJECT_NAME}_elf PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.elf")

# Добавляем custom команды для создания nro/nso файлов
add_custom_command(
    OUTPUT ${PROJECT_NAME}.nro
    COMMAND ${DEVKITPRO}/tools/bin/elf2nro $<TARGET_FILE:${PROJECT_NAME}_elf> ${PROJECT_NAME}.nro
    DEPENDS ${PROJECT_NAME}_elf
)

add_custom_command(
    OUTPUT ${PROJECT_NAME}.nso
    COMMAND ${DEVKITPRO}/tools/bin/elf2nso $<TARGET_FILE:${PROJECT_NAME}_elf> ${PROJECT_NAME}.nso
    DEPENDS ${PROJECT_NAME}_elf
)

# Добавляем цели для сборки разных форматов
add_custom_target(nro ALL DEPENDS ${PROJECT_NAME}.nro)
add_custom_target(nso ALL DEPENDS ${PROJECT_NAME}.nso)