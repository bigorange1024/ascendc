// @probe exp-fips203-mlkem-pke-keygen-k3
// @file compute/hat_dot_halfrows_ub.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `hat_dot_halfrows_ub.hpp` 为该子模块组件。 / Component: hat_dot_halfrows_ub.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1184B) + dk_pke.bin (1152B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: hat_dot_layout.hpp, hat_dot_ub_tiling.hpp, innerproduct_mod.hpp, multiply_ntts_ub.hpp, tiling.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * 本文件在 KeyGen 流水线中的位置：Launch 2 行 18 hat 点积（Â∘ŝ）与相关 UB/tiling。
 * 对齐：FIPS 203 Alg.13 / ML-KEM-768（k=3）。
 * 与 golden 关系：仅 I/O 等价验收；禁止把 Host/参考源码当作 AscendC 实现规格。
 * 文件：compute/hat_dot_halfrows_ub.hpp
 */
/**
 * @file hat_dot_halfrows_ub.hpp
 * @brief 行 18 dot-only 独立瘦 TPipe 探针（legacy half-row 布局，**非生产路径**）。
 *
 * 用途：HatDotUbKernel 在 UB 内对 Â·ŝ 做内积（无 ê、无 ByteEncode），布局对齐 innerproduct-k4-halfrows。
 *
 * 调用方：仅调试/spike；当 integration_config.hpp 中 HAT_LINE18_FULLPOLY=1（v2 默认）时，
 *         生产路径走 `2s1e_post_ntt_ub.hpp` j→p compute_on_ub，**不**实例化本类。
 *
 * 不变量：每 AIV kPPerAiv=2；ŝ 直读 ub_ntt；Process(ub_ntt, ub_that) 不写 GM；独立 ROM Init。
 *
 * Golden：HAT_LINE18_DOT_ONLY=1 时对拍 golden_t_hat_dot.bin；verify_result checkpoint pass 4。
 *
 * CMake：HAT_LINE18_DOT_ONLY、HAT_LINE18_FULLPOLY、ALG11_*；本文件与 FULLPOLY=1 生产路径互斥。
 */
#pragma once

#include "hat_dot_layout.hpp"
#include "hat_dot_ub_tiling.hpp"
#include "innerproduct_mod.hpp"
#include "multiply_ntts_ub.hpp"
#include "tiling.h"

