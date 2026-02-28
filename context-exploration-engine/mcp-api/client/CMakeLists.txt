cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mcp_client — Standalone MCP client library
#
# No Chimaera dependency. Usable by any C++ agent application that needs
# to connect to an MCP server over Streamable HTTP.
#
# Target: libmchips_mcp_client.so
# Alias:  mchips::mcp_client
#------------------------------------------------------------------------------

add_library(mchips_mcp_client_lib SHARED
  src/mcp_client.cc
  src/mcp_transport.cc
)

add_library(mchips::mcp_client ALIAS mchips_mcp_client_lib)

target_include_directories(mchips_mcp_client_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mchips_mcp_client_lib
  PUBLIC
    mchips_protocol
    httplib::httplib
)

set_target_properties(mchips_mcp_client_lib PROPERTIES
  OUTPUT_NAME mchips_mcp_client
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

#------------------------------------------------------------------------------
# Installation
#------------------------------------------------------------------------------
install(TARGETS mchips_mcp_client_lib
  EXPORT mchips_mcp_client_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)

#------------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
