cmake_minimum_required(VERSION 3.20)

#------------------------------------------------------------------------------
# mchips_sdk — MChiP Development SDK
#
# Provides base classes, task types, and client utilities for building
# MChiP ChiMods. This is a regular shared library (NOT a ChiMod itself).
#
# Target: libmchips_sdk.so
# Alias:  mchips::sdk
#------------------------------------------------------------------------------

add_library(mchips_sdk SHARED
  src/mchip_tasks.cc
  src/mchip_client.cc
  src/mchip_base.cc
)

add_library(mchips::sdk ALIAS mchips_sdk)

target_include_directories(mchips_sdk
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mchips_sdk
  PUBLIC
    mchips_protocol
    chimaera::cxx
)

set_target_properties(mchips_sdk PROPERTIES
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
)

#------------------------------------------------------------------------------
# Installation
#------------------------------------------------------------------------------
install(TARGETS mchips_sdk
  EXPORT mchips_sdk_targets
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)

install(DIRECTORY include/
  DESTINATION include
  FILES_MATCHING PATTERN "*.h"
)
