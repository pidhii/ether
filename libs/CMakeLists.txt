# single-file-modules
file (GLOB STD_LIBS ${CMAKE_SOURCE_DIR}/libs/*.eth)
install (FILES ${STD_LIBS} DESTINATION lib/ether)

# Ref
set (REF_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/ref)
set (REF_DST_DIR lib/ether/ref)
add_library (ref SHARED ${REF_SRC_DIR}/lib.c)
set_target_properties (ref
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "ref")
install (TARGETS ref DESTINATION ${REF_DST_DIR})

# Math
set (MATH_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/math)
set (MATH_DST_DIR lib/ether/math)
add_library (math SHARED ${MATH_SRC_DIR}/lib.c)
set_target_properties (math
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "math")
install (TARGETS math DESTINATION ${MATH_DST_DIR})

# Vector
set (VECTOR_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/vector)
set (VECTOR_DST_DIR lib/ether/vector)
add_library (vector SHARED ${VECTOR_SRC_DIR}/lib.c)
set_target_properties (vector
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "vector")
install (TARGETS vector DESTINATION ${VECTOR_DST_DIR})
install (FILES ${VECTOR_SRC_DIR}/lib.eth DESTINATION ${VECTOR_DST_DIR})

# JSON
function(build_json_module include_dirs library_dirs libraries)
  set (JSON_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/json)
  set (JSON_DST_DIR lib/ether/json)
  add_library (json SHARED ${JSON_SRC_DIR}/lib.c)
  set_target_properties (json
    PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "json")
  target_include_directories (json PRIVATE ${include_dirs})
  target_link_directories (json PRIVATE ${library_dirs})
  target_link_libraries (json PRIVATE ${libraries} -lbsd)
  install (TARGETS json DESTINATION ${JSON_DST_DIR})
endfunction()
# --
if (ENABLE_JSON)
  pkg_check_modules (JSON json-c)
  if (JSON_FOUND)
    build_json_module("${JSON_INCLUDE_DIRS}" "${JSON_LIBRARY_DIRS}" "${JSON_LIBRARIES}")
  elseif (NOT "${ExternalProject_FOUND}" STREQUAL "NOTFOUND")
    message (STATUS "Library json-c will be built manually")
    set (THIRD "${CMAKE_BINARY_DIR}/3rd")
    set (JSON_C_INSTALL "${THIRD}/json-c-install")
    ExternalProject_Add(json-c
      PREFIX "${THIRD}"
      INSTALL_DIR "${JSON_C_INSTALL}"
      GIT_REPOSITORY "https://github.com/json-c/json-c.git"
      GIT_PROGRESS TRUE
      CONFIGURE_COMMAND
        ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${JSON_C_INSTALL}
                         -DCMAKE_INSTALL_LIBDIR=${JSON_C_INSTALL}/lib
                         -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                         -DBUILD_TESTING=0
                         -DHAVE_ARC4RANDOM=0
                         -S${THIRD}/src/json-c
                         -B${THIRD}/src/json-c-build
      BUILD_COMMAND make -C ${THIRD}/src/json-c-build install)
    add_library (json-c-lib STATIC IMPORTED GLOBAL)
    file (MAKE_DIRECTORY "${JSON_C_INSTALL}")
    file (MAKE_DIRECTORY "${JSON_C_INSTALL}/lib")
    file (MAKE_DIRECTORY "${JSON_C_INSTALL}/include")
    set_target_properties (json-c-lib
      PROPERTIES
        IMPORTED_LOCATION "${JSON_C_INSTALL}/lib/libjson-c.a"
        INTERFACE_INCLUDE_DIRECTORIES "${JSON_C_INSTALL}/include")
    add_dependencies(json-c-lib json-c)
    build_json_module("${JSON_C_INSTALL}/include" "${JSON_C_INSTALL}/lib" "json-c-lib")
  else ()
    message (WARNING "Can't build Json-module.")
  endif ()
endif (ENABLE_JSON)

## Arb
#if (ENABLE_ARB)
  #check_include_file (arb.h ARB_AVAILABLE)
  #if (ARB_AVAILABLE)
    #set (ARB_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/arb)
    #set (ARB_DST_DIR lib/ether/arb)
    #add_library (arb SHARED ${ARB_SRC_DIR}/lib.c)
    #target_link_libraries (arb -larb)
    #set_target_properties (arb
      #PROPERTIES
        #POSITION_INDEPENDENT_CODE ON
        #PREFIX ""
        #OUTPUT_NAME "arb")
    #install (TARGETS arb DESTINATION ${ARB_DST_DIR})
  #else (ARB_AVAILABLE)
    #message (WARNING "Can't build Arb-module")
  #endif (ARB_AVAILABLE)
#endif (ENABLE_ARB)

# List
set (LIST_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/list)
set (LIST_DST_DIR lib/ether/list)
add_library (list SHARED ${LIST_SRC_DIR}/lib.c)
set_target_properties (list
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "list")
install (TARGETS list DESTINATION ${LIST_DST_DIR})
install (FILES ${LIST_SRC_DIR}/lib.eth DESTINATION ${LIST_DST_DIR})

# String
set (STRING_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/string)
set (STRING_DST_DIR lib/ether/string)
add_library (string SHARED ${STRING_SRC_DIR}/lib.c)
set_target_properties (string
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "string")
install (TARGETS string DESTINATION ${STRING_DST_DIR})
install (FILES ${STRING_SRC_DIR}/lib.eth DESTINATION ${STRING_DST_DIR})

# I/O
set (IO_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/io)
set (IO_DST_DIR lib/ether/io)
add_library (io SHARED ${IO_SRC_DIR}/lib.c)
set_target_properties (io
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "io")
install (TARGETS io DESTINATION ${IO_DST_DIR})
install (FILES ${IO_SRC_DIR}/lib.eth DESTINATION ${IO_DST_DIR})

# RegExp
set (REGEXP_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/regexp)
set (REGEXP_DST_DIR lib/ether/regexp)
add_library (regexp SHARED ${REGEXP_SRC_DIR}/lib.c)
set_target_properties (regexp
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "regexp")
install (TARGETS regexp DESTINATION ${REGEXP_DST_DIR})
install (FILES ${REGEXP_SRC_DIR}/lib.eth DESTINATION ${REGEXP_DST_DIR})

# OS
set (OS_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/os)
set (OS_DST_DIR lib/ether/os)
add_library (os SHARED ${OS_SRC_DIR}/lib.c)
set_target_properties (os
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "os")
install (TARGETS os DESTINATION ${OS_DST_DIR})
install (FILES ${OS_SRC_DIR}/lib.eth DESTINATION ${OS_DST_DIR})

# curses
set (CURSES_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/curses)
set (CURSES_DST_DIR lib/ether/curses)
add_library (curses SHARED ${CURSES_SRC_DIR}/lib.c)
set_target_properties (curses
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "curses")
target_link_libraries (curses -lcurses)
install (TARGETS curses DESTINATION ${CURSES_DST_DIR})
install (FILES ${CURSES_SRC_DIR}/lib.eth DESTINATION ${CURSES_DST_DIR})

# ctype
set (CTYPE_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/ctype)
set (CTYPE_DST_DIR lib/ether/ctype)
add_library (ctype SHARED ${CTYPE_SRC_DIR}/lib.c)
set_target_properties (ctype
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "ctype")
install (TARGETS ctype DESTINATION ${CTYPE_DST_DIR})
  install (FILES ${CTYPE_SRC_DIR}/lib.eth DESTINATION ${CTYPE_DST_DIR})

