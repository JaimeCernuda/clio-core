cmake_minimum_required(VERSION 3.20)
project(mchips_mcp_api VERSION 0.1.0)

#------------------------------------------------------------------------------
# External Dependencies (FetchContent)
#------------------------------------------------------------------------------
include(FetchContent)

# nlohmann/json — JSON parse/serialize, ordered_json for MCP spec compliance
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib — HTTP server + client (single-header, MIT)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3
  GIT_SHALLOW    TRUE
)
set(HTTPLIB_COMPILE OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(httplib)

#------------------------------------------------------------------------------
# Subdirectories
#------------------------------------------------------------------------------

# Protocol library — standalone MCP types, no Chimaera dependency
add_subdirectory(protocol)

# Server ChiMod — MCP gateway as a Chimaera module
add_subdirectory(server)

# Tools — IOWarp-specific MCP tool implementations
add_subdirectory(tools)

# Standalone MCP client — no Chimaera dependency
add_subdirectory(client)

# Python bindings (nanobind) — controlled by WRP_CORE_ENABLE_PYTHON
if(WRP_CORE_ENABLE_PYTHON)
  add_subdirectory(bindings)
endif()

# Demo programs
if(BUILD_TESTING)
  add_subdirectory(demo)
endif()
