
# single-file-modules
file (GLOB STD_LIBS ${CMAKE_SOURCE_DIR}/libs/*.eth)
install (FILES ${STD_LIBS} DESTINATION lib/ether)

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
