cmake_minimum_required(VERSION 3.20)

list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# HTTP proxy test (uses standalone dt_proxy_http, no Chimaera needed)
add_executable(test_http_proxy test_http_proxy.cc)
target_link_libraries(test_http_proxy PRIVATE
  dt_provenance::proxy_http
  Catch2::Catch2WithMain
)
catch_discover_tests(test_http_proxy)

# Session guard test
add_executable(test_session_guard test_session_guard.cc)
target_link_libraries(test_session_guard PRIVATE
  dt_provenance::proxy_http
  Catch2::Catch2WithMain
)
catch_discover_tests(test_session_guard)
