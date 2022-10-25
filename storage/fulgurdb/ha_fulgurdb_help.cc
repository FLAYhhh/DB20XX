#include "./ha_fulgurdb_help.h"

static void schema_add_inline_field(
              fulgurdb::Schema &schema,
              fulgurdb::TYPE_ID type_id,
              const std::string &field_name,
              uint32_t data_bytes) {
  fulgurdb::Field se_field(type_id, field_name, data_bytes, fulgurdb::Field::STORE_INLINE);

  schema.add_field(se_field);
}

static void schema_add_non_inline_field(
              fulgurdb::Schema &schema,
              fulgurdb::TYPE_ID type_id,
              const std::string &field_name,
              uint32_t length_bytes) {
  fulgurdb::Field se_field(type_id, field_name, length_bytes + 8, fulgurdb::Field::STORE_NON_INLINE);
  se_field.set_mysql_length_bytes(length_bytes);

  schema.add_field(se_field);
}
/**
*/
void generate_fulgur_schema(TABLE *form, fulgurdb::Schema &schema) {
  uint32_t field_num = form->s->fields;
  Field **sl_fieldp_array = form->s->field;
  for (uint32_t i = 0; i < field_num; i++) {
    Field *sl_fieldp = sl_fieldp_array[i];
    std::string field_name(sl_fieldp->field_name);
    uint32_t data_bytes = sl_fieldp->pack_length();
    // see {project_root}/include/field_types.h
    switch (sl_fieldp->type()) {
      case MYSQL_TYPE_TINY:
        schema_add_inline_field(schema, fulgurdb::TINYINT_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_SHORT:
        schema_add_inline_field(schema, fulgurdb::SMALLINT_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_INT24:
        schema_add_inline_field(schema, fulgurdb::MEDIUMINT_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_LONG:
        schema_add_inline_field(schema, fulgurdb::INT_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_LONGLONG:
        schema_add_inline_field(schema, fulgurdb::BIGINT_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_FLOAT:
        schema_add_inline_field(schema, fulgurdb::FLOAT_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_DOUBLE:
        schema_add_inline_field(schema, fulgurdb::DOUBLE_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_VARCHAR:
        schema_add_non_inline_field(schema, fulgurdb::VARCHAR_ID,
            field_name, sl_fieldp->get_length_bytes());
        break;
      case MYSQL_TYPE_DECIMAL:
        schema_add_inline_field(schema, fulgurdb::DECIMAL_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_YEAR:
        schema_add_inline_field(schema, fulgurdb::YEAR_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_DATE:
        schema_add_inline_field(schema, fulgurdb::DATE_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_TIME:
        schema_add_inline_field(schema, fulgurdb::TIME_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_DATETIME:
        schema_add_inline_field(schema, fulgurdb::DATETIME_ID,
            field_name, data_bytes);
        break;
      case MYSQL_TYPE_TIMESTAMP:
        schema_add_inline_field(schema, fulgurdb::TIMESTAMP_ID,
            field_name, data_bytes);
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
