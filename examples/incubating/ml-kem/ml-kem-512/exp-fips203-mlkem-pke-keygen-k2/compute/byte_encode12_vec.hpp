// @probe exp-fips203-mlkem-pke-keygen-k2
// @file compute/byte_encode12_vec.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `byte_encode12_vec.hpp` 为该子模块组件。 / Component: byte_encode12_vec.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (800B) + dk_pke.bin (768B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: byte_encode12_config.hpp, basic.hpp, kernel_operator.h, tiling.h, byte_encode12_rom_tables.h, byte_encode12_ub_load.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 19–20 ByteEncode₁₂：将 t̂/ŝ 编成 ek/dk polyvec。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-512（k=2）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/byte_encode12_vec.hpp
 */
#ifndef BYTE_ENCODE12_VEC_HPP
#define BYTE_ENCODE12_VEC_HPP

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
constexpr uint32_t kEncodeTilePairs = 32U;
constexpr uint32_t kEncodeTileCoeffs = kEncodeTilePairs * 2U;

constexpr uint32_t kVecScratchBytes = 1664U;
constexpr uint32_t kVecScratchInt32Slots = (kVecScratchBytes + sizeof(int32_t) - 1U) / sizeof(uint32_t);

constexpr uint32_t kEncodePackGroups = kEncodeTilePairs / 4U;
constexpr uint32_t kEncodePackWords = kEncodePackGroups * 3U;

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
 * 将 scratch UB 按固定字节偏移切成 Encode12VecWs 各字段。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void bind_encode12_vec_ws(LocalTensor<int32_t> &ws, Encode12VecWs &v)
{
    // 字节偏移与 kVecScratchBytes=1664 布局一一对应（aTile@0 … idx2@1248）
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
 * 按偶/奇下标 Gather 出 pair 系数到 t0/t1，供 12-bit 打包。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void gather_pairs_i32(LocalTensor<int32_t> &t0, LocalTensor<int32_t> &t1, LocalTensor<int32_t> &row,
                                        LocalTensor<int32_t> &idx, LocalTensor<int32_t> &idx2, uint32_t pairCount)
{
    using AscendC::Adds;
    using AscendC::CreateVecIndex;
    using AscendC::Gather;
    using AscendC::Muls;
    // idx=0..pairCount-1；字节偏移×8 取偶系数，再 +4 取奇系数（int32）
    CreateVecIndex(idx, static_cast<int32_t>(0), pairCount);
    Muls(idx2, idx, static_cast<int32_t>(8), static_cast<int32_t>(pairCount));
    Gather(t0, row, idx2.ReinterpretCast<uint32_t>(), 0U, pairCount);
    Adds(idx2, idx2, static_cast<int32_t>(4), static_cast<int32_t>(pairCount));
    Gather(t1, row, idx2.ReinterpretCast<uint32_t>(), 0U, pairCount);
}

/**
 * 保留系数低 12 bit（与 ByteEncode₁₂ 位宽一致）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
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
 * 由 pair (a,b) 计算三字节 b0/b1/b2 的中间字。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void compute_b012_tile(Encode12VecWs &v, uint32_t pairCount)
{
    using AscendC::Add;
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;

    const int32_t n = static_cast<int32_t>(pairCount);

    // 先截断到 12 bit，再按 FIPS ByteEncode₁₂ 拆成 3 字节：
    // b0=a[7:0], b1=(a[11:8]<<4)|b[3:0], b2=b[11:4]
    mask_low_bits_i32(v.t0, v.tmp, 12, pairCount);
    mask_low_bits_i32(v.t1, v.tmp, 12, pairCount);

    ShiftRight(v.tmp, v.t0, 8, n);
    Muls(v.b0W, v.tmp, static_cast<int32_t>(256), n);
    Sub(v.b0W, v.t0, v.b0W, n);

    ShiftRight(v.b2W, v.t1, 4, n);

    ShiftRight(v.tmp, v.t0, 8, n);
    ShiftRight(v.idx, v.t1, 4, n);
    Muls(v.idx2, v.idx, static_cast<int32_t>(16), n);
    Sub(v.idx2, v.t1, v.idx2, n);
    Muls(v.b1W, v.idx2, static_cast<int32_t>(16), n);
    Add(v.b1W, v.tmp, v.b1W, n);
}

/**
 * 标量路径把 b0/b1/b2 散写到输出字节缓冲。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void scatter_b012_scalar(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &b0, LocalTensor<int32_t> &b1,
                                           LocalTensor<int32_t> &b2, uint32_t pairBase, uint32_t pairCount)
{
    // 每 pair 写 3 字节；o = 3*(pairBase+i)
    for (uint32_t i = 0; i < pairCount; ++i) {
        const uint32_t o = 3U * (pairBase + i);
        const int32_t ii = static_cast<int32_t>(i);
        r.SetValue(o + 0U, static_cast<uint8_t>(b0.GetValue(ii) & 0xFF));
        r.SetValue(o + 1U, static_cast<uint8_t>(b1.GetValue(ii) & 0xFF));
        r.SetValue(o + 2U, static_cast<uint8_t>(b2.GetValue(ii) & 0xFF));
    }
}

#if BYTE_ENCODE12_SCATTER_VEC >= 1
__aicore__ inline uint8_t lane_byte_i32(LocalTensor<int32_t> &w, int32_t lane)
{
    return static_cast<uint8_t>(w.GetValue(lane) & 0xFF);
}

/**
 * 四个 pair 的 12-bit 组打包为连续字。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void pack_quad12_i32(LocalTensor<int32_t> &packW, LocalTensor<int32_t> &b0, LocalTensor<int32_t> &b1,
                                       LocalTensor<int32_t> &b2)
{
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
 * 向量路径散写 b0/b1/b2 到输出。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
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

__aicore__ inline void poly_byte_encode12_vec_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a, uint32_t coeffN,
                                                    LocalTensor<int32_t> &ws)
{
    Encode12VecWs v;
    bind_encode12_vec_ws(ws, v);

    const uint32_t pairs = coeffN / 2U;
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

constexpr uint32_t kEncodePolyPairs = 128U;
constexpr uint32_t kEncodePolyPackGroups = kEncodePolyPairs / 4U;
constexpr uint32_t kEncodePolyPackWords = kEncodePolyPackGroups * 3U;
constexpr uint32_t kPrefetchScratchInt32Slots = 1376U;
constexpr uint32_t kPrefetchScratchBytes = kPrefetchScratchInt32Slots * static_cast<uint32_t>(sizeof(int32_t));

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
 * 绑定 prefetch 路径的 ROM/scratch 视图。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
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
 * 将 Gather/交错 ROM 表装入 UB（Init 一次）。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
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
 * 按组打包 12-bit quad。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
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
 * prefetch 路径把打包结果写到输出字节区。
 * 对齐 FIPS 203 Alg.13 / ML-KEM-512（k=2）；与 golden 仅 I/O 等价。
 */
__aicore__ inline void scatter_poly12_prefetch(LocalTensor<uint8_t> &r, Encode12PrefetchWs &v)
{
    pack_quad12_groups(v.packW, v.b0W, v.b1W, v.b2W, kEncodePolyPackGroups);
    KYBER_PIPE_ALL();
    AscendC::DataCopy(r, v.packW.ReinterpretCast<uint8_t>(), kPolyBytes);
    KYBER_PIPE_ALL();
}
#endif

__aicore__ inline void poly_byte_encode12_prefetch_local(LocalTensor<uint8_t> &r, LocalTensor<int32_t> &a,
                                                         uint32_t coeffN, LocalTensor<int32_t> &ws)
{
    (void)coeffN;
    Encode12PrefetchWs v;
    bind_encode12_prefetch_ws(ws, v);
    deinterleave_pairs_once(v, a);

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
