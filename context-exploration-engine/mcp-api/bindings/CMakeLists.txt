cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_ext — Python bindings via nanobind
#
# Exposes the standalone McpClient to Python.
# Only built when WRP_CORE_ENABLE_PYTHON=ON.
#
# Target: mchips_ext.so (Python module)
#------------------------------------------------------------------------------

if(NOT COMMAND nanobind_add_module)
  message(WARNING "nanobind_add_module not found. MChiPs Python bindings will be skipped.")
  return()
endif()

find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)

nanobind_add_module(mchips_ext
  python_bindings.cc
)

target_link_libraries(mchips_ext
  PRIVATE
    mchips_mcp_client_lib
    mchips_protocol
)

install(TARGETS mchips_ext
  LIBRARY DESTINATION "${PYTHON_SITE_PACKAGES}"
)

message(STATUS "MChiPs Python bindings will be installed to: ${PYTHON_SITE_PACKAGES}")
