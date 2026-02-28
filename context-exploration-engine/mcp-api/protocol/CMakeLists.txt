cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_protocol — Standalone MCP protocol types
#
# No Chimaera dependency. Usable by any C++ application that needs
# JSON-RPC 2.0 and MCP type definitions.
#
# Target: libmchips_protocol.so
# Alias:  mchips::protocol
#------------------------------------------------------------------------------

add_library(mchips_protocol SHARED
  src/json_rpc.cc
  src/mcp_types.cc
  src/schema_generator.cc
)

add_library(mchips::protocol ALIAS mchips_protocol)

target_include_directories(mchips_protocol
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mchips_protocol
  PUBLIC
    nlohmann_json::nlohmann_json
)

set_target_properties(mchips_protocol PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

#------------------------------------------------------------------------------
# Installation
#------------------------------------------------------------------------------
install(TARGETS mchips_protocol
  EXPORT mchips_protocol_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)
