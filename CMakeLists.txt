cmake_minimum_required(VERSION 3.16)

project(wsh)

set(SRCS main.cc)

add_executable(wsh main.cc linenoise.c)

target_link_directories(wsh PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua-5.4.0/lib)
target_include_directories(wsh PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/lua-5.4.0/lib)
target_link_libraries(wsh lua dl)

add_executable(wcl client.cc linenoise.c)