class HatDotUbKernel {
public:
    __aicore__ inline explicit HatDotUbKernel(int32_t subCoreIdx) : subCoreIdx_(subCoreIdx)
    {
        pBegin_ = static_cast<int32_t>(subCoreIdx_) * hat_dot_ub::kPPerAiv;
        pEnd_ = pBegin_ + hat_dot_ub::kPPerAiv;
    }

    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    /** 分配瘦 TPipe + ROM；CPU_DEBUG 跳过（Process 走标量环） */
    __aicore__ inline void InitPipe()
    {
#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        const int32_t kN = hat_dot_ub::kN;
        const int32_t kRomPairs = hat_dot_ub::kRomPairCount;
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(hat_dot_ub::kDotScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(hat_dot_ub::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, static_cast<uint32_t>(kRomPairs) * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, static_cast<uint32_t>(kRomPairs) * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, static_cast<uint32_t>(kRomPairs) * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, static_cast<uint32_t>(kN) * sizeof(int32_t));

        AscendC::LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
        AscendC::LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
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

    /** InitPipe 后：j→p 内积写 ub_that（逻辑同 stageHatDotOnly，独立 scratch 布局） */
    __aicore__ inline void Process(AscendC::LocalTensor<int32_t> &ub_ntt, AscendC::LocalTensor<int32_t> &ub_that,
                                   AscendC::GlobalTensor<int32_t> &gm_a, uint32_t coeffN)
    {
#if defined(ASCENDC_CPU_DEBUG)
        const int32_t kN = static_cast<int32_t>(coeffN);
        const int32_t kHatQ = hat_dot_ub::kHatQ;
        const int32_t kSVec = static_cast<int32_t>(tiling::kHatK);
        int32_t prod[256];
        int64_t acc[256];

        for (int32_t p = pBegin_; p < pEnd_; ++p) {
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] = 0;
            }
            for (int32_t j = 0; j < kSVec; ++j) {
                int32_t fBuf[256];
                int32_t gBuf[256];
                const uint32_t aOff = hat_dot_layout::a_hat_offset(static_cast<uint16_t>(p), static_cast<uint16_t>(j));
                const uint32_t sOff = static_cast<uint32_t>(j) * coeffN;
                for (int32_t c = 0; c < kN; ++c) {
                    fBuf[c] = gm_a.GetValue(aOff + static_cast<uint32_t>(c));
                    gBuf[c] = ub_ntt.GetValue(sOff + static_cast<uint32_t>(c));
                }
                alg11_ub::multiply_ntts_scalar(prod, fBuf, gBuf);
                for (int32_t c = 0; c < kN; ++c) {
                    acc[c] += static_cast<int64_t>(prod[c]);
                }
            }
            const uint32_t localP = (static_cast<uint32_t>(p) - static_cast<uint32_t>(pBegin_)) * coeffN;
            const int64_t q64 = static_cast<int64_t>(kHatQ);
            for (int32_t c = 0; c < kN; ++c) {
                int64_t rem = acc[c] % q64;
                if (rem < 0) {
                    rem += q64;
                }
                ub_that.SetValue(localP + static_cast<uint32_t>(c), static_cast<int32_t>(rem));
            }
        }
#else
        ProcessVec(ub_ntt, ub_that, gm_a, coeffN);
#endif
    }

#ifndef ASCENDC_CPU_DEBUG
private:
    /**
     * 本函数为 KeyGen 流水线组件 `ProcessVec`（详见 STATUS/customspec）。
     * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
     */
    __aicore__ inline void ProcessVec(AscendC::LocalTensor<int32_t> &ub_ntt, AscendC::LocalTensor<int32_t> &ub_that,
                                      AscendC::GlobalTensor<int32_t> &gm_a, uint32_t coeffN)
    {
        const int32_t kN = static_cast<int32_t>(coeffN);
        const int32_t kPPerAiv = hat_dot_ub::kPPerAiv;
        const int32_t kHatQ = hat_dot_ub::kHatQ;
        const int32_t kSVec = static_cast<int32_t>(tiling::kHatK);

        AscendC::LocalTensor<int32_t> row = bufI32(hat_dot_ub::kOffRow, kN);
        AscendC::LocalTensor<int32_t> modT2 = bufI32(hat_dot_ub::kOffModT2, kN);
        AscendC::LocalTensor<int32_t> outLine = bufI32(hat_dot_ub::kOffOutLine, kPPerAiv * kN);
        AscendC::LocalTensor<int32_t> fLoc = bufI32(hat_dot_ub::kOffFLoc, kN);

        AscendC::LocalTensor<int32_t> wsLocal = wsQue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> gammaLocal = gammaLutQue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.DeQue<int32_t>();
        AscendC::LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.DeQue<int32_t>();
        alg11_vec::RomUbLuts rom;
        rom.gammaV = gammaLocal;
        rom.gatherEvenByte = gatherEvenLocal;
        rom.gatherOddByte = gatherOddLocal;
        rom.interleaveReorderByte = interleaveLocal;

        for (int32_t j = 0; j < kSVec; ++j) {
            AscendC::LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            AscendC::DataCopy(gPoly, ub_ntt[static_cast<uint32_t>(j) * coeffN], static_cast<uint32_t>(kN));
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            for (int32_t p = pBegin_; p < pEnd_; ++p) {
                const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
                const uint32_t lineOff = localP * static_cast<uint32_t>(kN);
                AscendC::LocalTensor<int32_t> lineP = outLine[lineOff];

                AscendC::LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                AscendC::DataCopy(fPoly, gm_a[hat_dot_layout::a_hat_offset(static_cast<uint16_t>(p), static_cast<uint16_t>(j))],
                                 static_cast<uint32_t>(kN));
                inQueueF_.EnQue(fPoly);
                fPoly = inQueueF_.DeQue<int32_t>();

                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                ALG11_PIPE_ALL();

                if (j == 0) {
                    AscendC::DataCopy(lineP, row, kN);
                    ALG11_PIPE_MTE2();
                } else {
                    AscendC::Add(lineP, lineP, row, kN);
                    ALG11_PIPE_ALL();
                }
            }

            inQueueG_.FreeTensor(gPoly);
        }

        for (int32_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = static_cast<uint32_t>(p - pBegin_) * static_cast<uint32_t>(kN);
            AscendC::LocalTensor<int32_t> lineP = outLine[localP];
            hat_ip::mod_q_final_vec(lineP, kHatQ, fLoc, modT2, kN);
            ALG11_PIPE_ALL();
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        AscendC::DataCopy(ub_that, outLine, static_cast<uint32_t>(kPPerAiv) * coeffN);
        ALG11_PIPE_MTE2();
    }
#endif

    int32_t subCoreIdx_;
    int32_t pBegin_{0};
    int32_t pEnd_{0};
    AscendC::TPipe pipe_;
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> wsQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueF_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> inQueueG_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gammaLutQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gatherEvenQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> gatherOddQue_;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> interleaveReorderQue_;
    alg11_vec::RomUbLuts romUb_;
};

/**
 * MIX 调试：ŝ 从 dst GM 读（拆 kernel / preset 路径）。
 */
class KernelHatDotHalfRowsMmix {
public:
    __aicore__ inline KernelHatDotHalfRowsMmix(int32_t subCoreIdx) : subCoreIdx_(subCoreIdx) {}

