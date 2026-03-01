cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# Gateway unit tests (no Chimaera dependency)
#------------------------------------------------------------------------------

# Session manager unit tests
add_executable(test_session_manager test_session_manager.cc)
target_include_directories(test_session_manager PRIVATE
  ${CMAKE_SOURCE_DIR}/context-runtime/test
)
target_link_libraries(test_session_manager
  PRIVATE mchips_mcp_gateway_runtime mchips_protocol
)
set_target_properties(test_session_manager PROPERTIES CXX_STANDARD 20)
add_test(NAME test_session_manager COMMAND test_session_manager)

# MChiP router unit tests
add_executable(test_mchip_router test_mchip_router.cc)
target_include_directories(test_mchip_router PRIVATE
  ${CMAKE_SOURCE_DIR}/context-runtime/test
)
target_link_libraries(test_mchip_router
  PRIVATE mchips_mcp_gateway_runtime mchips_protocol mchips_sdk
)
set_target_properties(test_mchip_router PROPERTIES CXX_STANDARD 20)
add_test(NAME test_mchip_router COMMAND test_mchip_router)

# Gateway ChiMod integration test (requires Chimaera server)
add_executable(test_gateway_chimod test_gateway_chimod.cc)
target_include_directories(test_gateway_chimod PRIVATE
  ${CMAKE_SOURCE_DIR}/context-runtime/test
)
target_link_libraries(test_gateway_chimod
  PRIVATE mchips_mcp_gateway_client mchips_protocol chimaera_cxx hshm::cxx
)
set_target_properties(test_gateway_chimod PROPERTIES CXX_STANDARD 20)
add_test(NAME test_gateway_chimod COMMAND test_gateway_chimod)
