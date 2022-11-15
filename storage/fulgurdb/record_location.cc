#include "./record_location.h"

namespace fulgurdb {
void RecordLocation::load_data_from_mysql(char *mysql_row_data, const Schema &schema) {
  char *fulgur_row_data = data_;

  // store null bytes
  uint32_t null_bytes = schema.get_null_byte_length();
  memcpy(fulgur_row_data, mysql_row_data, null_bytes);
  fulgur_row_data += null_bytes;
  mysql_row_data += null_bytes;

  // store fields
  for (uint32_t i = 0; i < schema.field_num(); i++) {
    const Field &field = schema.get_field(i);
    // 决定以inline方式存储在fulgurdb中的field，在mysql中一定是定长的。
    // create_table时，schema中就存储了field长度的信息。
    // insert时，来自mysql的row data中，该field一定是符合定长约定的。
    if (field.store_inline()) {
      uint32_t data_bytes = field.get_data_bytes();
      memcpy(fulgur_row_data, mysql_row_data, data_bytes);
      fulgur_row_data += data_bytes;
      mysql_row_data += data_bytes;
    } else {
    // TODO: 目前只支持varchar, 还需要支持blob等类型
    // non-inline的类型如varchar需要开辟额外的空间
      uint32_t length_bytes = field.get_mysql_length_bytes();
      // length_bytes的取值只可能是1或者2,见mysql官方文档
      uint32_t actual_data_length = 0;
      if (length_bytes == 1) {
        actual_data_length = *(uint8_t *)(mysql_row_data);
        *reinterpret_cast<uint8_t *>(fulgur_row_data) = *reinterpret_cast<uint8_t *>(mysql_row_data);
      } else if (length_bytes == 2) {
        actual_data_length = *(uint16_t *)(mysql_row_data);
        *reinterpret_cast<uint16_t *>(fulgur_row_data) = *reinterpret_cast<uint16_t *>(mysql_row_data);
      } else {
        fulgurdb::LOG_ERROR("invalid mysql length bytes");
      }
      fulgur_row_data += length_bytes;
      mysql_row_data += length_bytes;
      char *actual_data = (char *)malloc(actual_data_length);
      memcpy(actual_data, mysql_row_data, actual_data_length);
      // non-inline存储的field中，只存储真实数据的指针
      *reinterpret_cast<char **>(fulgur_row_data) = actual_data;

      fulgur_row_data += 8;
      mysql_row_data += actual_data_length;
    }
  }
}

void RecordLocation::load_data_to_mysql(char *mysql_row_data, const Schema &schema) {
  char *fulgur_row_data = data_;
  // restore null bytes
  uint32_t null_bytes = schema.get_null_byte_length();
  memcpy(mysql_row_data, fulgur_row_data, null_bytes);
  fulgur_row_data += null_bytes;
  mysql_row_data += null_bytes;

  // restore fields
  for (uint32_t i = 0; i < schema.field_num(); i++) {
    const Field &field = schema.get_field(i);
    if (field.store_inline()) {
      uint32_t data_bytes = field.get_data_bytes();
      memcpy(mysql_row_data, fulgur_row_data, data_bytes);
      fulgur_row_data += data_bytes;
      mysql_row_data += data_bytes;
    } else {
      if (field.get_field_type() == VARCHAR_ID) {
        uint32_t length_bytes = field.get_mysql_length_bytes();
        // length_bytes的取值只可能是1或者2,见mysql官方文档
        uint32_t actual_data_length = 0;
        if (length_bytes == 1) {
          actual_data_length = *(uint8_t *)(fulgur_row_data);
          *reinterpret_cast<uint8_t *>(mysql_row_data) = *reinterpret_cast<uint8_t *>(fulgur_row_data);
        } else if (length_bytes == 2) {
          actual_data_length = *(uint16_t *)(fulgur_row_data);
          *reinterpret_cast<uint16_t *>(mysql_row_data) = *reinterpret_cast<uint16_t *>(fulgur_row_data);
        } else {
          fulgurdb::LOG_ERROR("invalid mysql length bytes");
        }
        fulgur_row_data += length_bytes;
        mysql_row_data += length_bytes;

        char *actual_data = *reinterpret_cast<char **>(fulgur_row_data);
        memcpy(mysql_row_data, actual_data, actual_data_length);

        fulgur_row_data += 8;
        mysql_row_data += actual_data_length;
      }
    }
  }
}

}
