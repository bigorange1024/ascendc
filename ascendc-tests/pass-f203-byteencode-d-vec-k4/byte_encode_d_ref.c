/**
 * @file byte_encode_d_ref.c
 * @brief FIPS 203 Alg.5 ByteEncode_d 的纯 C 参考实现（golden 计算内核）。
 *        在流水线中的位置：由 scripts/gen_data.py 编译为 libbyte_encode_d_ref.so 并通过
 *        ctypes 调用，生成 output/golden_encoded.bin，供 AscendC kernel（byte_encode_d_vec.hpp/
 *        byte_encode_d_custom.cpp）跑完后与 output/encoded.bin 做逐字节对拍（见
 *        scripts/verify_result.py）。本文件只是「黑盒 oracle」，AscendC 侧实现不要求与其
 *        逐行同构，仅要求最终输出比特流一致。
 *        C 语言约束：本文件不含 main，仅提供 poly_byte_encode_d{4,5,10,11}_ref 供外部调用。
 */
#include "byte_encode_d_ref.h"

#include "f203_mlkem_params.h"

/**
 * d=4：8 系数 × 4bit → 4B/组。每组 8 个系数两两拼成一个字节（低 4bit 为偶数位系数，
 * 高 4bit 为奇数位系数），共 n/8 组，输出 n/2 字节。
 * @param out 输出比特流，长度 n/2 字节
 * @param in  输入系数数组，长度 n，每个元素需 < 16（否则 & 0xF 截断丢弃高位）
 * @param n   系数个数
 */
static void poly_byte_encode_d4_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        /* t[j]：本组（第 i 组）第 j 个系数的低 4bit。 */
        uint8_t t[8];
        for (int32_t j = 0; j < 8; ++j) {
            t[j] = (uint8_t)(in[8 * i + j] & 0xF);
        }
        /* 每两个系数拼成一个输出字节：t[2k] 占低 4bit，t[2k+1] 占高 4bit。 */
        out[i * 4 + 0] = (uint8_t)(t[0] | (t[1] << 4));
        out[i * 4 + 1] = (uint8_t)(t[2] | (t[3] << 4));
        out[i * 4 + 2] = (uint8_t)(t[4] | (t[5] << 4));
        out[i * 4 + 3] = (uint8_t)(t[6] | (t[7] << 4));
    }
}

/**
 * d=10：4 系数 × 10bit → 5B/组。10bit 系数跨字节边界不规则，需按比特位移拼装。
 * @param out 输出比特流，长度 n*10/8 字节
 * @param in  输入系数数组，长度 n，每个元素需 < 1024（否则 & 0x3FF 截断丢弃高位）
 * @param n   系数个数
 */
static void poly_byte_encode_d10_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 4; ++j) {
        /* t[k]：本组（第 j 组）第 k 个系数的低 10bit。 */
        uint16_t t[4];
        for (int32_t k = 0; k < 4; ++k) {
            t[k] = (uint16_t)(in[4 * j + k] & 0x3FF);
        }
        /* 4 个 10bit 系数 = 40bit，恰好拼成 5 字节；每个输出字节由相邻两个系数的
         * 部分比特拼接而成（低系数右移取高位残留 + 高系数左移填充低位）。 */
        out[5 * j + 0] = (uint8_t)((t[0] >> 0) & 0xFF);
        out[5 * j + 1] = (uint8_t)((t[0] >> 8) | ((t[1] << 2) & 0xFF));
        out[5 * j + 2] = (uint8_t)((t[1] >> 6) | ((t[2] << 4) & 0xFF));
        out[5 * j + 3] = (uint8_t)((t[2] >> 4) | ((t[3] << 6) & 0xFF));
        out[5 * j + 4] = (uint8_t)(t[3] >> 2);
    }
}

/**
 * d=5：8 系数 × 5bit → 5B/组（与 FIPS Alg.5 比特流及 ml-kem-1024 c₂ 布局一致）。
 * 8×5bit=40bit 恰好拼成 5 字节，故按 8 系数分组（而非 d=10 的 4 系数）。
 * @param out 输出比特流，长度 n*5/8 字节
 * @param in  输入系数数组，长度 n，每个元素需 < 32（否则 & 0x1F 截断丢弃高位）
 * @param n   系数个数
 */
