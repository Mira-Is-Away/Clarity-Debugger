cmake_minimum_required(VERSION 3.14)
project(MiraClarityLib LANGUAGES CXX)

set(CLARITY_DEBUGGER_VERSION "1.0.0")

add_library(mira_clarity INTERFACE)

target_include_directories(mira_clarity INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)