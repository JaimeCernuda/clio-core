cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# MChiPs Demo Programs
#------------------------------------------------------------------------------

# Demo MCP server — launches Chimaera runtime with MCP gateway
add_executable(mchips_demo_server demo_server.cc)
target_link_libraries(mchips_demo_server PRIVATE mchips_mcp_runtime mchips_tools)

# Demo MCP client — connects to server, lists tools, calls a tool
add_executable(mchips_demo_client demo_client.cc)
target_link_libraries(mchips_demo_client PRIVATE mchips_mcp_client_lib)
