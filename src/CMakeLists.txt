cmake_minimum_required(VERSION 2.8)

ADD_DEFINITIONS("-std=c++0x -fdump-rtl-expand")

IF(NOT CMAKE_BUILD_TYPE)
    SET(CMAKE_BUILD_TYPE Debug CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)


project(boost-forwarder)
include_directories (.)

add_executable(boost-forwarder main.cpp anet.cpp )

target_link_libraries(boost-forwarder stdc++ pthread boost_thread zmq)
