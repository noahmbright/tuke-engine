#pragma once

#include "tuke_engine.h"
#include <assert.h>
#include <string.h>

#define DEFAULT_TABLE_SIZE 64

struct CStringHashFunctor {
  inline u32 operator()(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
      hash ^= (uint8_t)(*str++);
      hash *= 16777619u;
    }
    return hash;
  }
};

struct CStringEqualityFunctor {
  inline bool operator()(const char *lhs, const char *rhs) {
    return (strcmp(lhs, rhs) == 0);
  }
};

enum HashMapOccupancy { HASHMAP_EMPTY, HASHMAP_OCCUPIED, HASHMAP_TOMBSTONE };

template <typename K, typename V> struct KeyValuePair {
  K key;
  V value;
  HashMapOccupancy occupancy;

  KeyValuePair() : key(), value(), occupancy(HASHMAP_EMPTY) {}
};

// implementation of an open addressing hash map
template <typename K, typename V, typename HashFunctor,
          typename EqualityFunctor>
struct HashMap {
  KeyValuePair<K, V> key_values[DEFAULT_TABLE_SIZE];
  u32 size;
  u32 capacity;

  HashMap() : key_values(), size(0), capacity(DEFAULT_TABLE_SIZE) {}

  u32 probe(const K &k) const {
    HashFunctor functor;
    const u32 hash = functor(k);

    // mask - if capacity is a power of 2, then capacity has a single bit set
    // mask = cap - 1 is that bit and all more significant ones set to 0, all
    // less significant set to 1
    // bitwise and with mask will then shrink table index into a range less than
    // cap
    const u32 mask = capacity - 1;
    assert((capacity & mask) == 0);
    for (u32 i = 0; i < capacity; i++) {
      const u32 table_index = (hash + i) & mask;
      const HashMapOccupancy occupancy = key_values[table_index].occupancy;

      if (occupancy == HASHMAP_OCCUPIED) {
        // TODO equality functor as template parameter?
        EqualityFunctor equality;
        if (equality(key_values[table_index].key, k)) {
          return table_index;
        }
      }

      if (occupancy == HASHMAP_EMPTY || occupancy == HASHMAP_TOMBSTONE) {
        return table_index;
      }
    }

    // key not found, nor an empty entry
    return capacity;
  }

  const V *find(const K &k) const {
    const u32 table_index = probe(k);
    if (table_index == capacity ||
        key_values[table_index].occupancy != HASHMAP_OCCUPIED) {
      return NULL;
    }

    return &key_values[table_index].value;
  }

  bool insert(const K &k, const V &v) {
    if (size == capacity) {
      return false;
    }

    // key_values[table_index].value = v;
    const u32 table_index = probe(k);
    if (table_index == capacity) {
      // no empty entry found
      return false;
    }

    key_values[table_index].key = k;
    key_values[table_index].value = v;
    key_values[table_index].occupancy = HASHMAP_OCCUPIED;
    size++;

    return true;
  }
};
