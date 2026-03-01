cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# MChiPs Demo Programs
#------------------------------------------------------------------------------

# Demo MCP server — launches Chimaera runtime with gateway + MChiP ChiMods
add_executable(mchips_demo_server demo_server.cc)
target_link_libraries(mchips_demo_server PRIVATE
  mchips_mcp_gateway_runtime
  mchips_mchip_cte_runtime
  mchips_mchip_cae_runtime
  mchips_mchip_cluster_runtime
  mchips_mchip_demo_runtime
)

# Demo MCP client — connects to server, lists tools, calls a tool
add_executable(mchips_demo_client demo_client.cc)
target_link_libraries(mchips_demo_client PRIVATE mchips_mcp_client_lib)
