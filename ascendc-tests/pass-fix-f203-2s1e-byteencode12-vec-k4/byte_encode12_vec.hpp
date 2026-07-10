#ifndef BYTE_ENCODE12_VEC_HPP
#define BYTE_ENCODE12_VEC_HPP

/**
 * @file byte_encode12_vec.hpp
 * @brief ByteEncode₁₂ 向量实现：tile=32 Gather 路径 + 整 poly prefetch 路径。
 *
 * 流水线位置：设备侧核心算法；由 poly_byte_encode12_local 在 VEC≥1 时调用。
 * 与 golden 关系：输出字节流须与 Alg.5 / ref.c / gen_data golden 逐字节一致（I/O 等价，非同构）。
 * 作用：解交错偶奇系数 → 掩 12 bit → 算 b0/b1/b2 → SoA→AoS 打包写回 384B。
 */

#include "byte_encode12_config.hpp"
#include "basic.hpp"
#include "kernel_operator.h"
#include "tiling.h"

#if BYTE_ENCODE12_VEC >= 1

#if BYTE_ENCODE12_PREFETCH >= 1
#include "byte_encode12_rom_tables.h"
#include "byte_encode12_ub_load.hpp"
#endif

namespace byte_encode12 {

constexpr uint32_t kPolyBytes = 384U;
constexpr uint32_t kPolysPerAiv = static_cast<uint32_t>(tiling::kEPerAiv);
constexpr uint32_t kAivShardBytes = kPolysPerAiv * kPolyBytes;
/** 每 tile 处理 32 对（64 系数）→ 96 字节输出 */
constexpr uint32_t kEncodeTilePairs = 32U;
constexpr uint32_t kEncodeTileCoeffs = kEncodeTilePairs * 2U;

/** tile 路径 scratch 字节/槽（含 aTile、t0/t1、b0..b2、pack、idx） */
constexpr uint32_t kVecScratchBytes = 1664U;
constexpr uint32_t kVecScratchInt32Slots = (kVecScratchBytes + sizeof(int32_t) - 1U) / sizeof(uint32_t);

/** 4 pair → 3 int32 word 打包：32 pair → 8 group → 24 word */
constexpr uint32_t kEncodePackGroups = kEncodeTilePairs / 4U;
constexpr uint32_t kEncodePackWords = kEncodePackGroups * 3U;

/** tile 路径工作区视图：各子缓冲在 scratch 内的偏移见 bind_encode12_vec_ws */
struct Encode12VecWs {
    LocalTensor<int32_t> aTile;
    LocalTensor<int32_t> t0;
    LocalTensor<int32_t> t1;
    LocalTensor<int32_t> tmp;
    LocalTensor<int32_t> b0W;
    LocalTensor<int32_t> b1W;
    LocalTensor<int32_t> b2W;
    LocalTensor<int32_t> packW;
    LocalTensor<int32_t> idx;
    LocalTensor<int32_t> idx2;
};

/**
 * 将连续 int32 scratch 按固定字节偏移切成 Encode12VecWs 各字段。
 * @param ws 整块工作区
 * @param v  输出视图（偏移：aTile@0, t0@256, t1@384, … idx2@1248）
 */
__aicore__ inline void bind_encode12_vec_ws(LocalTensor<int32_t> &ws, Encode12VecWs &v)
{
    auto base = ws.ReinterpretCast<uint8_t>();
    v.aTile = base[0].ReinterpretCast<int32_t>();
    v.t0 = base[256].ReinterpretCast<int32_t>();
    v.t1 = base[384].ReinterpretCast<int32_t>();
    v.tmp = base[512].ReinterpretCast<int32_t>();
    v.b0W = base[640].ReinterpretCast<int32_t>();
    v.b1W = base[768].ReinterpretCast<int32_t>();
    v.b2W = base[896].ReinterpretCast<int32_t>();
    v.packW = base[1024].ReinterpretCast<int32_t>();
    v.idx = base[1120].ReinterpretCast<int32_t>();
    v.idx2 = base[1248].ReinterpretCast<int32_t>();
}

/**
 * 从交错系数行 Gather 出偶/奇两路（字节索引 8i / 8i+4）。
 * @param t0/t1     输出偶/奇系数，长度 pairCount
 * @param row       输入交错 int32 行（tile 内）
 * @param idx/idx2  临时索引缓冲
 * @param pairCount 对数（通常 32）
 */
__aicore__ inline void gather_pairs_i32(LocalTensor<int32_t> &t0, LocalTensor<int32_t> &t1, LocalTensor<int32_t> &row,
                                        LocalTensor<int32_t> &idx, LocalTensor<int32_t> &idx2, uint32_t pairCount)
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;
    // idx = 0..pairCount-1；字节偏移 = 8*idx（偶），再 +4（奇）
    CreateVecIndex(idx, static_cast<int32_t>(0), pairCount);
    Muls(idx2, idx, static_cast<int32_t>(8), static_cast<int32_t>(pairCount));
    Gather(t0, row, idx2.ReinterpretCast<uint32_t>(), 0U, pairCount);
    Adds(idx2, idx2, static_cast<int32_t>(4), static_cast<int32_t>(pairCount));
    Gather(t1, row, idx2.ReinterpretCast<uint32_t>(), 0U, pairCount);
}

