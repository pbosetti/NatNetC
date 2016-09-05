cmake_minimum_required(VERSION 3.0)
project(NatNetC)

# Dealing with platform
execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpmachine OUTPUT_VARIABLE TARGET_PLATFORM)
string(REGEX MATCH i586-poky-linux IS_POKY_LINUX ${TARGET_PLATFORM})
if(IS_POKY_LINUX)
  message(STATUS "Building for i586-poky-linux (Edison) platform")
  add_definitions(-DPOKY_LINUX=1)
  # set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPOKY_LINUX=1")
endif()

# Files
set(SOURCE_DIR NatNetC)
file(GLOB SOURCES "${SOURCE_DIR}/NatNet*.c")
include_directories(include)


# TARGETS
# Common
add_compile_options(-std=c99)
include_directories(/usr/local/include)
find_library(YAML NAMES yaml)

# static library
add_library(NatNetC_static STATIC ${SOURCES})
target_compile_definitions(NatNetC_static PRIVATE -DNATNET_YAML=1)
target_link_libraries(NatNetC_static ${YAML})

# dynamic library
add_library(NatNetC SHARED ${SOURCES})
target_compile_definitions(NatNetC PRIVATE -DNATNET_YAML=1)
target_link_libraries(NatNetC ${YAML})

# test executable
add_executable(NatNetCLI NatNetCLI/NatNetMain.c)
target_link_libraries(NatNetCLI NatNetC_static)
target_link_libraries(NatNetCLI pthread)
# install(TARGETS mm_gps_test DESTINATION bin)