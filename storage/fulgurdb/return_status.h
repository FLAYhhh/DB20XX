#pragma once
namespace fulgurdb {
enum ReturnStatus {
  FULGUR_SUCCESS = 0,
  FULGUR_BLOCK_FULL,
  FULGUR_END_OF_TABLE,
  FULGUR_INVALID_RECORD_LOCATION
};

}
