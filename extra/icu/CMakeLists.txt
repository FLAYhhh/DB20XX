# Copyright (c) 2017, 2021, Oracle and/or its affiliates.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

# This CMake file builds ICU.
# We build:
# - libicustubdata.a, a stub library for data, which we don't use.
# - libicui18n.a, the internationalization library.
# - libicuuc.a, a common utility library.
#
# To save some space and time, we have removed directories which are not
# needed by MySQL:
#   source/allinone
#   source/config
#   source/data
#   source/extra
#   source/layoutex
#   source/samples
#   source/test
#   source/tools


PROJECT (ICU)

MY_CHECK_CXX_COMPILER_WARNING("-Wmissing-profile" HAS_MISSING_PROFILE)
IF(HAS_MISSING_PROFILE)
  STRING_APPEND(CMAKE_CXX_FLAGS " ${HAS_MISSING_PROFILE}")
ENDIF()

IF(MY_COMPILER_IS_GNU_OR_CLANG)
  SET(ICU_LINUX_COMPILE_OPTIONS
    "-Wno-undef"
    "-Wno-deprecated-declarations"
    "-Wno-error"
    "-Wno-unused-parameter"
    "-Wno-missing-field-initializers"
    "-Wno-sign-compare"
    "-Wno-type-limits"
  )
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wreturn-local-addr" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

IF(MY_COMPILER_IS_GNU)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "-Wno-stringop-overflow")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wunused-but-set-variable" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmisleading-indentation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wmaybe-uninitialized" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wimplicit-int-float-conversion" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wrestrict" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

MY_CHECK_CXX_COMPILER_WARNING("-Wstringop-truncation" HAS_WARN_FLAG)
IF(HAS_WARN_FLAG)
  LIST(APPEND ICU_LINUX_COMPILE_OPTIONS "${HAS_WARN_FLAG}")
ENDIF()

IF(WIN32)
  ADD_DEFINITIONS(
    -DU_STATIC_IMPLEMENTATION=1
    -DU_IMPORT=
    -DU_EXPORT=
  )
ENDIF()

IF(MSVC AND NOT WIN32_CLANG)
  # ICU does not build cleanly with Visual Studio's /permissive-
  STRING(REPLACE "/permissive-" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # anachronism used : modifiers on data are ignored
  STRING_APPEND(CMAKE_CXX_FLAGS " /wd4229")
ENDIF()

IF(WITH_ICU STREQUAL "bundled")
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_C_FLAGS "${MY_COMPILER_FLAG_FLTO}")
  REMOVE_CMAKE_COMPILER_FLAGS(CMAKE_CXX_FLAGS "${MY_COMPILER_FLAG_FLTO}")
ENDIF()

ADD_SUBDIRECTORY(source/common)
ADD_SUBDIRECTORY(source/i18n)
ADD_SUBDIRECTORY(source/stubdata)
