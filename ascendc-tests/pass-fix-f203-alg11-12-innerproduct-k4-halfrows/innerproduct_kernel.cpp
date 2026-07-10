/**
 * @file innerproduct_kernel.cpp
 * @brief 4×4×1 半行内积：GM 全量 a_hat / s_hat（alg13 行主序），双 AIV 各算 2 行 t̂[p]（无 ê）。
 *
 * 语义同单核内积：t̂[p] = mod_q( Σ_j MultiplyNTTs(Â[p,j], ŝ[j]) )。
 * 分核：blockIdx 决定 p ∈ [pBegin, pEnd)，pBegin = blockIdx * kPPerAiv。
 * CPU 孪生走 hat_ip::innerproduct_scalar_halfrows；设备走 ProcessHalfRows。
 */
#include "kernel_operator.h"
#if !defined(ASCENDC_CPU_DEBUG) && ALG11_MEM_OPS == 1
#include "alg11_rom_tables.cpp"
#endif
#include "innerproduct_layout.h"
#include "innerproduct_mod.hpp"
#include "innerproduct_tiling.h"
#if defined(ASCENDC_CPU_DEBUG)
#include "innerproduct_ub_scalar.hpp"
#endif
#include "multiply_ntts_ub.hpp"

using namespace AscendC;

constexpr int32_t kN = innerproduct_tiling::kN;
constexpr int32_t kPOut = innerproduct_tiling::kPOut;
constexpr int32_t kSVec = innerproduct_tiling::kSVec;
constexpr int32_t kPPerAiv = innerproduct_tiling::kPPerAiv;
constexpr int32_t kRomPairs = innerproduct_tiling::kRomPairCount;
constexpr int32_t kUseCores = innerproduct_tiling::kBlockDim;
constexpr int32_t kHatQ = innerproduct_tiling::kHatQ;

/**
 * 双 AIV 半行内积：每核只维护本地 outLine[kPPerAiv*N]，写回 t̂ 的对应行段。
 */
class KernelHatInnerProductHalfRows {
public:
    __aicore__ inline KernelHatInnerProductHalfRows() {}

    /**
     * 从 scratch_ 按 int32 偏移切出 LocalTensor。
     * @param offInts 相对 scratch 的 int32 下标
     * @param len     元素个数
     */
    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    /**
     * 绑定全量 GM，并按 blockIdx 计算本核负责的输出行区间 [pBegin_, pEnd_)。
     * 设备路径同时初始化 TPipe 与 ROM LUT。
     * @param aHat Â GM [P_OUT*S_VEC, N]
     * @param sHat ŝ GM [S_VEC, N]
     * @param tHat t̂ GM [P_OUT, N]
     */
    __aicore__ inline void Init(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat)
    {
        aHatGm_ = aHat;
        sHatGm_ = sHat;
        tHatGm_ = tHat;

        aGm_.SetGlobalBuffer((__gm__ int32_t *)aHat, kPOut * kSVec * kN);
        sGm_.SetGlobalBuffer((__gm__ int32_t *)sHat, kSVec * kN);
        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, kPOut * kN);

