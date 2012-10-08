# - Find the GLS includes and library
#
# This module accepts the following environment variables:
#  GLS_ROOT - Specify the location of libgls
#
# This module defines
#  GLS_FOUND - If false, do not try to use libgls.
#  GLS_INCLUDE_DIRS - Where to find the headers.
#  GLS_LIBRARIES - The libraries to link against to use libgls.

# Copyright (C) 2012
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

INCLUDE(FindPkgConfig OPTIONAL)

IF(PKG_CONFIG_FOUND)
    INCLUDE(FindPkgConfig)
    PKG_CHECK_MODULES(GLS gls)
ELSE(PKG_CONFIG_FOUND)
    FIND_PATH(GLS_INCLUDE_DIRS gls/gls.h
        $ENV{GLS_ROOT}/include
        $ENV{GLS_ROOT}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/include
        /usr/include
        /sw/include # Fink
        /opt/local/include # DarwinPorts
        /opt/csw/include # Blastwave
        /opt/include
        /usr/freeware/include
    )
    FIND_LIBRARY(GLS_LIBRARIES 
        NAMES gls libgls
        PATHS
        $ENV{GLS_ROOT}/lib
        $ENV{GLS_ROOT}
        ~/Library/Frameworks
        /Library/Frameworks
        /usr/local/lib
        /usr/lib
        /sw/lib
        /opt/local/lib
        /opt/csw/lib
        /opt/lib
        /usr/freeware/lib64
    )
    SET(GLS_FOUND "NO")
    IF(GLS_LIBRARIES AND GLS_INCLUDE_DIRS)
        SET(GLS_FOUND "YES")
    ENDIF(GLS_LIBRARIES AND GLS_INCLUDE_DIRS)
ENDIF(PKG_CONFIG_FOUND)