    /** 独立 TPipe：ŝ 从 dst GM 读入（mixPass 分 kernel 调试，非 UB 融合路径） */
    __aicore__ inline void Init(GM_ADDR aHat, GM_ADDR dstDump, GM_ADDR tHat)
    {
        const int32_t kN = hat_dot_ub::kN;
        const int32_t kPOut = static_cast<int32_t>(tiling::kHatK);

        aHatGm_ = aHat;
        dstGm_ = dstDump;
        tHatGm_ = tHat;

        pBegin_ = static_cast<int32_t>(subCoreIdx_) * hat_dot_ub::kPPerAiv;
        pEnd_ = pBegin_ + hat_dot_ub::kPPerAiv;

        sDstOff_ = (subCoreIdx_ == 0) ? static_cast<uint32_t>(tiling::dstSOffAiv0) : static_cast<uint32_t>(tiling::dstSOffAiv1);

        aGm_.SetGlobalBuffer((__gm__ int32_t *)aHat, static_cast<uint32_t>(tiling::kHatKK) * static_cast<uint32_t>(kN));
        dstGmTensor_.SetGlobalBuffer((__gm__ int32_t *)dstDump, static_cast<uint32_t>(tiling::kDstPolys) * static_cast<uint32_t>(kN));
        tGm_.SetGlobalBuffer((__gm__ int32_t *)tHat, static_cast<uint32_t>(kPOut) * static_cast<uint32_t>(kN));
    }

    /**
     * 按开关跑 S3 → 行18 → 行19–20；可选从 dump 预设灌入。
     * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
     */
    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        const auto *aGm = reinterpret_cast<const __gm__ int32_t *>(aHatGm_);
        const auto *dstGm = reinterpret_cast<const __gm__ int32_t *>(dstGm_);
        auto *tGm = reinterpret_cast<__gm__ int32_t *>(tHatGm_);
        const int32_t kN = hat_dot_ub::kN;
        const int32_t kHatQ = hat_dot_ub::kHatQ;
        const int32_t kSVec = static_cast<int32_t>(tiling::kHatK);

