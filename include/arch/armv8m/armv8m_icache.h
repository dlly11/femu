/**
 * @file armv8m_icache.h
 * @brief Decoded instruction cache for ARMv8-M emulator
 *
 * Caches DecodedInsn structures by PC to avoid re-decoding in loops.
 * This provides significant speedup for tight loops common in firmware.
 */

#ifndef ARMV8M_ICACHE_H
#define ARMV8M_ICACHE_H

#include "arch/armv8m/armv8m_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define INSN_CACHE_SIZE 4096  /* Must be power of 2 */

/*============================================================================
 * Instruction Cache Structures
 *============================================================================*/

/**
 * Single entry in the instruction cache.
 */
typedef struct {
    uint32_t pc;            /**< PC of cached instruction */
    uint32_t generation;    /**< Cache generation (for invalidation) */
    DecodedInsn insn;       /**< Cached decoded instruction */
} InsnCacheEntry;

/**
 * Instruction cache context.
 */
typedef struct {
    InsnCacheEntry entries[INSN_CACHE_SIZE];
    uint32_t generation;    /**< Current generation (incremented on invalidate) */
    uint64_t hits;          /**< Cache hit counter */
    uint64_t misses;        /**< Cache miss counter */
} InsnCache;

/*============================================================================
 * Instruction Cache API
 *============================================================================*/

/**
 * Initialize instruction cache.
 *
 * @param cache     Cache to initialize
 */
void armv8m_icache_init(InsnCache *cache);

/**
 * Lookup decoded instruction in cache.
 *
 * @param cache     Instruction cache
 * @param pc        Program counter to lookup
 * @return          Pointer to cached instruction, or NULL if not found
 */
const DecodedInsn *armv8m_icache_lookup(InsnCache *cache, uint32_t pc);

/**
 * Insert decoded instruction into cache.
 *
 * @param cache     Instruction cache
 * @param pc        Program counter
 * @param insn      Decoded instruction to cache
 */
void armv8m_icache_insert(InsnCache *cache, uint32_t pc, const DecodedInsn *insn);

/**
 * Invalidate entire cache.
 * Call this on code modification or CPU reset.
 *
 * @param cache     Instruction cache
 */
void armv8m_icache_invalidate(InsnCache *cache);

/**
 * Invalidate a specific address range.
 * Call this when code is modified at specific addresses.
 *
 * @param cache     Instruction cache
 * @param start     Start address
 * @param size      Size of range in bytes
 */
void armv8m_icache_invalidate_range(InsnCache *cache, uint32_t start, uint32_t size);

/**
 * Get cache statistics.
 *
 * @param cache     Instruction cache
 * @param hits      Output: number of hits
 * @param misses    Output: number of misses
 */
void armv8m_icache_get_stats(const InsnCache *cache, uint64_t *hits, uint64_t *misses);

/**
 * Reset cache statistics.
 *
 * @param cache     Instruction cache
 */
void armv8m_icache_reset_stats(InsnCache *cache);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_ICACHE_H */
