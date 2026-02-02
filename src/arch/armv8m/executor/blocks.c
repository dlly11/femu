/**
 * @file blocks.c
 * @brief Basic block cache implementation
 *
 * Builds and caches basic blocks - sequences of instructions that execute
 * without branches. This reduces per-instruction dispatch overhead.
 */

#include "arch/armv8m/armv8m_blocks.h"
#include "arch/armv8m/armv8m_decoder.h"
#include <string.h>

/*============================================================================
 * Internal Helpers
 *============================================================================*/

/**
 * Compute cache index from PC.
 */
static inline uint32_t block_index(uint32_t pc)
{
    /* Use bits from PC, accounting for Thumb alignment */
    return (pc >> 1) & (BLOCK_CACHE_SIZE - 1);
}

/**
 * Check if instruction type is a branch (terminates block).
 */
bool armv8m_blocks_is_terminator(InstructionType type)
{
    switch (type) {
        case INSN_BRANCH:
        case INSN_BRANCH_LINK:
        case INSN_BRANCH_EXCHANGE:
        case INSN_BRANCH_LINK_EXCHANGE:
        case INSN_COMPARE_BRANCH:
        case INSN_TABLE_BRANCH:
        case INSN_IT:
        case INSN_SVC:
        case INSN_SG:
        case INSN_BXNS:
        case INSN_BLXNS:
            return true;
        default:
            return false;
    }
}

/**
 * Determine block end type from instruction.
 */
static BlockEndType get_end_type(const DecodedInsn *insn)
{
    switch (insn->type) {
        case INSN_BRANCH:
            return (insn->cond != COND_AL) ? BLOCK_END_COND_BRANCH : BLOCK_END_BRANCH;

        case INSN_BRANCH_LINK:
        case INSN_BRANCH_LINK_EXCHANGE:
        case INSN_BLXNS:
            return BLOCK_END_CALL;

        case INSN_BRANCH_EXCHANGE:
        case INSN_BXNS:
            /* Check if it's a return (BX LR) */
            if (insn->rm == 14) {  /* LR */
                return BLOCK_END_RETURN;
            }
            return BLOCK_END_INDIRECT;

        case INSN_TABLE_BRANCH:
            return BLOCK_END_INDIRECT;

        case INSN_COMPARE_BRANCH:
            return BLOCK_END_COND_BRANCH;

        case INSN_IT:
            return BLOCK_END_IT_BLOCK;

        case INSN_SVC:
        case INSN_SG:
            return BLOCK_END_SYSTEM;

        default:
            return BLOCK_END_MAX_SIZE;
    }
}

/**
 * Build a new basic block starting at the given PC.
 */
static bool build_block(BasicBlock *block, uint32_t pc,
                        const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                        void *mem_ctx)
{
    block->start_pc = pc;
    block->num_insns = 0;
    block->total_size = 0;
    block->exec_count = 0;
    block->link_taken = NULL;
    block->link_not_taken = NULL;

    uint32_t current_pc = pc;

    while (block->num_insns < MAX_BLOCK_SIZE) {
        /* Fetch instruction bytes */
        const uint8_t *mem = mem_get_ptr(mem_ctx, current_pc, 4);
        if (!mem) {
            /* Can't fetch - block ends here */
            if (block->num_insns == 0) {
                return false;  /* Can't build empty block */
            }
            break;
        }

        /* Decode instruction */
        DecodedInsn *insn = &block->insns[block->num_insns];
        armv8m_decode_init(insn);
        int size = armv8m_decode(mem, current_pc, insn);

        if (size < 0) {
            /* Decode error - block ends here */
            if (block->num_insns == 0) {
                return false;  /* Can't build empty block */
            }
            break;
        }

        block->num_insns++;
        block->total_size += (uint16_t)size;
        current_pc += (uint32_t)size;

        /* Check if this instruction terminates the block */
        if (armv8m_blocks_is_terminator(insn->type)) {
            block->end_type = get_end_type(insn);

            /* Calculate branch targets */
            switch (insn->type) {
                case INSN_BRANCH:
                case INSN_BRANCH_LINK:
                    block->target_taken = (uint32_t)((int32_t)insn->pc + 4 + insn->branch_offset);
                    block->target_not_taken = current_pc;
                    break;

                case INSN_COMPARE_BRANCH:
                    block->target_taken = (uint32_t)((int32_t)insn->pc + 4 + insn->branch_offset);
                    block->target_not_taken = current_pc;
                    break;

                default:
                    block->target_taken = 0;
                    block->target_not_taken = current_pc;
                    break;
            }

            break;
        }

        /* Check for load/store multiple that modifies PC (implicit return) */
        if (insn->type == INSN_LOAD_MULTIPLE && (insn->register_list & (1 << 15))) {
            block->end_type = BLOCK_END_RETURN;
            block->target_taken = 0;  /* Unknown */
            block->target_not_taken = current_pc;
            break;
        }
    }

    /* Block reached max size without terminator */
    if (block->num_insns >= MAX_BLOCK_SIZE) {
        block->end_type = BLOCK_END_MAX_SIZE;
        block->target_taken = 0;
        block->target_not_taken = current_pc;
    }

    block->end_pc = current_pc;
    return block->num_insns > 0;
}

