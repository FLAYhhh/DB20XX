# Copyright (c) 2006, 2021, Oracle and/or its affiliates.
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

SET(DB20XX_PLUGIN_DYNAMIC "ha_db20xx")
include_directories(
    include
    libs
    libs/masstree-beta
    libs/libcuckoo
)

file(GLOB DB20XX_SRC "src/*.cc")
SET(MASSTREE_SRC
libs/masstree-beta/compiler.cc
libs/masstree-beta/kvthread.cc
libs/masstree-beta/str.cc
libs/masstree-beta/string.cc
libs/masstree-beta/string_slice.cc
libs/masstree-beta/straccum.cc
)

list(APPEND DB20XX_SOURCES ${DB20XX_SRC} ${MASSTREE_SRC})

add_compile_options(-Wall -Werror)

ADD_DEFINITIONS(-DMYSQL_SERVER)
MYSQL_ADD_PLUGIN(DB20XX ${DB20XX_SOURCES} STORAGE_ENGINE DEFAULT)
