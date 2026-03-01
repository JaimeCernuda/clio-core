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

# SDK library — MChiP base classes, task types, client utilities
add_subdirectory(sdk)

# Gateway ChiMod — HTTP frontend, session management, MChiP routing
add_subdirectory(gateway)

# MChiP ChiMods — each MChiP is its own ChiMod
add_subdirectory(mchips/cte)
add_subdirectory(mchips/cae)
add_subdirectory(mchips/cluster)
add_subdirectory(mchips/demo)

# Standalone MCP client — no Chimaera dependency
add_subdirectory(client)

# Python bindings (nanobind) — controlled by WRP_CORE_ENABLE_PYTHON
if(WRP_CORE_ENABLE_PYTHON)
  add_subdirectory(bindings)
endif()

# Demo programs (always built)
add_subdirectory(demo)
