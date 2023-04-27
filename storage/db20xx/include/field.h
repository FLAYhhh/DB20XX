#pragma once
#include <cstdint>
#include "./data_types.h"
#include "./utils.h"

namespace db20xx {

/**
 */
class Field {
  friend class Schema;
  friend class Record;

 public:
  static const bool STORE_INLINE = true;
  static const bool STORE_NON_INLINE = false;
  Field(TYPE_ID type_id, std::string name, uint32_t data_bytes,
        uint32_t off_in_record, bool store_inline, uint32_t mysql_pack_length,
        uint32_t off_in_mysql_record)
      : field_type_id_(type_id),
        field_name_(name),
        data_bytes_(data_bytes),
        off_in_record_(off_in_record),
        store_inline_(store_inline),
        mysql_pack_length_(mysql_pack_length),
        off_in_mysql_record_(off_in_mysql_record) {}

  bool store_inline() const { return store_inline_; }

  uint32_t get_offset_in_record() const { return off_in_record_; }

  uint32_t get_data_bytes() const { return data_bytes_; }

  void set_mysql_length_bytes(uint32_t length_bytes) {
    mysql_length_bytes_ = length_bytes;
  }

  uint32_t get_mysql_length_bytes() const { return mysql_length_bytes_; }

  TYPE_ID get_field_type() const { return field_type_id_; }

  /**
  @brief
    given a record, make @data[out param] point to field data,
    and make len equals data length.
  @args
    arg1 record: 指向一条记录
    arg2 data[out param]: 指向这个field的真实数据(相对于meta data而言)
    arg3 len[out param]: 该field真实数据的长度.
  */
  void get_field_data(const char *record, const char *&data,
                      uint32_t &len) const {
    if (store_inline_) {
      data = record + off_in_record_;
      len = data_bytes_;
    } else {
      const char *fixed_size_meta = record + off_in_record_;
      if (mysql_length_bytes_ == 1) {
        len = *reinterpret_cast<const uint8_t *>(fixed_size_meta);
        fixed_size_meta += 1;
      } else if (mysql_length_bytes_ == 2) {
        len = *reinterpret_cast<const uint16_t *>(fixed_size_meta);
        fixed_size_meta += 2;
      } else {
        // TODO:panic
      }
      data = *reinterpret_cast<char **>(const_cast<char *>(fixed_size_meta));
    }
  }

  void get_mysql_field_data(const char *record, const char *&data, uint32_t &len) const {
    if (store_inline_) {
      data = record + off_in_mysql_record_;
      len = data_bytes_;
    } else {
      const char *p = record + off_in_mysql_record_;
      if (mysql_length_bytes_ == 1) {
        len = *reinterpret_cast<const uint8_t *>(p);
        p += 1;
      } else if (mysql_length_bytes_ == 2) {
        len = *reinterpret_cast<const uint16_t *>(p);
        p += 2;
      } else {
        // TODO:panic
      }
      data = p;
    }
  }

 private:
  TYPE_ID field_type_id_ = TYPE_ID_UPBOUND;
  std::string field_name_ = "";
  uint32_t data_bytes_ = 0;     // 该field占用的inline字节数
  uint32_t off_in_record_ = 0;  // 该field在db20xx record中的偏移
  bool store_inline_ = true;    // 该field是否采用inline方式存储

  // Field中需要额外存储一些MySQL server层中Field的元数据.
  // 当MySQL的row buf存储到db20xx中时, 如果该field为变长数据类型，
  // 我们需要知道该field用多少个字节记录数据长度。
  uint32_t mysql_length_bytes_ = 0;
  uint32_t mysql_pack_length_ =
      0;  // total bytes occupied by a field in mysql internal format
  uint32_t off_in_mysql_record_ = 0;
};
}  // namespace db20xx
