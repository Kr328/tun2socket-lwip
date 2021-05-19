cmake_minimum_required(VERSION 3.10)

project(bridge C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/../../lwip ${CMAKE_CURRENT_LIST_DIR}/../build/lwip)

add_library(native STATIC
        init.h init.c
        interface.h interface.c
        device.h device.c
        utils.h utils.c
        queues.h queues.c
        tcp.h tcp.c
        udp.h udp.c)
target_link_libraries(native lwip)

add_custom_command(
        TARGET native POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:lwip> ${CMAKE_CURRENT_LIST_DIR}/../build/liblwip.a
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:native> ${CMAKE_CURRENT_LIST_DIR}/../build/libnative.a
)