/**
 * 就地取低 bits 位：v := v - ((v>>bits)<<bits)，等价 v & ((1<<bits)-1)。
 * @param v     输入/输出
 * @param tmp   临时
 * @param bits  保留位数（本路径 12）
 * @param count 元素个数
 */
__aicore__ inline void mask_low_bits_i32(LocalTensor<int32_t> &v, LocalTensor<int32_t> &tmp, int32_t bits,
                                          uint32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t scale = static_cast<int32_t>(1) << bits;
    ShiftRight(tmp, v, bits, n);
    Muls(tmp, tmp, scale, n);
    Sub(v, v, tmp, n);
}

/**
 * 由 t0/t1（已解交错）计算 Alg.5 三字节分量 b0/b1/b2（仍为 SoA int32）。
 * @param v         工作区（读写 t0/t1/tmp/b0W/b1W/b2W，复用 idx 作临时）
 * @param pairCount 对数
 * 公式：b0=t0&0xFF；b2=t1>>4；b1=(t0>>8)|((t1&0xF)<<4)
 */
__aicore__ inline void compute_b012_tile(Encode12VecWs &v, uint32_t pairCount)
{
    using AscendC::Add;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;

    const int32_t n = static_cast<int32_t>(pairCount);

    // 先掩 12 bit
    mask_low_bits_i32(v.t0, v.tmp, 12, pairCount);
    mask_low_bits_i32(v.t1, v.tmp, 12, pairCount);

    // b0 = t0 低 8 bit：t0 - ((t0>>8)*256)
    ShiftRight(v.tmp, v.t0, 8, n);
    Muls(v.b0W, v.tmp, static_cast<int32_t>(256), n);
    Sub(v.b0W, v.t0, v.b0W, n);

    // b2 = t1 >> 4
    ShiftRight(v.b2W, v.t1, 4, n);

    // b1 = (t0>>8) | ((t1 & 0xF) << 4)；用 idx/idx2 作临时
    ShiftRight(v.tmp, v.t0, 8, n);
    ShiftRight(v.idx, v.t1, 4, n);
    Muls(v.idx2, v.idx, static_cast<int32_t>(16), n);
    Sub(v.idx2, v.t1, v.idx2, n);
    Muls(v.b1W, v.idx2, static_cast<int32_t>(16), n);
    Add(v.b1W, v.tmp, v.b1W, n);
}

