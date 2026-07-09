/**
 * @file f203_encrypt_at_r5_kernel.cpp
 * @brief G3 合并核 at_r5（FIPS 203 Alg.14 §18–§19 前置）：
 *        uTr[kP=5, kN] ← Σ_j matM[j, p] *_NTT rHat[j]，输出连续 [û | tr̂]。
 *
 * 背景（详 INTEGRATION_PLAN §2.3）：
 *   旧 G3 拆 4 核（g3_linear/g3_linear4/at_r/t_dot_r）+ 多 session 路径已证伪
 *   ——SIM CAModel AIV func_key ≥ 5 → 507000；多 session 会清空 binary cache。
 *   新方案：单核 at_r5 收完 4 个 û + 1 个 tr̂；
 *   host 端把 t̂ 当 Â 的「虚拟列 4」拼到 matM[(j*kP+p)*kN+c]：
 *     p ∈ [0..3]：matM[j,p,·] = Â[j,p,·]（aHat 自身布局）
 *     p = 4     ：matM[j,4,·] = t̂[j,·]
 *   设备算法 = at_r 同 innerproduct UB 流水，仅 kPOut 4→5。
 *
 * 内层循环顺序：外 j 内 p（与原 at_r 一致）。
 *   - 每个 r̂[j] 一次 DataCopy 复用 kP 次（减少 GM→UB 流量）
 *   - matM 在 GM 上「先 j 后 p」线性排布，外 j 内 p 时 fPoly 顺读，访存最优
 *
 * 与 at_r 区别：
 *   - 输入 aHat → matM；偏移 a_hat_offset_at(p,j) → mat_offset(j,p)（参数顺序对调）
 *   - kPOut 4 → kP 5；scratch outLine 段多 1*N int32
 *   - 输出 4 行 → 5 行，连续 DataCopy 写回（uTr[0..3]=û、uTr[4]=tr̂）
 */
#include "kernel_operator.h"

// SIM device link 每个 .o 必须自带 GM ROM 符号定义（不跨 .o 解析）。
// 旧路径 g3_linear.cpp 通过 unity-include alg11_rom_tables.cpp 持有 gAlg11*Gm；
// 本 TU 需自己持有同样符号，否则 SIM merge_aiv_obj_text 会 undefined（CPU build 反过来会 multiple definition）。
// 解法：把符号 rename 成 gAtR5*Gm（与 g3_linear.o 内符号互不冲突），
//      然后 include 同一份定义模板 alg11_rom_tables.cpp 即可。
#define gAlg11GammasGm                gAtR5GammasGm
#define gAlg11GatherEvenByteGm        gAtR5GatherEvenByteGm
#define gAlg11GatherOddByteGm         gAtR5GatherOddByteGm
#define gAlg11InterleaveReorderByteGm gAtR5InterleaveReorderByteGm
#include "alg11_rom_tables.cpp"

#include "f203_encrypt_at_r5_layout.h"
#include "f203_encrypt_at_r5_tiling.h"
#include "innerproduct_mod.hpp"   // hat_ip::mod_q_final_vec（与 at_r 共享）
#if defined(ASCENDC_CPU_DEBUG)
#include "f203_encrypt_at_r5_ub_scalar.hpp"
#endif
#include "multiply_ntts_ub.hpp"   // alg11_ub::compute_on_ub / init_rom_luts_ub（引用 gAlg11*Gm 被本 TU 宏改名为 gAtR5*Gm）

using namespace AscendC;

namespace {

constexpr int32_t kN = at_r5_tiling::kN;
constexpr int32_t kK = at_r5_tiling::kK;            // 求和维度 = 4
constexpr int32_t kP = at_r5_tiling::kP;            // 输出列数 = 5
constexpr int32_t kRomPairs = at_r5_tiling::kRomPairCount;
constexpr int32_t kUseCores = at_r5_tiling::kBlockDim;
constexpr int32_t kHatQ = at_r5_tiling::kHatQ;

/**
 * 单核 at_r5 op：复刻 at_r 的 KernelHatInnerProduct 结构，仅维度从 (kPOut, kSVec)
 * = (4, 4) 改为 (kP, kK) = (5, 4)。Init / ProcessFullPoly 行内不再展开注释，只标差异。
 */
class KernelAtR5 {
public:
    __aicore__ inline KernelAtR5() {}