static void poly_byte_encode_d5_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        /* t[j]：本组第 j 个系数的低 5bit。 */
        uint8_t t[8];
        for (int32_t j = 0; j < 8; ++j) {
            t[j] = (uint8_t)(in[8 * i + j] & 0x1F);
        }
        /* 5 个输出字节各由 2~3 个相邻系数的比特片段拼接而成（右移取残留高位 +
         * 左移填充低位，最终按位或组装；&0xFF 只是防御性截断，理论上不会溢出）。 */
        out[i * 5 + 0] = (uint8_t)(0xFF & ((t[0] >> 0) | (t[1] << 5)));
        out[i * 5 + 1] = (uint8_t)(0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7)));
        out[i * 5 + 2] = (uint8_t)(0xFF & ((t[3] >> 1) | (t[4] << 4)));
        out[i * 5 + 3] = (uint8_t)(0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6)));
        out[i * 5 + 4] = (uint8_t)(0xFF & ((t[6] >> 2) | (t[7] << 3)));
    }
}

/**
 * d=11：8 系数 × 11bit → 11B/组（ML-KEM-1024 c₁ 单 poly 352B）。
 * 8×11bit=88bit 恰好拼成 11 字节。
 * @param out 输出比特流，长度 n*11/8 字节
 * @param in  输入系数数组，长度 n，每个元素需 < 2048（否则 & 0x7FF 截断丢弃高位）
 * @param n   系数个数
 */
static void poly_byte_encode_d11_c(uint8_t *out, const int32_t *in, int32_t n)
{
    for (int32_t j = 0; j < n / 8; ++j) {
        /* t[k]：本组第 k 个系数的低 11bit（需 uint16_t 容纳，>8bit）。 */
        uint16_t t[8];
        for (int32_t k = 0; k < 8; ++k) {
            t[k] = (uint16_t)(in[8 * j + k] & 0x7FF);
        }
        /* 11 个输出字节各由 1~3 个相邻系数的比特片段拼接而成，规则同 d=5/d=10：
         * 右移取上一系数的高位残留，左移把下一系数的低位填入本字节空位。 */
        out[11 * j + 0] = (uint8_t)((t[0] >> 0) & 0xFF);
        out[11 * j + 1] = (uint8_t)((t[0] >> 8) | ((t[1] << 3) & 0xFF));
        out[11 * j + 2] = (uint8_t)((t[1] >> 5) | ((t[2] << 6) & 0xFF));
        out[11 * j + 3] = (uint8_t)((t[2] >> 2) & 0xFF);
        out[11 * j + 4] = (uint8_t)((t[2] >> 10) | ((t[3] << 1) & 0xFF));
        out[11 * j + 5] = (uint8_t)((t[3] >> 7) | ((t[4] << 4) & 0xFF));
        out[11 * j + 6] = (uint8_t)((t[4] >> 4) | ((t[5] << 7) & 0xFF));
        out[11 * j + 7] = (uint8_t)((t[5] >> 1) & 0xFF);
        out[11 * j + 8] = (uint8_t)((t[5] >> 9) | ((t[6] << 2) & 0xFF));
        out[11 * j + 9] = (uint8_t)((t[6] >> 6) | ((t[7] << 5) & 0xFF));
        out[11 * j + 10] = (uint8_t)(t[7] >> 3);
    }
}

/* 以下为对外导出的 C 接口（供 byte_encode_d_ref.h 声明、scripts/gen_data.py 用 ctypes 调用），
 * 各自转发到对应 d 值的静态实现，函数名以 d 值区分，无分支逻辑。 */

void poly_byte_encode_d4_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d4_c(out, in, n);
}

void poly_byte_encode_d10_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d10_c(out, in, n);
}

void poly_byte_encode_d5_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d5_c(out, in, n);
}

void poly_byte_encode_d11_ref(uint8_t *out, const int32_t *in, int32_t n)
{
    poly_byte_encode_d11_c(out, in, n);
}
