#include "./tuple.h"

namespace fulgurdb {
void Tuple::load_data_from_mysql(char *mysql_row_data, const Schema &schema) {
  char *fulgur_row_data = data_;
  for (uint32_t i = 0; i < schema.field_num(); i++) {
    const Field &field = schema.get_field(i);
    // 决定以inline方式存储在fulgurdb中的field，在mysql中一定是定长的。
    // create_table时，schema中就存储了field长度的信息。
    // insert时，来自mysql的row data中，该field一定是符合定长约定的。
    if (field.store_inline()) {
      uint32_t field_length = field.get_field_length();
      memcpy(fulgur_row_data, mysql_row_data, field_length);
      fulgur_row_data += field_length;
      mysql_row_data += field_length;
    } else {
    // non-inline的类型如varchar需要开辟额外的空间
      uint32_t field_length = field.get_field_length();
      uint32_t length_bytes = field.get_mysql_length_bytes();
      // length_bytes的取值只可能是1或者2,见mysql官方文档
      uint32_t actual_data_length = 0;
      if (length_bytes == 1)
        actual_data_length = *(uint8_t *)(mysql_row_data);
      else if (length_bytes == 2)
        actual_data_length = *(uint16_t *)(mysql_row_data);
      else
        fulgurdb::LOG_ERROR("invalid mysql length bytes");
      char *actual_data = (char *)malloc(actual_data_length);
      memcpy(actual_data, mysql_row_data + length_bytes, actual_data_length);
      // non-inline存储的field中，只存储真实数据的指针
      *reinterpret_cast<char **>(fulgur_row_data) = actual_data;

      fulgur_row_data += field_length;
      mysql_row_data += length_bytes + actual_data_length;
    }
  }
}
}
