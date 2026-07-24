/**
 * @file f203_alg7_rej_scalar.c
 * @brief Alg.7 rej 标量实现（Host 可链接、Python ctypes 对照、设备语义金标准）。
 *
 * 流水线位置：不参与设备热路径；F203_ALG7_REJ_IMPL=0 时设备用 f203_alg7_rej_scalar.hpp 同逻辑。
 *
 * 与 golden 关系：本文件为 rej 段最简 oracle；向量路径产出须与本函数 bit-exact（对同一 d1/d2）。
 */
#include "f203_alg7_rej_scalar.h"

/**
 * 标量 rej 主循环：对每个候选对 (d1[i], d2[i]) 依次尝试填入 â，满 n_out 早停。
 * 拒绝条件：v >= q（不写入、不消耗输出槽位）。
 */
uint32_t f203_alg7_rej_scalar_from_d12(const int32_t *d1, const int32_t *d2, uint32_t npairs, int32_t q,
                                       int32_t *a_hat, uint32_t n_out)
{
    uint32_t j = 0U;  // â 已填系数计数
    for (uint32_t i = 0U; i < npairs && j < n_out; ++i) {
        // 先尝试 d1[i]（Alg.7 规范顺序：同一三元组内 d1 优先）
        const int32_t v1 = d1[i];
        if (v1 < q) {
            a_hat[j] = v1;
            ++j;
            if (j >= n_out) {
                break;  // 已满 256，不再读 d2[i]
            }
        }
        // 再尝试 d2[i]（仅当 â 未满）
        const int32_t v2 = d2[i];
        if (v2 < q && j < n_out) {
            a_hat[j] = v2;
            ++j;
        }
    }
    return j;
}
