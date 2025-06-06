#ifndef __HASH_H
#define __HASH_H

#include "types.h"
#include "list.h"

struct hash_entry {
    struct list_head list;
};

struct hash_table {
    struct spinlock lock; // lock
    uint64 size; // table size
    struct hash_entry *hash_head; // hash entry
    struct hash_table_operation *op; // hash table operations
};

// Hash table operations
//
// Every hash table should implement them.
struct hash_table_operation {
    // lookup a hash node
    void *(*hash_lookup)(struct hash_table *table, void *key);

    // insert a hash node
    int (*hash_insert)(struct hash_table *table, void *node);

    // delete a hash node
    void (*hash_delete)(struct hash_table *table, void *node);

    // hash up a key
    uint64 (*hash_key)(void *key);
};


// ----------generic hash table operations---------

/**
 * @brief get a hash entry from the hash table by the key.
 * 
 * @param table hash table
 * @param key key to lookup
 * @attention call with holding the lock of hash table
 * @return pointer to the hash entry. return with holding the lock of hash table
 */
static inline struct hash_entry *hash_get_entry(struct hash_table *table, void *key) {
    uint64 hash = table->op->hash_key(key);
    struct hash_entry *entry = &table->hash_head[hash % table->size];
    return entry;
}

/**
 * @brief initialize a hash table.
 * 
 * @param table hash table to initialize
 */
static inline void hash_table_entry_init(struct hash_table *table) {
    table->hash_head = (struct hash_entry *) kmalloc(sizeof(struct hash_entry) * table->size);
    for (uint64 i = 0; i < table->size; i++) {
        INIT_LIST_HEAD(&table->hash_head[i].list);
    }
}




// -----------utilities for hashing----------

/* Name hashing routines. Initial hash value */
/* Hash courtesy of the R5 hash in reiserfs modulo sign bits */
#define init_name_hash() 0

/* partial hash update function. Assume roughly 4 bits per character */
static inline uint64 partial_name_hash(unsigned long c, unsigned long prevhash) {
    return (prevhash + (c << 4) + (c >> 4)) * 11;
}

/*
 * Finally: cut down the number of bits to a int value (and try to avoid
 * losing bits)
 */
static inline uint64 end_name_hash(unsigned long hash) { return (unsigned int) hash; }

/* Compute the hash for a name string. */
static inline uint64 full_name_hash(const unsigned char *name, unsigned int len) {
    unsigned long hash = init_name_hash();
    while (len--)
        hash = partial_name_hash(*name++, hash);
    return end_name_hash(hash);
}

#endif