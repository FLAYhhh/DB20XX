#pragma once
#include <openssl/pem.h>
#include <sys/types.h>
#include <cassert>
#include <cstdint>
#include <unordered_set>
#include "data_types.h"
#include "epoch.h"
#include "index_indirection.h"
#include "return_status.h"
#include "utils.h"
#include "record_location.h"
#include "record_block.h"
#include "record.h"
namespace fulgurdb {


//FIXME
class TransactionContext {
public:
  /**
   *@brief
   *  Initialize transaction status;
   *  transaction_id_: (current global epoch id, txn_id in this epoch)
   *  epoch_id_: current global epoch id
   *  thread_id_: get from caller
   */
  void begin_transaction(uint64_t thread_id) {
    // transaction id is allocated at enter_epoch time
    transaction_id_ = GlocalEpochManager::enter_epoch(thread_id);
    epoch_id_ = transaction_id_ >> 32;
    thread_id_ = thread_id;
    //is_written_ = false;
    // timestamp_ = DateFunctions::Now();
    //isolation_level_ = isolation;

    //gc_set_ = std::make_shared<GCSet>();
    //gc_object_set_ = std::make_shared<GCObjectSet>();
  }

  void apply_insert(RecordLocation &rec_loc) {
    RecordHeader *rec_header = rec_loc.get_record_header();

    //FIXME
    rec_header->set_transaction_id(transaction_id_);
    rec_header->set_last_read_timestamp(transaction_id_);

    // no need to set next item pointer.
    // Add the new tuple into the insert set
    add_to_insert_set(rec_loc);
  }

  /**
   *@brief
   *  apply_read() and get_visibility() are used for table scan
   *  without index
   */
  int apply_read(RecordLocation &rec_loc, bool read_own) {
    if (read_own)
      return apply_read_own(rec_loc);
    else
      return apply_read_not_own(rec_loc);
  }

  /**
   * FIXME: consider effects of update/delete operations
   * TODO: move visibility judge to apply read
   *@brief
   *  currently, get_visibility() only works for read operation
   */
  int get_visibility(RecordLocation &rec_loc) {
    // FIXME we do not hold lock of this record version,
    //       what if its header is changed by other threads
    //       during current thread's execution?
    RecordHeader *rec_header = rec_loc.get_record_header();
    uint64_t record_begin_ts = rec_header->get_begin_timestamp();
    uint64_t record_end_ts = rec_header->get_end_timestamp();
    uint64_t record_txn_id = rec_header->get_transaction_id();
    bool ownership = (transaction_id_ == record_txn_id);

    bool committed_version = (transaction_id_ >= record_begin_ts);
    bool invalidated_version = (transaction_id_ >= record_end_ts);

    if (ownership == true) {
      // condition: current transaction holds this record version
      // sub-case1: this is a newly inserted/updated record version
      if (record_begin_ts == MAX_TIMESTAMP
          && record_end_ts == MAX_TIMESTAMP) {
        return RecordVersionVisibility::VISIBLE;
      // sub-case2: TODO (version in read_own set)
      } else if (record_end_ts == INVALID_TIMESTAMP) {
        return RecordVersionVisibility::DELETED;
      } else {
        return RecordVersionVisibility::INVISIBLE;
      }
    } else {
      // condition: current transaction doesn't hold this record version
      // sub-case1: this record version is owned by other transaction
      if (record_txn_id != INVALID_TRANSACTION_ID) {
        // sub-sub-case1: this version has not been commited by another txn
        if (record_begin_ts == MAX_TIMESTAMP) {
          return RecordVersionVisibility::INVISIBLE;
        } else { // sub-sub-case2: has been commited by another txn
          if (committed_version && !invalidated_version) {
            return RecordVersionVisibility::VISIBLE;
          } else {
            return RecordVersionVisibility::INVISIBLE;
          }
        }
      // sub-case2: this record version isn't owned by any transaction
      } else {
        if (committed_version && !invalidated_version) {
          return RecordVersionVisibility::VISIBLE;
        } else {
          return RecordVersionVisibility::INVISIBLE;
        }
      }
    }
  }

  int read_traverse_version_chain(RecordVersionHead &version_head,
                                  bool read_own,
                                  RecordLocation &rec_loc) {
    int retry_time = 0;
    int ret = FULGUR_RETRY;
    while (ret == FULGUR_RETRY && retry_time < 5) {
      if (read_own) {
        ret = read_own_traverse_vchain(version_head, rec_loc);
      } else {
        ret = read_unown_traverse_vchain(version_head, rec_loc);
      }
    }

    if (ret == FULGUR_RETRY)
      ret = FULGUR_FAIL;
    return ret;
  }

  ReturnStatus get_transaction_status() {
    if (should_abort_)
      return FULGUR_TRANSACTION_ABORT;
    else
      return FULGUR_SUCCESS;
  }

