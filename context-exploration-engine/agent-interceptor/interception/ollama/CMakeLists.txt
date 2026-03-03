cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# dt_intercept_ollama — Ollama Interception ChiMod (pool 803)
#------------------------------------------------------------------------------

# ChiMod client library
add_chimod_client(
  SOURCES
    # Header-only client for now
  LINK_LIBRARIES
    chimaera_admin_client
    dt_provenance::protocol
)

# ChiMod runtime library
add_chimod_runtime(
  SOURCES
    src/ollama_runtime.cc
    src/autogen/dt_intercept_ollama_lib_exec.cc
  LINK_LIBRARIES
    chimaera_admin_runtime
    dt_provenance::protocol
    $<BUILD_INTERFACE:httplib::httplib>
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

# Tests
if(DT_PROVENANCE_ENABLE_TESTS)
  add_subdirectory(test)
endif()
