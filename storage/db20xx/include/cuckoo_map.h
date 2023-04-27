//===----------------------------------------------------------------------===//
//
//                         Peloton
//
// cuckoo_map.h
//
// Identification: src/include/container/cuckoo_map.h
//
// Copyright (c) 2015-16, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "./libcuckoo/cuckoohash_map.hh"
#include "./libcuckoo/default_hasher.hh"

// CUCKOO_MAP_TEMPLATE_ARGUMENTS
#define CUCKOO_MAP_TEMPLATE_ARGUMENTS                                \
  template <typename KeyType, typename ValueType, typename HashType, \
            typename PredType>

// CUCKOO_MAP_DEFAULT_ARGUMENTS
#define CUCKOO_MAP_DEFAULT_ARGUMENTS                    \
  template <typename KeyType, typename ValueType,       \
            typename HashType = DefaultHasher<KeyType>, \
            typename PredType = std::equal_to<KeyType>>

// CUCKOO_MAP_TYPE
#define CUCKOO_MAP_TYPE CuckooMap<KeyType, ValueType, HashType, PredType>

// Iterator type
#define CUCKOO_MAP_ITERATOR_TYPE \
  typename cuckoohash_map<KeyType, ValueType, HashType, PredType>::locked_table

CUCKOO_MAP_DEFAULT_ARGUMENTS
class CuckooMap {
 public:
  CuckooMap();
  CuckooMap(size_t initial_size);
  ~CuckooMap();

  // Inserts a item
  bool Insert(const KeyType &key, ValueType value);

  // Inserts the item if not present, updates value otherwise
  // Upsert operations always succeed
  void Upsert(const KeyType &key, ValueType value);

  // Extracts item with high priority
  bool Update(const KeyType &key, ValueType value);

  // Extracts the corresponding value
  bool Find(const KeyType &key, ValueType &value) const;

  // Delete key from the cuckoo_map
  bool Erase(const KeyType &key);

  // Checks whether the cuckoo_map contains key
  bool Contains(const KeyType &key);

  // Clears the tree (thread safe, not atomic)
  void Clear();

  // Returns item count in the cuckoo_map
  size_t GetSize() const;

  // Checks if the cuckoo_map is empty
  bool IsEmpty() const;

  // Lock the table and get iterator
  // The table would be unlock when the iterator
  // is out of scope
  CUCKOO_MAP_ITERATOR_TYPE
  GetIterator();

  CUCKOO_MAP_ITERATOR_TYPE
  GetConstIterator() const;

 private:
  // cuckoo map
  typedef cuckoohash_map<KeyType, ValueType, HashType, PredType> cuckoo_map_t;

  cuckoo_map_t cuckoo_map;
};

CUCKOO_MAP_TEMPLATE_ARGUMENTS
CUCKOO_MAP_TYPE::CuckooMap() {}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
CUCKOO_MAP_TYPE::CuckooMap(size_t initial_size) : cuckoo_map(initial_size) {}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
CUCKOO_MAP_TYPE::~CuckooMap() {}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
bool CUCKOO_MAP_TYPE::Insert(const KeyType &key, ValueType value) {
  auto status = cuckoo_map.insert(key, value);
  //  LOG_TRACE("insert status : %d", status);
  return status;
}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
void CUCKOO_MAP_TYPE::Upsert(const KeyType &key, ValueType value) {
  auto update_fn = []([[maybe_unused]]ValueType &value) { return; };
  cuckoo_map.upsert(key, update_fn, value);
}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
bool CUCKOO_MAP_TYPE::Update(const KeyType &key, ValueType value) {
  auto status = cuckoo_map.update(key, value);
  //  LOG_TRACE("update status : %d", status);
  return status;
}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
bool CUCKOO_MAP_TYPE::Erase(const KeyType &key) {
  auto status = cuckoo_map.erase(key);
  //  LOG_TRACE("erase status : %d", status);
  return status;
}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
bool CUCKOO_MAP_TYPE::Find(const KeyType &key, ValueType &value) const {
  auto status = cuckoo_map.find(key, value);
  //  LOG_TRACE("find status : %d", status);
  return status;
}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
bool CUCKOO_MAP_TYPE::Contains(const KeyType &key) {
  return cuckoo_map.contains(key);
}

CUCKOO_MAP_TEMPLATE_ARGUMENTS
void CUCKOO_MAP_TYPE::Clear() { cuckoo_map.clear(); }

CUCKOO_MAP_TEMPLATE_ARGUMENTS
size_t CUCKOO_MAP_TYPE::GetSize() const { return cuckoo_map.size(); }

CUCKOO_MAP_TEMPLATE_ARGUMENTS
bool CUCKOO_MAP_TYPE::IsEmpty() const { return cuckoo_map.empty(); }

CUCKOO_MAP_TEMPLATE_ARGUMENTS
CUCKOO_MAP_ITERATOR_TYPE
CUCKOO_MAP_TYPE::GetIterator() { return cuckoo_map.lock_table(); }

CUCKOO_MAP_TEMPLATE_ARGUMENTS
CUCKOO_MAP_ITERATOR_TYPE
CUCKOO_MAP_TYPE::GetConstIterator() const {
  // WARNING: This is a compiler hack and should never be used elsewhere
  // If you are considering using this, please ask Marcel first
  // We need the const iterator on the const object and the cuckoohash
  // library returns a lock_table object. The other option would be to
  // Modify the cuckoohash library which is not neat.
  auto locked_table = const_cast<CuckooMap *>(this)->cuckoo_map.lock_table();
  return locked_table;
}

//#endif
