// @probe stable-mlkem-f203-pke-keygen-k4
// @file prep/alg7/f203_alg7_d12_vec.hpp
// @layer prep
// @role prep/alg7：FIPS203 Algorithm 7 SampleNTT（CBD+rej 采样 ŝ/ê 等）；含 XOF、rej 标量/向量与 compact LUT ROM。 / Alg.7 SampleNTT rejection-sampling prep kernels. 本文件 `f203_alg7_d12_vec.hpp` 为该子模块组件。 / Component: f203_alg7_d12_vec.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: f203_alg7_config.h, f203_alg7_layout.h, f203_alg7_g.hpp, f203_alg7_shake_xof.hpp, f203_alg7_rej_scalar.hpp, f203_alg7_deinterleave_rom.h, f203_alg7_rej_vec.hpp, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_alg7_d12_vec.hpp
 * @brief Alg.7 设备全链编排：单 TPipe 内 SHAKE(UB)→解交织→d1/d2→rej→â（最大核心模块）。
 *
 * 流水线位置：
 *   entry.cpp 调用 BuildAlg7SampleNttFromSeedD → 本文件编排 TPipe/UB/Que → 链末 DataCopy 落 GM。
 *   子模块：g.hpp(ρ)、shake_xof.hpp(SHAKE)、rej_vec.hpp 或 rej_scalar.hpp(rej)。
 *
 * 数学契约（N=256, q=3329, 单 poly (j,i)）：
 *   line 5–6：SHAKE128(ρ||j||i) → 672B XOF
 *   line 6–7：每 3 字节 (C0,C1,C2) → d1 = C0 + 256·(C1 mod 16), d2 = ⌊C1/16⌋ + 16·C2
 *   line 8–15：rej 从 d1/d2 顺序取 <q 系数填满 â[256]
 *
 * UB 策略：SHAKE、d12、rej 共用 scratch，不经 GM 中转；仅 xof/d1/d2/â 在链末对拍写出。
 *
 * 与 golden 关系：output/{d1,d2,a_hat}.bin 与 scripts/gen_data.py 一致；xof 可选 dump。
 * 生产默认：F203_ALG7_REJ_IMPL=1, F203_ALG7_D12_GATHER=0, F203_ALG7_DUMP_XOF=0。
 */
#pragma once

#include "f203_alg7_config.h"
#include "f203_alg7_layout.h"
#include "f203_alg7_g.hpp"
#include "f203_alg7_shake_xof.hpp"
#include "f203_alg7_rej_scalar.hpp"

#if F203_ALG7_D12_GATHER
#include "f203_alg7_deinterleave_rom.h"
#endif

#if F203_ALG7_REJ_IMPL != F203_ALG7_REJ_SCALAR
#include "f203_alg7_rej_vec.hpp"
#endif

#include "kernel_operator.h"

