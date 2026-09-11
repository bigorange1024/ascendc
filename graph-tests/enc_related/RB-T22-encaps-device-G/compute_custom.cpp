/**
 * @file compute_custom.cpp
 * @brief RB-T22 Launch2：握手 + μ←Decompress₁(m) + Decode₁₂→CBD(coins←设备G)→Â/ŷ→pack→c。
 *
 * 相对 T21：coins 已由 Launch1 设备 G 写入 OFF_COINS；本核 CBD/Encrypt 链不变。
 * 禁 Host 预喂最终 t̂/y/e/Â/ŷ/u/v/c/μ/coins/K：
 *   AIC：Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV：Set(1)→Wait(3) → μ←m → Decode₁₂ → CBD → Â←SampleNTT(ρ) → ŷ←NTT(y) → Mul → Set(4)
 *        → Set(1)→Wait(3) → INTT+噪(e1/e2 来自 CBD) → pack
 *
 * Flag：1/3 复用 + 4=GATE；永禁 5/7。
 * F203_AHAT16_BLOCK_DIM 须为 1（见 sample_ntt_device.hpp 硬锁）。
 */
// 在任何可能间接 include a_hat config 的头之前硬锁（防 NPU 半边 Â）
#ifdef F203_AHAT16_BLOCK_DIM
#undef F203_AHAT16_BLOCK_DIM
#endif
#define F203_AHAT16_BLOCK_DIM 1

#include "cbd_device.hpp"
#include "decode12_device.hpp"
#include "kernel_operator.h"
#include "light_cube.hpp"
#include "mu_embed_device.hpp"
#include "ntt_device_math.hpp"
#include "sample_ntt_device.hpp"
#include "tiling.h"
#include "uv_device_math.hpp"

namespace {

constexpr int32_t kDu = 11;
constexpr int32_t kDv = 5;
constexpr int32_t kC1PolyBytes = (tiling::kPolyN * kDu) / 8; // 352
constexpr int32_t kC2PolyBytes = (tiling::kPolyN * kDv) / 8; // 160
constexpr int32_t kC1Bytes = tiling::kK * kC1PolyBytes;      // 1408
constexpr int32_t kC2Bytes = kC2PolyBytes;                   // 160

} // namespace

__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

__aicore__ inline int32_t Compress5(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(tiling::kQ)) {
        x = static_cast<uint32_t>(tiling::kQ) - 1u;
    }
    const uint32_t d0 = x * 1290176u;
    return static_cast<int32_t>(((d0 + (1u << 26)) >> 27) & 0x1fu);
}

__aicore__ inline int32_t Compress11(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(tiling::kQ)) {
        x = static_cast<uint32_t>(tiling::kQ) - 1u;
    }
    uint64_t d0 = static_cast<uint64_t>(x) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<int32_t>(d0 & 0x7ffu);
}

__aicore__ inline void ByteEncode5(AscendC::LocalTensor<uint8_t> &out,
                                   const AscendC::LocalTensor<int32_t> &in, int32_t n)
{
    for (int32_t i = 0; i < n / 8; ++i) {
        uint8_t t[8];
        for (int32_t j = 0; j < 8; ++j) {
            t[j] = static_cast<uint8_t>(in.GetValue(static_cast<uint32_t>(8 * i + j)) & 0x1F);
        }
        const uint32_t base = static_cast<uint32_t>(i * 5);
        out.SetValue(base + 0, static_cast<uint8_t>(0xFF & ((t[0] >> 0) | (t[1] << 5))));
        out.SetValue(base + 1, static_cast<uint8_t>(0xFF & ((t[1] >> 3) | (t[2] << 2) | (t[3] << 7))));
        out.SetValue(base + 2, static_cast<uint8_t>(0xFF & ((t[3] >> 1) | (t[4] << 4))));
        out.SetValue(base + 3, static_cast<uint8_t>(0xFF & ((t[4] >> 4) | (t[5] << 1) | (t[6] << 6))));
        out.SetValue(base + 4, static_cast<uint8_t>(0xFF & ((t[6] >> 2) | (t[7] << 3))));
    }
}