  int commit() {
    // do something
    // TODO
    if (read_only_) {
      reset();
      return  FULGUR_SUCCESS;
    }

    // Read Own:
    // for read own records, we just need to free the latch
    for (auto &rloc : txn_read_own_set_) {
      RecordHeader *rloc_header = rloc.get_record_header();
      assert(rloc.get_record_header() != nullptr);
      rloc_header->set_transaction_id(INVALID_TRANSACTION_ID);
    }

    // Insert:
    // for insert records, we need to make it visiable,
    // free latch & set begin/end timestamp
    for (auto &rloc : txn_insert_set_) {
      RecordHeader *rloc_header = rloc.get_record_header();
      assert(rloc.get_record_header() != nullptr);

      rloc_header->set_begin_timestamp(transaction_id_);
      rloc_header->set_end_timestamp(MAX_TIMESTAMP);
      rloc_header->set_transaction_id(INVALID_TRANSACTION_ID);
    }

    // then reset status
    reset();
    return FULGUR_SUCCESS;
  }

  void abort() {

  }

  void set_read_only() {
    read_only_ = true;
  }


private:
  int read_unown_traverse_vchain(RecordVersionHead &version_head,
                                 RecordLocation &rec_loc) {
    RecordHeader *version_iter = version_head.ptr_->get_record_header();
    while (version_iter != nullptr) {
      version_iter->latch_.lock();
      // begin_ts_ is immutable in version chain,
      // so we can find the first version that satisfy
      // version_iter->begin_ts_ <= transaction_id_
      if (transaction_id_ < version_iter->begin_ts_) {
        version_iter->latch_.unlock();
        version_iter = version_iter->older_->get_record_header();
        continue;
      }

      // At this point, we've got the first version that
      // satisfy version_iter->begin_ts_ <= transaction_id_

      // if it's a stable old version
      if (version_iter->end_ts_ != MAX_TIMESTAMP
          && transaction_id_ <= version_iter->end_ts_) {
        version_iter->latch_.unlock();
        rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
        // no need to set last_read_ts_ for an old version
        return FULGUR_SUCCESS;
      } else {
        // if it's the latest version
        version_iter->latch_.lock();
        // Got a free latest version, we can always read it
        if (version_iter->txn_id_ == INVALID_TRANSACTION_ID) {
          update_last_read_ts_if_need(version_iter);
          version_iter->latch_.unlock();
          rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
          return FULGUR_SUCCESS;
        // A temporary latest version, but not stable
        } else if (version_iter->txn_id_ != INVALID_TRANSACTION_ID) {
          if (version_iter->txn_id_ < transaction_id_) {
            // A younger transaction is holding the version
            version_iter->latch_.unlock();
            return FULGUR_RETRY;
          } else if (transaction_id_ < version_iter->txn_id_) {
            // An older transaction is holding the version
            update_last_read_ts_if_need(version_iter);
            version_iter->latch_.unlock();
            rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
            return FULGUR_SUCCESS;
          } else if (transaction_id_ == version_iter->txn_id_) {
            //FIXME: read from read_own_set ?
            version_iter->latch_.unlock();
            rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
            return FULGUR_SUCCESS;
          }
        }
      }

      // panic: should not reach here
      assert(false);
    }

    // No valid version
    return FULGUR_FAIL;
  }

  int read_own_traverse_vchain(RecordVersionHead &version_head,
                               RecordLocation &rec_loc) {
    RecordHeader *version_iter = version_head.ptr_->get_record_header();
    version_iter->latch_.lock();
    // begin_ts_ is immutable in version chain,
    if (transaction_id_ < version_iter->begin_ts_) {
      version_iter->latch_.unlock();
      return FULGUR_FAIL;
    }

    // a deleted version
    if (version_iter->end_ts_ == 0) {
      version_iter->latch_.unlock();
      return FULGUR_FAIL;
    }
    // not the latest version anymore
    else if (version_iter->end_ts_ < transaction_id_) {
      version_iter->latch_.unlock();
      return FULGUR_RETRY;
    // still the latest version, and free
    } else if (version_iter->txn_id_ == INVALID_TRANSACTION_ID) {
      if (transaction_id_ < version_iter->last_read_ts_) {
        version_iter->latch_.unlock();
        return FULGUR_FAIL;
      } else {
        update_last_read_ts_if_need(version_iter);
        version_iter->latch_.unlock();
        rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
        return FULGUR_SUCCESS;
      }
    // latest version, but not free
    } else if (version_iter->txn_id_ != INVALID_TRANSACTION_ID) {
      if (version_iter->txn_id_ < transaction_id_) {
        version_iter->latch_.unlock();
        return FULGUR_RETRY;
      } else if (transaction_id_ < version_iter->txn_id_) {
        version_iter->latch_.unlock();
        return FULGUR_FAIL;
      } else if (transaction_id_ == version_iter->txn_id_) {
        version_iter->latch_.unlock();
        rec_loc.record_ = reinterpret_cast<Record *>(version_iter);
        return FULGUR_SUCCESS;
      }
    }
    // panic: should not reach here
    assert(false);
    return FULGUR_FAIL;
  }

