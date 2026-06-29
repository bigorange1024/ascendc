// @probe pass-fix-f203-alg13-device-keygen-k4
// @file prep/alg7/f203_alg7_rej_scalar.h
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_rej_scalar.h` 为该子模块组件。 / Component: f203_alg7_rej_scalar.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 段 0×AIC + 2×AIV block（block0 负载更重）；CPU SUCCESS 日志中 AIC_* 为 tikicpu 仿真伪影，非 prep 真拓扑。
// @depends #include: stdint.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。

/**
 * @file f203_alg7_rej_scalar.h
 * @brief FIPS 203 Alg.7 第 8–15 行：纯 C 标量 rejection sampling 接口（Host / golden 对照）。
 *
 * 数学契约：对平面 d1[npairs]、d2[npairs] 按 i=0..npairs-1 顺序检查；
 * 若 d[i]<q 则写入 â 下一空位；先 d1[i] 后 d2[i]；满 N=256 即停。
 * 与规范「边扫候选边填系数」等价；本探针固定 672B XOF → npairs=224。
 *
 * 流水线位置：Host 测试脚本、CPU golden；设备侧语义相同实现见 f203_alg7_rej_scalar.hpp / .c。
 *
 * 与 golden 关系：scripts/test_rej_scalar_c.py、gen_data.py 标量参考须与本函数逐系数一致。
 */
#ifndef F203_ALG7_REJ_SCALAR_H
#define F203_ALG7_REJ_SCALAR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** ML-KEM 多项式长度与模数（与 f203_alg7_layout.h 一致）。 */
#define F203_ALG7_KYBER_N 256U
#define F203_ALG7_KYBER_Q 3329

/**
 * 从平面 d1、d2 顺序执行 rej，写入 a_hat[0..n_out-1]。
 *
 * @param d1     候选系数 d1[npairs]，int32，已由 Alg.7 line 7 算出
 * @param d2     候选系数 d2[npairs]，int32
 * @param npairs 候选对数（本探针固定 224）
 * @param q      模数（3329）
 * @param a_hat  输出 NTT 域多项式缓冲，至少 n_out 个 int32
 * @param n_out  目标系数个数（256）
 * @return       实际写入系数个数；672B 固定预 squeeze 下应等于 n_out
 */
uint32_t f203_alg7_rej_scalar_from_d12(const int32_t *d1, const int32_t *d2, uint32_t npairs, int32_t q,
                                       int32_t *a_hat, uint32_t n_out);

#ifdef __cplusplus
}
#endif

#endif /* F203_ALG7_REJ_SCALAR_H */
