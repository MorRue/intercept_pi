# ~~~
# Summary:      Local, non-generic plugin setup
# Copyright (c) 2020-2021 Mike Rossiter
# License:      GPLv3+
# ~~~

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.


# -------- Options ----------

# Cloudsmith repositories that CI uploads built artifacts to. They do not
# exist yet; the upload step is skipped unless CI has Cloudsmith
# credentials, so builds are unaffected until then. Previously these named
# opencpn/* -- the project's own repositories, which we cannot upload to.

set(OCPN_TEST_REPO
    "MorRue/intercept-alpha"
    CACHE STRING "Default repository for untagged builds"
)
set(OCPN_BETA_REPO
    "MorRue/intercept-beta"
    CACHE STRING
    "Default repository for tagged builds matching 'beta'"
)
set(OCPN_RELEASE_REPO
    "MorRue/intercept-prod"
    CACHE STRING
    "Default repository for tagged builds not matching 'beta'"
)

#
#
# -------  Plugin setup --------
#
set(PKG_NAME intercept_pi)
set(PKG_VERSION 0.1.0) # Major.Minor.Patch
set(PKG_PRERELEASE "")  # Empty, or a tag like 'beta'

set(DISPLAY_NAME Intercept)    # Dialogs, installer artifacts, ...
set(PLUGIN_API_NAME Intercept) # As of GetCommonName() in plugin API
set(PKG_SUMMARY "Course to steer onto a reported position")
set(PKG_DESCRIPTION [=[
Computes the course to steer to reach a reported position, including the
case where that position is itself moving. Intended for closing a vessel
in distress: enter the reported position and time, and the plugin gives
bearing, distance and estimated time of arrival from own ship.
]=])

set(PKG_AUTHOR "momo")
set(PKG_IS_OPEN_SOURCE "yes")
set(PKG_HOMEPAGE https://github.com/MorRue/intercept_pi)
set(PKG_INFO_URL https://github.com/MorRue/intercept_pi)

set(SRC
    src/intercept_pi.h
    src/intercept_pi.cpp
    src/plug_utils.cpp
    src/plug_utils.h
)

set(PKG_API_LIB api-18)  #  A dir in opencpn-libs/ e. g., api-17 or api-16

# Minimum version which should be loaded. Ignored if api < 22.
# Allows plugins using functions from 1.22+ API to be loaded by
# hosts only supporting 1.21; plugins then needs to check if host
# supports 1.22+ before using such functions.
## set(MIN_API_VERSION 1.21)

macro(late_init)
  # Perform initialization after the PACKAGE_NAME library, compilers
  # and ocpn::api is available.
  if (APPLE)
    target_compile_definitions(${PACKAGE_NAME} PUBLIC OCPN_GHC_FILESYSTEM)
  endif ()
endmacro ()

macro(add_plugin_libraries)
  # Add libraries required by this plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/libs/std_filesystem")
  target_link_libraries(${PACKAGE_NAME} ocpn::filesystem)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/tinyxml")
  target_link_libraries(${PACKAGE_NAME} ocpn::tinyxml)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxJSON")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxjson)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/plugin_dc")
  target_link_libraries(${PACKAGE_NAME} ocpn::plugin-dc)

  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/jsoncpp")
  target_link_libraries(${PACKAGE_NAME} ocpn::jsoncpp)

  # The wxsvg library enables SVG overall in the plugin
  add_subdirectory("${CMAKE_SOURCE_DIR}/opencpn-libs/wxsvg")
  target_link_libraries(${PACKAGE_NAME} ocpn::wxsvg)
endmacro ()
