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
namespace fulgurdb {

class Table;
class ThreadContext;

class TransactionContext {
 public:
  bool on_going();
  void begin_transaction(uint64_t thread_id);

  void mvto_insert(Record *record, Table *table, ThreadContext *thd_ctx);
  int mvto_update(Record *old_record, char *new_mysql_record, Table *table,
                  ThreadContext *thd_ctx);
  int mvto_delete(Record *record, Table *table, ThreadContext *thd_ctx);
  int mvto_get_visibility(Record *record);
  int mvto_read_single_version(Record *record, bool read_own);

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
  int mvto_read_single_own(Record *record);
  int mvto_read_single_unown(Record *record);
  bool get_version_ownership(Record *record, uint64_t transaction_id);
  bool owned_by_me(Record *record);
  bool owned_by_nobody(Record *record);
  void reset();

  void add_to_modify_set(Record *record);
  /*
  void add_to_insert_set(Record *record);
  void add_to_update_set(Record *record);
  void add_to_delete_set(Record *record);
  void add_to_ins_del_set(Record *record);
  void add_to_read_own_set(Record *record);

  bool in_update_set(Record *record);
  bool in_delete_set(Record *record);
  bool erase_from_delele_set(Record *record);
  */
 private:
  bool started_ = false;
  bool should_abort_ = false;
  uint64_t transaction_id_ = 0;
  uint64_t epoch_id_ = 0;
  uint64_t thread_id_ = 0;
  // bool read_only_ = false;

  std::unordered_set<Record *> txn_modify_set_;
  /*
  std::unordered_set<Record *> txn_insert_set_;
  std::unordered_set<Record *> txn_update_set_;
  std::unordered_set<Record *> txn_delete_set_;
  // Insert and Delete in the same transaction
  std::unordered_set<Record *> txn_ins_del_set_;
  std::unordered_set<Record *> txn_read_set_;
  std::unordered_set<Record *> txn_read_own_set_;
  */
};

}  // namespace fulgurdb
