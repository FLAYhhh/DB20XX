#include "./ha_fulgurdb_help.h"
#include "thread_context.h"

static void schema_add_inline_field(
              fulgurdb::Schema &schema,
              fulgurdb::TYPE_ID type_id,
              const std::string &field_name,
              uint32_t data_bytes,
              uint32_t &offset_in_rec) {
  fulgurdb::Field se_field(type_id, field_name, data_bytes,
              offset_in_rec, fulgurdb::Field::STORE_INLINE);
  offset_in_rec += data_bytes;

  schema.add_field(se_field);
}

static void schema_add_non_inline_field(
              fulgurdb::Schema &schema,
              fulgurdb::TYPE_ID type_id,
              const std::string &field_name,
              uint32_t length_bytes,
              uint32_t &offset_in_rec) {
  // non-inline方式存储的数据, field中的内容为(length_bytes + external data ptr)
  fulgurdb::Field se_field(type_id, field_name,
          length_bytes + sizeof(uint64_t), offset_in_rec,
          fulgurdb::Field::STORE_NON_INLINE);
  se_field.set_mysql_length_bytes(length_bytes);
  offset_in_rec += length_bytes = sizeof(uint64_t);

  schema.add_field(se_field);
}
/**
*/
void generate_fulgur_schema(TABLE *form, fulgurdb::Schema &schema) {
  uint32_t field_num = form->s->fields;
  Field **sl_fieldp_array = form->s->field;
  uint32_t offset_in_rec = form->s->null_bytes;
  for (uint32_t i = 0; i < field_num; i++) {
    Field *sl_fieldp = sl_fieldp_array[i];
    std::string field_name(sl_fieldp->field_name);
    uint32_t data_bytes = sl_fieldp->pack_length();
    // see {project_root}/include/field_types.h
    switch (sl_fieldp->type()) {
      case MYSQL_TYPE_TINY:
        schema_add_inline_field(schema, fulgurdb::TINYINT_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_SHORT:
        schema_add_inline_field(schema, fulgurdb::SMALLINT_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_INT24:
        schema_add_inline_field(schema, fulgurdb::MEDIUMINT_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_LONG:
        schema_add_inline_field(schema, fulgurdb::INT_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_LONGLONG:
        schema_add_inline_field(schema, fulgurdb::BIGINT_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_FLOAT:
        schema_add_inline_field(schema, fulgurdb::FLOAT_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_DOUBLE:
        schema_add_inline_field(schema, fulgurdb::DOUBLE_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_VARCHAR:
        schema_add_non_inline_field(schema, fulgurdb::VARCHAR_ID,
            field_name, sl_fieldp->get_length_bytes(), offset_in_rec);
        break;
      case MYSQL_TYPE_DECIMAL:
        schema_add_inline_field(schema, fulgurdb::DECIMAL_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_YEAR:
        schema_add_inline_field(schema, fulgurdb::YEAR_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_DATE:
        schema_add_inline_field(schema, fulgurdb::DATE_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_TIME:
        schema_add_inline_field(schema, fulgurdb::TIME_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_DATETIME:
        schema_add_inline_field(schema, fulgurdb::DATETIME_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_TIMESTAMP:
        schema_add_inline_field(schema, fulgurdb::TIMESTAMP_ID,
            field_name, data_bytes, offset_in_rec);
        break;
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_NEWDATE:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_TIME2:
      case MYSQL_TYPE_TYPED_ARRAY:
      case MYSQL_TYPE_BOOL:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_GEOMETRY:
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_INVALID:
        fulgurdb::LOG_ERROR("not support field type");
        break;
    }
  }
}

extern handlerton *fulgurdb_hton;
fulgurdb::threadinfo_type *get_threadinfo() {
  // ha_data is thread local data for storage engine
  // fulgurdb_hton->slot is the thread local data index for fulgurdb
  fulgurdb::ThreadContext *thd_ctx = reinterpret_cast<fulgurdb::ThreadContext *>
                (current_thd->get_ha_data(fulgurdb_hton->slot)->ha_ptr);
  if (thd_ctx == nullptr) {
    thd_ctx = new fulgurdb::ThreadContext(current_thd->thread_id());
    current_thd->get_ha_data(fulgurdb_hton->slot)->ha_ptr = thd_ctx;
  }
  fulgurdb::threadinfo_type *ti = thd_ctx->get_threadinfo();
  return ti;
}


fulgurdb::ThreadContext *get_thread_ctx() {
  fulgurdb::ThreadContext *thd_ctx = reinterpret_cast<fulgurdb::ThreadContext *>
                (current_thd->get_ha_data(fulgurdb_hton->slot)->ha_ptr);
  if (thd_ctx == nullptr) {
    thd_ctx = new fulgurdb::ThreadContext(current_thd->thread_id());
    current_thd->get_ha_data(fulgurdb_hton->slot)->ha_ptr = thd_ctx;
  }
  return thd_ctx;
}
