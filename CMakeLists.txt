cmake_minimum_required(VERSION 3.14)
project(MiraClarityLib VERSION 0.1.1 LANGUAGES CXX)

add_library(mira_clarity INTERFACE)

target_include_directories(mira_clarity INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)