/**
 * 标量 SoA→AoS：将 b0/b1/b2 按 pair 写到 r 的 3 字节槽。
 * @param r         输出字节缓冲
 * @param b0/b1/b2  SoA 分量
 * @param pairBase  本 tile 在 poly 内的 pair 起点
 * @param pairCount 本 tile 对数
 */
__aicore__ inline void scatter_b012_scalar(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &b0, LocalTensor<int32_t> &b1,
                                           LocalTensor<int32_t> &b2, uint32_t pairBase, uint32_t pairCount)
{
    // i：tile 内 pair；o：全局字节偏移 = 3*(pairBase+i)
    for (uint32_t i = 0; i < pairCount; ++i) {
        const uint32_t o = 3U * (pairBase + i);
        const int32_t ii = static_cast<int32_t>(i);
        r.SetValue(o + 0U, static_cast<uint8_t>(b0.GetValue(ii) & 0xFF));
        r.SetValue(o + 1U, static_cast<uint8_t>(b1.GetValue(ii) & 0xFF));
        r.SetValue(o + 2U, static_cast<uint8_t>(b2.GetValue(ii) & 0xFF));
    }
}

#if BYTE_ENCODE12_SCATTER_VEC >= 1
/**
 * 取 int32 lane 的低 8 bit 作打包字节。
 * @param w    源 LocalTensor
 * @param lane 下标
 */
__aicore__ inline uint8_t lane_byte_i32(LocalTensor<int32_t> &w, int32_t lane)
{
    return static_cast<uint8_t>(w.GetValue(lane) & 0xFF);
}

/**
 * 将 32 对的 b0/b1/b2 按 4 pair→3 int32 little-endian 字打包进 packW。
 * @param packW 输出，长度 kEncodePackWords
 * @param b0/b1/b2 各 32 lane
 * 字布局（每 group）：w0=b00|b10|b20|b01；w1=b11|b21|b02|b12；w2=b22|b03|b13|b23
 */
__aicore__ inline void pack_quad12_i32(LocalTensor<int32_t> &packW, LocalTensor<int32_t> &b0, LocalTensor<int32_t> &b1,
                                       LocalTensor<int32_t> &b2)
{
    // g：4-pair 组；p=g*4 为组内首 pair；wi=g*3 为字下标
    for (uint32_t g = 0; g < kEncodePackGroups; ++g) {
        const int32_t p = static_cast<int32_t>(g * 4U);
        const int32_t wi = static_cast<int32_t>(g * 3U);
        const uint8_t b00 = lane_byte_i32(b0, p);
        const uint8_t b10 = lane_byte_i32(b1, p);
        const uint8_t b20 = lane_byte_i32(b2, p);
        const uint8_t b01 = lane_byte_i32(b0, p + 1);
        const uint8_t b11 = lane_byte_i32(b1, p + 1);
        const uint8_t b21 = lane_byte_i32(b2, p + 1);
        const uint8_t b02 = lane_byte_i32(b0, p + 2);
        const uint8_t b12 = lane_byte_i32(b1, p + 2);
        const uint8_t b22 = lane_byte_i32(b2, p + 2);
        const uint8_t b03 = lane_byte_i32(b0, p + 3);
        const uint8_t b13 = lane_byte_i32(b1, p + 3);
        const uint8_t b23 = lane_byte_i32(b2, p + 3);

        const int32_t w0 = static_cast<int32_t>(b00) | (static_cast<int32_t>(b10) << 8) |
                           (static_cast<int32_t>(b20) << 16) | (static_cast<int32_t>(b01) << 24);
        const int32_t w1 = static_cast<int32_t>(b11) | (static_cast<int32_t>(b21) << 8) |
                           (static_cast<int32_t>(b02) << 16) | (static_cast<int32_t>(b12) << 24);
        const int32_t w2 = static_cast<int32_t>(b22) | (static_cast<int32_t>(b03) << 8) |
                           (static_cast<int32_t>(b13) << 16) | (static_cast<int32_t>(b23) << 24);
        packW.SetValue(wi + 0, w0);
        packW.SetValue(wi + 1, w1);
        packW.SetValue(wi + 2, w2);
    }
}

