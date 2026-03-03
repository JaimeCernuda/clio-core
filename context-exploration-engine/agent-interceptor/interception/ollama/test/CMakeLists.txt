cmake_minimum_required(VERSION 3.20)

# Ollama interception tests

add_executable(test_ollama_parsing test_ollama_parsing.cc)
target_link_libraries(test_ollama_parsing PRIVATE
  dt_provenance::protocol
  Catch2::Catch2WithMain
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)
set_target_properties(test_ollama_parsing PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME test_ollama_parsing COMMAND test_ollama_parsing)

add_executable(test_ollama_forwarding test_ollama_forwarding.cc)
target_link_libraries(test_ollama_forwarding PRIVATE
  Catch2::Catch2WithMain
  $<BUILD_INTERFACE:httplib::httplib>
  $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)
set_target_properties(test_ollama_forwarding PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
add_test(NAME test_ollama_forwarding COMMAND test_ollama_forwarding)