    /** scratch 内取一段 int32 子张量（偏移单位 = int32）。 */
    __aicore__ inline LocalTensor<int32_t> bufI32(int32_t offInts, int32_t len)
    {
        return scratch_.GetWithOffset<int32_t>(static_cast<uint32_t>(len),
                                               static_cast<uint32_t>(offInts) * sizeof(int32_t));
    }

    /**
     * Init：绑定 GM 指针、分配 UB scratch/queue、加载 Alg.11 ROM LUT 一次。
     *
     * @param matM host 拼装的合并矩阵 [kK, kP, kN] int32（详见 layout.h::mat_offset）
     * @param rHat r̂ polyvec [kK, kN] int32
     * @param uTr  输出 [kP, kN] int32（uTr[0..3]=û、uTr[4]=tr̂）
     */
    __aicore__ inline void Init(GM_ADDR matM, GM_ADDR rHat, GM_ADDR uTr)
    {
        matGm_ = matM;
        rHatGm_ = rHat;
        uTrGm_ = uTr;

        // GM 张量绑定：matM 视为 kK*kP*kN 个 int32；rHat 为 kK*kN；输出 kP*kN。
        mGm_.SetGlobalBuffer((__gm__ int32_t *)matM, kK * kP * kN);
        rGm_.SetGlobalBuffer((__gm__ int32_t *)rHat, kK * kN);
        uGm_.SetGlobalBuffer((__gm__ int32_t *)uTr, kP * kN);

#if defined(ASCENDC_CPU_DEBUG)
        (void)pipe_;
        (void)scratch_;
#else
        // scratch = 8*N int32（acc/row/modT2/outLine 5 段，详 tiling.h）。
        pipe_.InitBuffer(scratch_, static_cast<uint32_t>(at_r5_tiling::kScratchInts * sizeof(int32_t)));
        pipe_.InitBuffer(wsQue_, 1, static_cast<uint32_t>(at_r5_tiling::kVecWsInts * sizeof(int32_t)));
        pipe_.InitBuffer(inQueueF_, 1, kN * sizeof(int32_t));   // f = matM[j, p] poly
        pipe_.InitBuffer(inQueueG_, 1, kN * sizeof(int32_t));   // g = rHat[j] poly
        pipe_.InitBuffer(gammaLutQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherEvenQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(gatherOddQue_, 1, kRomPairs * sizeof(int32_t));
        pipe_.InitBuffer(interleaveReorderQue_, 1, kN * sizeof(int32_t));

        // 与 at_r 同：一次性初始化 Alg.11 ROM LUT（gamma + 偶/奇 gather + interleave）。
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
     * ProcessFullPoly：执行 uTr[p] = Σ_j matM[j,p] *_NTT rHat[j]，p ∈ [0..kP-1]。
     *
     * 流水：
     *   外层 j ∈ [0..kK-1]：load r̂[j] 一次 → 内层 p ∈ [0..kP-1] 逐次 load matM[j,p]，basemul 累加到 outLine[p]。
     *   累加完成后对每行 line 做 Barrett final mod；最后 outLine 整体 DataCopy 到 uTrGm。
     */
    __aicore__ inline void ProcessFullPoly()
    {
        // 复用 scratch 中预分配的临时片段（位置定义见 tiling.h）。
        LocalTensor<int32_t> row = bufI32(at_r5_tiling::kOffRow, kN);
        LocalTensor<int32_t> modT2 = bufI32(at_r5_tiling::kOffModT2, kN);
        LocalTensor<int32_t> outLine = bufI32(at_r5_tiling::kOffOutLine, kP * kN);
        LocalTensor<int32_t> fLoc = bufI32(at_r5_tiling::kOffAcc, kN);

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

        // 外层 j 共用 r̂[j]：每个 j 加载 1 次 rPoly，复用 kP 次内层 basemul。
        for (int32_t j = 0; j < kK; ++j) {
            LocalTensor<int32_t> gPoly = inQueueG_.AllocTensor<int32_t>();
            DataCopy(gPoly, rGm_[at_r5_layout::r_hat_offset(j)], kN);
            inQueueG_.EnQue(gPoly);
            gPoly = inQueueG_.DeQue<int32_t>();

            for (int32_t p = 0; p < kP; ++p) {
                const uint32_t lineOff = static_cast<uint32_t>(p) * static_cast<uint32_t>(kN);
                LocalTensor<int32_t> lineP = outLine[lineOff];

                LocalTensor<int32_t> fPoly = inQueueF_.AllocTensor<int32_t>();
                // 关键差异点：matM 偏移按「先 j 后 p」线性存储，故 mat_offset(j, p) 而非 a_hat_offset_at(p, j)。
                DataCopy(fPoly, mGm_[at_r5_layout::mat_offset(j, p)], kN);
                inQueueF_.EnQue(fPoly);
                fPoly = inQueueF_.DeQue<int32_t>();

                // basemul：row = f *_NTT g（Alg.11，γ 来自 ROM LUT）
                alg11_ub::compute_on_ub(row, fPoly, gPoly, wsLocal, rom);
                inQueueF_.FreeTensor(fPoly);
                ALG11_PIPE_ALL();

                // 累加到 outLine[p]：j==0 首次直接覆盖（DataCopy 比 Add 省时）；后续 +=
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

        // 各行做 Barrett final mod_q（acc 值范围达 kK·(q-1)²，需折半到 [0, q)）。
        for (int32_t p = 0; p < kP; ++p) {
            LocalTensor<int32_t> lineP = outLine[static_cast<uint32_t>(p) * static_cast<uint32_t>(kN)];
#if defined(ASCENDC_CPU_DEBUG)
            hat_ip::mod_q_final_vec(lineP, kHatQ, kN);
#else
            hat_ip::mod_q_final_vec(lineP, kHatQ, fLoc, modT2, kN);
#endif
            ALG11_PIPE_ALL();
        }

        // 资源回收：重新 EnQue LUT（下次 launch 仍持有），释放 wsLocal。
        gammaLutQue_.EnQue(gammaLocal);
        gatherEvenQue_.EnQue(gatherEvenLocal);
        gatherOddQue_.EnQue(gatherOddLocal);
        interleaveReorderQue_.EnQue(interleaveLocal);
        wsQue_.FreeTensor(wsLocal);

        // 整段输出一次写回：[kP, kN] 连续，匹配 host D2H 拆分（uHat = uTr[0..3]、tr̂ = uTr[4]）。
        DataCopy(uGm_[0], outLine, kP * kN);
        ALG11_PIPE_MTE2();
    }

    /** Process 派发：CPU 走标量孪生；SIM/NPU 走 ProcessFullPoly。 */
    __aicore__ inline void Process()
    {
#if defined(ASCENDC_CPU_DEBUG)
        at_r5_scalar::at_r5_scalar_compute(matGm_, rHatGm_, uTrGm_);
#else
        ProcessFullPoly();
#endif
    }

private:
    GM_ADDR matGm_{nullptr};
    GM_ADDR rHatGm_{nullptr};
    GM_ADDR uTrGm_{nullptr};
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
    GlobalTensor<int32_t> mGm_;
    GlobalTensor<int32_t> rGm_;
    GlobalTensor<int32_t> uGm_;
};

} // namespace

/**
 * 设备入口：单核 AIV_ONLY；blockDim 由 host 传 1（与 at_r 一致；kBlockDim=1）。
 *
 * 注意：仍走 AIV_ONLY（不是 MIX_AIC_1_2）。本核位于 SIM AIV-only 配额（详 INTEGRATION_PLAN §4）。
 */
extern "C" __global__ __aicore__ void f203_encrypt_at_r5(GM_ADDR matM, GM_ADDR rHat, GM_ADDR uTr)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (GetBlockIdx() >= kUseCores) {
        return;
    }
    KernelAtR5 op;
    op.Init(matM, rHat, uTr);
    op.Process();
}

#ifndef __CCE_KT_TEST__
/** Host 直调入口（与 at_r 同模板，提供给 ICPU_RUN_KF / npu_lib 自动生成 aclrtlaunch 包装）。 */
void f203_encrypt_at_r5_do(uint32_t blockDim, void *l2ctrl, void *stream,
                           uint8_t *matM, uint8_t *rHat, uint8_t *uTr)
{
    f203_encrypt_at_r5<<<blockDim, l2ctrl, stream>>>(matM, rHat, uTr);
}
#endif
