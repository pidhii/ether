FIND_PACKAGE(PkgConfig REQUIRED)


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

# Gtk
set (GTK_SRC_DIR ${CMAKE_SOURCE_DIR}/libs/gtk)
set (GTK_DST_DIR lib/ether/gtk)
add_library (gtk SHARED ${GTK_SRC_DIR}/lib.c)
set_target_properties (gtk
  PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    PREFIX ""
    OUTPUT_NAME "gtk")
pkg_check_modules (GTK3 REQUIRED gtk+-3.0)
#pkg_check_modules (GOBJECT2 REQUIRED gobject-2.0)
target_include_directories (gtk PRIVATE ${GTK3_INCLUDE_DIRS})
target_link_directories (gtk PRIVATE ${GTK3_LIBRARY_DIRS})
target_link_libraries (gtk PRIVATE ${GTK3_LIBRARIES})
install (TARGETS gtk DESTINATION ${GTK_DST_DIR})
install (FILES ${GTK_SRC_DIR}/lib.eth DESTINATION ${GTK_DST_DIR})

