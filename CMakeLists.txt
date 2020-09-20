cmake_minimum_required (VERSION 3.15)
project (ether VERSION 0.1.0)
enable_language (C)

#include (ExternalProject)
find_package (BISON REQUIRED)
find_package (FLEX REQUIRED)


option (BUILD_SHARED_LIBRARY "Build shared library \"ether.so\"" OFF)
option (BUILD_STATIC_LIBRARY "Build static library \"ether.a\"" OFF)
option (DISABLE_BUILTINS "Disable all builtins" OFF)

option (ENABLE_ARB "Build an Arb-module" ON)
option (ENABLE_GTK "Build a Gtk-module" ON)
option (ENABLE_REGEXP "Build a Regexp-module" ON)


set (WARNING_FLAGS "-Wall -Werror -Wextra -Wno-unused -Wno-error=cpp")
#set (WARNING_FLAGS "-Wfatal-errors ${WARNING_FLAGS}")
file (GLOB ETHER_SRC
  ${CMAKE_SOURCE_DIR}/src/*.c
  ${CMAKE_SOURCE_DIR}/codeine/src/*.c)
set (ETHER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

set (CMAKE_C_STANDARD 99)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS} -rdynamic")
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -ggdb -DETH_DEBUG_MODE")
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast -flto")

if (DISABLE_BUILTINS)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DETHER_DISABLE_BUILTINS=1")
endif (DISABLE_BUILTINS)

include_directories (${ETHER_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/codeine/include)

set_source_files_properties(${CMAKE_SOURCE_DIR}/src/vm.c
  PROPERTIES COMPILE_FLAGS "-fno-gcse -fno-crossjumping")

string(TOUPPER ${CMAKE_BUILD_TYPE} BLDTYPE)
add_definitions (
  -DETHER_VERSION="${PROJECT_VERSION}"
  -DETHER_BUILD="${CMAKE_BUILD_TYPE}"
  -DETHER_BUILD_FLAGS="${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BLDTYPE}}"
)

if (CMAKE_INSTALL_PREFIX)
  add_definitions (
    -DETHER_PREFIX="${CMAKE_INSTALL_PREFIX}"
  )
endif (CMAKE_INSTALL_PREFIX)

set (SYNTAX_DIR ${CMAKE_SOURCE_DIR}/src/syntax)
BISON_TARGET (parser ${SYNTAX_DIR}/parser.y ${CMAKE_BINARY_DIR}/parser.c)
FLEX_TARGET (scanner ${SYNTAX_DIR}/scanner.l ${CMAKE_BINARY_DIR}/scanner.c)
ADD_FLEX_BISON_DEPENDENCY (scanner parser)


# object files:
set (SYNTAX_SRC ${BISON_parser_OUTPUTS} ${FLEX_scanner_OUTPUTS})
add_library (ether_obj OBJECT ${ETHER_SRC} ${SYNTAX_SRC})
target_include_directories (ether_obj PRIVATE ${CMAKE_BINARY_DIR})

get_target_property(BUILD_FALGS ether_obj COMPILE_OPTIONS)

# install headers:
install (DIRECTORY ${ETHER_INCLUDE_DIR} DESTINATION ".")
install (DIRECTORY "${CMAKE_SOURCE_DIR}/codeine/include/codeine" DESTINATION "include")

# shared library:
if (BUILD_SHARED_LIBRARY)
  add_library (ether_so SHARED ${ETHER_SRC} ${SYNTAX_SRC})
  set_target_properties (ether_so
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      OUTPUT_NAME "ether")
  install (TARGETS ether_so DESTINATION "lib")
endif (BUILD_SHARED_LIBRARY)

# static library:
if (BUILD_STATIC_LIBRARY)
  add_library (ether_a STATIC $<TARGET_OBJECTS:ether_obj>)
  set_target_properties (ether_a PROPERTIES OUTPUT_NAME "ether")
    install (TARGETS ether_a DESTINATION "lib")
endif (BUILD_STATIC_LIBRARY)

# executable:
add_executable (ether ${CMAKE_SOURCE_DIR}/main.c $<TARGET_OBJECTS:ether_obj>)
target_link_libraries (ether -lm -ldl -lpcre -lreadline)
install (TARGETS ether DESTINATION bin)

# directory for modules
install (DIRECTORY DESTINATION "lib/ether")

if (CMAKE_INSTALL_PREFIX)
  set (ETHER_PREFIX "${CMAKE_INSTALL_PREFIX}")
  configure_file (${CMAKE_SOURCE_DIR}/ether.pc ${CMAKE_BINARY_DIR}/ether.pc @ONLY)
  install (FILES ${CMAKE_BINARY_DIR}/ether.pc DESTINATION lib/pkgconfig)
endif (CMAKE_INSTALL_PREFIX)


# Install scripts and libraries
install (PROGRAMS ${CMAKE_SOURCE_DIR}/dope.eth RENAME "dope" DESTINATION bin)
add_subdirectory (libs)

# Tests
enable_testing ()
add_test (TestFuzzy ether -L${CMAKE_SOURCE_DIR}/t ${CMAKE_SOURCE_DIR}/t/test.eth testmode)
add_test (TestBuiltins ether -L${CMAKE_SOURCE_DIR}/t ${CMAKE_SOURCE_DIR}/t/builtins.eth)
add_test (TestCmdArg ether -L${CMAKE_SOURCE_DIR}/t ${CMAKE_SOURCE_DIR}/t/cmdarg.eth)

