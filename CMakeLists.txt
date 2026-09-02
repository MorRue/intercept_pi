# ~~~
# Summary:      Main plugin build script
# Copyright (c) 2020-2021 Mike Rossiter
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# -------- Cmake setup ---------
#
cmake_minimum_required(VERSION 3.12.0)
include(${CMAKE_SOURCE_DIR}/cmake/CmakeSetup.cmake NO_POLICY_SCOPE)
message(STATUS "Cmake version: ${CMAKE_VERSION}.")


# -------- Build setup  --------
#
include(PluginOptions)
if (BUILD_TYPE)
  message(STATUS "Building: ${BUILD_TYPE}")
else ()
  message(STATUS "Configuring")
endif ()

# -------- Plugin setup ----------
#
include(Plugin.cmake)

# -------- Setup completed, build the plugin --------
#
project(${PKG_NAME} VERSION ${PKG_VERSION})
include(PluginCompiler)

add_library(${CMAKE_PROJECT_NAME} SHARED EXCLUDE_FROM_ALL ${SRC})
include_directories(BEFORE ${CMAKE_BINARY_DIR}/include)

# -------- Standalone GribReader test --------
#
# Links src/grib_reader.cpp directly, without the OpenCPN plugin SDK or the
# wxWidgets GUI toolkit: GribReader's public API only needs wxBase types
# (wxFileName, wxDateTime), so this looks up wxWidgets itself instead of
# going through the plugin's own wx setup, which only runs once BUILD_TYPE
# is set (see the two-phase configure note in CLAUDE.md) and pulls in the
# full GUI component list this test does not need.
find_package(wxWidgets COMPONENTS base)
if (wxWidgets_FOUND)
  include(${wxWidgets_USE_FILE})
  add_executable(test_grib_reader test/test_grib_reader.cpp src/grib_reader.cpp)
  target_include_directories(test_grib_reader PRIVATE src)
  # The system wx headers found above default to a GUI build (wxUSE_GUI=1
  # in their setup.h); without this, wx/wx.h pulls in GUI class definitions
  # that emit vtables this test can't link against, since it only pulls in
  # the wxWidgets base component. `wx-config --cflags base` sets the same
  # define when used directly.
  target_compile_definitions(test_grib_reader PRIVATE wxUSE_GUI=0)
  target_link_libraries(test_grib_reader ${wxWidgets_LIBRARIES})
  enable_testing()
  add_test(NAME grib_reader_test
           COMMAND test_grib_reader "${CMAKE_SOURCE_DIR}/test/data")

  # -------- Standalone datum ageing test --------
  #
  # Same rationale as test_grib_reader above: ComputeAgedDatum() and its
  # GribReader dependency only need wxBase types, so this links both .cpp
  # files directly rather than pulling in the full plugin build.
  add_executable(test_datum_age test/test_datum_age.cpp src/datum_age.cpp
                 src/grib_reader.cpp)
  target_include_directories(test_datum_age PRIVATE src)
  target_compile_definitions(test_datum_age PRIVATE wxUSE_GUI=0)
  target_link_libraries(test_datum_age ${wxWidgets_LIBRARIES})
  add_test(NAME datum_age_test
           COMMAND test_datum_age "${CMAKE_SOURCE_DIR}/test/data")
else ()
  message(WARNING
          "wxWidgets (base) not found; skipping test_grib_reader and "
          "test_datum_age")
endif ()

add_subdirectory("opencpn-libs/${PKG_API_LIB}")
target_link_libraries(${CMAKE_PROJECT_NAME} ocpn::api)
if (NOT "${MIN_API_VERSION}" STREQUAL "")
  if ("${API_VERSION}" VERSION_GREATER "1.21")
    set(API_VERSION ${MIN_API_VERSION})
  endif ()
endif ()

set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(VERSION_PATCH ${PROJECT_VERSION_PATCH})
if (PROJECT_VERSION_TWEAK STREQUAL "")
  set(PROJECT_VERSION_TWEAK 0)
endif ()


set(PACKAGE_NAME ${CMAKE_PROJECT_NAME})

# Create the minimim ocpn version id string like ov50
string(REGEX REPLACE "\([0-9]\)\.\([0-9]\).*" "ov\\1\\2"
       OCPN_MIN_VERSION ${API_OCPN_MIN_VERSION})

if (COMMAND late_init)
  late_init()
endif ()

# Optional IDE setup, typically target_include_directories() etc.
if (EXISTS ide_setup.cmake)
  include(ide_setup.cmake)
endif ()

# Configure the flatpak manifest
file(
  GLOB manifest ${PROJECT_SOURCE_DIR}/flatpak/org.opencpn.OpenCPN.Plugin.*.yaml
)
include(ConfigureManifest)
configure_manifest(${manifest} _new_manifest)

# Set up targets. Targets sets up new targets with BUILD_TYPE set to
# 'flatpak', 'android' or 'tarball'. The initial call without BUILD_TYPE ends
# here.
#
include(Targets)
create_targets(${_new_manifest})

if ("${BUILD_TYPE}" STREQUAL "")
  return ()
endif ()

if (NOT ${BUILD_TYPE} STREQUAL "flatpak")
  # Build package as required (flatpak already dealt with).
  #
  if (APPLE)
    include(MacosWxwidgets)
  endif ()
  include(PluginInstall)
  if (QT_ANDROID)
    include(AndroidLibs)
  else ()
    include(PluginLibs)
  endif ()
  include(PluginLocalization)

  include_directories(BEFORE ${CMAKE_BINARY_DIR}/include)

  if (COMMAND add_plugin_libraries)
    add_plugin_libraries()
  endif ()
endif ()

configure_file(
  ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_BINARY_DIR}/include/config.h
  @ONLY
)

configure_file(
  # The cloudsmith upload script
  ${CMAKE_SOURCE_DIR}/ci/upload.sh.in ${CMAKE_BINARY_DIR}/upload.sh
  @ONLY
)
configure_file(
  # The cloudsmith upload script, windows bat file.
  ${CMAKE_SOURCE_DIR}/ci/upload.bat.in ${CMAKE_BINARY_DIR}/upload.bat
  @ONLY
)

set(checksum "@checksum@")
configure_file(
  # The XML metadata file
  ${CMAKE_SOURCE_DIR}/plugin.xml.in
  ${CMAKE_BINARY_DIR}/${pkg_displayname}.xml.in
  @ONLY
)