__aicore__ inline void ByteEncode11(AscendC::LocalTensor<uint8_t> &out,
                                    const AscendC::LocalTensor<int32_t> &in, int32_t n)
{
    for (int32_t j = 0; j < n / 8; ++j) {
        uint16_t t[8];
        for (int32_t k = 0; k < 8; ++k) {
            t[k] = static_cast<uint16_t>(in.GetValue(static_cast<uint32_t>(8 * j + k)) & 0x7FF);
        }
        const uint32_t base = static_cast<uint32_t>(11 * j);
        out.SetValue(base + 0, static_cast<uint8_t>((t[0] >> 0) & 0xFF));
        out.SetValue(base + 1, static_cast<uint8_t>((t[0] >> 8) | ((t[1] << 3) & 0xFF)));
        out.SetValue(base + 2, static_cast<uint8_t>((t[1] >> 5) | ((t[2] << 6) & 0xFF)));
        out.SetValue(base + 3, static_cast<uint8_t>((t[2] >> 2) & 0xFF));
        out.SetValue(base + 4, static_cast<uint8_t>((t[2] >> 10) | ((t[3] << 1) & 0xFF)));
        out.SetValue(base + 5, static_cast<uint8_t>((t[3] >> 7) | ((t[4] << 4) & 0xFF)));
        out.SetValue(base + 6, static_cast<uint8_t>((t[4] >> 4) | ((t[5] << 7) & 0xFF)));
        out.SetValue(base + 7, static_cast<uint8_t>((t[5] >> 1) & 0xFF));
        out.SetValue(base + 8, static_cast<uint8_t>((t[5] >> 9) | ((t[6] << 2) & 0xFF)));
        out.SetValue(base + 9, static_cast<uint8_t>((t[6] >> 6) | ((t[7] << 5) & 0xFF)));
        out.SetValue(base + 10, static_cast<uint8_t>(t[7] >> 3));
    }
}

