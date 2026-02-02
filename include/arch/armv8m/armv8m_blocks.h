/**
 * @file armv8m_blocks.h
 * @brief Basic block cache for ARMv8-M emulator
 *
 * Caches sequences of instructions (basic blocks) to reduce per-instruction
 * dispatch overhead. Blocks terminate at branches, IT instructions, or
 * after reaching maximum size.
 */

#ifndef ARMV8M_BLOCKS_H
#define ARMV8M_BLOCKS_H

#include "arch/armv8m/armv8m_decoder.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define MAX_BLOCK_SIZE 32           /* Maximum instructions per block */
#define BLOCK_CACHE_SIZE 1024       /* Number of cached blocks (power of 2) */

/*============================================================================
 * Basic Block Structures
 *============================================================================*/

/**
 * Block termination reason.
 */
typedef enum {
    BLOCK_END_BRANCH,           /**< Unconditional branch (B) */
    BLOCK_END_COND_BRANCH,      /**< Conditional branch */
    BLOCK_END_CALL,             /**< Function call (BL, BLX) */
    BLOCK_END_RETURN,           /**< Return (BX LR or POP {PC}) */
    BLOCK_END_INDIRECT,         /**< Indirect branch (BX Rm) */
    BLOCK_END_IT_BLOCK,         /**< IT instruction (complex condition) */
    BLOCK_END_MAX_SIZE,         /**< Block reached max size */
    BLOCK_END_SYSTEM,           /**< System instruction (SVC, etc.) */
} BlockEndType;

/**
 * Cached basic block.
 */
typedef struct BasicBlock {
    uint32_t start_pc;          /**< PC of first instruction */
    uint32_t end_pc;            /**< PC after last instruction */
    uint16_t num_insns;         /**< Number of instructions */
    uint16_t total_size;        /**< Total size in bytes */
    BlockEndType end_type;      /**< Why the block ended */
    uint32_t generation;        /**< Cache generation */

    /* Cached decoded instructions */
    DecodedInsn insns[MAX_BLOCK_SIZE];

    /* Branch targets for linking */
    uint32_t target_taken;      /**< Branch taken target PC */
    uint32_t target_not_taken;  /**< Branch not taken target PC (next block) */

    /* Direct block links (set by linker) */
    struct BasicBlock *link_taken;      /**< Link to taken target block */
    struct BasicBlock *link_not_taken;  /**< Link to not-taken target block */

    /* Execution statistics */
    uint64_t exec_count;        /**< Times this block was executed */
} BasicBlock;

/**
 * Block cache context.
 */
typedef struct BlockCache {
    BasicBlock blocks[BLOCK_CACHE_SIZE];
    uint32_t generation;        /**< Current generation */
    uint64_t hits;              /**< Cache hits */
    uint64_t misses;            /**< Cache misses */
    uint64_t blocks_built;      /**< Total blocks built */
} BlockCache;

/*============================================================================
 * Block Cache API
 *============================================================================*/

/**
 * Initialize block cache.
 *
 * @param cache     Cache to initialize
 */
void armv8m_blocks_init(BlockCache *cache);

/**
 * Lookup or build a basic block starting at the given PC.
 *
 * @param cache     Block cache
 * @param pc        Starting PC
 * @param mem_get_ptr Function to get memory pointer
 * @param mem_ctx   Context for mem_get_ptr
 * @return          Pointer to block, or NULL on error
 */
BasicBlock *armv8m_blocks_get(BlockCache *cache, uint32_t pc,
                               const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                               void *mem_ctx);

/**
 * Invalidate entire block cache.
 *
 * @param cache     Block cache
 */
void armv8m_blocks_invalidate(BlockCache *cache);

/**
 * Invalidate blocks containing addresses in range.
 *
 * @param cache     Block cache
 * @param start     Start address
 * @param size      Size of range
 */
void armv8m_blocks_invalidate_range(BlockCache *cache, uint32_t start, uint32_t size);

/**
 * Get cache statistics.
 *
 * @param cache     Block cache
 * @param hits      Output: cache hits
 * @param misses    Output: cache misses
 */
void armv8m_blocks_get_stats(const BlockCache *cache, uint64_t *hits, uint64_t *misses);

/**
 * Reset cache statistics.
 *
 * @param cache     Block cache
 */
void armv8m_blocks_reset_stats(BlockCache *cache);

/**
 * Check if instruction type terminates a basic block.
 *
 * @param type      Instruction type
 * @return          true if this instruction ends a block
 */
bool armv8m_blocks_is_terminator(InstructionType type);

/**
 * Link a block to its successor blocks.
 * Call this after executing a block to establish links for faster chaining.
 *
 * @param cache     Block cache
 * @param block     Block to link
 * @param mem_get_ptr Function to get memory pointer
 * @param mem_ctx   Context for mem_get_ptr
 */
void armv8m_blocks_link(BlockCache *cache, BasicBlock *block,
                        const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                        void *mem_ctx);

/**
 * Get next block following a completed block execution.
 * Uses links if available, otherwise looks up/builds new block.
 *
 * @param cache     Block cache
 * @param current   Current block just executed
 * @param taken     true if branch was taken, false otherwise
 * @param pc        Current PC (used if no link available)
 * @param mem_get_ptr Function to get memory pointer
 * @param mem_ctx   Context for mem_get_ptr
 * @return          Next block to execute, or NULL
 */
BasicBlock *armv8m_blocks_get_next(BlockCache *cache, BasicBlock *current, bool taken,
                                   uint32_t pc,
                                   const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                                   void *mem_ctx);

#ifdef __cplusplus
}
#endif

#endif /* ARMV8M_BLOCKS_H */