/**
 * 向量打包后 DataCopy 96B 到输出 r[byteBase..]。
 * @param r        输出 poly 字节缓冲
 * @param packW    打包字缓冲
 * @param b0W/b1W/b2W SoA 分量
 * @param byteBase 本 tile 字节起点 = pairBase*3
 */
__aicore__ inline void scatter_b012_vec(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &packW,
                                        LocalTensor<int32_t> &b0W, LocalTensor<int32_t> &b1W, LocalTensor<int32_t> &b2W,
                                        uint32_t byteBase)
{
    pack_quad12_i32(packW, b0W, b1W, b2W);
    KYBER_PIPE_ALL();
    AscendC::DataCopy(r[byteBase], packW.ReinterpretCast<uint8_t>(), kEncodeTilePairs * 3U);
    KYBER_PIPE_ALL();
}
#endif

/**
 * tile=32 向量编码整 poly：循环 pairs/32 次 Gather→b012→scatter。
 * @param r      输出 uint8[384]
 * @param a      输入 int32[coeffN]
 * @param coeffN 系数数（须能被 64 整除）
 * @param ws     kVecScratch 工作区
 */
__aicore__ inline void poly_byte_encode12_vec_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN,
                                                    LocalTensor<int32_t> &ws)
{
    Encode12VecWs v;
    bind_encode12_vec_ws(ws, v);

    const uint32_t pairs = coeffN / 2U;
    // tile：0..pairs/32-1；每轮 64 系数 → 96 字节
    for (uint32_t tile = 0; tile < pairs / kEncodeTilePairs; ++tile) {
        const uint32_t coeffBase = tile * kEncodeTileCoeffs;
        const uint32_t pairBase = tile * kEncodeTilePairs;
        const uint32_t byteBase = pairBase * 3U;

        AscendC::DataCopy(v.aTile, a[coeffBase], kEncodeTileCoeffs);
        KYBER_PIPE_ALL();
        gather_pairs_i32(v.t0, v.t1, v.aTile, v.idx, v.idx2, kEncodeTilePairs);
        KYBER_PIPE_ALL();
        compute_b012_tile(v, kEncodeTilePairs);
        KYBER_PIPE_ALL();
#if BYTE_ENCODE12_SCATTER_VEC >= 1
        scatter_b012_vec(r, v.packW, v.b0W, v.b1W, v.b2W, byteBase);
#else
        scatter_b012_scalar(r, v.b0W, v.b1W, v.b2W, pairBase, kEncodeTilePairs);
#endif
    }
}

#if BYTE_ENCODE12_PREFETCH >= 1

/** 整 poly：128 pair；打包 32 group → 96 word → 384B */
constexpr uint32_t kEncodePolyPairs = 128U;
constexpr uint32_t kEncodePolyPackGroups = kEncodePolyPairs / 4U;
constexpr uint32_t kEncodePolyPackWords = kEncodePolyPackGroups * 3U;
constexpr uint32_t kPrefetchScratchInt32Slots = 1376U;
constexpr uint32_t kPrefetchScratchBytes = kPrefetchScratchInt32Slots * static_cast<uint32_t>(sizeof(int32_t));

/** prefetch 工作区：整 poly 宽 t0/t1/b* + ROM 索引 */
struct Encode12PrefetchWs {
    LocalTensor<int32_t> t0;
    LocalTensor<int32_t> t1;
    LocalTensor<int32_t> tmp;
    LocalTensor<int32_t> b0W;
    LocalTensor<int32_t> b1W;
    LocalTensor<int32_t> b2W;
    LocalTensor<int32_t> packW;
    LocalTensor<int32_t> idxEven;
    LocalTensor<int32_t> idxOdd;
};