/*============================================================================
 * Public API
 *============================================================================*/

void armv8m_blocks_init(BlockCache *cache)
{
    memset(cache, 0, sizeof(BlockCache));
    cache->generation = 1;
}

BasicBlock *armv8m_blocks_get(BlockCache *cache, uint32_t pc,
                               const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                               void *mem_ctx)
{
    uint32_t idx = block_index(pc);
    BasicBlock *block = &cache->blocks[idx];

    /* Check if we have a valid cached block */
    if (block->generation == cache->generation && block->start_pc == pc) {
        cache->hits++;
        return block;
    }

    /* Cache miss - build new block */
    cache->misses++;

    if (!build_block(block, pc, mem_get_ptr, mem_ctx)) {
        return NULL;
    }

    block->generation = cache->generation;
    cache->blocks_built++;

    return block;
}

void armv8m_blocks_invalidate(BlockCache *cache)
{
    cache->generation++;
    if (cache->generation == 0) {
        cache->generation = 1;
    }

    /* Clear all block links (they may point to invalidated blocks) */
    for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
        cache->blocks[i].link_taken = NULL;
        cache->blocks[i].link_not_taken = NULL;
    }
}

void armv8m_blocks_invalidate_range(BlockCache *cache, uint32_t start, uint32_t size)
{
    uint32_t end = start + size;

    /* For small ranges, check individual blocks */
    if (size <= 256) {
        for (int i = 0; i < BLOCK_CACHE_SIZE; i++) {
            BasicBlock *block = &cache->blocks[i];

            if (block->generation == cache->generation) {
                /* Check if block overlaps with invalidation range */
                if (block->start_pc < end && block->end_pc > start) {
                    block->generation = 0;
                    block->link_taken = NULL;
                    block->link_not_taken = NULL;
                }

                /* Also clear links if they point into the range */
                if (block->link_taken) {
                    uint32_t target = block->target_taken;
                    if (target >= start && target < end) {
                        block->link_taken = NULL;
                    }
                }
                if (block->link_not_taken) {
                    uint32_t target = block->target_not_taken;
                    if (target >= start && target < end) {
                        block->link_not_taken = NULL;
                    }
                }
            }
        }
    } else {
        /* Large range - full invalidation is more efficient */
        armv8m_blocks_invalidate(cache);
    }
}

void armv8m_blocks_get_stats(const BlockCache *cache, uint64_t *hits, uint64_t *misses)
{
    if (hits) {
        *hits = cache->hits;
    }
    if (misses) {
        *misses = cache->misses;
    }
}

void armv8m_blocks_reset_stats(BlockCache *cache)
{
    cache->hits = 0;
    cache->misses = 0;
}

/*============================================================================
 * Block Linking
 *============================================================================*/

void armv8m_blocks_link(BlockCache *cache, BasicBlock *block,
                        const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                        void *mem_ctx)
{
    /* Don't link indirect branches or system calls - targets are unknown */
    if (block->end_type == BLOCK_END_INDIRECT ||
        block->end_type == BLOCK_END_RETURN ||
        block->end_type == BLOCK_END_SYSTEM) {
        return;
    }

    /* Try to link the taken path */
    if (block->target_taken != 0 && block->link_taken == NULL) {
        BasicBlock *target = armv8m_blocks_get(cache, block->target_taken, mem_get_ptr, mem_ctx);
        if (target && target->generation == cache->generation) {
            block->link_taken = target;
        }
    }

    /* Try to link the not-taken path (fall-through) */
    if (block->target_not_taken != 0 && block->link_not_taken == NULL) {
        BasicBlock *target = armv8m_blocks_get(cache, block->target_not_taken, mem_get_ptr, mem_ctx);
        if (target && target->generation == cache->generation) {
            block->link_not_taken = target;
        }
    }
}

BasicBlock *armv8m_blocks_get_next(BlockCache *cache, BasicBlock *current, bool taken,
                                   uint32_t pc,
                                   const uint8_t *(*mem_get_ptr)(void *ctx, uint32_t addr, uint32_t size),
                                   void *mem_ctx)
{
    BasicBlock *next = NULL;

    /* Try to use existing links */
    if (taken && current->link_taken) {
        next = current->link_taken;
        /* Verify link is still valid */
        if (next->generation == cache->generation && next->start_pc == pc) {
            cache->hits++;
            return next;
        }
        /* Link is stale, clear it */
        current->link_taken = NULL;
    } else if (!taken && current->link_not_taken) {
        next = current->link_not_taken;
        /* Verify link is still valid */
        if (next->generation == cache->generation && next->start_pc == pc) {
            cache->hits++;
            return next;
        }
        /* Link is stale, clear it */
        current->link_not_taken = NULL;
    }

    /* No valid link - lookup or build block */
    next = armv8m_blocks_get(cache, pc, mem_get_ptr, mem_ctx);

    /* Establish link for next time */
    if (next && next->generation == cache->generation) {
        if (taken) {
            current->link_taken = next;
        } else {
            current->link_not_taken = next;
        }
    }

    return next;
}
