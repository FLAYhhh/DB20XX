#pragma once
#include "utils.h"
#include "schema.h"

namespace fulgurdb {

//class Record;
class RecordLocation {
friend class Table;
friend class RecordBlock;
public:
  /**
  @brief
    used in sequential table scan, we use RecordLocation as
    a scan cursor. this funciton reset the cursor state.
  */
  void reset() {
    block_id_ = 0;
    idx_in_block_ = 0;
    record_ = nullptr;
  }

  /**
  @brief
    used in sequential table scan, increase record location to next.
    we will correct the cursor in Table::table_scan_get().
  */
  void inc_cursor() {
    idx_in_block_ += 1;
  }

  void load_data_from_mysql(char *mysql_row_data, const Schema &schema);
  void load_data_to_mysql(char *mysql_row_data, const Schema &schema);

  const char *get_record_payload() const;


private:
  uint32_t block_id_ = 0;
  uint32_t idx_in_block_ = 0;
  char *record_ = nullptr;
};

}