namespace F203Alg7 {

/** 全管道同步屏障：向量 intrinsic 与标量段之间须刷新依赖。 */
#define F203_ALG7_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

// ---------- UB 尺寸编译期选型（随 D12_GATHER / REJ_IMPL 变化）----------

#if F203_ALG7_D12_GATHER
constexpr uint32_t kD12WsInt32Active = kD12WsGatherInt32;  // 8×224：含 Gather 索引 ROM 槽
#else
constexpr uint32_t kD12WsInt32Active = kD12WsScalarInt32;  // 5×224：c0,c1,c2,t0,t1
#endif
#if F203_ALG7_REJ_IMPL != F203_ALG7_REJ_SCALAR
constexpr uint32_t kRejScratchInt32 = kRejVecWsInt32;  // rej 向量 WS（见 layout.h）
#else
constexpr uint32_t kRejScratchInt32 = 0U;
#endif
/** scratchBuf 总 int32 元素数 = d12 工作区 + rej 工作区（复用同一块 UB）。 */
constexpr uint32_t kScratchInt32ElemsActive = kD12WsInt32Active + kRejScratchInt32;

// SHAKE 子系统 UB 分块尺寸（与 ShakeXofKernel 契约对齐）
constexpr uint32_t kShakeXUbBytes = 64U;   // 34B 消息 + 32B 硬件对齐
constexpr uint32_t kShakeStagingUbBytes = ShakeXofKernel::SHAKE_XOF_STAGING_BYTES;
constexpr uint32_t kShakeLenUbBytes = 32U; // 1×uint32 lengths + 对齐
#if F203_ALG7_D12_GATHER
/** Gather 路径：xof 每字节零扩展为 int32 expanded[672]，须 32B 对齐。 */
constexpr uint32_t kDeinterleaveExpandedUbBytes =
    ((kDeinterleaveExpandedLen * sizeof(int32_t) + 31U) / 32U) * 32U;
#endif

/**
 * d12 阶段 UB 工作区视图：将 scratch int32 缓冲切分为 c0/c1/c2 与临时 t0/t1。
 * Gather 实验路径额外持有 idxC0/C1/C2（解交织 ROM 拷入 UB）。
 */
struct Alg7D12VecWs {
    AscendC::LocalTensor<int32_t> c0;  // [kCandPairs] 候选字节 C0
    AscendC::LocalTensor<int32_t> c1;  // [kCandPairs] 候选字节 C1
    AscendC::LocalTensor<int32_t> c2;  // [kCandPairs] 候选字节 C2
    AscendC::LocalTensor<int32_t> t0;  // 向量 d12 临时 / rej 复用 d1Work
    AscendC::LocalTensor<int32_t> t1;  // 向量 d12 临时 / rej 复用 d2Work
#if F203_ALG7_D12_GATHER
    AscendC::LocalTensor<int32_t> idxC0;
    AscendC::LocalTensor<int32_t> idxC1;
    AscendC::LocalTensor<int32_t> idxC2;
#endif
};

/**
 * 将连续 scratch UB 绑定为 Alg7D12VecWs 各字段（平面 int32 切片，步长 kCandPairs）。
 * @param base scratchBuf 起始 uint8，内部 ReinterpretCast 为 int32
 */
__aicore__ inline void BindAlg7D12Ws(AscendC::LocalTensor<uint8_t> &base, Alg7D12VecWs &w)
{
    AscendC::LocalTensor<int32_t> baseI32 = base.ReinterpretCast<int32_t>();
    w.c0 = baseI32[0];
    w.c1 = baseI32[kCandPairs];
    w.c2 = baseI32[2U * kCandPairs];
    w.t0 = baseI32[3U * kCandPairs];
    w.t1 = baseI32[4U * kCandPairs];
#if F203_ALG7_D12_GATHER
    w.idxC0 = baseI32[5U * kCandPairs];
    w.idxC1 = baseI32[6U * kCandPairs];
    w.idxC2 = baseI32[7U * kCandPairs];
#endif
}

/**
 * 向量取低 bits 位：v ← v mod 2^bits（无除法 intrinsic 时用 ShiftRight+Muls+Sub 模拟）。
 * @param v     输入输出 UB int32[count]
 * @param tmp   临时 UB，与 v 同长
 * @param bits  保留低位数（d12 用 4：C1 mod 16）
 */
__aicore__ inline void MaskLowBitsI32(AscendC::LocalTensor<int32_t> &v, AscendC::LocalTensor<int32_t> &tmp, int32_t bits,
                                      uint32_t count)
{
    using AscendC::Muls;
    using AscendC::ShiftRight;
    using AscendC::Sub;
    const int32_t n = static_cast<int32_t>(count);
    const int32_t scale = static_cast<int32_t>(1) << bits;
    ShiftRight(tmp, v, bits, n);       // tmp = v >> bits
    Muls(tmp, tmp, scale, n);          // tmp = (v>>bits)*2^bits
    Sub(v, v, tmp, n);                 // v = v - tmp = v mod 2^bits
}

/**
 * 标量解交织：从 xof_ub 按 (C0,C1,C2) 三元组顺序拆出 224 组候选字节。
 * 索引：base = 3*c，xof[base], xof[base+1], xof[base+2]。
 * 用于 F203_ALG7_D12_GATHER=0 生产路径（Phase2 tick 优于 Gather）。
 */
__aicore__ inline void DeinterleaveCandScalarFromUb(const AscendC::LocalTensor<uint8_t> &xofUb, Alg7D12VecWs &w)
{
    for (uint32_t c = 0; c < kCandPairs; ++c) {
        const uint32_t base = 3U * c;
        w.c0.SetValue(c, static_cast<int32_t>(xofUb.GetValue(base)));
        w.c1.SetValue(c, static_cast<int32_t>(xofUb.GetValue(base + 1U)));
        w.c2.SetValue(c, static_cast<int32_t>(xofUb.GetValue(base + 2U)));
    }
    F203_ALG7_PIPE_ALL();
}

#if F203_ALG7_D12_GATHER

/**
 * Init 阶段：将 deinterleave ROM 常量拷入 UB 索引张量（每核一次，非热路径）。
 */
__aicore__ inline void InitAlg7DeinterleaveRomUb(Alg7D12VecWs &w)
{
    for (uint32_t i = 0U; i < kDeinterleaveRomLen; ++i) {
        w.idxC0.SetValue(i, kAlg7DeinterleaveC0Byte[i]);
        w.idxC1.SetValue(i, kAlg7DeinterleaveC1Byte[i]);
        w.idxC2.SetValue(i, kAlg7DeinterleaveC2Byte[i]);
    }
    F203_ALG7_PIPE_ALL();
}

/**
 * xof 每字节零扩展进 expanded[j]（int32），供 int32 Gather 按字节偏移索引。
 * expanded 长度 kDeinterleaveExpandedLen=672。
 */
__aicore__ inline void PackXofBytesToExpandedInt32(const AscendC::LocalTensor<uint8_t> &xofUb,
                                                     AscendC::LocalTensor<int32_t> &expanded)
{
    for (uint32_t i = 0U; i < kDeinterleaveExpandedLen; ++i) {
        expanded.SetValue(i, static_cast<int32_t>(xofUb.GetValue(i)));
    }
    F203_ALG7_PIPE_ALL();
}

/**
 * 向量解交织：expanded + 3×Gather(ROM 字节索引) → c0/c1/c2。
 * 实验路径 F203_ALG7_D12_GATHER=1。
 */
__aicore__ inline void DeinterleaveCandGatherFromUb(const AscendC::LocalTensor<uint8_t> &xofUb, Alg7D12VecWs &w,
                                                       AscendC::LocalTensor<int32_t> &expanded)
{
    using AscendC::Gather;

    PackXofBytesToExpandedInt32(xofUb, expanded);
    Gather(w.c0, expanded, w.idxC0.ReinterpretCast<uint32_t>(), 0U, kCandPairs);
    Gather(w.c1, expanded, w.idxC1.ReinterpretCast<uint32_t>(), 0U, kCandPairs);
    Gather(w.c2, expanded, w.idxC2.ReinterpretCast<uint32_t>(), 0U, kCandPairs);
    F203_ALG7_PIPE_ALL();
}

#endif  // F203_ALG7_D12_GATHER

/**
 * 解交织分发：标量 GetValue（默认）或 Gather+ROM（实验）。
 * @param expanded Gather 路径专用缓冲；标量路径可传占位张量
 */
__aicore__ inline void DeinterleaveCandFromUb(const AscendC::LocalTensor<uint8_t> &xofUb, Alg7D12VecWs &w,
                                               AscendC::LocalTensor<int32_t> &expanded)
{
#if F203_ALG7_D12_GATHER
    InitAlg7DeinterleaveRomUb(w);
    DeinterleaveCandGatherFromUb(xofUb, w, expanded);
#else
    (void)expanded;
    DeinterleaveCandScalarFromUb(xofUb, w);
#endif
}

/**
 * 链末对拍：xof_ub 整段 DataCopy 到 GM（仅 F203_ALG7_DUMP_XOF=1 时由上层调用）。
 */
__aicore__ inline void DumpXofUbToGm(const AscendC::LocalTensor<uint8_t> &xofUb, __gm__ uint8_t *xof_gm)
{
    AscendC::GlobalTensor<uint8_t> xofGm;
    xofGm.SetGlobalBuffer(xof_gm, kXofBytes);
    AscendC::DataCopy(xofGm, xofUb, kXofBytes);
    F203_ALG7_PIPE_ALL();
}

/**
 * Alg.7 line 7：向量计算 d1/d2[kCandPairs]。
 *
 * 公式（对每个候选 c）：
 *   d1 = C0 + 256 * (C1 mod 16)
 *   d2 = floor(C1/16) + 16 * C2
 *
 * @param w      已填充 c0/c1/c2 的工作区；t0/t1 作向量临时
 * @param d1Out  输出 UB int32[224]
 * @param d2Out  输出 UB int32[224]
 */
__aicore__ inline void ComputeD12Vec(Alg7D12VecWs &w, AscendC::LocalTensor<int32_t> &d1Out,
                                     AscendC::LocalTensor<int32_t> &d2Out)
{
    using AscendC::Add;
    using AscendC::Adds;
    using AscendC::Muls;
    using AscendC::ShiftRight;

    const int32_t n = static_cast<int32_t>(kCandPairs);

    // t0 = C1 mod 16
    Adds(w.t0, w.c1, static_cast<int32_t>(0), n);
    F203_ALG7_PIPE_ALL();
    MaskLowBitsI32(w.t0, w.t1, 4, kCandPairs);

    // d1 = C0 + 256 * (C1 mod 16)
    Muls(w.t1, w.t0, static_cast<int32_t>(256), n);
    Add(d1Out, w.c0, w.t1, n);
    F203_ALG7_PIPE_ALL();

    // d2 = (C1 >> 4) + 16 * C2
    ShiftRight(w.t0, w.c1, 4, n);
    Muls(w.t1, w.c2, static_cast<int32_t>(16), n);
    Add(d2Out, w.t0, w.t1, n);
    F203_ALG7_PIPE_ALL();
}

// 前向声明：全链与仅 d12 子链共用实现
__aicore__ inline void BuildAlg7SampleNttFromSeedD(uint32_t seed_d, uint8_t poly_j, uint8_t poly_i,
                                                   __gm__ uint8_t *xof_gm, __gm__ int32_t *d1_gm, __gm__ int32_t *d2_gm,
                                                   __gm__ int32_t *a_hat_gm);

/**
 * 仅 d1/d2 子链（a_hat_gm 传 nullptr 跳过 rej 与 â 写出）。
 * 用于分段调试或中间量对拍。
 */
__aicore__ inline void BuildAlg7D12FromSeedD(uint32_t seed_d, uint8_t poly_j, uint8_t poly_i, __gm__ uint8_t *xof_gm,
                                             __gm__ int32_t *d1_gm, __gm__ int32_t *d2_gm)
{
    BuildAlg7SampleNttFromSeedD(seed_d, poly_j, poly_i, xof_gm, d1_gm, d2_gm, nullptr);
}

/**
 * Alg.7 全链主函数：SEED_D + (j,i) → SHAKE → d1/d2 → rej → â[256]。
 *
 * @param seed_d   Host 传入 SEED_D
 * @param poly_j   矩阵下标 j
 * @param poly_i   矩阵下标 i
 * @param xof_gm   可选 GM 672B；nullptr 或 DUMP_XOF=0 时不写
 * @param d1_gm    GM int32[224] 对拍输出
 * @param d2_gm    GM int32[224] 对拍输出
 * @param a_hat_gm GM int32[256]；nullptr 时跳过 rej 与 â 落盘
 *
 * TPipe 资源：shake 四缓冲 + xof + d1/d2/aHat Que + scratch（d12+rej 复用）+ 可选 deinterleaveBuf。
 */
__aicore__ inline void BuildAlg7SampleNttFromSeedD(uint32_t seed_d, uint8_t poly_j, uint8_t poly_i,
                                                   __gm__ uint8_t *xof_gm, __gm__ int32_t *d1_gm, __gm__ int32_t *d2_gm,
                                                   __gm__ int32_t *a_hat_gm)
{
    // ---- Phase G：栈上派生 ρ（无 UB 分配）----
    uint8_t rho[kRhoBytes];
    BuildRhoFromSeedD(seed_d, rho);

    // ---- TPipe 初始化：单核内所有 UB/Que 一次申请 ----
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeXBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeLenBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> shakeStagingBuf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> xofBuf;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d1Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> d2Que;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> aHatQue;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratchBuf;
#if F203_ALG7_D12_GATHER
    AscendC::TBuf<AscendC::TPosition::VECCALC> deinterleaveBuf;
#endif
    pipe.InitBuffer(shakeXBuf, kShakeXUbBytes);
    pipe.InitBuffer(shakeLenBuf, kShakeLenUbBytes);
    pipe.InitBuffer(shakeStagingBuf, kShakeStagingUbBytes);
    pipe.InitBuffer(xofBuf, kXofUbBytes);
    pipe.InitBuffer(d1Que, 1, kD12Bytes);
    pipe.InitBuffer(d2Que, 1, kD12Bytes);
    pipe.InitBuffer(aHatQue, 1, kAHatBytes);
    pipe.InitBuffer(scratchBuf, kScratchInt32ElemsActive * sizeof(int32_t));
#if F203_ALG7_D12_GATHER
    pipe.InitBuffer(deinterleaveBuf, kDeinterleaveExpandedUbBytes);
#endif

    AscendC::LocalTensor<uint8_t> xUb = shakeXBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint32_t> lenUb = shakeLenBuf.Get<uint32_t>();
    AscendC::LocalTensor<uint8_t> shakeStagingUb = shakeStagingBuf.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> xofUb = xofBuf.Get<uint8_t>();

    // ---- Phase SHAKE：ρ||j||i → 672B xof_ub ----
    FillSampleSeedUb(rho, poly_j, poly_i, xUb, lenUb);
    RunShake128SampleNttUb(xUb, lenUb, xofUb, shakeStagingUb);
    F203_ALG7_PIPE_ALL();

    // ---- Phase 解交织 + d12：scratch 绑定工作区 ----
    AscendC::LocalTensor<uint8_t> scratchU8 = scratchBuf.Get<uint8_t>();
    Alg7D12VecWs ws{};
    BindAlg7D12Ws(scratchU8, ws);
#if F203_ALG7_D12_GATHER
    AscendC::LocalTensor<int32_t> expandedUb = deinterleaveBuf.Get<int32_t>();
    DeinterleaveCandFromUb(xofUb, ws, expandedUb);
#else
    AscendC::LocalTensor<int32_t> expandedUnused = scratchU8[0].ReinterpretCast<int32_t>();
    DeinterleaveCandFromUb(xofUb, ws, expandedUnused);
#endif

    AscendC::LocalTensor<int32_t> d1Local = d1Que.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> d2Local = d2Que.AllocTensor<int32_t>();
    ComputeD12Vec(ws, d1Local, d2Local);

    // ---- Phase rej → â（可选：a_hat_gm==nullptr 跳过）----
    if (a_hat_gm != nullptr) {
        AscendC::LocalTensor<int32_t> aLocal = aHatQue.AllocTensor<int32_t>();
        uint32_t filled = 0U;
#if F203_ALG7_REJ_IMPL != F203_ALG7_REJ_SCALAR
        {
            // 向量 rej：复用 ws.t0/t1 为 d1Work/d2Work，ws.c0 作 tmp；scratch 后半为 rej WS
            Alg7RejVecWs rejWs{};
            AscendC::LocalTensor<int32_t> rejScratch =
                scratchU8[kD12WsInt32Active * sizeof(int32_t)].ReinterpretCast<int32_t>();
            BindAlg7RejVecWs(rejScratch, rejWs);
            InitAlg7InterleaveRomUb(rejWs.idxRom);
            AscendC::LocalTensor<int32_t> d1Work = ws.t0;
            AscendC::LocalTensor<int32_t> d2Work = ws.t1;
            filled = RejVecBulkFromD12Ub(d1Local, d2Local, d1Work, d2Work, ws.c0, rejWs, aLocal);
        }
#else
        filled = RejScalarFromD12Ub(d1Local, d2Local, aLocal, kCandPairs);
#endif
        (void)filled;  // 672B 固定策略下应恒为 kKyberN

        // EnQue/DeQue 序列化后 DataCopy â → GM
        AscendC::GlobalTensor<int32_t> aHatGm;
        aHatGm.SetGlobalBuffer(a_hat_gm, kKyberN);
        aHatQue.EnQue(aLocal);
        aLocal = aHatQue.DeQue<int32_t>();
        F203_ALG7_PIPE_ALL();
        AscendC::DataCopy(aHatGm, aLocal, kKyberN);
        F203_ALG7_PIPE_ALL();
        aHatQue.FreeTensor(aLocal);
    }

    // ---- 链末：d1/d2 落 GM（对拍中间量）----
    AscendC::GlobalTensor<int32_t> d1OutGm;
    AscendC::GlobalTensor<int32_t> d2OutGm;
    d1OutGm.SetGlobalBuffer(d1_gm, kCandPairs);
    d2OutGm.SetGlobalBuffer(d2_gm, kCandPairs);

    d1Que.EnQue(d1Local);
    d1Local = d1Que.DeQue<int32_t>();
    F203_ALG7_PIPE_ALL();
    AscendC::DataCopy(d1OutGm, d1Local, kCandPairs);
    F203_ALG7_PIPE_ALL();
    d1Que.FreeTensor(d1Local);

    d2Que.EnQue(d2Local);
    d2Local = d2Que.DeQue<int32_t>();
    F203_ALG7_PIPE_ALL();
    AscendC::DataCopy(d2OutGm, d2Local, kCandPairs);
    F203_ALG7_PIPE_ALL();
    d2Que.FreeTensor(d2Local);

    // ---- 可选 xof GM dump ----
    if (xof_gm != nullptr) {
#if F203_ALG7_DUMP_XOF
        DumpXofUbToGm(xofUb, xof_gm);
#endif
    }
}

}  // namespace F203Alg7