        // 本核输出行：AIV0 → [0,2)，AIV1 → [2,4)（kPPerAiv=2）
        pBegin_ = static_cast<int32_t>(GetBlockIdx()) * kPPerAiv;
        pEnd_ = pBegin_ + kPPerAiv;

#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(innerproduct_tiling::kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(innerproduct_tiling::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kN * sizeof(int32_t));

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
     * 设备/SIM：对本核 [pBegin,pEnd) 做 j→p MultiplyNTTs 累加、final mod，写回 t̂ 对应行段。
     */
    __aicore__ inline void ProcessHalfRows()
    {
        if (GetBlockIdx() >= kUseCores) {
            return;
        }

        // 本地只留 kPPerAiv 行累加缓冲（非整幅 P_OUT）
        LocalTensor<int32_t> row = bufI32(innerproduct_tiling::kOffRow, kN);
        LocalTensor<int32_t> modT2 = bufI32(innerproduct_tiling::kOffModT2, kN);
        LocalTensor<int32_t> outLine = bufI32(innerproduct_tiling::kOffOutLine, kPPerAiv * kN);
        LocalTensor<int32_t> fLoc = bufI32(innerproduct_tiling::kOffAcc, kN);

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

        // 外循环 j：ŝ[j] 本核搬一次，供本核负责的各 p 复用
        for (int32_t j = 0; j < kSVec; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, sGm_[innerproduct_layout::s_hat_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            // 仅遍历本核输出行
            for (int32_t p = pBegin_; p < pEnd_; ++p) {
                const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
                const uint32_t lineOff = localP * static_cast<uint32_t>(kN);
                LocalTensor<int32_t> lineP = outLine[lineOff];

                LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                DataCopy(fPoly, aGm_[innerproduct_layout::a_hat_offset(p, j)], kN);
                inQueueF_.EnQue(fPoly);
                fPoly = inQueueF_.DeQue<int32_t>();

                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                ALG11_PIPE_ALL();

                if (j == 0) {
                    DataCopy(lineP, row, kN);
                    ALG11_PIPE_MTE2();
                } else {
                    Add(lineP, lineP, row, kN);
                    ALG11_PIPE_ALL();
                }
            }

            inQueueG_.FreeTensor(gPoly);
        }

        // 本核各行 final mod
        for (int32_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
            LocalTensor<int32_t> lineP = outLine[localP * static_cast<uint32_t>(kN)];
#if defined(ASCENDC_CPU_DEBUG)
            hat_ip::mod_q_final_vec(lineP, kHatQ, kN);
#else
            hat_ip::mod_q_final_vec(lineP, kHatQ, fLoc, modT2, kN);
#endif
            ALG11_PIPE_ALL();
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        // 写回 t̂ 从 pBegin 起的连续 kPPerAiv 行
        const uint32_t tOff = static_cast<uint32_t>(pBegin_) * static_cast<uint32_t>(kN);
        DataCopy(tGm_[tOff], outLine, kPPerAiv * kN);
        ALG11_PIPE_MTE2();
    }

    /**
     * 入口：过滤多余核；CPU 孪生走标量半行，否则走向量半行。
     */
    __aicore__ inline void Process()
    {
        if (GetBlockIdx() >= kUseCores) {
            return;
        }
#if defined(ASCENDC_CPU_DEBUG)
        hat_ip::innerproduct_scalar_halfrows(aHatGm_, sHatGm_, tHatGm_, pBegin_, pEnd_);
#else
        ProcessHalfRows();
#endif
    }

private:
    GM_ADDR aHatGm_{nullptr};
    GM_ADDR sHatGm_{nullptr};
    GM_ADDR tHatGm_{nullptr};
    int32_t pBegin_{0};
    int32_t pEnd_{0};
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
    GlobalTensor<int32_t> aGm_;
    GlobalTensor<int32_t> sGm_;
    GlobalTensor<int32_t> tGm_;
};

/**
 * Kernel 入口：AIV-only，双核各写半幅 t̂。
 * @param aHat Â GM
 * @param sHat ŝ GM
 * @param tHat t̂ GM
 */
extern "C" __global__ __aicore__ void hat_innerproduct_halfrows_custom(GM_ADDR aHat, GM_ADDR sHat, GM_ADDR tHat)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    KernelHatInnerProductHalfRows op;
    op.Init(aHat, sHat, tHat);
    op.Process();
}

#ifndef __CCE_KT_TEST__
/**
 * Host launch 包装。
 */
void hat_innerproduct_halfrows_custom_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *aHat, uint8_t *sHat,
                                         uint8_t *tHat)
{
    hat_innerproduct_halfrows_custom<<<blockDim, l2ctrl, stream>>>(aHat, sHat, tHat);
}
#endif
