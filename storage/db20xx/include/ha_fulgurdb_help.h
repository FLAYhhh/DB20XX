#pragma once
// header files exported by SE
#include "utils.h"
#include "schema.h"
#include "engine.h"


// header files exported by SL and used in ha_fulgurdb.cc
#include "storage/fulgurdb/ha_fulgurdb.h"
#include "my_dbug.h"
#include "mysql/plugin.h"
#include "sql/sql_class.h"
#include "sql/sql_plugin.h"
#include "thread_context.h"
#include "typelib.h"


/**
@brief generate SE's fulgurdb::Schema from SL's TABLE
@param form SL层TABLE的元数据
@param schema SE层table的schema(colunme type等信息)
*/
void generate_fulgur_schema(TABLE *form, fulgurdb::Schema &schema);
fulgurdb::threadinfo_type *get_threadinfo();
fulgurdb::ThreadContext *get_thread_ctx();
