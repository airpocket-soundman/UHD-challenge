# ESP-IDF CMakeLists.txt for M5Stack CoreS3 UHD Detection
cmake_minimum_required(VERSION 3.16)

# Set project name
set(PROJECT_NAME "uhd_detection_core3")

# Include ESP-IDF build system
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Define project
project(${PROJECT_NAME})