  /**
   *@brief caller must hold the latch of record header
   */
  void update_last_read_ts_if_need(RecordHeader *record_header) {
    if (record_header->last_read_ts_ < transaction_id_)
      record_header->last_read_ts_ = transaction_id_;
  }

  int apply_read_own(RecordLocation &rec_loc) {
    RecordHeader *rec_header = rec_loc.get_record_header();
    // Case 1: current transaction is not the owner,
    //         we need to Retry or Fail
    if (!owned_by_me(rec_header)) {
      // Sub case 1: Now, someone has the ownership
      if (!owned_by_nobody(rec_header)){
        uint64_t record_txn_id = rec_header->get_transaction_id();
        // sub sub case 1:
        // A later transaction has owned the version to write,
        // current transaction should abort.
        if (transaction_id_ < record_txn_id) {
          return FULGUR_FAIL;
        // sub sub case 2:
        // An earlier transaction has owned the version to write,
        // current transaction still has chance to wait for the
        // earlier transaction to commit and own its created version.
        } else {
          return FULGUR_RETRY;
        }
      // Sub case 2: nobody has ownership of this version,
      //             so, we can try to own it.
      } else {
        bool ret = get_version_ownership(rec_header, transaction_id_);
        // Get the ownership successfully, add it to read_own_set
        if (ret == true) {
          add_to_read_own_set(rec_loc);
          rec_header->latch_.lock();
          rec_header->last_read_ts_ = transaction_id_;
          rec_header->latch_.unlock();
          return true;
        // Another transaction got the ownership, we have to retry
        // or abort
        } else {
          // The "Another transaction" is older, we have to abort
          if (rec_header->get_transaction_id() > transaction_id_)
            return FULGUR_FAIL;
          // The "Another transaction" is younger, we still have chance
          else
            return FULGUR_RETRY;
        }
      }
    // Case 2: current transaction is the owner, just return true;
    } else {
      return FULGUR_SUCCESS;
    }
  }

  int apply_read_not_own(RecordLocation &rec_loc) {
    RecordHeader *rec_header = rec_loc.get_record_header();
    rec_header->latch_.lock();
    if (transaction_id_ <= rec_header->end_ts_) {
      if (rec_header->last_read_ts_ < transaction_id_)
        rec_header->last_read_ts_ = transaction_id_;
      rec_header->latch_.unlock();
      return FULGUR_SUCCESS;
    } else {
      rec_header->latch_.unlock();
      return FULGUR_RETRY;
    }
  }

  /**
   *@brief
   *  if current version has not been owned and is the newest version,
   *  we can get the ownership
   */
  bool get_version_ownership(RecordHeader *header, uint64_t transaction_id) {
    header->latch_.lock();
    if (header->txn_id_ != INVALID_TRANSACTION_ID ||
        header->end_ts_ != MAX_TIMESTAMP) {
      header->latch_.unlock();
      return false;
    } else {
      header->txn_id_ = transaction_id;
      header->latch_.unlock();
      return true;
    }
  }

  bool owned_by_me (RecordHeader *header) {
    if (header->get_transaction_id() == transaction_id_)
      return true;
    else
      return false;
  }

  bool owned_by_nobody(RecordHeader *header) {
    if (header->get_transaction_id() == INVALID_TRANSACTION_ID)
      return true;
    else
      return false;
  }

  void reset() {

  }

  void add_to_insert_set(RecordLocation &rec_loc) {
    txn_insert_set_.insert(rec_loc);
  }

  void add_to_update_set(RecordLocation &rec_loc) {
    txn_update_set_.insert(rec_loc);
  }

  void add_to_delete_set(RecordLocation &rec_loc) {
    txn_delete_set_.insert(rec_loc);
  }

  void add_to_read_own_set(RecordLocation &rec_loc) {
    txn_read_own_set_.insert(rec_loc);
  }


private:
  //bool started_ = false;
  bool should_abort_ = false;
  //uint64_t last_read_timestamp_ = 0;
  //uint64_t commit_timestamp_ = 0;
  uint64_t transaction_id_ = 0;
  uint64_t epoch_id_ = 0;
  uint64_t thread_id_ = 0;
  //uint64_t timestamp_ = 0;
  //bool is_written_ = 0;
  bool read_only_ = false;

  std::unordered_set<RecordLocation> txn_insert_set_;
  std::unordered_set<RecordLocation> txn_update_set_;
  std::unordered_set<RecordLocation> txn_delete_set_;
  std::unordered_set<RecordLocation> txn_read_set_;
  std::unordered_set<RecordLocation> txn_read_own_set_;
};

}
