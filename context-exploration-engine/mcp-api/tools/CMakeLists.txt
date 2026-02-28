cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_tools — IOWarp-specific MCP tool implementations
#
# Implements CTE, CAE, and cluster monitoring tools as MCP tool handlers.
# Depends on Chimaera + CTE/CAE client libraries.
#
# Target: libmchips_tools.so
# Alias:  mchips::tools
#------------------------------------------------------------------------------

add_library(mchips_tools SHARED
  src/cte_tools.cc
  src/cae_tools.cc
  src/cluster_tools.cc
)

add_library(mchips::tools ALIAS mchips_tools)

target_include_directories(mchips_tools
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mchips_tools
  PUBLIC
    mchips_protocol
    chimaera::cxx
    wrp_cte_core_client
    wrp_cae_core_client
)

set_target_properties(mchips_tools PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

#------------------------------------------------------------------------------
# Installation
#------------------------------------------------------------------------------
install(TARGETS mchips_tools
  EXPORT mchips_tools_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)
