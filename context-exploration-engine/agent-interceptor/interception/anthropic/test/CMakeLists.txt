cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# Anthropic parsing test (standalone, no Chimaera needed — uses protocol lib)
add_executable(test_anthropic_parsing test_anthropic_parsing.cc)
target_link_libraries(test_anthropic_parsing PRIVATE
  dt_provenance::protocol
  Catch2::Catch2WithMain
)
catch_discover_tests(test_anthropic_parsing)

# Anthropic forwarding test (needs httplib for mock server)
add_executable(test_anthropic_forwarding test_anthropic_forwarding.cc)
target_link_libraries(test_anthropic_forwarding PRIVATE
  dt_provenance::protocol
  $<BUILD_INTERFACE:httplib::httplib>
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
  Catch2::Catch2WithMain
)
catch_discover_tests(test_anthropic_forwarding)
