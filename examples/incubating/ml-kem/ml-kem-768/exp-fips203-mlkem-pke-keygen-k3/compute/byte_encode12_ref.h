// @probe exp-fips203-mlkem-pke-keygen-k3
// @file compute/byte_encode12_ref.h
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `byte_encode12_ref.h` 为该子模块组件。 / Component: byte_encode12_ref.h.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: stdint.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 19–20 ByteEncode₁₂：将 t̂/ŝ 编成 ek/dk polyvec。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/byte_encode12_ref.h
 */
/**
 * @file byte_encode12_ref.h
 * @brief Host/C golden：FIPS 203 Alg.5 ByteEncode₁₂ 单 poly / polyvec API。
 *
 * 用途：gen_data.py 编译 libbyte_encode12_ref.so，生成 golden_ek.bin、golden_sk.bin（各 4×384B）。
 *
 * 调用方：scripts/gen_data.py；设备侧见 byte_encode12_pair.hpp。
 *
 * 不变量：BYTE_ENCODE12_POLY_BYTES=384；n=256 系数；每对系数 12 bit 打包为 3 字节。
 *
 * Golden：即本库输出；verify_result.py cmp ek_out/sk_out。
 *
 * CMake：无（gen_data gcc -shared 编译 byte_encode12_ref.c）。
 */
#ifndef BYTE_ENCODE12_REF_H
#define BYTE_ENCODE12_REF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BYTE_ENCODE12_POLY_BYTES 384
#define BYTE_ENCODE12_POLYVEC_BYTES (4 * BYTE_ENCODE12_POLY_BYTES)

void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n);
void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n);

#ifdef __cplusplus
}
#endif

#endif
