cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mchip_cte — CTE MChiP ChiMod
#
# Exposes 12 CTE storage tools via the MChiP interface.
#
# Targets:
#   mchips_mchip_cte_client  — client stub
#   mchips_mchip_cte_runtime — runtime with CTE tool handlers
#------------------------------------------------------------------------------

add_chimod_client(
  SOURCES
    src/mchip_cte_client.cc
  LINK_LIBRARIES
    mchips_sdk
)

add_chimod_runtime(
  SOURCES
    src/mchip_cte_runtime.cc
    src/autogen/mchip_cte_lib_exec.cc
  LINK_LIBRARIES
    mchips_sdk
    wrp_cte_core_client
)
