
/** ByteEncode₁₂ 标量参考（Host/对照）；设备生产走 vec/prefetch。 */
/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 19–20 ByteEncode₁₂：将 t̂/ŝ 编成 ek/dk polyvec。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/byte_encode12_ref.c
 */
/**
 * @file byte_encode12_ref.c
 * @brief FIPS 203 Alg.5 ByteEncode₁₂ Host 参考实现（标量位打包）。
 *
 * 用途：poly_byte_encode12_ref 将 int32[n] 低 12 bit 压入 uint8[3*n/2]；polyvec 按 poly 拼接。
 *
 * 调用方：gen_data.py ctypes → golden_ek/sk；与 byte_encode12_pair.hpp 设备语义位级一致。
 *
 * 不变量：pairs=n/2；t0=a[2i]&0xFFF、t1=a[2i+1]&0xFFF；3 字节交织布局见 FIPS Alg.5。
 *
 * Golden：output/ek_out.bin、sk_out.bin 对拍；mixPass=7 可隔离 Encode。
 *
 * 设备对照：BYTE_ENCODE12_VEC=1 向量路径须与此 ref 逐字节一致。
 */
#include "byte_encode12_ref.h"

void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n)
{
    const int32_t pairs = n / 2;
    for (int32_t i = 0; i < pairs; i++) {
        const uint16_t t0 = (uint16_t)(a[2 * i] & 0xFFF);
        const uint16_t t1 = (uint16_t)(a[2 * i + 1] & 0xFFF);
        r[3 * i + 0] = (uint8_t)(t0 & 0xFF);
        r[3 * i + 1] = (uint8_t)((t0 >> 8) | ((t1 << 4) & 0xF0));
        r[3 * i + 2] = (uint8_t)(t1 >> 4);
    }
}

/**
 * 本函数为 KeyGen 流水线组件 `polyvec_byte_encode12_ref`（详见 STATUS/customspec）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n)
{
    for (int32_t p = 0; p < k; p++) {
        poly_byte_encode12_ref(r + p * BYTE_ENCODE12_POLY_BYTES, polys + p * n, n);
    }
}
