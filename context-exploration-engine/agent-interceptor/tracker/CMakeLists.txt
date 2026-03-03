cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# dt_tracker — Conversation Tracker ChiMod (pool 810)
#------------------------------------------------------------------------------

# Standalone conversation threading library (testable without Chimaera)
add_library(dt_tracker_threading SHARED
  src/conversation_threading.cc
)
add_library(dt_provenance::tracker_threading ALIAS dt_tracker_threading)

target_include_directories(dt_tracker_threading
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(dt_tracker_threading
  PUBLIC
    $<BUILD_INTERFACE:dt_protocol>
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

set_target_properties(dt_tracker_threading PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

# ChiMod client library
add_chimod_client(
  SOURCES
    src/tracker_client.cc
  LINK_LIBRARIES
    chimaera_admin_client
    $<BUILD_INTERFACE:dt_protocol>
)

# ChiMod runtime library
add_chimod_runtime(
  SOURCES
    src/tracker_runtime.cc
    src/autogen/dt_tracker_lib_exec.cc
  LINK_LIBRARIES
    chimaera_admin_runtime
    $<BUILD_INTERFACE:dt_tracker_threading>
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

# Tests
if(DT_PROVENANCE_ENABLE_TESTS)
  add_subdirectory(test)
endif()
