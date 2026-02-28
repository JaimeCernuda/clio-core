cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_mchip_cluster — Cluster MChiP ChiMod
#
# Exposes IOWarp cluster monitoring tools via the MChiP interface.
#
# Targets:
#   mchips_mchip_cluster_client  — client stub
#   mchips_mchip_cluster_runtime — runtime with cluster tool handlers
#------------------------------------------------------------------------------

add_chimod_client(
  SOURCES
    src/mchip_cluster_client.cc
  LINK_LIBRARIES
    mchips_sdk
)

add_chimod_runtime(
  SOURCES
    src/mchip_cluster_runtime.cc
    src/autogen/mchip_cluster_lib_exec.cc
  LINK_LIBRARIES
    mchips_sdk
)
