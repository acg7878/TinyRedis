CMAKE_MINIMUM_REQUIRED(VERSION 3.16)

PROJECT(TinyRedis)

set(CMAKE_CXX_STANDARD 17) 
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(TinyRedis 
    src/base/thread/threadpool.cpp
    src/server/tinyredis.cpp
)
target_include_directories(TinyRedis
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_subdirectory(test)