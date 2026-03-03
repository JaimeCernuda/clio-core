cmake_minimum_required(VERSION 3.20)

# OpenAI interception tests

add_executable(test_openai_parsing test_openai_parsing.cc)
target_link_libraries(test_openai_parsing PRIVATE
  dt_provenance::protocol
  Catch2::Catch2WithMain
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)
set_target_properties(test_openai_parsing PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME test_openai_parsing COMMAND test_openai_parsing)

add_executable(test_openai_forwarding test_openai_forwarding.cc)
target_link_libraries(test_openai_forwarding PRIVATE
  Catch2::Catch2WithMain
  $<BUILD_INTERFACE:httplib::httplib>
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)
set_target_properties(test_openai_forwarding PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME test_openai_forwarding COMMAND test_openai_forwarding)
