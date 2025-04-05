cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

#set(USE_DEBUG "-g")
#set(USE_FLAGS "${USE_DEBUG}")
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${USE_FLAGS}")
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${USE_FLAGS}")

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS "-O3 -Wall -Wextra")
set(CMAKE_C_FLAGS "-O3 -Wall -Wextra")

SET(LIBMAD_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_mad.c
        ${CMAKE_CURRENT_SOURCE_DIR}/reframe_mp3.c
        ${CMAKE_CURRENT_SOURCE_DIR}/av_parsers.c
)

SET(LIBMAD_LIB
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/libmad.a
)

SET(LIBMAD_INC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_filter(libmad
        "${LIBMAD_SRC}"
        "${LIBMAD_LIB}"
        []
        GPAC_HAS_MAD
        "${LIBMAD_INC}"
        ""
        "1")
