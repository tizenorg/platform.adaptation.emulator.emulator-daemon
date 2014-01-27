cmake_minimum_required(VERSION 2.6)
project(emuld CXX)

# to install pkgconfig setup file.
SET(EXEC_PREFIX "\${prefix}")
#SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")
SET(VERSION 1.1)

#include(FindPkgConfig)
#pkg_check_modules(rpkgs REQUIRED ecore-x)
#add_definitions(${rpkgs_CFLAGS} -DUSE_ONLY_ONE_MODULE)

set(PROJECT_MAJOR_VERSION "1")
set(PROJECT_MINOR_VERSION "1")
set(PROJECT_RELEASE_VERSION "0")
set(CMAKE_VERBOSE_MAKEFILE OFF)

# does not check target type (SIMUL/REAL)
#FIND_PROGRAM(UNAME NAMES uname)
#EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
#IF("${ARCH}" STREQUAL "arm")
#   ADD_DEFINITIONS("-DTARGET -DHWREV_CHECK")
#   MESSAGE("add -DTARGET")
#ELSE("${ARCH}" STREQUAL "arm")
#   ADD_DEFINITIONS("-DSIMULATOR")
#   MESSAGE("add -DSIMULATOR")
#ENDIF("${ARCH}" STREQUAL "arm")

ADD_DEFINITIONS("-DTARGET -DHWREV_CHECK")

#add_definitions(-D_ESTIMATE_PERFORMANCE)
#add_definitions(-D_DEFAULT_FONT_NAME="Vera")
add_definitions(-Werror -Wall -O3 -omit-frame-pointer)
#add_definitions(-Wall -g -D_DEBUG)
add_definitions(-Iinclude)
ADD_EXECUTABLE(${PROJECT_NAME} src/emuld.cpp src/evdi.cpp src/emuld_proc.cpp src/client.cpp src/get_status.cpp src/log.cpp)

target_link_libraries(${PROJECT_NAME} ${rpkgs_LDFLAGS} ${GLES_LDFLAGS})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} "-lpthread -lvconf -ldeviced" )

install(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)
