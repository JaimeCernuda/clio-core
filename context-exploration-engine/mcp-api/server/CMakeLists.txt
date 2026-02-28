cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mcp — MCP Server ChiMod
#
# A Chimaera module that provides an MCP-compliant HTTP gateway.
# Uses add_chimod_client() / add_chimod_runtime() from IowarpCoreCommon.cmake.
#
# Targets:
#   mchips_mcp_client  — client stub for submitting MCP tasks
#   mchips_mcp_runtime — runtime with HTTP server, sessions, tool dispatch
#------------------------------------------------------------------------------

# Client library — lightweight, for submitting MCP tasks to the runtime
add_chimod_client(
  SOURCES
    src/mcp_client.cc
  LINK_LIBRARIES
    mchips_protocol
)

# Runtime library — full MCP gateway with HTTP server
add_chimod_runtime(
  SOURCES
    src/mcp_runtime.cc
    src/http_server.cc
    src/session_manager.cc
    src/tool_registry.cc
    src/autogen/mcp_lib_exec.cc
  LINK_LIBRARIES
    mchips_protocol
    mchips_tools
    httplib::httplib
)

#------------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------------
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
