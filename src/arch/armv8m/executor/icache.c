/**
 * @file icache.c
 * @brief Decoded instruction cache implementation
 *
 * Implements a direct-mapped instruction cache keyed by PC.
 * Provides O(1) lookup to avoid re-decoding instructions in loops.
 */

#include "arch/armv8m/armv8m_icache.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Compute cache index from PC.
 * Uses bits [12:1] since Thumb instructions are half-word aligned.
 */
static inline uint32_t cache_index(uint32_t pc)
{
    return (pc >> 1) & (INSN_CACHE_SIZE - 1);
}

/*============================================================================
 * Public API
 *============================================================================*/

void armv8m_icache_init(InsnCache *cache)
{
    memset(cache, 0, sizeof(InsnCache));
    cache->generation = 1;  /* Start at 1 so 0 is always invalid */
}

const DecodedInsn *armv8m_icache_lookup(InsnCache *cache, uint32_t pc)
{
    uint32_t idx = cache_index(pc);
    InsnCacheEntry *entry = &cache->entries[idx];

    /* Check if entry is valid and matches PC */
    if (entry->generation == cache->generation && entry->pc == pc) {
        cache->hits++;
        return &entry->insn;
    }

    cache->misses++;
    return NULL;
}

void armv8m_icache_insert(InsnCache *cache, uint32_t pc, const DecodedInsn *insn)
{
    uint32_t idx = cache_index(pc);
    InsnCacheEntry *entry = &cache->entries[idx];

    entry->pc = pc;
    entry->generation = cache->generation;
    entry->insn = *insn;
}

void armv8m_icache_invalidate(InsnCache *cache)
{
    /* Simply incrementing generation invalidates all entries */
    cache->generation++;

    /* Handle wrap-around (0 is reserved as invalid) */
    if (cache->generation == 0) {
        cache->generation = 1;
    }
}

void armv8m_icache_invalidate_range(InsnCache *cache, uint32_t start, uint32_t size)
{
    /* For small ranges, invalidate individual entries */
    if (size <= 64) {
        /* Invalidate entries for each possible instruction address */
        for (uint32_t addr = start; addr < start + size; addr += 2) {
            uint32_t idx = cache_index(addr);
            InsnCacheEntry *entry = &cache->entries[idx];

            if (entry->pc >= start && entry->pc < start + size) {
                entry->generation = 0;  /* Mark as invalid */
            }
        }
    } else {
        /* For large ranges, full invalidation is more efficient */
        armv8m_icache_invalidate(cache);
    }
}

void armv8m_icache_get_stats(const InsnCache *cache, uint64_t *hits, uint64_t *misses)
{
    if (hits) {
        *hits = cache->hits;
    }
    if (misses) {
        *misses = cache->misses;
    }
}

void armv8m_icache_reset_stats(InsnCache *cache)
{
    cache->hits = 0;
    cache->misses = 0;
}
