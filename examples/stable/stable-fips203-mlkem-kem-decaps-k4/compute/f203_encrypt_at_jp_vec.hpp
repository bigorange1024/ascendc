/**
 * @file f203_encrypt_at_jp_vec.hpp
 * @brief Alg.14 行 18 NTT 域内积：Âᵀ∘ŷ（4 列 û）+ 可选 tr̂（kP=5 第 5 行）。
 * 流水线：单 launch 内由 `f203_encrypt_l18_l19` 在 NTT(y) 后调用；输出驻留 UB 供 INTT k=8。
 * 对齐：pass-fix-f203-alg11-12-innerproduct-k4-halfrows 的 ProcessHalfRows；
 *       GM 索引用 encrypt_at_jp_layout（flat(j,p)，Encrypt 读 Âᵀ 索引）。
 * kP=5 pad→8 UB 布局（`ProcessToUbMaybeTrHat(..., unifiedUTrPad8=true)`）：
 *   AIV0 dstUb[4,N]: [uTr0, uTr1, uTr4(tr̂), 0]
 *   AIV1 dstUb[4,N]: [uTr2, uTr3, 0, 0]
 * INTT 后 scatter：行 0–3→u+e₁；行 4 时域→v+e₂。
 * 约束：û/tr̂ 主路径 DataCopy 写 UB；禁止 SetValue 写 GM 再 MTE 读回（SIM 不可见）。
 *
 * 流水线位置：FIPS 203 Alg.14 / ML-KEM-1024（k=4）K-PKE.Encrypt；本文件属 stable-fips203-mlkem-pke-encrypt-k4。
 * 与 golden：最终对拍 output/c.bin（中间态默认不落盘）。
 */
#pragma once

#include "f203_encrypt_at_jp_layout.hpp"
#include "f203_encrypt_at_jp_mod.hpp"
#include "f203_encrypt_at_jp_tiling.h"
#include "f203_l18_l19_tiling.h"
#include "kernel_operator.h"
#include "multiply_ntts_ub.hpp"

namespace encrypt_at_jp {

class EncryptAtJpHalfRowsVec {
public:
    __aicore__ inline EncryptAtJpHalfRowsVec() {}

