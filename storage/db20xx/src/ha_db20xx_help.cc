#include <sys/types.h>
#include <cstdint>
#include <iterator>
#include "../include/ha_db20xx_help.h"
#include "../include/thread_context.h"

static void schema_add_inline_field(db20xx::Schema &schema,
                                    db20xx::TYPE_ID type_id,
                                    const std::string &field_name,
                                    uint32_t data_bytes,
                                    uint32_t &offset_in_fulgur_rec,
                                    uint32_t &offset_in_mysql_rec) {
  db20xx::Field se_field(type_id, field_name, data_bytes,
                           offset_in_fulgur_rec, db20xx::Field::STORE_INLINE,
                           data_bytes, offset_in_mysql_rec);
  offset_in_fulgur_rec += data_bytes;
  offset_in_mysql_rec += data_bytes;

  schema.add_field(se_field);
}

static void schema_add_non_inline_field(db20xx::Schema &schema,
                                        db20xx::TYPE_ID type_id,
                                        const std::string &field_name,
                                        uint32_t length_bytes,
                                        uint32_t &offset_in_fulgur_rec,
                                        uint32_t &offset_in_mysql_rec,
                                        uint32_t mysql_pack_length) {
  // non-inline方式存储的数据, field中的内容为(length_bytes + external data ptr)
  db20xx::Field se_field(type_id, field_name, length_bytes + sizeof(uint64_t),
                           offset_in_fulgur_rec,
                           db20xx::Field::STORE_NON_INLINE, mysql_pack_length,
                           offset_in_mysql_rec);
  se_field.set_mysql_length_bytes(length_bytes);
  offset_in_fulgur_rec += (length_bytes + sizeof(uint64_t));
  offset_in_mysql_rec += mysql_pack_length;

  schema.add_field(se_field);
}
/**
 */
void generate_fulgur_schema(TABLE *form, db20xx::Schema &schema) {
  uint32_t field_num = form->s->fields;
  Field **sl_fieldp_array = form->s->field;
  uint32_t offset_in_fulgur_rec = form->s->null_bytes;
  uint32_t offset_in_mysql_rec = form->s->null_bytes;
  for (uint32_t i = 0; i < field_num; i++) {
    Field *sl_fieldp = sl_fieldp_array[i];
    std::string field_name(sl_fieldp->field_name);
    uint32_t data_bytes = sl_fieldp->pack_length();
    // see {project_root}/include/field_types.h
    switch (sl_fieldp->type()) {
      case MYSQL_TYPE_TINY:
        schema_add_inline_field(schema, db20xx::TINYINT_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_SHORT:
        schema_add_inline_field(schema, db20xx::SMALLINT_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_INT24:
        schema_add_inline_field(schema, db20xx::MEDIUMINT_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_LONG:
        schema_add_inline_field(schema, db20xx::INT_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_LONGLONG:
        schema_add_inline_field(schema, db20xx::BIGINT_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_FLOAT:
        schema_add_inline_field(schema, db20xx::FLOAT_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_DOUBLE:
        schema_add_inline_field(schema, db20xx::DOUBLE_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_STRING:
        schema_add_inline_field(schema, db20xx::CHAR_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_VARCHAR:
        schema_add_non_inline_field(schema, db20xx::VARCHAR_ID, field_name,
                                    sl_fieldp->get_length_bytes(),
                                    offset_in_fulgur_rec, offset_in_mysql_rec,
                                    data_bytes);
        break;
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
        schema_add_inline_field(schema, db20xx::DECIMAL_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_YEAR:
        schema_add_inline_field(schema, db20xx::YEAR_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_DATE:
        schema_add_inline_field(schema, db20xx::DATE_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_TIME:
        schema_add_inline_field(schema, db20xx::TIME_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_DATETIME:
        schema_add_inline_field(schema, db20xx::DATETIME_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_TIMESTAMP:
        schema_add_inline_field(schema, db20xx::TIMESTAMP_ID, field_name,
                                data_bytes, offset_in_fulgur_rec,
                                offset_in_mysql_rec);
        break;
      case MYSQL_TYPE_BLOB:
        // Field_blob's format: [length_bytes | ptr]
        schema_add_non_inline_field(schema, db20xx::BLOB_ID, field_name,
                                    sl_fieldp->pack_length() - sizeof(void *),
                                    offset_in_fulgur_rec, offset_in_mysql_rec,
                                    data_bytes);
        break;
      case MYSQL_TYPE_DATETIME2:
      case MYSQL_TYPE_NEWDATE:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_TIMESTAMP2:
      case MYSQL_TYPE_TIME2:
      case MYSQL_TYPE_TYPED_ARRAY:
      case MYSQL_TYPE_BOOL:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_ENUM:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
      case MYSQL_TYPE_VAR_STRING:
      case MYSQL_TYPE_GEOMETRY:
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_INVALID:
        db20xx::LOG_ERROR("No support field type[%d]", sl_fieldp->type());
        break;
    }
  }
}

extern handlerton *db20xx_hton;
db20xx::threadinfo_type *get_threadinfo() {
  // ha_data is thread local data for storage engine
  // db20xx_hton->slot is the thread local data index for db20xx
  db20xx::ThreadContext *thd_ctx =
      reinterpret_cast<db20xx::ThreadContext *>(
          current_thd->get_ha_data(db20xx_hton->slot)->ha_ptr);
  if (thd_ctx == nullptr) {
    thd_ctx = new db20xx::ThreadContext(current_thd->thread_id());
    current_thd->get_ha_data(db20xx_hton->slot)->ha_ptr = thd_ctx;
  }
  db20xx::threadinfo_type *ti = thd_ctx->get_threadinfo();
  return ti;
}

db20xx::ThreadContext *get_thread_ctx() {
  db20xx::ThreadContext *thd_ctx =
      reinterpret_cast<db20xx::ThreadContext *>(
          current_thd->get_ha_data(db20xx_hton->slot)->ha_ptr);
  if (thd_ctx == nullptr) {
    thd_ctx = new db20xx::ThreadContext(current_thd->thread_id());
    current_thd->get_ha_data(db20xx_hton->slot)->ha_ptr = thd_ctx;
  }
  return thd_ctx;
}
