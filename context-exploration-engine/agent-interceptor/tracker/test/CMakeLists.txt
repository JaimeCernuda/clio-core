cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# Conversation threading test (standalone, no Chimaera)
add_executable(test_conversation_threading test_conversation_threading.cc)
target_link_libraries(test_conversation_threading PRIVATE
  dt_provenance::tracker_threading
  Catch2::Catch2WithMain
)
catch_discover_tests(test_conversation_threading)
