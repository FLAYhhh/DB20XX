#include "./ha_fulgurdb_help.h"

static void append_field_to_schema(fulgurdb::Schema &schema,
            Field *sl_field, bool in_place) {
  fulgurdb::Field se_field(std::string(sl_field->field_name),
          sl_field->field_length, in_place);

  schema.add_field(se_field);
}

/**
*/
void generate_fulgur_schema(TABLE *form, fulgurdb::Schema &schema) {
  uint32_t field_num = form->s->fields;
  Field **sl_fieldp_array = form->s->field;
  for (uint32_t i = 0; i < field_num; i++) {
    Field *sl_fieldp = sl_fieldp_array[i];
    //std::string field_name(sl_fieldp->field_name);
    //uint32_t field_length = sl_fieldp->field_length;
    // see {project_root}/include/field_types.h
    // FIXME:确认每种类型是否是in-place存储方式
    switch (sl_fieldp->type()) {
      case MYSQL_TYPE_DECIMAL:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TINY:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_SHORT:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_LONG:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_FLOAT:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_DOUBLE:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_NULL:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TIMESTAMP:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_LONGLONG:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_INT24:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_DATE:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TIME:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_DATETIME:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_DATETIME2:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_YEAR:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_NEWDATE:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_VARCHAR:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_BIT:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TIMESTAMP2:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TIME2:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TYPED_ARRAY:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_BOOL:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_JSON:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_NEWDECIMAL:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_ENUM:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_SET:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_TINY_BLOB:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_MEDIUM_BLOB:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_LONG_BLOB:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_BLOB:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_VAR_STRING:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_STRING:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_GEOMETRY:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
      case MYSQL_TYPE_INVALID:
        append_field_to_schema(schema, sl_fieldp, true);
        break;
    }
  }
}
