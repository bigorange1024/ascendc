/**
 * @file f203_decrypt_su_dot_kernel.cpp
 * @brief Decrypt 流水线（1-kernel fused）G3 独立入口：ŵ ← Σ_j MultiplyNTTs(ŝ[j], û[j])。
 *
 * 对齐 FIPS 203 Alg.15 行 6 的 NTT 域内积（单 poly 输出 ŵ[N]）：
 *   对 j=0..k-1 做 Alg.11/12 MultiplyNTTs(ŝ_j, û_j)，累加后 mod q。
 *
 * 本文件为**分段探针**独立 AIV kernel；生产融合路径走 `decrypt_g4::su_dot_impl`
 *（见 f203_decrypt_su_dot_impl.hpp），逻辑同构。
 * 探针：pass-fix-f203-alg15-pke-decrypt-device-k3（compute/su_dot）。
 *
 * golden I/O：输入中间态 ŝ/û（由 decode + NTT 产生）；输出 ŵ[N] int32（生产不落盘）。
 * CPU：标量 Barrett + MultiplyNTTs；SIM/NPU：向量 alg11_ub::compute_on_ub + hat_ip::mod_q_final_vec。
 */
#include "kernel_operator.h"

/* ROM 符号改名，避免与其它 TU 链接时 gAlg11* 重复定义 */
#define gAlg11GammasGm                gSuDotGammasGm
#define gAlg11GatherEvenByteGm        gSuDotGatherEvenByteGm
#define gAlg11GatherOddByteGm         gSuDotGatherOddByteGm
#define gAlg11InterleaveReorderByteGm gSuDotInterleaveReorderByteGm
#include "alg11_rom_tables.cpp"

#include "f203_decrypt_layout.h"
#include "innerproduct_mod.hpp"
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

namespace {

constexpr int32_t kN = static_cast<int32_t>(F203_DECRYPT_N);
constexpr int32_t kK = static_cast<int32_t>(F203_DECRYPT_K);
constexpr int32_t kHatQ = static_cast<int32_t>(F203_DECRYPT_Q);
/** Alg.11 系数对个数：N/2 = 128。 */
constexpr int32_t kRomPairs = kN / 2;
/** 向量工作区 int32 个数：8 条 lane × pairCount（与 alg11_tiling::kVecWsInts 一致）。 */
constexpr int32_t kVecWsInts = 8 * kRomPairs;
/** scratch：row / modT2 / accLine / fLoc 各 N，共 4N。 */
constexpr int32_t kScratchInts = 4 * kN;

#if defined(ASCENDC_CPU_DEBUG)
#include "alg11_gammas.h"

/**
 * Barrett 约化到 [0,q)：与 alg11_ub::barrett_red_coeff / 参考实现同构。
 * 两步乘加移位后做一次条件减 q。
 */
__aicore__ inline int32_t barrett_red(int32_t x)
{
    const int32_t q = kHatQ;
    int32_t t = x + (q & (x >> 31));
    int32_t t1 = static_cast<int32_t>((static_cast<int64_t>(t) * 78) >> 18);
    x = t - t1 * q;
    int32_t t2 = static_cast<int32_t>((static_cast<int64_t>(x) * 5039) >> 24);
    x = x - t2 * q;
    x = x - (q & ~((x - q) >> 31));
    return x;
}

/**
 * 标量 Alg.11 MultiplyNTTs：h ← f ⊙ g（NTT 域，按 γ 对做 basemul）。
 * @param h/f/g 各 [N]；每对 (f[2i],f[2i+1]) × (g[2i],g[2i+1]) 用 kAlg11Gammas[i]
 */
__aicore__ inline void multiply_ntts_scalar(int32_t *h, const int32_t *f, const int32_t *g)
{
    for (int32_t i = 0; i < kN / 2; ++i) {
        const int32_t gamma = kAlg11Gammas[i];
        const int32_t a0 = f[i * 2];
        const int32_t a1 = f[i * 2 + 1];
        const int32_t b0 = g[i * 2];
        const int32_t b1 = g[i * 2 + 1];
        const int32_t a1b1 = barrett_red(a1 * b1);
        h[i * 2] = barrett_red(a0 * b0 + a1b1 * gamma);
        h[i * 2 + 1] = barrett_red(a0 * b1 + a1 * b0);
    }
}

/**
 * CPU 孪生：ŵ = Σ_j MultiplyNTTs(ŝ_j, û_j) mod q，标量写 GM。
 */
__aicore__ inline void su_dot_scalar(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
{
    const auto *sGm = reinterpret_cast<const __gm__ int32_t *>(sHatGm);
    const auto *uGm = reinterpret_cast<const __gm__ int32_t *>(uHatGm);
    auto *wGm = reinterpret_cast<__gm__ int32_t *>(wHatGm);
    int32_t acc[kN];
    int32_t prod[kN];
    for (int32_t c = 0; c < kN; ++c) {
        acc[c] = 0;
    }
    for (int32_t j = 0; j < kK; ++j) {
        multiply_ntts_scalar(prod, sGm + j * kN, uGm + j * kN);
        for (int32_t c = 0; c < kN; ++c) {
            acc[c] += prod[c];
        }
    }
    /* 累加和可能 ≥ q；最终约化到 [0,q) */
    for (int32_t c = 0; c < kN; ++c) {
        int32_t x = acc[c];
        x %= kHatQ;
        if (x < 0) {
            x += kHatQ;
        }
        wGm[c] = x;
    }
}
#endif

/**
 * AIV 上 ŵ ← ⟨ŝ, û⟩ 的算子类。
 * Init：绑 GM、分配 UB（scratch / 输入队 / ROM LUT）；Process：k 次向量 MultiplyNTTs 累加 + final mod。
 */
class KernelSuDot {
public:
    __aicore__ inline KernelSuDot() {}

    /** scratch_ 上按 int32 偏移切 LocalTensor。 */
    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    /**
     * 绑定 ŝ/û/ŵ GM；设备路径预取 Alg.11 ROM（γ、Gather 偶/奇字节索引、interleave 重排）到 UB。
     */
    __aicore__ inline void Init(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
    {
        sAddr_ = sHatGm;
        uAddr_ = uHatGm;
        wAddr_ = wHatGm;
        sGm_.SetGlobalBuffer((__gm__ int32_t *)sHatGm, kK * kN);
        uGm_.SetGlobalBuffer((__gm__ int32_t *)uHatGm, kK * kN);
        wGm_.SetGlobalBuffer((__gm__ int32_t *)wHatGm, kN);
#if !defined(ASCENDC_CPU_DEBUG)
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kN * sizeof(int32_t));
        /* Init 阶段一次性把 ROM 拷进 UB，禁止 Compute 热路径 SetValue 填表 */
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
        romUb_.gatherEvenByte = gatherEvenLocal;
        romUb_.gatherOddByte = gatherOddLocal;
        romUb_.interleaveReorderByte = interleaveLocal;
        alg11_ub::init_rom_luts_ub(romUb_, kRomPairs);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
#endif
    }

    /**
     * 计算 ŵ 并写回 wGm_。
     * CPU：su_dot_scalar；设备：对每个 j DataCopy ŝ_j/û_j → compute_on_ub → 累加 → mod_q_final_vec。
     */
    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        su_dot_scalar(sAddr_, uAddr_, wAddr_);
#else
        /* scratch 分区：0:row  N:modT2  2N:accLine  3N:fLoc（mod 辅助） */
        LocalTensor<int32_t> row = bufI32(0, kN);
        LocalTensor<int32_t> modT2 = bufI32(kN, kN);
        LocalTensor<int32_t> accLine = bufI32(2 * kN, kN);
        LocalTensor<int32_t> fLoc = bufI32(3 * kN, kN);

        LocalTensor<int32_t> wsLocal = wsQue_.AllocTensor<int32_t>();
        LocalTensor<int32_t> gammaLocal = gammaLutQue_.DeQue<int32_t>();
        LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.DeQue<int32_t>();
        LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.DeQue<int32_t>();
        LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.DeQue<int32_t>();
        alg11_vec::RomUbLuts rom;
        rom.gammaV = gammaLocal;
        rom.gatherEvenByte = gatherEvenLocal;
        rom.gatherOddByte = gatherOddLocal;
        rom.interleaveReorderByte = interleaveLocal;

        for (int32_t j = 0; j < kK; ++j) {
            LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(fPoly, sGm_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)], kN);
            DataCopy(gPoly, uGm_[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)], kN);
            inQueueF_.EnQue(fPoly);
            inQueueG_.EnQue(gPoly);
            fPoly = inQueueF_.DeQue<int32_t>();
            gPoly = inQueueG_.DeQue<int32_t>();
            /* row ← MultiplyNTTs(ŝ_j, û_j)（向量 Alg.11） */
            alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
            inQueueF_.FreeTensor(fPoly);
            inQueueG_.FreeTensor(gPoly);
            if (j == 0) {
                DataCopy(accLine, row, kN);
            } else {
                Add(accLine, accLine, row, kN);
            }
        }
        /* 累加和 → [0,q)；写 ŵ */
        hat_ip::mod_q_final_vec(accLine, kHatQ, fLoc, modT2, kN);
        DataCopy(wGm_[0], accLine, kN);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);