    __aicore__ inline AscendC::LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    __aicore__ inline void Init(GM_ADDR aHat, GM_ADDR yHat, int32_t pBegin, int32_t pEnd)
    {
        constexpr int32_t kN = encrypt_at_jp_tiling::kN;
        constexpr int32_t kK = encrypt_at_jp_tiling::kK;
        aHatGm_ = aHat;
        yHatGm_ = yHat;
        pBegin_ = pBegin;
        pEnd_ = pEnd;

        // aHat 允许为空（例如仅计算 tr_hat_ntt 的路径），此时禁止访问 aGm_。
        if (aHat != nullptr) {
            aGm_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(aHat), kK * kK * kN);
        }
        yGm_.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(yHat), kK * kN);

        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(encrypt_at_jp_tiling::kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(encrypt_at_jp_tiling::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(inQueueG_, 1, kN * sizeof(int32_t));
        pipe_.InitBuffer(gammaLutQue_, 1, encrypt_at_jp_tiling::kRomPairCount * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, encrypt_at_jp_tiling::kRomPairCount * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, encrypt_at_jp_tiling::kRomPairCount * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kN * sizeof(int32_t));

        AscendC::LocalTensor<int32_t> gammaLocal = gammaLutQue_.AllocTensor<int32_t>();
        romUb_.gammaV = gammaLocal;
        AscendC::LocalTensor<int32_t> gatherEvenLocal = gatherEvenQue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> gatherOddLocal = gatherOddQue_.AllocTensor<int32_t>();
        AscendC::LocalTensor<int32_t> interleaveLocal = interleaveReorderQue_.AllocTensor<int32_t>();
        romUb_.gatherEvenByte = gatherEvenLocal;
        romUb_.gatherOddByte = gatherOddLocal;
        romUb_.interleaveReorderByte = interleaveLocal;
        alg11_ub::init_rom_luts_ub(romUb_, encrypt_at_jp_tiling::kRomPairCount);
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
    }

    /** 行 2 decode_t_hat 的 UB 驻留区（长度 kK*kN），与本类 pipe_ 同寿命，避免被其它 TPipe 复写。 */
    __aicore__ inline AscendC::LocalTensor<int32_t> THatUb()
    {
        return bufI32(encrypt_at_jp_tiling::kOffTHatUb, encrypt_at_jp_tiling::kK * encrypt_at_jp_tiling::kN);
    }

#if F203_BYTE_DECODE12_IMPL >= 1
    /** ByteDecode₁₂ Alg7 备用路径工作区（仅 F203_BYTE_DECODE12_IMPL=1 时分配）。 */
    __aicore__ inline AscendC::LocalTensor<int32_t> Decode12WsUb()
    {
        return bufI32(encrypt_at_jp_tiling::kOffDecode12Ws, encrypt_at_jp_tiling::kDecode12WsInts);
    }
#endif

    /** 向量内积主循环：û[p] 累加在 UB outLine，final mod 后 DataCopy 到 dstUb（布局 localP*N）。 */
    __aicore__ inline void ProcessToUb(AscendC::LocalTensor<int32_t> &dstUb)
    {
        ProcessToUbMaybeTrHat(dstUb, /*tHat*/ nullptr, /*trHatNtt*/ nullptr, /*doTrHat*/ false, /*tHatUbOpt*/ nullptr);
    }

    /**
     * halfrows 内积（û）+ 可选追加 tr_hat（kP=5 统一 uTr pad→8）。
     *
     * @param unifiedUTrPad8 true：dstUb 为 [kInttPolysPerAiv,kN]；AIV0 写 [û0,û1,tr̂,0]，AIV1 写 [û2,û3,0,0]。
     */
    __aicore__ inline void ProcessToUbMaybeTrHat(AscendC::LocalTensor<int32_t> &dstUb, GM_ADDR tHat,
                                                GM_ADDR trHatNtt, bool doTrHat,
                                                const AscendC::LocalTensor<int32_t> *tHatUbOpt,
                                                bool unifiedUTrPad8 = false)
    {
        constexpr int32_t kN = encrypt_at_jp_tiling::kN;
        constexpr int32_t kK = encrypt_at_jp_tiling::kK;
        constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;
        AscendC::LocalTensor<int32_t> row = bufI32(encrypt_at_jp_tiling::kOffRow, kN);
        AscendC::LocalTensor<int32_t> modT2 = bufI32(encrypt_at_jp_tiling::kOffModT2, kN);
        AscendC::LocalTensor<int32_t> outLine = bufI32(encrypt_at_jp_tiling::kOffOutLine, encrypt_at_jp_tiling::kPPerAiv * kN);
        AscendC::LocalTensor<int32_t> trLine = bufI32(encrypt_at_jp_tiling::kOffTrLine, kN);
        AscendC::LocalTensor<int32_t> fLoc = bufI32(encrypt_at_jp_tiling::kOffAcc, kN);

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

        AscendC::GlobalTensor<int32_t> tGm;
        AscendC::GlobalTensor<int32_t> trGm;
        if (doTrHat && trHatNtt != nullptr && (tHatUbOpt != nullptr || tHat != nullptr)) {
            if (tHat != nullptr) {
                tGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(tHat), kK * kN);
            }
            trGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(trHatNtt), kN);
        } else {
            doTrHat = false;
        }

