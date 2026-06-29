#ifndef STAGE2_DEBUG_PRINT_HPP
#define STAGE2_DEBUG_PRINT_HPP

#ifdef ASCENDC_CPU_DEBUG
#include "kernel_operator.h"
#include <cstdio>

/** Stage2 结束后由 AIC block0 打印 GM 矩阵（行优先 int32） */
__aicore__ inline void kyber_print_stage2_matrix(const char *tag, GM_ADDR base, uint32_t rows, uint32_t cols)
{
    const bool isAic = AscendC::GetSubBlockNum() == 1;
    if (!isAic || AscendC::GetSubBlockIdx() != 0) {
        return;
    }
    auto *gm = (__gm__ int32_t *)base;
    printf("\n[STAGE2_DBG] %s shape=[%u,%u] int32 (row-major)\n", tag, rows, cols);
    for (uint32_t r = 0; r < rows; r++) {
        printf("row%02u:", r);
        for (uint32_t c = 0; c < cols; c++) {
            printf(" %d", static_cast<int>(gm[static_cast<size_t>(r) * cols + c]));
        }
        printf("\n");
    }
    fflush(stdout);
}
#else
// SIM/NPU：字符串字面量为 __gm__，用宏避免 const char* 签名不匹配
#define kyber_print_stage2_matrix(tag, base, rows, cols) \
    do {                   \
        (void)(tag);       \
        (void)(base);      \
        (void)(rows);      \
        (void)(cols);      \
    } while (0)
#endif

#endif
