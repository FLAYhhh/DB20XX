#pragma once
#include "./utils.h"
#include "./masstree-beta/kvthread.hh"
#include "./masstree-beta/masstree.hh"
#include "./masstree-beta/masstree_tcursor.hh"
#include "./record_location.h"

namespace fulgurdb {

// FIXME: 当前设置的限制没有什么依据
constexpr uint32_t FULGUR_MAX_KEYS = 255;
constexpr uint32_t FULGUR_MAX_KEY_PARTS = 255;
constexpr uint32_t FULGUR_MAX_KEY_LENGTH = 255;
using namespace Masstree;

struct Key {
  Key(char *s, uint32_t l, bool own = true):
    data(s), len(l), own_mem(own) {}
  ~Key() {
    if (own_mem) free(data);
  }

  char *data;
  uint32_t len;
  bool own_mem = true;
};

struct KeyInfo {
  /**
    mysql keypart counted from 1,
    fulgurdb keypart counted from 0;
  */
  void add_key_part(uint32_t key_part) {
    key_parts.push_back(key_part - 1);
  }

  Schema schema;
  std::vector<int> key_parts;
  uint32_t key_len = 0;
};

class Scanner {

};

class Index {
public:
  Index(void) {}
  Index(const KeyInfo &keyinfo):keyinfo_(keyinfo) {}
  ~Index() {}

  virtual bool get(const Key &key, RecordLocation &tloc, threadinfo &ti) const = 0;

  //virtual int scan(const Key &key, bool matchfirst, Scanner& scanner, threadinfo &ti) const;

  //virtual int rscan(const Key &key, bool matchfirst, Scanner& scanner, threadinfo &ti) const;

  virtual bool put(const Key &key, const RecordLocation &tloc, threadinfo &ti) = 0;


  /**
  @brief
    build key from a fulgurdb record
  */
  std::unique_ptr<Key> build_key(const char *record) {
    char *key_data = (char *)malloc(sizeof(keyinfo_.key_len));
    char *key_cursor = key_data;
    for (auto i : keyinfo_.key_parts) {
      const Field &field = keyinfo_.schema.get_field(i);
      const char *field_data = nullptr;
      uint32_t data_len = 0;

      field.get_field_data(record, field_data, data_len);
      memcpy(key_cursor, field_data, data_len);
      key_cursor += data_len;
    }

    return std::unique_ptr<Key>(new Key(key_data, keyinfo_.key_len));
  }

private:
  KeyInfo keyinfo_;
};



class MasstreeIndex: public Index {
  struct fulgurdb_masstree_params : public nodeparams<15, 15> {
    typedef RecordLocation* value_type;
    typedef value_print<value_type> value_print_type;
    typedef threadinfo threadinfo_type;
  };

  typedef basic_table<fulgurdb_masstree_params> fulgur_masstree_type;
  typedef RecordLocation* leafvalue_type;
public:
  MasstreeIndex(void) {}
  MasstreeIndex(const KeyInfo &keyinfo):Index(keyinfo) {}
  ~MasstreeIndex() {}

  void initialize(threadinfo& ti) {
    masstree_.initialize(ti);
  }

  void destroy(threadinfo &ti) {
    masstree_.destroy(ti);
  }

  /**
  @brief
    put a key-value pair to masstree. key consists of columns, values is
    corresponding RecordLocation of that Record.

  @return values
    @retval1 true: first put
    @retval2 false: not first put, update old value
  */
  bool put(const Key &key, const RecordLocation &rloc, threadinfo &ti) override {
    typename fulgur_masstree_type::cursor_type lp(masstree_, Str(key.data, key.len));
    bool found = lp.find_insert(ti);
    if (!found) {
      ti.observe_phantoms(lp.node());
    }

    apply_put(lp.value(), rloc, ti);
    lp.finish(1, ti);
    return found;
  }

  /**
    @brief
      given key, get the value(RecordLocation of a fulgurdb row) of the key
    @return values
      @retval1 true: key exists
      @retval2 false: key doesnot exist
    FIXME: same problem with apply_put
  */
  bool get(const Key &key, RecordLocation &tloc, threadinfo &ti) const override {
    typename fulgur_masstree_type::unlocked_cursor_type lp(masstree_, Str(key.data, key.len));
    bool found = lp.find_unlocked(ti);
    if (found)
      tloc = *lp.value();

    return found;
  }

  // TODO
  //int scan(const Key &key, bool matchfirst, Scanner& scanner, threadinfo &ti) const override {}

  // TODO
  //int rscan(const Key &key, bool matchfirst, Scanner& scanner, threadinfo &ti) const override {}

private:
  /**
  FIXME: masstree should manage leafvalue carefully to avoid concurrent problems.
  */
  void apply_put(leafvalue_type &value,
                 const RecordLocation &rloc, threadinfo &ti) {
    (void) ti; //FIXME thread local内存池分配value的内存
    value = new RecordLocation(rloc);
  }

private:
  fulgur_masstree_type masstree_;
};

}
