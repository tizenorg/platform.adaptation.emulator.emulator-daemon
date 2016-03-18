cmake_minimum_required(VERSION 2.6)
project(emuld CXX)

SET(EMULD_MAJOR_VERSION 0)
SET(EMULD_MINOR_VERSION 10)
SET(EMULD_RELEASE_NUMBER 6)

SET(VERSION ${EMULD_MAJOR_VERSION}.${EMULD_MINOR_VERSION}.${EMULD_RELEASE_NUMBER})

set(CMAKE_VERBOSE_MAKEFILE OFF)

IF("$ENV{CFLAGS}" MATCHES "-DMOBILE")
    OPTION(USE_D_MOBILE "Use Mobile Def" ON)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DWEARABLE")
    OPTION(USE_D_WEARABLE "Use Wearable Def" ON)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DTV")
    OPTION(USE_D_TV "Use TV Def" ON)
ENDIF()

IF("$ENV{CFLAGS}" MATCHES "-DUNKNOWN_PROFILE")
    ADD_DEFINITIONS("-DUNKNOWN_PROFILE")
ENDIF()

SET(SRCS
    src/emuld.cpp
    src/net.cpp
    src/common.cpp
)

IF(MSGPROC_HDS STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_HDS")
    SET(SRCS ${SRCS} src/msgproc_hds.cpp)
ENDIF()

IF(MSGPROC_CMD STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_CMD")
    SET(SRCS ${SRCS} src/msgproc_cmd.cpp)
ENDIF()

IF(MSGPROC_PACKAGE STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_PACKAGE")
    SET(SRCS ${SRCS} src/msgproc_package.cpp)
ENDIF()

IF(MSGPROC_SYSTEM STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_SYSTEM")
    SET(SRCS ${SRCS} src/msgproc_system.cpp)
ENDIF()

IF(MSGPROC_VCONF STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_VCONF")
    SET(SRCS ${SRCS} src/msgproc_vconf.cpp)
ENDIF()

IF(MSGPROC_SUSPEND STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_SUSPEND")
    SET(SRCS ${SRCS} src/msgproc_suspend.cpp)
ENDIF()

IF(MSGPROC_LOCATION STREQUAL on)
    ADD_DEFINITIONS("-DENABLE_LOCATION")
    SET(SRCS ${SRCS} src/msgproc_location.cpp)
ENDIF()

IF(USE_D_MOBILE)
    SET(SRCS ${SRCS}
        src/mobile.cpp
    )

ELSEIF(USE_D_WEARABLE)
    SET(SRCS ${SRCS}
        src/wearable.cpp
    )
ELSEIF(USE_D_TV)
    SET(SRCS ${SRCS}
        src/tv.cpp
    )
ELSE(USE_D_MOBILE)
    SET(SRCS ${SRCS}
        src/stub.cpp
    )
ENDIF(USE_D_MOBILE)

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

INCLUDE(FindPkgConfig)

SET(PKG_MODULE
    vconf
    deviced
    dlog
    glib-2.0
    ecore
    edbus
    capi-network-connection
    eventsystem
)

pkg_check_modules(PKGS REQUIRED ${PKG_MODULE})

SET(LIBS
    ${LIBS}
    libemuld
    pthread
    dl
)

#IF(USE_D_TV)
#   SET(LIBS ${LIBS}
#       sinative
#       si-api
#   )
#ENDIF(USE_D_TV)

ADD_DEFINITIONS("-DENABLE_DLOG_OUT")
ADD_DEFINITIONS(-Wall -O3)

FOREACH(flag ${PKGS_CFLAGS})
    SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EXTRA_CFLAGS} -Wall -fPIE -Werror")
SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -pie -Werror -Wall")

ADD_EXECUTABLE(${PROJECT_NAME} ${SRCS})

TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${PKGS_LDFLAGS} ${LIBS})

INSTALL(TARGETS ${PROJECT_NAME} DESTINATION /usr/bin)

ADD_SUBDIRECTORY(src/libemuld)
