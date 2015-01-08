cmake_minimum_required(VERSION 2.6)
project(emuld CXX)

# to install pkgconfig setup file.
SET(EXEC_PREFIX "\${prefix}")
#SET(LIBDIR "\${prefix}/lib")
SET(INCLUDEDIR "\${prefix}/include")
SET(VERSION 1.1)

set(PROJECT_MAJOR_VERSION "1")
set(PROJECT_MINOR_VERSION "1")
set(PROJECT_RELEASE_VERSION "0")
set(CMAKE_VERBOSE_MAKEFILE OFF)

IF("$ENV{CFLAGS}" MATCHES "-DMOBILE")
	OPTION(USE_D_MOBILE "Use Mobile Def" ON)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DWEARABLE")
	OPTION(USE_D_WEARABLE "Use Wearable Def" ON)
ENDIF()

SET(SRCS
	src/emuld.cpp
	src/evdi.cpp
	src/client.cpp
	src/common.cpp
)

IF(USE_D_MOBILE)
	SET(SRCS ${SRCS}
		src/mobile.cpp
		src/mobile_dev.cpp
		src/common_dev.cpp
	)
ENDIF(USE_D_MOBILE)

IF(USE_D_WEARABLE)
	SET(SRCS ${SRCS}
		src/wearable.cpp
		src/wearable_dev.cpp
		src/common_dev.cpp
	)
ENDIF(USE_D_WEARABLE)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

INCLUDE(FindPkgConfig)

SET(PKG_MODULE
	vconf
	deviced
	dlog
	ecore
	edbus
)

pkg_check_modules(PKGS REQUIRED ${PKG_MODULE})

ADD_DEFINITIONS("-DENABLE_DLOG_OUT")
ADD_DEFINITIONS(-Wall -O3 -omit-frame-pointer)

FOREACH(flag ${PKGS_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS}")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${PKGS_LDFLAGS} -lpthread)

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)
