# - Try to find the GLS library (libgls)
#
# Once done this will define
#
#  GLS_FOUND - System has libgls
#  GLS_INCLUDE_DIR - The libgls include directory
#  GLS_LIBRARIES - The libraries needed to use libgls

# Adapted from FindGnuTLS.cmake 2012-12-06, Martin Lambers.
# Original copyright notice:
#=============================================================================
# Copyright 2009 Kitware, Inc.
# Copyright 2009 Philip Lowman <philip@yhbt.com>
# Copyright 2009 Brad Hards <bradh@kde.org>
# Copyright 2006 Alexander Neundorf <neundorf@kde.org>
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)


IF(GLS_INCLUDE_DIR AND GLS_LIBRARY)
    # in cache already
    SET(GLS_FIND_QUIETLY TRUE)
ENDIF(GLS_INCLUDE_DIR AND GLS_LIBRARY)

FIND_PACKAGE(PkgConfig QUIET)
IF(PKG_CONFIG_FOUND)
    # try using pkg-config to get the directories and then use these values
    # in the FIND_PATH() and FIND_LIBRARY() calls
    PKG_CHECK_MODULES(PC_GLS QUIET gls)
    SET(GLS_VERSION_STRING ${PC_GLS_VERSION})
ENDIF()

FIND_PATH(GLS_INCLUDE_DIR gls/gls.h HINTS ${PC_GLS_INCLUDE_DIRS})

FIND_LIBRARY(GLS_LIBRARY NAMES gls libgls HINTS ${PC_GLS_LIBRARY_DIRS})

MARK_AS_ADVANCED(GLS_INCLUDE_DIR GLS_LIBRARY)

# handle the QUIETLY and REQUIRED arguments and set GLS_FOUND to TRUE if 
# all listed variables are TRUE
INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(GLS
    REQUIRED_VARS GLS_LIBRARY GLS_INCLUDE_DIR
    VERSION_VAR GLS_VERSION_STRING
)

IF(GLS_FOUND)
    SET(GLS_LIBRARIES ${GLS_LIBRARY})
    SET(GLS_INCLUDE_DIRS ${GLS_INCLUDE_DIR})
ENDIF()
