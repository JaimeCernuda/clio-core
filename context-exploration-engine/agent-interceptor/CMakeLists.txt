cmake_minimum_required(VERSION 3.20)
project(dt_provenance VERSION 0.1.0)

#------------------------------------------------------------------------------
# External Dependencies (FetchContent)
#------------------------------------------------------------------------------
include(FetchContent)

# nlohmann/json — JSON parse/serialize for interaction records
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
set(JSON_BuildTests OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(nlohmann_json)

# cpp-httplib — HTTP server (proxy) + HTTPS client (interception)
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3
  GIT_SHALLOW    TRUE
)
set(HTTPLIB_COMPILE OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(httplib)

# Catch2 — Unit testing framework
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)

#------------------------------------------------------------------------------
# Subdirectories
#------------------------------------------------------------------------------

# Protocol library — standalone types, parsers, no Chimaera dependency
add_subdirectory(protocol)

# HTTP Proxy ChiMod (pool 800)
add_subdirectory(proxy)

# Interception ChiMods (pools 801-803)
add_subdirectory(interception/anthropic)
add_subdirectory(interception/openai)
add_subdirectory(interception/ollama)

# Conversation Tracker ChiMod (pool 810)
add_subdirectory(tracker)

# Demo launcher and tools
add_subdirectory(demo)
add_subdirectory(tools)

#------------------------------------------------------------------------------
# Testing
#------------------------------------------------------------------------------
option(DT_PROVENANCE_ENABLE_TESTS "Build DTProvenance unit and integration tests" ON)
if(DT_PROVENANCE_ENABLE_TESTS)
  enable_testing()
endif()