/**
 * 按 128-wide 布局绑定 prefetch scratch。
 * @param ws 整块 int32 工作区
 * @param v  输出视图
 */
__aicore__ inline void bind_encode12_prefetch_ws(LocalTensor<int32_t> &ws, Encode12PrefetchWs &v)
{
    auto base = ws.ReinterpretCast<uint8_t>();
    v.t0 = base[0].ReinterpretCast<int32_t>();
    v.t1 = base[kEncodePolyPairs * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.tmp = base[2U * kEncodePolyPairs * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.b0W = base[3U * kEncodePolyPairs * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.b1W = base[4U * kEncodePolyPairs * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.b2W = base[5U * kEncodePolyPairs * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.packW = base[6U * kEncodePolyPairs * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.idxEven = base[(6U * kEncodePolyPairs + kEncodePolyPackWords) * sizeof(int32_t)].ReinterpretCast<int32_t>();
    v.idxOdd = base[(6U * kEncodePolyPairs + kEncodePolyPackWords + kEncodePolyPairs) * sizeof(int32_t)]
                   .ReinterpretCast<int32_t>();
}

/**
 * 预装 ROM even/odd 索引到 UB（可在多 poly 间复用；当前每次 encode 仍会再拷）。
 * @param ws prefetch scratch
 */
__aicore__ inline void init_encode12_prefetch_rom(LocalTensor<int32_t> &ws)
{
    Encode12PrefetchWs v;
    bind_encode12_prefetch_ws(ws, v);
    byte_encode12_ub_load::copy_rom_int32_ub(v.idxEven, ::gByteEncode12GatherEvenByteGm,
                                             static_cast<int32_t>(kEncodePolyPairs));
    byte_encode12_ub_load::copy_rom_int32_ub(v.idxOdd, ::gByteEncode12GatherOddByteGm,
                                            static_cast<int32_t>(kEncodePolyPairs));
    KYBER_PIPE_ALL();
}

/**
 * 一次 Gather×2：整 poly 解交错到 t0/t1（先拷 ROM 索引）。
 * @param v  prefetch 工作区
 * @param a  输入系数行 [256] int32
 */
__aicore__ inline void deinterleave_pairs_once(Encode12PrefetchWs &v, LocalTensor<int32_t> &a)
{
    using AscendC::Gather;
    byte_encode12_ub_load::copy_rom_int32_ub(v.idxEven, ::gByteEncode12GatherEvenByteGm,
                                             static_cast<int32_t>(kEncodePolyPairs));
    byte_encode12_ub_load::copy_rom_int32_ub(v.idxOdd, ::gByteEncode12GatherOddByteGm,
                                            static_cast<int32_t>(kEncodePolyPairs));
    KYBER_PIPE_ALL();
    Gather(v.t0, a, v.idxEven.ReinterpretCast<uint32_t>(), 0U, kEncodePolyPairs);
    KYBER_PIPE_ALL();
    Gather(v.t1, a, v.idxOdd.ReinterpretCast<uint32_t>(), 0U, kEncodePolyPairs);
    KYBER_PIPE_ALL();
}

#if BYTE_ENCODE12_SCATTER_VEC >= 1
/**
 * 与 pack_quad12_i32 相同布局，但 groups 可变（整 poly 为 32）。
 * @param packW 输出字缓冲
 * @param b0/b1/b2 SoA
 * @param groups 4-pair 组数
 */
__aicore__ inline void pack_quad12_groups(LocalTensor<int32_t> &packW, LocalTensor<int32_t> &b0, LocalTensor<int32_t> &b1,
                                          LocalTensor<int32_t> &b2, uint32_t groups)
{
    for (uint32_t g = 0; g < groups; ++g) {
        const int32_t p = static_cast<int32_t>(g * 4U);
        const int32_t wi = static_cast<int32_t>(g * 3U);
        const uint8_t b00 = lane_byte_i32(b0, p);
        const uint8_t b10 = lane_byte_i32(b1, p);
        const uint8_t b20 = lane_byte_i32(b2, p);
        const uint8_t b01 = lane_byte_i32(b0, p + 1);
        const uint8_t b11 = lane_byte_i32(b1, p + 1);
        const uint8_t b21 = lane_byte_i32(b2, p + 1);
        const uint8_t b02 = lane_byte_i32(b0, p + 2);
        const uint8_t b12 = lane_byte_i32(b1, p + 2);
        const uint8_t b22 = lane_byte_i32(b2, p + 2);
        const uint8_t b03 = lane_byte_i32(b0, p + 3);
        const uint8_t b13 = lane_byte_i32(b1, p + 3);
        const uint8_t b23 = lane_byte_i32(b2, p + 3);

        const int32_t w0 = static_cast<int32_t>(b00) | (static_cast<int32_t>(b10) << 8) |
                           (static_cast<int32_t>(b20) << 16) | (static_cast<int32_t>(b01) << 24);
        const int32_t w1 = static_cast<int32_t>(b11) | (static_cast<int32_t>(b21) << 8) |
                           (static_cast<int32_t>(b02) << 16) | (static_cast<int32_t>(b12) << 24);
        const int32_t w2 = static_cast<int32_t>(b22) | (static_cast<int32_t>(b03) << 8) |
                           (static_cast<int32_t>(b13) << 16) | (static_cast<int32_t>(b23) << 24);
        packW.SetValue(wi + 0, w0);
        packW.SetValue(wi + 1, w1);
        packW.SetValue(wi + 2, w2);
    }
}

/**
 * 整 poly 打包后一次 DataCopy 384B 到 r。
 * @param r 输出
 * @param v 含 b0/b1/b2/packW 的 prefetch 工作区
 */
__aicore__ inline void scatter_poly12_prefetch(LocalTensor<uint8_t> &r, Encode12PrefetchWs &v)
{
    pack_quad12_groups(v.packW, v.b0W, v.b1W, v.b2W, kEncodePolyPackGroups);
    KYBER_PIPE_ALL();
    AscendC::DataCopy(r, v.packW.ReinterpretCast<uint8_t>(), kPolyBytes);
    KYBER_PIPE_ALL();
}
#endif

/**
 * prefetch 路径：整 poly 一次解交错 + 128-wide b012 + 整包写出。
 * @param r      输出 uint8[384]
 * @param a      输入 int32[256]
 * @param coeffN 保留参数（当前未用，固定 128 pair）
 * @param ws     kPrefetchScratch 工作区
 */
__aicore__ inline void poly_byte_encode12_prefetch_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a,
                                                         uint32_t coeffN, LocalTensor<int32_t> &ws)
{
    (void)coeffN;
    Encode12PrefetchWs v;
    bind_encode12_prefetch_ws(ws, v);
    deinterleave_pairs_once(v, a);

    // 复用 tile 路径的 compute_b012_tile：把 prefetch 缓冲映射到 Encode12VecWs 视图
    Encode12VecWs compute;
    compute.t0 = v.t0;
    compute.t1 = v.t1;
    compute.tmp = v.tmp;
    compute.b0W = v.b0W;
    compute.b1W = v.b1W;
    compute.b2W = v.b2W;
    compute.idx = v.idxEven;
    compute.idx2 = v.idxOdd;
    compute_b012_tile(compute, kEncodePolyPairs);
    KYBER_PIPE_ALL();
#if BYTE_ENCODE12_SCATTER_VEC >= 1
    scatter_poly12_prefetch(r, v);
#else
    scatter_b012_scalar(r, v.b0W, v.b1W, v.b2W, 0U, kEncodePolyPairs);
#endif
}

#endif // BYTE_ENCODE12_PREFETCH >= 1

} // namespace byte_encode12

#endif // BYTE_ENCODE12_VEC >= 1

#endif
