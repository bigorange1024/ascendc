/**
 * @file byte_decode_d_ref.c
 * @brief FIPS 203 Alg.6 ByteDecode_d 的纯 C 参考实现（golden 计算内核）。
 *        在流水线中的位置：由 scripts/gen_data.py 编译为 libbytedecode_gen.so 并通过
 *        ctypes 调用（与上游 pass-f203-byteencode-d-vec-k4/byte_encode_d_ref.c 一起链接），
 *        先用 encode ref 把随机 comp 编码为 input/encoded.bin，再用本文件的 decode ref
 *        还原、与原始 comp round-trip 校验后作为 output/golden_comp.bin，供 AscendC kernel
 *        （byte_decode_d_vec.hpp/byte_decode_d_custom.cpp）跑完后与 output/comp.bin 对拍
 *        （见 scripts/verify_result.py）。本文件只是「黑盒 oracle」，AscendC 侧实现不要求
 *        与其逐行同构，仅要求最终还原系数一致。
 *        C 语言约束：本文件不含 main，仅提供 poly_byte_decode_d{4,5,10,11}_ref 供外部调用。
 */
#include "byte_decode_d_ref.h"

#include "f203_mlkem_params.h"

/**
 * d=4（ByteEncode_d4 的逆）：1B → 2 系数 × 4bit。每字节低 4bit 为偶数位系数，
 * 高 4bit 为奇数位系数。
 * @param out 输出系数数组，长度 n，元素落在 [0,16)
 * @param in  输入比特流，长度 n/2 字节
 * @param n   系数个数
 */
static void poly_byte_decode_d4_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 2; ++i) {
        out[2 * i + 0] = (int32_t)(in[i] & 0xFu);
        out[2 * i + 1] = (int32_t)((in[i] >> 4) & 0xFu);
    }
}

/**
 * d=10（ByteEncode_d10 的逆）：5B → 4 系数 × 10bit。与 encode 侧对称，每个系数由
 * 相邻两字节的比特片段拼接（右移取本字节残留高位 + 左移拼上一字节的低位）。
 * @param out 输出系数数组，长度 n，元素落在 [0,1024)
 * @param in  输入比特流，长度 n*10/8 字节
 * @param n   系数个数
 */
static void poly_byte_decode_d10_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 4; ++j) {
        /* base：本组（第 j 组）5 个输入字节的起始地址。 */
        const uint8_t *base = &in[5 * j];
        /* t[k]：还原出的第 k 个 10bit 系数。 */
        uint16_t t[4];
        t[0] = (uint16_t)(0x3FFu & ((base[0] >> 0) | ((uint16_t)base[1] << 8)));
        t[1] = (uint16_t)(0x3FFu & ((base[1] >> 2) | ((uint16_t)base[2] << 6)));
        t[2] = (uint16_t)(0x3FFu & ((base[2] >> 4) | ((uint16_t)base[3] << 4)));
        t[3] = (uint16_t)(0x3FFu & ((base[3] >> 6) | ((uint16_t)base[4] << 2)));
        for (int32_t k = 0; k < 4; ++k) {
            out[4 * j + k] = (int32_t)t[k];
        }
    }
}

/**
 * d=5：5B/组 → 8×5bit 系数（Alg.6 逆 ml-kem-1024 c₂），与 encode 侧 poly_byte_encode_d5_c
 * 严格对称：每个系数由 1~2 个相邻输入字节的比特片段拼接。
 * @param out 输出系数数组，长度 n，元素落在 [0,32)
 * @param in  输入比特流，长度 n*5/8 字节
 * @param n   系数个数
 */
static void poly_byte_decode_d5_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        /* offset：本组（第 i 组）5 个输入字节在 in 中的起始偏移。 */
        const int32_t offset = i * 5;
        /* t[j]：还原出的第 j 个 5bit 系数。 */
        uint8_t t[8];
        t[0] = (uint8_t)(0x1Fu & (in[offset + 0] >> 0));
        t[1] = (uint8_t)(0x1Fu & ((in[offset + 0] >> 5) | (in[offset + 1] << 3)));
        t[2] = (uint8_t)(0x1Fu & (in[offset + 1] >> 2));
        t[3] = (uint8_t)(0x1Fu & ((in[offset + 1] >> 7) | (in[offset + 2] << 1)));
        t[4] = (uint8_t)(0x1Fu & ((in[offset + 2] >> 4) | (in[offset + 3] << 4)));
        t[5] = (uint8_t)(0x1Fu & (in[offset + 3] >> 1));
        t[6] = (uint8_t)(0x1Fu & ((in[offset + 3] >> 6) | (in[offset + 4] << 2)));
        t[7] = (uint8_t)(0x1Fu & (in[offset + 4] >> 3));
        for (int32_t j = 0; j < 8; ++j) {
            out[8 * i + j] = (int32_t)t[j];
        }
    }
}

/**
 * d=11：11B/组 → 8×11bit 系数（ML-KEM-1024 c₁），与 encode 侧 poly_byte_encode_d11_c
 * 严格对称：每个系数由 1~2 个相邻输入字节的比特片段拼接。
 * @param out 输出系数数组，长度 n，元素落在 [0,2048)
 * @param in  输入比特流，长度 n*11/8 字节
 * @param n   系数个数
 */
static void poly_byte_decode_d11_c(int32_t *out, const uint8_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 8; ++j) {
        /* base：本组（第 j 组）11 个输入字节的起始地址。 */
        const uint8_t *base = &in[11 * j];
        /* t[k]：还原出的第 k 个 11bit 系数（需 uint16_t 容纳）。 */
        uint16_t t[8];
        t[0] = (uint16_t)(0x7FFu & ((base[0] >> 0) | ((uint16_t)base[1] << 8)));
        t[1] = (uint16_t)(0x7FFu & ((base[1] >> 3) | ((uint16_t)base[2] << 5)));
        t[2] = (uint16_t)(0x7FFu & ((base[2] >> 6) | ((uint16_t)base[3] << 2) | ((uint16_t)base[4] << 10)));
        t[3] = (uint16_t)(0x7FFu & ((base[4] >> 1) | ((uint16_t)base[5] << 7)));
        t[4] = (uint16_t)(0x7FFu & ((base[5] >> 4) | ((uint16_t)base[6] << 4)));
        t[5] = (uint16_t)(0x7FFu & ((base[6] >> 7) | ((uint16_t)base[7] << 1) | ((uint16_t)base[8] << 9)));
        t[6] = (uint16_t)(0x7FFu & ((base[8] >> 2) | ((uint16_t)base[9] << 6)));
        t[7] = (uint16_t)(0x7FFu & ((base[9] >> 5) | ((uint16_t)base[10] << 3)));
        for (int32_t k = 0; k < 8; ++k) {
            out[8 * j + k] = (int32_t)t[k];
        }
    }
}

/* 以下为对外导出的 C 接口（供 byte_decode_d_ref.h 声明、scripts/gen_data.py 用 ctypes 调用），
 * 各自转发到对应 d 值的静态实现，函数名以 d 值区分，无分支逻辑。 */

void poly_byte_decode_d4_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d4_c(out, in, n);
}

void poly_byte_decode_d10_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d10_c(out, in, n);
}

void poly_byte_decode_d5_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d5_c(out, in, n);
}

void poly_byte_decode_d11_ref(int32_t *out, const uint8_t *in, int32_t n)
{
    poly_byte_decode_d11_c(out, in, n);
}
