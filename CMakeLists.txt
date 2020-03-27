cmake_minimum_required (VERSION 3.15)
project (ether)
enable_language (C)

#include (ExternalProject)
find_package (BISON REQUIRED)
find_package (FLEX REQUIRED)


set (WARNING_FLAGS "-Wall -Werror -Wextra -Wno-unused -Wno-error=cpp")
#set (WARNING_FLAGS "-Wfatal-errors ${WARNING_FLAGS}")
file (GLOB ETHER_SRC ${CMAKE_SOURCE_DIR}/src/*.c)
set (ETHER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)


set (CMAKE_C_STANDARD 99)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS} -rdynamic")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -ggdb -DETH_DEBUG_MODE")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast")
include_directories (${ETHER_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/codeine/include)


set (SYNTAX_DIR ${CMAKE_SOURCE_DIR}/src/syntax)
BISON_TARGET (parser ${SYNTAX_DIR}/parser.y ${CMAKE_BINARY_DIR}/parser.c)
FLEX_TARGET (scanner ${SYNTAX_DIR}/scanner.l ${CMAKE_BINARY_DIR}/scanner.c)
ADD_FLEX_BISON_DEPENDENCY (scanner parser)


set (SYNTAX_SRC ${BISON_parser_OUTPUTS} ${FLEX_scanner_OUTPUTS})
add_library (ether_obj OBJECT ${ETHER_SRC} ${SYNTAX_SRC})
target_include_directories (ether_obj PRIVATE ${CMAKE_BINARY_DIR})


add_executable (ether ${CMAKE_SOURCE_DIR}/main.c $<TARGET_OBJECTS:ether_obj>)
target_link_libraries (ether -lm -ldl)
install (TARGETS ether DESTINATION bin)
