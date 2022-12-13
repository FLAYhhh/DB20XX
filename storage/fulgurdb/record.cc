#include "record.h"
#include <cstdint>
#include <cstring>
#include "data_types.h"
namespace fulgurdb {
//======================manipulate record header===============================
void Record::init() {
  header_.latch_.init();
  header_.txn_id_ = INVALID_TRANSACTION_ID;
  header_.last_read_ts_ = INVALID_READ_TIMESTAMP;
  header_.begin_ts_ = MAX_TIMESTAMP;
  header_.end_ts_ = MAX_TIMESTAMP;
  header_.older_version_ = nullptr;
  header_.newer_version_ = nullptr;
  header_.vchain_head_ = nullptr;
}

void Record::lock_header() { header_.latch_.lock(); }
void Record::unlock_header() { header_.latch_.unlock(); }

void Record::set_transaction_id(uint64_t txn_id) { header_.txn_id_ = txn_id; }
void Record::set_begin_timestamp(uint64_t begin_ts) {
  header_.begin_ts_ = begin_ts;
}
void Record::set_end_timestamp(uint64_t end_ts) { header_.end_ts_ = end_ts; }
void Record::set_last_read_timestamp(uint64_t last_read_ts) {
  header_.last_read_ts_ = last_read_ts;
}
void Record::set_older_version(Record *record) {
  header_.older_version_ = record;
}
void Record::set_newer_version(Record *record) {
  header_.newer_version_ = record;
}

uint64_t Record::get_transaction_id() const { return header_.txn_id_; }
uint64_t Record::get_begin_timestamp() const { return header_.begin_ts_; }
uint64_t Record::get_end_timestamp() const { return header_.end_ts_; }
uint64_t Record::get_last_read_timestamp() const {
  return header_.last_read_ts_;
}
Record *Record::get_newer_version() { return header_.newer_version_; }
Record *Record::get_older_version() { return header_.older_version_; }

void Record::set_vchain_head(VersionChainHead *vchain_head) {
  header_.vchain_head_ = vchain_head;
}
VersionChainHead *Record::get_vchain_head() { return header_.vchain_head_; }

//===========================load data======================================
char *Record::get_payload() { return payload_; }
RecordHeader *Record::get_header() { return &header_; }

void Record::load_data_from_mysql(char *mysql_record, const Schema &schema) {
  char *fulgur_row_data = payload_;

  // store null bytes
  uint32_t null_bytes = schema.get_null_byte_length();
  memcpy(fulgur_row_data, mysql_record, null_bytes);
  fulgur_row_data += null_bytes;
  mysql_record += null_bytes;

  // store fields
  for (uint32_t i = 0; i < schema.field_num(); i++) {
    const Field &field = schema.get_field(i);
    // 以inline方式存储在fulgurdb中的field，在mysql中一定是定长的。
    // create_table时，schema中就存储了field长度的信息。
    // insert时，来自mysql的row data中，该field一定是符合定长约定的。
    if (field.store_inline()) {
      uint32_t data_bytes = field.get_data_bytes();
      memcpy(fulgur_row_data, mysql_record, data_bytes);
      fulgur_row_data += data_bytes;
      mysql_record += data_bytes;
    } else if (field.get_field_type() == VARCHAR_ID) {
      uint32_t length_bytes = field.get_mysql_length_bytes();
      // length_bytes的取值只可能是1或者2,见mysql官方文档
      uint32_t actual_data_length = 0;
      if (length_bytes == 1) {
        actual_data_length = *(uint8_t *)(mysql_record);
        *reinterpret_cast<uint8_t *>(fulgur_row_data) =
            *reinterpret_cast<uint8_t *>(mysql_record);
      } else if (length_bytes == 2) {
        actual_data_length = *(uint16_t *)(mysql_record);
        *reinterpret_cast<uint16_t *>(fulgur_row_data) =
            *reinterpret_cast<uint16_t *>(mysql_record);
      } else {
        fulgurdb::LOG_ERROR("invalid mysql length bytes");
      }
      fulgur_row_data += length_bytes;
      mysql_record += length_bytes;
      char *actual_data = (char *)malloc(actual_data_length);
      memcpy(actual_data, mysql_record, actual_data_length);
      // non-inline存储的field中，只存储真实数据的指针
      *reinterpret_cast<char **>(fulgur_row_data) = actual_data;

      fulgur_row_data += 8;
      mysql_record += field.mysql_pack_length_ - length_bytes;
    } else if (field.get_field_type() == BLOB_ID) {
      uint32_t length_bytes = field.get_mysql_length_bytes();
      // blob's length_bytes的取值可能是{1,2,3,4},见mysql官方文档
      if (length_bytes < 1 || 4 < length_bytes){
        fulgurdb::LOG_ERROR("invalid mysql length bytes");
      }
      uint32_t actual_data_length = 0;
      memcpy(fulgur_row_data, mysql_record, length_bytes);
      memcpy(&actual_data_length, mysql_record, length_bytes);

      fulgur_row_data += length_bytes;
      mysql_record += length_bytes;
      char *mysql_blob_ptr = *reinterpret_cast<char **>(mysql_record);
      char *actual_data = (char *)malloc(actual_data_length);
      memcpy(actual_data, mysql_blob_ptr, actual_data_length);
      // non-inline存储的field中，只存储真实数据的指针
      *reinterpret_cast<char **>(fulgur_row_data) = actual_data;

      fulgur_row_data += sizeof(char *);
      mysql_record += sizeof(char *);
    }
  }
}

void Record::load_data_to_mysql(char *mysql_record, const Schema &schema) {
  char *fulgur_row_data = payload_;
  // restore null bytes
  uint32_t null_bytes = schema.get_null_byte_length();
  memcpy(mysql_record, fulgur_row_data, null_bytes);
  fulgur_row_data += null_bytes;
  mysql_record += null_bytes;

  // restore fields
  for (uint32_t i = 0; i < schema.field_num(); i++) {
    const Field &field = schema.get_field(i);
    if (field.store_inline()) {
      uint32_t data_bytes = field.get_data_bytes();
      memcpy(mysql_record, fulgur_row_data, data_bytes);
      fulgur_row_data += data_bytes;
      mysql_record += data_bytes;
    } else {
      if (field.get_field_type() == VARCHAR_ID) {
        uint32_t length_bytes = field.get_mysql_length_bytes();
        // length_bytes的取值只可能是1或者2,见mysql官方文档
        uint32_t actual_data_length = 0;
        if (length_bytes == 1) {
          actual_data_length = *(uint8_t *)(fulgur_row_data);
          *reinterpret_cast<uint8_t *>(mysql_record) =
              *reinterpret_cast<uint8_t *>(fulgur_row_data);
        } else if (length_bytes == 2) {
          actual_data_length = *(uint16_t *)(fulgur_row_data);
          *reinterpret_cast<uint16_t *>(mysql_record) =
              *reinterpret_cast<uint16_t *>(fulgur_row_data);
        } else {
          fulgurdb::LOG_ERROR("invalid mysql length bytes");
        }
        fulgur_row_data += length_bytes;
        mysql_record += length_bytes;

        char *actual_data = *reinterpret_cast<char **>(fulgur_row_data);
        memcpy(mysql_record, actual_data, actual_data_length);

        fulgur_row_data += 8;
        mysql_record += field.mysql_pack_length_ - length_bytes;
      } else if (field.get_field_type() == BLOB_ID) {
        uint32_t length_bytes = field.get_mysql_length_bytes();
        // blob's length_bytes的取值可能是{1,2,3,4},见mysql官方文档
        uint32_t actual_data_length = 0;
        if (length_bytes < 1 || 4 < length_bytes) {
          fulgurdb::LOG_ERROR("invalid mysql length bytes");
        }
        memcpy(&actual_data_length, fulgur_row_data, actual_data_length);
        fulgur_row_data += length_bytes;
        mysql_record += length_bytes;

        char *mysql_blob_ptr = *reinterpret_cast<char **>(mysql_record);
        char *actual_data = *reinterpret_cast<char **>(fulgur_row_data);
        memcpy(mysql_blob_ptr, actual_data, actual_data_length);

        fulgur_row_data += sizeof(char *);
        mysql_record += sizeof(char *);
      }
    }
  }
}
}  // namespace fulgurdb
