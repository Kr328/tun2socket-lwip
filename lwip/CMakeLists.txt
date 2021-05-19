cmake_minimum_required(VERSION 3.10)

project(lwip C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE LWIP_SOURCES
        ${CMAKE_CURRENT_LIST_DIR}/core/*.c
        ${CMAKE_CURRENT_LIST_DIR}/core/ipv4/*.c
        ${CMAKE_CURRENT_LIST_DIR}/core/ipv6/*.c
        ${CMAKE_CURRENT_LIST_DIR}/api/*.c
        )

if (UNIX)
    file(GLOB_RECURSE LWIP_PORT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/ports/unix/*.c)
elseif (WIN32)
    file(GLOB_RECURSE LWIP_PORT_SOURCES ${CMAKE_CURRENT_LIST_DIR}/ports/win32/*.c)
else ()
    message(FATAL_ERROR "Unsupported platform")
endif ()

add_library(lwip STATIC ${LWIP_SOURCES} ${LWIP_PORT_SOURCES})
target_include_directories(lwip PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(lwip)

if (UNIX)
    target_include_directories(lwip PUBLIC ${CMAKE_CURRENT_LIST_DIR}/ports/unix/include)
elseif (WIN32)
    target_include_directories(lwip PUBLIC ${CMAKE_CURRENT_LIST_DIR}/ports/win32/include)
else ()
    message(FATAL_ERROR "Unsupported platform")
endif ()