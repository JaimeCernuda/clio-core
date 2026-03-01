cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# SDK unit tests (no Chimaera dependency)
#------------------------------------------------------------------------------

add_executable(test_tool_registrar test_tool_registrar.cc)
target_include_directories(test_tool_registrar PRIVATE
  ${CMAKE_SOURCE_DIR}/context-runtime/test
)
target_link_libraries(test_tool_registrar
  PRIVATE mchips_sdk mchips_protocol
)
set_target_properties(test_tool_registrar PROPERTIES CXX_STANDARD 20)
add_test(NAME test_tool_registrar COMMAND test_tool_registrar)
