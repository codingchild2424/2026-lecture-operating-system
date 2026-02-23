/*
 * hashtable_solution.c - Parallel Hash Table (Solution)
 *
 * Implements both coarse-grained and fine-grained locking strategies.
 *
 * Coarse-grained: A single global mutex protects the entire table.
 *   - Simple to implement, but limits parallelism.
 *   - All threads contend on the same lock.
 *
 * Fine-grained: Each bucket has its own mutex.
 *   - Threads operating on different buckets can proceed in parallel.
 *   - Better throughput, especially with many buckets and threads.
 */

#include <stdio.h>
#include <stdlib.h>
#include "hashtable.h"

/*
 * Hash function: maps a key to a bucket index.
 */
static int hash(int key)
{
    return key % NBUCKETS;
}

/*
 * Helper: acquire the appropriate lock before accessing a bucket.
 */
static void lock_acquire(struct hashtable *ht, int bucket_idx)
{
    switch (ht->strategy) {
    case LOCK_COARSE:
        pthread_mutex_lock(&ht->global_lock);
        break;
    case LOCK_FINE:
        pthread_mutex_lock(&ht->buckets[bucket_idx].lock);
        break;
    case LOCK_NONE:
        break;
    }
}

/*
 * Helper: release the appropriate lock after accessing a bucket.
 */
static void lock_release(struct hashtable *ht, int bucket_idx)
{
    switch (ht->strategy) {
    case LOCK_COARSE:
        pthread_mutex_unlock(&ht->global_lock);
        break;
    case LOCK_FINE:
        pthread_mutex_unlock(&ht->buckets[bucket_idx].lock);
        break;
    case LOCK_NONE:
        break;
    }
}

/*
 * Initialize the hash table.
 */
void hashtable_init(struct hashtable *ht, lock_strategy_t strategy)
{
    ht->strategy = strategy;

    for (int i = 0; i < NBUCKETS; i++) {
        ht->buckets[i].head = NULL;
        pthread_mutex_init(&ht->buckets[i].lock, NULL);
    }

    pthread_mutex_init(&ht->global_lock, NULL);
}

/*
 * Destroy the hash table.
 */
void hashtable_destroy(struct hashtable *ht)
{
    for (int i = 0; i < NBUCKETS; i++) {
        struct entry *e = ht->buckets[i].head;
        while (e != NULL) {
            struct entry *next = e->next;
            free(e);
            e = next;
        }
        ht->buckets[i].head = NULL;
        pthread_mutex_destroy(&ht->buckets[i].lock);
    }

    pthread_mutex_destroy(&ht->global_lock);
}

/*
 * Insert a key-value pair.
 */
void hashtable_insert(struct hashtable *ht, int key, int value)
{
    int idx = hash(key);

    lock_acquire(ht, idx);

    /* Check if key already exists */
    struct entry *e = ht->buckets[idx].head;
    while (e != NULL) {
        if (e->key == key) {
            e->value = value;
            lock_release(ht, idx);
            return;
        }
        e = e->next;
    }

    /* Key not found: insert at head */
    struct entry *new_entry = malloc(sizeof(struct entry));
    if (new_entry == NULL) {
        fprintf(stderr, "malloc failed\n");
        lock_release(ht, idx);
        return;
    }
    new_entry->key = key;
    new_entry->value = value;
    new_entry->next = ht->buckets[idx].head;
    ht->buckets[idx].head = new_entry;

    lock_release(ht, idx);
}

/*
 * Look up a key.
 */
int hashtable_lookup(struct hashtable *ht, int key)
{
    int idx = hash(key);

    lock_acquire(ht, idx);

    struct entry *e = ht->buckets[idx].head;
    while (e != NULL) {
        if (e->key == key) {
            int val = e->value;
            lock_release(ht, idx);
            return val;
        }
        e = e->next;
    }

    lock_release(ht, idx);
    return -1;
}

/*
 * Delete a key.
 */
int hashtable_delete(struct hashtable *ht, int key)
{
    int idx = hash(key);

    lock_acquire(ht, idx);

    struct entry **pp = &ht->buckets[idx].head;
    while (*pp != NULL) {
        if ((*pp)->key == key) {
            struct entry *victim = *pp;
            *pp = victim->next;
            free(victim);
            lock_release(ht, idx);
            return 1;
        }
        pp = &(*pp)->next;
    }

    lock_release(ht, idx);
    return 0;
}
