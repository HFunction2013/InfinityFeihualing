cmake_minimum_required(VERSION 3.15)
project(FeihuaLing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W3 /utf-8 /permissive-)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
endif()

include_directories(${CMAKE_SOURCE_DIR}/include)

set(SOURCES
    src/main.cpp
    src/game.cpp
    src/player.cpp
    src/poetry_db.cpp
    src/idiom_db.cpp
    src/save_system.cpp
)

add_executable(FeihuaLing ${SOURCES})

# Copy data directory to output for running convenience
add_custom_command(TARGET FeihuaLing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink
        ${CMAKE_SOURCE_DIR}/third_party
        $<TARGET_FILE_DIR:FeihuaLing>/third_party
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:FeihuaLing>/saves
)
