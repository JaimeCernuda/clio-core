cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# dt_proxy — HTTP Proxy ChiMod (pool 800)
#------------------------------------------------------------------------------

# The proxy forwards LLM requests via Monitor handler on Chimaera worker threads.
# A Python Flask bridge (context-visualizer) translates agent HTTP requests
# into pool_stats://800.0:local:<json> queries that land in the Monitor handler.

# Standalone HTTP server + session guard (kept for reference / standalone tests)
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
    $<BUILD_INTERFACE:dt_protocol>
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
# Monitor handler does all LLM forwarding on Chimaera worker threads.
# httplib is still needed for the ForwardDirect() SSL client.
add_chimod_runtime(
  SOURCES
    src/proxy_runtime.cc
    src/autogen/dt_proxy_lib_exec.cc
  LINK_LIBRARIES
    chimaera_admin_runtime
    $<BUILD_INTERFACE:dt_protocol>
    $<BUILD_INTERFACE:httplib::httplib>
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    $<BUILD_INTERFACE:dt_provenance_dt_tracker_client>
    $<BUILD_INTERFACE:wrp_cte_core_client>
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Tests
if(DT_PROVENANCE_ENABLE_TESTS)
  add_subdirectory(test)
endif()
