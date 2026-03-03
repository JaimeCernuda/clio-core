cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# ctx_writer — CTE verification tool
#------------------------------------------------------------------------------

add_executable(ctx_writer ctx_writer.cc)

target_link_libraries(ctx_writer PRIVATE
  chimaera::cxx
  dt_provenance::protocol
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)

# The tracker client is header-only, but we need its include path
target_include_directories(ctx_writer PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../tracker/include
)

set_target_properties(ctx_writer PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)
