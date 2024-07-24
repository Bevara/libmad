cmake_minimum_required(VERSION 3.7)
project(Filters)

include(filters.cmake)

add_definitions(-fpic)

SET(LIBMAD_SRC
        ${CMAKE_CURRENT_SOURCE_DIR}/dec_mad.c
        ${CMAKE_CURRENT_SOURCE_DIR}/reframe_mp3.c
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