        for (int32_t j = 0; j < kK; ++j) {
            AscendC::LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            AscendC::DataCopy(gPoly, yGm_[encrypt_at_jp_layout::y_hat_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            for (int32_t p = pBegin_; p < pEnd_; ++p) {
                const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
                const uint32_t lineOff = localP * static_cast<uint32_t>(kN);
                AscendC::LocalTensor<int32_t> lineP = outLine[lineOff];

                AscendC::LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                AscendC::DataCopy(fPoly, aGm_[encrypt_at_jp_layout::a_hat_offset_jp(j, p)], kN);
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

            if (doTrHat) {
                // 优先走 UB：tHatUbOpt 持有 [kK,kN] 连续 int32（行 2 ByteDecode₁₂ 的中间结果驻留 UB）。
                // 若为空则回退 GM 读（仍禁止 DataCopy 读 GM，以免 SIM 可见性问题）。
                AscendC::LocalTensor<int32_t> fPolyT = inQueueF_.AllocTensor<int32_t>();
                if (tHatUbOpt != nullptr) {
                    AscendC::LocalTensor<int32_t> tRow =
                        (*tHatUbOpt)[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)];
                    AscendC::DataCopy(fPolyT, tRow, static_cast<uint32_t>(kN));
                } else {
                    const __gm__ int32_t *tPtr = reinterpret_cast<const __gm__ int32_t *>(tHat);
                    const uint32_t base = static_cast<uint32_t>(j) * static_cast<uint32_t>(kN);
                    for (int32_t c = 0; c < kN; ++c) {
                        fPolyT.SetValue(static_cast<uint32_t>(c), tPtr[base + static_cast<uint32_t>(c)]);
                    }
                }
                AscendC::PipeBarrier<PIPE_ALL>();
                alg11_ub::compute_on_ub(row, fPolyT, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPolyT);
                ALG11_PIPE_ALL();

                if (j == 0) {
                    AscendC::DataCopy(trLine, row, kN);
                    ALG11_PIPE_MTE2();
                } else {
                    AscendC::Add(trLine, trLine, row, kN);
                    ALG11_PIPE_ALL();
                }
            }

            inQueueG_.FreeTensor(gPoly);
        }

        for (int32_t p = pBegin_; p < pEnd_; ++p) {
            const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
            AscendC::LocalTensor<int32_t> lineP = outLine[localP * static_cast<uint32_t>(kN)];
            mod_q_final_vec(lineP, kQ, fLoc, modT2, kN);
            ALG11_PIPE_ALL();
        }

        if (doTrHat) {
            // trLine 可能含负值（alg11 basemul 结果为有符号代表），而共享库 Barrett wrap 末步假设输入非负。
            // 这里先做 2 次「若负则 +q」的归一化，把范围推回到 [0, 2q) 再做 Barrett final mod。
            {
                auto &tr_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&trLine);
                auto &mask_u32 = *reinterpret_cast<AscendC::LocalTensor<uint32_t> *>(&fLoc);
                AscendC::ShiftRight(mask_u32, tr_u32, 31U, kN); // 负数→1，非负→0
                AscendC::Muls(modT2, fLoc, kQ, kN);
                AscendC::Add(trLine, trLine, modT2, kN);
                AscendC::PipeBarrier<PIPE_ALL>();
                AscendC::ShiftRight(mask_u32, tr_u32, 31U, kN);
                AscendC::Muls(modT2, fLoc, kQ, kN);
                AscendC::Add(trLine, trLine, modT2, kN);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
            mod_q_final_vec(trLine, kQ, fLoc, modT2, kN);
            ALG11_PIPE_ALL();
            if (!unifiedUTrPad8 && trHatNtt != nullptr) {
                AscendC::DataCopy(trGm[0], trLine, kN);
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        if (unifiedUTrPad8) {
            constexpr int32_t kRows = static_cast<int32_t>(tiling::kInttPolysPerAiv);
            for (int32_t lp = 0; lp < kRows; ++lp) {
                AscendC::LocalTensor<int32_t> dstRow = dstUb[static_cast<uint32_t>(lp) * static_cast<uint32_t>(kN)];
                if (lp < (pEnd_ - pBegin_)) {
                    const uint32_t srcOff = static_cast<uint32_t>(lp) * static_cast<uint32_t>(kN);
                    AscendC::DataCopy(dstRow, outLine[srcOff], static_cast<uint32_t>(kN));
                } else if (doTrHat && lp == 2) {
                    AscendC::DataCopy(dstRow, trLine, static_cast<uint32_t>(kN));
                } else {
                    AscendC::Duplicate(dstRow, 0, static_cast<uint32_t>(kN));
                }
                AscendC::PipeBarrier<PIPE_ALL>();
            }
        } else {
            const uint32_t elemCount = static_cast<uint32_t>(pEnd_ - pBegin_) * static_cast<uint32_t>(kN);
            AscendC::DataCopy(dstUb, outLine, elemCount);
            AscendC::PipeBarrier<PIPE_ALL>();
        }
    }

    /** 3 launch 路径：final mod 后 DataCopy 写 uNtt GM。 */
    __aicore__ inline void ProcessToGm(GM_ADDR uNtt)
    {
        constexpr int32_t kN = encrypt_at_jp_tiling::kN;
        constexpr int32_t kK = encrypt_at_jp_tiling::kK;
        constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;
        AscendC::GlobalTensor<int32_t> uGm;
        uGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uNtt));

        AscendC::LocalTensor<int32_t> row = bufI32(encrypt_at_jp_tiling::kOffRow, kN);
        AscendC::LocalTensor<int32_t> modT2 = bufI32(encrypt_at_jp_tiling::kOffModT2, kN);
        AscendC::LocalTensor<int32_t> outLine = bufI32(encrypt_at_jp_tiling::kOffOutLine, encrypt_at_jp_tiling::kPPerAiv * kN);
        AscendC::LocalTensor<int32_t> fLoc = bufI32(encrypt_at_jp_tiling::kOffAcc, kN);

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

        for (int32_t j = 0; j < kK; ++j) {
            AscendC::LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            AscendC::DataCopy(gPoly, yGm_[encrypt_at_jp_layout::y_hat_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            for (int32_t p = pBegin_; p < pEnd_; ++p) {
                const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
                const uint32_t lineOff = localP * static_cast<uint32_t>(kN);
                AscendC::LocalTensor<int32_t> lineP = outLine[lineOff];

                AscendC::LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                AscendC::DataCopy(fPoly, aGm_[encrypt_at_jp_layout::a_hat_offset_jp(j, p)], kN);
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
            const uint32_t localP = static_cast<uint32_t>(p - pBegin_);
            AscendC::LocalTensor<int32_t> lineP = outLine[localP * static_cast<uint32_t>(kN)];
            mod_q_final_vec(lineP, kQ, fLoc, modT2, kN);
            ALG11_PIPE_ALL();
        }

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        const uint32_t dstOff = static_cast<uint32_t>(pBegin_) * static_cast<uint32_t>(kN);
        const uint32_t elemCount = static_cast<uint32_t>(pEnd_ - pBegin_) * static_cast<uint32_t>(kN);
        AscendC::DataCopy(uGm[dstOff], outLine, elemCount);
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    /**
     * kP=5 融合预备：计算 tr_hat_ntt = Σ_j MultiplyNTTs(t_hat[j], y_hat[j]) mod q，并写 GM。
     *
     * - 输入 tHat: [kK,kN] int32（GM）；yHat: 已在 Init() 绑定
     * - 输出 trHatNtt: [kN] int32（GM，单 poly）
     *
     * 约束：复用本类 scratch（row/mod/outLine 等）；只写 1 行，不占用 halfrows outLine 布局。
     */
    __aicore__ inline void ProcessTrHatToGm(GM_ADDR tHat, GM_ADDR trHatNtt)
    {
        constexpr int32_t kN = encrypt_at_jp_tiling::kN;
        constexpr int32_t kK = encrypt_at_jp_tiling::kK;
        constexpr int32_t kQ = encrypt_at_jp_tiling::kHatQ;

        AscendC::GlobalTensor<int32_t> tGm;
        tGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(tHat), kK * kN);
        AscendC::GlobalTensor<int32_t> outGm;
        outGm.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(trHatNtt), kN);

        AscendC::LocalTensor<int32_t> row = bufI32(encrypt_at_jp_tiling::kOffRow, kN);
        AscendC::LocalTensor<int32_t> modT2 = bufI32(encrypt_at_jp_tiling::kOffModT2, kN);
        // 用 outLine 的第 0 行作为累加缓冲（长度 kN）
        AscendC::LocalTensor<int32_t> lineP = bufI32(encrypt_at_jp_tiling::kOffOutLine, kN);
        AscendC::LocalTensor<int32_t> fLoc = bufI32(encrypt_at_jp_tiling::kOffAcc, kN);

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

        for (int32_t j = 0; j < kK; ++j) {
            AscendC::LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            AscendC::DataCopy(gPoly, yGm_[encrypt_at_jp_layout::y_hat_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            AscendC::LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
            AscendC::DataCopy(fPoly, tGm[static_cast<uint32_t>(j) * static_cast<uint32_t>(kN)], kN);
            inQueueF_.EnQue(fPoly);
            fPoly = inQueueF_.DeQue<int32_t>();

            alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
            inQueueF_.FreeTensor(fPoly);
            inQueueG_.FreeTensor(gPoly);
            ALG11_PIPE_ALL();

            if (j == 0) {
                AscendC::DataCopy(lineP, row, kN);
                ALG11_PIPE_MTE2();
            } else {
                AscendC::Add(lineP, lineP, row, kN);
                ALG11_PIPE_ALL();
            }
        }

        mod_q_final_vec(lineP, kQ, fLoc, modT2, kN);
        ALG11_PIPE_ALL();
        AscendC::DataCopy(outGm[0], lineP, kN);
        AscendC::PipeBarrier<PIPE_ALL>();

        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);
    }

private:
    GM_ADDR aHatGm_{nullptr};
    GM_ADDR yHatGm_{nullptr};
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
    AscendC::GlobalTensor<int32_t> aGm_;
    AscendC::GlobalTensor<int32_t> yGm_;
};

} // namespace encrypt_at_jp
