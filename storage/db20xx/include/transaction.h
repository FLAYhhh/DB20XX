#pragma once
#include <openssl/pem.h>
#include <sys/types.h>
#include <cassert>
#include <cstdint>
#include <unordered_set>
#include "data_types.h"
#include "epoch.h"
#include "record.h"
#include "record_block.h"
#include "return_status.h"
#include "utils.h"
#include "version_chain.h"
namespace db20xx {

class Table;
class ThreadContext;

class TransactionContext {
  friend class Table;
 public:
  bool on_going();
  void begin_transaction(uint64_t thread_id);

  void mvto_insert(Record *record, VersionChainHead *vchain_head, Table *table, ThreadContext *thd_ctx);
  int mvto_update(Record *old_record, char *new_mysql_record, Table *table,
                  ThreadContext *thd_ctx);
  int mvto_delete(Record *record, Table *table, ThreadContext *thd_ctx);

  /**
   * @args
   *   @arg3 record[output] get a version visible to current transaction
   */
  int mvto_read_version_chain(VersionChainHead &version_head, bool read_own,
                              Record *&record);
  int get_transaction_status();
  void set_abort();
  int commit();
  void abort();

 private:
  void update_last_read_ts_if_need(Record *record);
  int mvto_read_vchain_unown(VersionChainHead &vchain_head, Record *&record);
  int mvto_read_vchain_own(VersionChainHead &vchain_head, Record *&record);
  void reset();
  void add_to_modify_set(Record *record);

 private:
  bool started_ = false;
  bool should_abort_ = false;
  uint64_t transaction_id_ = 0;
  uint64_t epoch_id_ = 0;
  uint64_t thread_id_ = 0;

  // TODO: rename to txn_own_set_;
  std::unordered_set<Record *> txn_modify_set_;
};

}  // namespace db20xx
