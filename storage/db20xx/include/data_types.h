#pragma once
#include <cstdint>
#include <limits>
#include "./utils.h"

namespace db20xx {
const int NONE_INLINE_PTR_SIZE = 8;
enum TYPE_ID {
  TINYINT_ID = 0,
  SMALLINT_ID,
  MEDIUMINT_ID,
  INT_ID,
  BIGINT_ID,
  DECIMAL_ID,
  FLOAT_ID,
  DOUBLE_ID,
  CHAR_ID,
  VARCHAR_ID,
  MEDIUMTEXT_ID,
  YEAR_ID,
  DATE_ID,
  TIME_ID,
  DATETIME_ID,
  TIMESTAMP_ID,
  BLOB_ID,
  TYPE_ID_UPBOUND
};
const int FURGUR_TYPE_NUMS = TYPE_ID_UPBOUND;

// MVTO metadata in record header
const uint64_t INVALID_TRANSACTION_ID = 0;
const uint64_t INVALID_READ_TIMESTAMP = 0;
const uint64_t INVALID_TIMESTAMP = 0;
const uint64_t MIN_TIMESTAMP = 0;
const uint64_t MAX_TIMESTAMP = std::numeric_limits<uint64_t>::max();

// epoch-based transaction id
const uint64_t INVALID_EPOCH_ID = std::numeric_limits<uint64_t>::max();

//@brief visibility status of mvcc records
enum RecordVersionVisibility {
  INVALID = 0,
  INVISIBLE,
  DELETED,
  VISIBLE
};


// FIXME: 当前设置的限制没有什么依据
constexpr uint32_t FULGUR_MAX_KEYS = 255;
constexpr uint32_t FULGUR_MAX_KEY_PARTS = 255;
constexpr uint32_t FULGUR_MAX_KEY_LENGTH = 255;

}
