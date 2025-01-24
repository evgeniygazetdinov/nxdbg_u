# Включаем поддержку C++
enable_language(CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Настройка сборки сервиса
set(SERVICE_SOURCES
    source/main.cpp
)

# Создаем библиотеку
add_library(${PROJECT_NAME}_service STATIC ${SERVICE_SOURCES})

target_include_directories(${PROJECT_NAME}_service
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBNX}/include
)

target_link_directories(${PROJECT_NAME}_service
    PUBLIC
        ${LIBNX}/lib
)

target_link_libraries(${PROJECT_NAME}_service
    PUBLIC
        nx
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME}_elf ${SERVICE_SOURCES})
target_link_libraries(${PROJECT_NAME}_elf PRIVATE ${PROJECT_NAME}_service)
set_target_properties(${PROJECT_NAME}_elf PROPERTIES OUTPUT_NAME "${PROJECT_NAME}.elf")

# Добавляем флаги компиляции для C++
target_compile_options(${PROJECT_NAME}_service PRIVATE
    -ffunction-sections
    -fdata-sections
    -march=armv8-a+crc+crypto
    -mtune=cortex-a57
    -mtp=soft
    -fPIE
    -fno-rtti
    -fno-exceptions
)

target_compile_options(${PROJECT_NAME}_elf PRIVATE
    -ffunction-sections
    -fdata-sections
    -march=armv8-a+crc+crypto
    -mtune=cortex-a57
    -mtp=soft
    -fPIE
    -fno-rtti
    -fno-exceptions
)

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