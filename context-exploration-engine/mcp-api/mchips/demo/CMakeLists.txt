cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mchip_demo — Demo MChiP ChiMod
#
# Simple echo/add tools for testing and benchmarking the MChiP pipeline.
#
# Targets:
#   mchips_mchip_demo_client  — client stub
#   mchips_mchip_demo_runtime — runtime with echo/add tool handlers
#------------------------------------------------------------------------------

add_chimod_client(
  SOURCES
    src/mchip_demo_client.cc
  LINK_LIBRARIES
    mchips_sdk
)

add_chimod_runtime(
  SOURCES
    src/mchip_demo_runtime.cc
    src/autogen/mchip_demo_lib_exec.cc
  LINK_LIBRARIES
    mchips_sdk
)

if(MCHIPS_ENABLE_TESTS)
  add_subdirectory(test)
endif()
