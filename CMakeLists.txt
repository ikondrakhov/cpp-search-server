cmake_minimum_required(VERSION 3.8 FATAL_ERROR)
project(search-server)

set(CMAKE_CXX_STANDARD 17)
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(
        CMAKE_CXX_FLAGS_DEBUG
        "${CMAKE_CXX_FLAGS_DEBUG} /JMC"
    )
else()
    set(
        CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic -Wno-unused-parameter -Wno-implicit-fallthrough"
    )
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/search-server
)

file(GLOB source
    ${CMAKE_CURRENT_SOURCE_DIR}/search-server/*.cpp
)

add_executable(
    search-server
    ${source}
)