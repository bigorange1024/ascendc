/**
 * @file byte_encode12_ref.c
 * @brief FIPS 203 Alg.5 ByteEncode₁₂ — Host/C golden（与 byte_encode12_pair.hpp 设备语义一致）。
 *
 * 流水线位置：host 侧参考实现；供对照或其它工具链接，本探针 golden 主路径走 Python/v2。
 * 与 golden 关系：逐 pair 打包规则与设备标量路径、verify 期望字节流一致（I/O 等价）。
 * 作用：实现单 poly / polyvec 的 12-bit 紧打包。
 */
#include "byte_encode12_ref.h"

/**
 * 单 poly ByteEncode₁₂：每 2 个 12-bit 系数打成 3 字节。
 * @param r 输出字节缓冲，长度 pairs*3
 * @param a 输入系数，dtype int32，形状 [n]
 * @param n 系数个数（偶数）；pairs = n/2
 * 编码：t0=a[2i]&0xFFF，t1=a[2i+1]&0xFFF →
 *   r[3i]=t0 低 8 bit；r[3i+1]=(t0>>8)|(t1<<4 的高 4)；r[3i+2]=t1>>4
 */
void poly_byte_encode12_ref(uint8_t *r, const int32_t *a, int32_t n)
{
    const int32_t pairs = n / 2;
    // 按系数对打包：偶下标 t0、奇下标 t1 → 3 字节
    for (int32_t i = 0; i < pairs; i++) {
        const uint16_t t0 = (uint16_t)(a[2 * i] & 0xFFF);
        const uint16_t t1 = (uint16_t)(a[2 * i + 1] & 0xFFF);
        r[3 * i + 0] = (uint8_t)(t0 & 0xFF);
        r[3 * i + 1] = (uint8_t)((t0 >> 8) | ((t1 << 4) & 0xF0));
        r[3 * i + 2] = (uint8_t)(t1 >> 4);
    }
}

/**
 * polyvec：对 k 个连续 poly 依次调用 poly_byte_encode12_ref。
 * @param r     输出，步进 BYTE_ENCODE12_POLY_BYTES
 * @param polys 输入，步进 n 个 int32
 * @param k     poly 个数
 * @param n     每 poly 系数数
 */
void polyvec_byte_encode12_ref(uint8_t *r, const int32_t *polys, int32_t k, int32_t n)
{
    // p：poly 下标；输出/输入按 poly 步进
    for (int32_t p = 0; p < k; p++) {
        poly_byte_encode12_ref(r + p * BYTE_ENCODE12_POLY_BYTES, polys + p * n, n);
    }
}
