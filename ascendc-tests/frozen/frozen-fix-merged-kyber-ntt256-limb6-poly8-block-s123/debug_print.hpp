#ifndef DEBUG_PRINT_HPP
#define DEBUG_PRINT_HPP

#ifdef ASCENDC_CPU_DEBUG
#include "kernel_operator.h"
#include <cstdio>

__aicore__ inline void kyber_print_s0_int8(const char *tag, GM_ADDR base, uint32_t rows, uint32_t cols)
{
    if (AscendC::GetSubBlockNum() == 1 || AscendC::GetSubBlockIdx() != 0) {
        return;
    }
    auto *gm = (__gm__ int8_t *)base;
    printf("\n[STAGE1_DBG] %s shape=[%u,%u] int8 (row-major, block [HI|LO])\n", tag, rows, cols);
    for (uint32_t r = 0; r < rows; r++) {
        const char *blk = (r < rows / 2) ? "HI" : "LO";
        printf("row%02u(%s):", r, blk);
        for (uint32_t c = 0; c < cols && c < 16U; c++) {
            printf(" %d", static_cast<int>(gm[static_cast<size_t>(r) * cols + c]));
        }
        if (cols > 16U) {
            printf(" ...");
        }
        printf("\n");
    }
    fflush(stdout);
}

__aicore__ inline void kyber_print_stage2_matrix(const char *tag, GM_ADDR base, uint32_t rows, uint32_t cols)
{
    if (AscendC::GetSubBlockNum() != 1 || AscendC::GetSubBlockIdx() != 0) {
        return;
    }
    auto *gm = (__gm__ int32_t *)base;
    printf("\n[STAGE2_DBG] %s shape=[%u,%u] int32 (row-major, block [HI|LO])\n", tag, rows, cols);
    for (uint32_t r = 0; r < rows; r++) {
        const char *blk = (r < rows / 2) ? "HI" : "LO";
        printf("row%02u(%s):", r, blk);
        for (uint32_t c = 0; c < cols && c < 8U; c++) {
            printf(" %d", static_cast<int>(gm[static_cast<size_t>(r) * cols + c]));
        }
        if (cols > 8U) {
            printf(" ...");
        }
        printf("\n");
    }
    fflush(stdout);
}
#else
#define kyber_print_s0_int8(tag, base, rows, cols) \
    do {                                           \
        (void)(tag);                               \
        (void)(base);                              \
        (void)(rows);                              \
        (void)(cols);                              \
    } while (0)
#define kyber_print_stage2_matrix(tag, base, rows, cols) \
    do {                                                 \
        (void)(tag);                                     \
        (void)(base);                                    \
        (void)(rows);                                    \
        (void)(cols);                                    \
    } while (0)
#endif

#endif