        int32_t prod[hat_dot_ub::kN];
        int64_t acc[hat_dot_ub::kN];

        for (int32_t p = pBegin_; p < pEnd_; ++p) {
            for (int32_t c = 0; c < kN; ++c) {
                acc[c] = 0;
            }
            for (int32_t j = 0; j < kSVec; ++j) {
                const __gm__ int32_t *aPoly = aGm + hat_dot_layout::a_hat_offset(static_cast<uint16_t>(p), static_cast<uint16_t>(j));
                const __gm__ int32_t *sPoly =
                    dstGm + sDstOff_ * static_cast<uint32_t>(kN) + static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
                alg11_ub::multiply_ntts_scalar(prod, aPoly, sPoly);
                for (int32_t c = 0; c < kN; ++c) {
                    acc[c] += static_cast<int64_t>(prod[c]);
                }
            }
            for (int32_t c = 0; c < kN; ++c) {
                const int64_t q64 = static_cast<int64_t>(kHatQ);
                int64_t rem = acc[c] % q64;
                if (rem < 0) {
                    rem += q64;
                }
                tGm[p * kN + c] = static_cast<int32_t>(rem);
            }
        }
#else
        ProcessHalfRowsFromDst();
#endif
    }

private:
    /**
     * 本函数为 KeyGen 流水线组件 `ProcessHalfRowsFromDst`（详见 STATUS/customspec）。
     * 对齐 FIPS 203 Alg.13 / ML-KEM-768（k=3）；与 golden 仅 I/O 等价。
     */
    __aicore__ inline void ProcessHalfRowsFromDst()
    {
        HatDotUbKernel dot(subCoreIdx_);
        dot.InitPipe();
        const uint32_t coeffN = static_cast<uint32_t>(hat_dot_ub::kN);
        AscendC::TPipe tmpPipe;
        AscendC::TBuf<AscendC::TPosition::VECCALC> sBuf;
        AscendC::TBuf<AscendC::TPosition::VECCALC> tBuf;
        tmpPipe.InitBuffer(sBuf, static_cast<uint32_t>(tiling::kHatK) * coeffN * sizeof(int32_t));
        tmpPipe.InitBuffer(tBuf, static_cast<uint32_t>(hat_dot_ub::kPPerAiv) * coeffN * sizeof(int32_t));
        AscendC::LocalTensor<int32_t> ub_s = sBuf.GetWithOffset<int32_t>(static_cast<uint32_t>(tiling::kHatK) * coeffN, 0);
        AscendC::LocalTensor<int32_t> ub_t = tBuf.GetWithOffset<int32_t>(static_cast<uint32_t>(hat_dot_ub::kPPerAiv) * coeffN, 0);
        AscendC::DataCopy(ub_s, dstGmTensor_[sDstOff_ * coeffN], static_cast<uint32_t>(tiling::kHatK) * coeffN);
        ALG11_PIPE_MTE2();
        dot.Process(ub_s, ub_t, aGm_, coeffN);
        const uint32_t tOff = static_cast<uint32_t>(pBegin_) * coeffN;
        AscendC::DataCopy(tGm_[tOff], ub_t, static_cast<uint32_t>(hat_dot_ub::kPPerAiv) * coeffN);
        ALG11_PIPE_MTE2();
    }

    int32_t subCoreIdx_;
    int32_t pBegin_{0};
    int32_t pEnd_{0};
    uint32_t sDstOff_{0};
    GM_ADDR aHatGm_{nullptr};
    GM_ADDR dstGm_{nullptr};
    GM_ADDR tHatGm_{nullptr};
    AscendC::GlobalTensor<int32_t> aGm_;
    AscendC::GlobalTensor<int32_t> dstGmTensor_;
    AscendC::GlobalTensor<int32_t> tGm_;
};
