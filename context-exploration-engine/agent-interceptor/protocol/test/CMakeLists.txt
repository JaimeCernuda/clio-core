cmake_minimum_required(VERSION 3.20)

# Include Catch2 CMake integration
list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# Provider detection tests
add_executable(test_provider_detection test_provider_detection.cc)
target_link_libraries(test_provider_detection PRIVATE dt_provenance::protocol Catch2::Catch2WithMain)
catch_discover_tests(test_provider_detection)

# Session extraction tests
add_executable(test_session_extraction test_session_extraction.cc)
target_link_libraries(test_session_extraction PRIVATE dt_provenance::protocol Catch2::Catch2WithMain)
catch_discover_tests(test_session_extraction)

# Anthropic parser tests
add_executable(test_anthropic_parser test_anthropic_parser.cc)
target_link_libraries(test_anthropic_parser PRIVATE dt_provenance::protocol Catch2::Catch2WithMain)
catch_discover_tests(test_anthropic_parser)

# OpenAI parser tests
add_executable(test_openai_parser test_openai_parser.cc)
target_link_libraries(test_openai_parser PRIVATE dt_provenance::protocol Catch2::Catch2WithMain)
catch_discover_tests(test_openai_parser)

# Stream reassembly tests
add_executable(test_stream_reassembly test_stream_reassembly.cc)
target_link_libraries(test_stream_reassembly PRIVATE dt_provenance::protocol Catch2::Catch2WithMain)
catch_discover_tests(test_stream_reassembly)

# Interaction record tests
add_executable(test_interaction_record test_interaction_record.cc)
target_link_libraries(test_interaction_record PRIVATE dt_provenance::protocol Catch2::Catch2WithMain)
catch_discover_tests(test_interaction_record)
