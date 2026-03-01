cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# Demo MChiP integration tests (requires Chimaera server)
#------------------------------------------------------------------------------

add_executable(test_mchip_demo test_mchip_demo.cc)
target_include_directories(test_mchip_demo PRIVATE
  ${CMAKE_SOURCE_DIR}/context-runtime/test
)
target_link_libraries(test_mchip_demo
  PRIVATE mchips_sdk mchips_protocol chimaera_cxx hshm::cxx
)
set_target_properties(test_mchip_demo PROPERTIES CXX_STANDARD 20)
add_test(NAME test_mchip_demo COMMAND test_mchip_demo)
