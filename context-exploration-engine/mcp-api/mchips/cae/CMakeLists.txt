cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mchip_cae — CAE MChiP ChiMod
#
# Exposes CAE data format transformation tools via the MChiP interface.
#
# Targets:
#   mchips_mchip_cae_client  — client stub
#   mchips_mchip_cae_runtime — runtime with CAE tool handlers
#------------------------------------------------------------------------------

add_chimod_client(
  SOURCES
    src/mchip_cae_client.cc
  LINK_LIBRARIES
    mchips_sdk
)

add_chimod_runtime(
  SOURCES
    src/mchip_cae_runtime.cc
    src/autogen/mchip_cae_lib_exec.cc
  LINK_LIBRARIES
    mchips_sdk
    wrp_cae_core_client
)
