cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# Top-level integration tests and benchmarks
#------------------------------------------------------------------------------

# Native client integration test (requires Chimaera server)
add_executable(test_native_client test_native_client.cc)
target_include_directories(test_native_client PRIVATE
  ${CMAKE_SOURCE_DIR}/context-runtime/test
)
target_link_libraries(test_native_client
  PRIVATE mchips_native_client mchips_protocol chimaera_cxx hshm::cxx
)
set_target_properties(test_native_client PROPERTIES CXX_STANDARD 20)
add_test(NAME test_native_client COMMAND test_native_client)

# Native vs HTTP benchmark
add_executable(bench_native_vs_http bench_native_vs_http.cc)
target_link_libraries(bench_native_vs_http
  PRIVATE
    mchips_native_client
    mchips_mcp_client_lib
    mchips_protocol
    chimaera_cxx
    hshm::cxx
)
set_target_properties(bench_native_vs_http PROPERTIES CXX_STANDARD 20)

# Standalone C++ MCP benchmark server (no Chimaera)
add_executable(bench_server_cpp bench_server_cpp.cc)
target_link_libraries(bench_server_cpp
  PRIVATE mchips_protocol mchips_mcp_client_lib
)
set_target_properties(bench_server_cpp PROPERTIES CXX_STANDARD 20)