/** AIV0：u,v → Compress₁₁/₅ + ByteEncode → c[1568]。 */
__aicore__ inline void PackUvToC(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cWs, GM_ADDR cOut)
{
    using namespace tiling;
    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmCWs;
    AscendC::GlobalTensor<uint8_t> gmCOut;
    gmU.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(uGm),
                        static_cast<uint32_t>(kK * kPolyN));
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(vGm),
                        static_cast<uint32_t>(kPolyN));
    gmCWs.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cWs),
                          static_cast<uint32_t>(kCBytes));
    gmCOut.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(cOut),
                           static_cast<uint32_t>(kCBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queComp;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queBytes;
    constexpr uint32_t kPolyB = static_cast<uint32_t>(kPolyN) * sizeof(int32_t);
    pipe.InitBuffer(queIn, 1, kPolyB);
    pipe.InitBuffer(queComp, 1, kPolyB);
    pipe.InitBuffer(queBytes, 1, static_cast<uint32_t>(kC1PolyBytes));

    AscendC::LocalTensor<int32_t> inLocal = queIn.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> compLocal = queComp.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> encLocal = queBytes.AllocTensor<uint8_t>();

    for (int32_t p = 0; p < kK; ++p) {
        AscendC::DataCopy(inLocal, gmU[static_cast<uint32_t>(p * kPolyN)],
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            compLocal.SetValue(static_cast<uint32_t>(i),
                               Compress11(inLocal.GetValue(static_cast<uint32_t>(i))));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        ByteEncode11(encLocal, compLocal, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        const uint32_t off = static_cast<uint32_t>(p * kC1PolyBytes);
        AscendC::DataCopy(gmCWs[off], encLocal, static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmCOut[off], encLocal, static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    AscendC::DataCopy(inLocal, gmV, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kPolyN; ++i) {
        compLocal.SetValue(static_cast<uint32_t>(i),
                           Compress5(inLocal.GetValue(static_cast<uint32_t>(i))));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    ByteEncode5(encLocal, compLocal, kPolyN);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmCWs[static_cast<uint32_t>(kC1Bytes)], encLocal,
                      static_cast<uint32_t>(kC2Bytes));
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmCOut[static_cast<uint32_t>(kC1Bytes)], encLocal,
                      static_cast<uint32_t>(kC2Bytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    queIn.FreeTensor(inLocal);
    queComp.FreeTensor(compLocal);
    queBytes.FreeTensor(encLocal);
}

/**
 * MIX 核：KERNEL_TYPE_MIX_AIC_1_2；blockDim=1。
 * @param out  [out] 握手成功魔数
 * @param cOut [out] 密文 c[1568]
 * @param ws   [in/out] coins/ρ/ek/m/ζ/γ/mat；设备写 μ/t̂/yee/Â/ŷ/u/v/c
 */
extern "C" __global__ __aicore__ void compute_custom(GM_ADDR out, GM_ADDR cOut, GM_ADDR ws,
                                                     TilingData tiling)
{
    (void)tiling;
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    using namespace tiling;
    GM_ADDR traceGm = ws + OFF_TRACE;
    GM_ADDR matA = ws + OFF_MAT_A;
    GM_ADDR matB = ws + OFF_MAT_B;
    GM_ADDR matCNtt = ws + OFF_MAT_C_NTT;
    GM_ADDR matCIntt = ws + OFF_MAT_C_INTT;
    GM_ADDR uGm = ws + OFF_U;
    GM_ADDR vGm = ws + OFF_V;
    GM_ADDR cWs = ws + OFF_C;

    const bool isAic = (AscendC::GetSubBlockNum() == 1);
    const int32_t subIdx = static_cast<int32_t>(AscendC::GetSubBlockIdx());

    if (isAic) {
        rb_t15::LightCube cube;
        cube.Init();

        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);

        cube.Process(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);

        CrossWait(kFlagGate);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);

        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);

        cube.Process(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        CrossSet(kFlagAicDone);
    } else {
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);

            // ---------- T04 契约：设备 μ←Decompress₁(m)（禁 Host 预喂最终 μ）----------
            rb_t21::ComputeMuEmbedFromM(ws);
            TraceMark(traceGm, SLOT_AIV0_MU_DONE, MAGIC_AIV0_MU_DONE);

            // ---------- T18 契约：设备 ByteDecode₁₂(ek)→t̂（禁 Host 预喂）----------
            rb_t19::ComputeTHatDecode12(ws);
            TraceMark(traceGm, SLOT_AIV0_BD12_DONE, MAGIC_AIV0_BD12_DONE);

            // ---------- T16 契约：设备 CBD(coins)→y‖e1‖e2 ----------
            rb_t17::ComputeYeeCbd(ws);
            TraceMark(traceGm, SLOT_AIV0_CBD_DONE, MAGIC_AIV0_CBD_DONE);

            // ---------- T14：Â←SampleNTT(ρ)、ŷ←NTT(y) ----------
            rb_t15_ahat::ComputeAHatSampleNtt(ws);
            TraceMark(traceGm, SLOT_AIV0_AHAT_DONE, MAGIC_AIV0_AHAT_DONE);
            rb_t15_ntt::ComputeYHatNtt(ws);
            TraceMark(traceGm, SLOT_AIV0_YHAT_DONE, MAGIC_AIV0_YHAT_DONE);

            rb_t15::ComputeNttDomainUv(ws);
            TraceMark(traceGm, SLOT_AIV0_MUL_DONE, MAGIC_AIV0_MUL_DONE);
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(kFlagGate);

        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            // e1/e2 读自 CBD 区 OFF_E1/OFF_E2（YEE 别名）
            rb_t15::ComputeInttAddNoise(ws);
            TraceMark(traceGm, SLOT_AIV0_UV_DONE, MAGIC_AIV0_UV_DONE);

            PackUvToC(uGm, vGm, cWs, cOut);
            TraceMark(traceGm, SLOT_AIV0_PACK_DONE, MAGIC_AIV0_PACK_DONE);

            AscendC::PipeBarrier<PIPE_ALL>();
            __gm__ uint32_t *op = reinterpret_cast<__gm__ uint32_t *>(out);
            *op = MAGIC_OUT_OK;
            AscendC::PipeBarrier<PIPE_ALL>();
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_INTT, MAGIC_AIV1_POST_WAIT3_INTT);
        }
    }
}
