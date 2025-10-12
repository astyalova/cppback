cmake_minimum_required(VERSION 3.18)
project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)

# Подключаем Conan для cmake_multi
include(${CMAKE_BINARY_DIR}/conanbuildinfo_multi.cmake)
conan_basic_setup(TARGETS)

include_directories(${CONAN_INCLUDE_DIRS})

# Потоки
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/sdk.h
    src/model.h
    src/model.cpp
    src/tagged.h
    src/boost_json.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/json_serializer.h
    src/json_serializer.cpp
    src/json_logger.h
    src/json_logger.cpp
    src/request_handler.cpp
    src/request_handler.h
)

target_link_libraries(game_server PRIVATE
    ${CONAN_LIBS}
    Threads::Threads
)

