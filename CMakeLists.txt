cmake_minimum_required (VERSION 3.15)
project (ether VERSION 0.5.0)
enable_language (C CXX)

include (CheckIncludeFile REQUIRED)
include (CheckCCompilerFlag)
include (ExternalProject OPTIONAL RESULT_VARIABLE ExternalProject_FOUND)

find_package (BISON REQUIRED)
find_package (FLEX REQUIRED)
find_package (PkgConfig REQUIRED)

string(REPLACE "." ";" BISON_VERSION_LIST ${BISON_VERSION})
list(GET BISON_VERSION_LIST 0 BISON_VERSION_MAJOR)
list(GET BISON_VERSION_LIST 1 BISON_VERSION_MINOR)
list(GET BISON_VERSION_LIST 2 BISON_VERSION_PATCH)
#if (${BISON_VERSION_MAJOR} LESS "3" OR ${BISON_VERSION_MINOR} LESS "7")
  #message(SEND_ERROR "

################################################################################
#                                  OPTIONS
#
# ------------------------------- Libraries ------------------------------------
option (BUILD_SHARED_LIBRARY "Build shared library \"ether.so\"" OFF)
option (BUILD_STATIC_LIBRARY "Build static library \"ether.a\"" ON)
option (BUILD_CPPAPI_SHARED_LIBRARY "Build shared library \"ether.so\"" ON)
option (BUILD_CPPAPI_STATIC_LIBRARY "Build static library \"ether.a\"" ON)
# -------------------------------- Modules ------------------------------------
option (ENABLE_ARB "Build an Arb-module" YES)
option (ENABLE_JSON "Build a Json-module" YES)
# ------------------------------ Dev-Options -----------------------------------
option (DISABLE_BUILTINS "Disable all builtins" OFF)


################################################################################
#                                   FLAGS
#
set (WARNING_FLAGS "-Wall -Werror -Wextra -Wno-unused -Wno-error=cpp -Wno-maybe-uninitialized")
# ------------------------------ Common Flags ----------------------------------
set (CMAKE_C_STANDARD 99)
set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${WARNING_FLAGS} -rdynamic")
# --
if (DISABLE_BUILTINS)
  set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DETHER_DISABLE_BUILTINS=1")
endif (DISABLE_BUILTINS)
# --
if (CMAKE_INSTALL_PREFIX)
  add_definitions (-DETHER_PREFIX="${CMAKE_INSTALL_PREFIX}")
endif (CMAKE_INSTALL_PREFIX)
# --
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/vm.c
  PROPERTIES COMPILE_FLAGS "-fno-gcse -fno-crossjumping")
# ------------------------------- Debug Flags ----------------------------------
set (CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -ggdb -DETH_DEBUG_MODE")
# ------------------------------ Release Flags ---------------------------------
set (CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -march=native -flto")
# --
string (TOUPPER "${CMAKE_BUILD_TYPE}" BLDTYPE)
add_definitions (
  -DETHER_VERSION="${CMAKE_PROJECT_VERSION}"
  -DETHER_BUILD="${CMAKE_BUILD_TYPE}"
  -DETHER_BUILD_FLAGS="${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${BLDTYPE}}"
)

################################################################################
#                                  HEADERS
#
set (ETHER_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
include_directories (${ETHER_INCLUDE_DIR})
include_directories (${CMAKE_SOURCE_DIR}/codeine/include)
include_directories (${CMAKE_SOURCE_DIR}/src)


################################################################################
#                                SOURCE FILES
#
file (GLOB ETHER_SRC
  ${CMAKE_SOURCE_DIR}/src/*.c
  ${CMAKE_SOURCE_DIR}/codeine/src/*.c)
file (GLOB ETHER_CPPAPI_SRC ${CMAKE_SOURCE_DIR}/src/c++/*.cpp)
set (SYNTAX_DIR ${CMAKE_SOURCE_DIR}/src/syntax)
BISON_TARGET (parser ${SYNTAX_DIR}/parser.y ${CMAKE_BINARY_DIR}/parser.c)
FLEX_TARGET (scanner ${SYNTAX_DIR}/scanner.l ${CMAKE_BINARY_DIR}/scanner.c)
ADD_FLEX_BISON_DEPENDENCY (scanner parser)


# object files:
set (SYNTAX_SRC ${BISON_parser_OUTPUTS} ${FLEX_scanner_OUTPUTS})
add_library (ether_obj OBJECT ${ETHER_SRC} ${SYNTAX_SRC})
target_include_directories (ether_obj PRIVATE ${CMAKE_BINARY_DIR})

add_library (ether_c++_obj OBJECT ${ETHER_CPPAPI_SRC})

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

# shared C++ API library:
if (BUILD_CPPAPI_SHARED_LIBRARY)
  add_library (ether++_so SHARED ${ETHER_CPPAPI_SRC})
  set_target_properties (ether++_so
    PROPERTIES
      POSITION_INDEPENDENT_CODE ON
      OUTPUT_NAME "ether++")
  install (TARGETS ether++_so DESTINATION "lib")
endif (BUILD_CPPAPI_SHARED_LIBRARY)

# static C++ API library:
if (BUILD_CPPAPI_STATIC_LIBRARY)
  add_library (ether++_a STATIC $<TARGET_OBJECTS:ether_c++_obj>)
  set_target_properties (ether++_a PROPERTIES OUTPUT_NAME "ether++")
  install (TARGETS ether++_a DESTINATION "lib")
endif (BUILD_CPPAPI_STATIC_LIBRARY)

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
add_test (Builtins ether -L${CMAKE_SOURCE_DIR}/t ${CMAKE_SOURCE_DIR}/t/builtins.eth)
add_test (CmdArg ether -L${CMAKE_SOURCE_DIR}/t ${CMAKE_SOURCE_DIR}/t/cmdarg.eth)
add_test (Samples ether ${CMAKE_SOURCE_DIR}/samples/test-samples.eth ${CMAKE_SOURCE_DIR}/samples)
add_test (Dope ether ${CMAKE_SOURCE_DIR}/t/test-dope.eth ${CMAKE_INSTALL_PREFIX}/bin/dope ${CMAKE_SOURCE_DIR}/t)