#endif
    }

private:
    GM_ADDR sAddr_{nullptr};
    GM_ADDR uAddr_{nullptr};
    GM_ADDR wAddr_{nullptr};
    TPipe pipe_;
    TBuf<TPosition::VECCALC> scratch_;
    TQue<QuePosition::VECIN, 1> wsQue_;
    TQue<QuePosition::VECIN, 1> inQueueF_;
    TQue<QuePosition::VECIN, 1> inQueueG_;
    TQue<QuePosition::VECIN, 1> gammaLutQue_;
    TQue<QuePosition::VECIN, 1> gatherEvenQue_;
    TQue<QuePosition::VECIN, 1> gatherOddQue_;
    TQue<QuePosition::VECIN, 1> interleaveReorderQue_;
    alg11_vec::RomUbLuts romUb_;
    GlobalTensor<int32_t> sGm_;
    GlobalTensor<int32_t> uGm_;
    GlobalTensor<int32_t> wGm_;
};

} // namespace

/**
 * 独立 AIV kernel：ŝ + û → ŵ。
 * @param sHatGm ŝ [k×N]；@param uHatGm û [k×N]；@param wHatGm ŵ [N]
 * 前置：仅 blockIdx==0。
 */
extern "C" __global__ __aicore__ void f203_decrypt_su_dot(GM_ADDR sHatGm, GM_ADDR uHatGm, GM_ADDR wHatGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() != 0) {
        return;
    }
    KernelSuDot op;
    op.Init(sHatGm, uHatGm, wHatGm);
    op.Process();
}

#ifndef __CCE_KT_TEST__
/** Host 侧 launch 包装。 */
void f203_decrypt_su_dot_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *sHatGm, uint8_t *uHatGm,
                            uint8_t *wHatGm)
{
    f203_decrypt_su_dot<<<blockDim, l2ctrl, stream>>>(sHatGm, uHatGm, wHatGm);
}
#endif
