cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# dt_ctx_untangler — Context Untangler ChiMod (pool 820)
#------------------------------------------------------------------------------

# ChiMod client library
add_chimod_client(
  SOURCES
    src/ctx_untangler_client.cc
  LINK_LIBRARIES
    chimaera_admin_client
)

# ChiMod runtime library
add_chimod_runtime(
  SOURCES
    src/ctx_untangler_runtime.cc
    src/autogen/dt_ctx_untangler_lib_exec.cc
  LINK_LIBRARIES
    chimaera_admin_runtime
    $<BUILD_INTERFACE:wrp_cte_core_client>
    $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

# Tests
if(DT_PROVENANCE_ENABLE_TESTS)
  add_subdirectory(test)
endif()
