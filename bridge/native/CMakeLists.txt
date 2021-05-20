cmake_minimum_required(VERSION 3.10)

project(bridge C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if (APPLE)
    SET(CMAKE_C_ARCHIVE_CREATE   "<CMAKE_AR> Scr <TARGET> <LINK_FLAGS> <OBJECTS>")
    SET(CMAKE_C_ARCHIVE_FINISH   "<CMAKE_RANLIB> -no_warning_for_no_symbols -c <TARGET>")
endif()

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lwip ${CMAKE_CURRENT_BINARY_DIR}/lwip)

add_custom_target(
        lwip-export
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lwip> ${CMAKE_CURRENT_BINARY_DIR}/liblwip.a
)

add_library(native STATIC
        init.h init.c
        interface.h interface.c
        link.h link.c
        utils.h utils.c
        queues.h queues.c
        tcp.h tcp.c
        udp.h udp.c)
target_link_libraries(native lwip)
add_dependencies(native lwip-export)