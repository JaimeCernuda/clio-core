cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# dt_proxy — HTTP Proxy ChiMod (pool 800)
#------------------------------------------------------------------------------

# The proxy has two parts:
# 1. A standalone HTTP server library (no Chimaera dependency needed for tests)
# 2. The ChiMod runtime (needs Chimaera)

# Standalone HTTP server + session guard (testable without Chimaera)
add_library(dt_proxy_http SHARED
  src/http_proxy_server.cc
  src/session_guard.cc
)
add_library(dt_provenance::proxy_http ALIAS dt_proxy_http)

target_include_directories(dt_proxy_http
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dt_proxy_http
  PUBLIC
    dt_provenance::protocol
    $<BUILD_INTERFACE:httplib::httplib>
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

set_target_properties(dt_proxy_http PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

# ChiMod client library
add_chimod_client(
  SOURCES
    src/proxy_client.cc
  LINK_LIBRARIES
    chimaera_admin_client
)

# ChiMod runtime library
add_chimod_runtime(
  SOURCES
    src/proxy_runtime.cc
    src/autogen/dt_proxy_lib_exec.cc
  LINK_LIBRARIES
    chimaera_admin_runtime
    $<BUILD_INTERFACE:dt_proxy_http>
)

# Tests
if(DT_PROVENANCE_ENABLE_TESTS)
  add_subdirectory(test)
endif()
