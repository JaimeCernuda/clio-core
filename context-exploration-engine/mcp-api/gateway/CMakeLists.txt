cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mcp_gateway — MCP Gateway ChiMod
#
# HTTP gateway that discovers MChiP ChiMods, aggregates their tools,
# and routes tools/call requests via Chimaera's distributed task system.
#
# Targets:
#   mchips_mcp_gateway_client  — client stub for submitting gateway tasks
#   mchips_mcp_gateway_runtime — full HTTP gateway with MChiP routing
#------------------------------------------------------------------------------

# Client library — lightweight, for submitting gateway tasks
add_chimod_client(
  SOURCES
    src/gateway_client.cc
  LINK_LIBRARIES
    mchips_protocol
    mchips_sdk
)

# Runtime library — full MCP gateway with HTTP server and MChiP routing
add_chimod_runtime(
  SOURCES
    src/gateway_runtime.cc
    src/http_server.cc
    src/session_manager.cc
    src/mchip_router.cc
    src/sse_writer.cc
    src/autogen/mcp_gateway_lib_exec.cc
  LINK_LIBRARIES
    mchips_protocol
    mchips_sdk
    httplib::httplib
)

#------------------------------------------------------------------------------
# Tests
#------------------------------------------------------------------------------
if(MCHIPS_ENABLE_TESTS)
  add_subdirectory(test)
endif()
