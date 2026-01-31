cmake_minimum_required(VERSION 3.11)

project(game_server CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.86.0 REQUIRED)
if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

add_library(model STATIC
    src/model.cpp
    src/model.h
    src/loot_generator.cpp
    src/loot_generator.h
    src/extra_data.cpp
    src/extra_data.h
    src/tagged.h
	src/player.h
	src/ticker.h
	src/application.h
)

target_include_directories(model PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(model
    PUBLIC
        CONAN_PKG::boost
)

add_executable(game_server
    src/main.cpp
    src/http_server.cpp
    src/http_server.h
    src/request_handler.cpp
    src/request_handler.h
    src/json_loader.cpp
    src/json_loader.h
	src/json_serializer.cpp
	src/json_serializer.h
	src/json_logger.h
	src/json_logger.cpp
    src/state_serialization.cpp
    src/state_serialization.h
    src/boost_json.cpp
    src/sdk.h
    src/records_repository.h
    src/postgres.cpp
    src/postgres.h
)

target_include_directories(game_server PRIVATE CONAN_PKG::boost)
target_link_libraries(game_server
    PRIVATE
        model
        CONAN_PKG::boost
        CONAN_PKG::libpq
        CONAN_PKG::libpqxx
        Threads::Threads
)
