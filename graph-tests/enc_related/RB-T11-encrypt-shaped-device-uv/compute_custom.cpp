/**
 * @file compute_custom.cpp
 * @brief RB-T11 Launch2：T03 握手 + T10 设备算 u,v + T06 pack→c。
 *
 * 生产 Encrypt 拼装外形（设备拓扑；禁 Host 预喂最终 u,v）：
 *   AIC：Wait(1)→CubeNTT→Set(3) → Wait(4) → Wait(1)→CubeINTT→Set(3)
 *   AIV：Set(1)→Wait(3) → Mul → Set(4) → Set(1)→Wait(3) → INTT+噪 → pack(u,v→c)
 *
 * 背景：T09 外形但 Host 预喂 u,v；本刀接 T10 真拓扑后 pack。
 * 结论：flag 1/3 复用、4=GATE；永禁 5/7；单库双核第二 launch。
 * 模式：CrossCore modeId=0x2；Host SynchronizeStream 夹在两 launch 之间。
 *
 * 若挂死 TRACE 假设：
 *   - 无 POST_WAIT3_NTT → 卡 NTT 握手
 *   - 有 POST_WAIT3_NTT 无 MUL_DONE → 卡 MultiplyNTTs
 *   - 有 SET4 无 INTT → 卡 GATE
 *   - 有 PRE_SET1_INTT 无 POST_WAIT3_INTT → 卡 INTT 复用
 *   - 有 UV_DONE 无 PACK_DONE → 卡 pack
 */
#include "kernel_operator.h"
#include "light_cube.hpp"
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

/** TRACE 标量写。 */
__aicore__ inline void TraceMark(GM_ADDR traceGm, uint32_t slot, uint32_t magic)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    __gm__ uint32_t *p = reinterpret_cast<__gm__ uint32_t *>(traceGm);
    *(p + slot) = magic;
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Wait：mode 0x2，PIPE_MTE2。 */
__aicore__ inline void CrossWait(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreWaitFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** CrossCore Set：mode 0x2，PIPE_MTE2。 */
__aicore__ inline void CrossSet(uint16_t flagId)
{
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::CrossCoreSetFlag<0x2, PIPE_MTE2>(flagId);
    AscendC::PipeBarrier<PIPE_ALL>();
}

/** Compress_5：Barrett 标量（与 T06 同式，仅 I/O）。 */
__aicore__ inline int32_t Compress5(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(tiling::kQ)) {
        x = static_cast<uint32_t>(tiling::kQ) - 1u;
    }
    const uint32_t d0 = x * 1290176u;
    return static_cast<int32_t>(((d0 + (1u << 26)) >> 27) & 0x1fu);
}

/** Compress_11：u64 Barrett 标量。 */
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

/** ByteEncode_5：8 系数×5bit → 5B。 */
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

/** ByteEncode_11：8 系数×11bit → 11B。 */
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

/**
 * AIV0：设备算出的 u,v → Compress₁₁/₅ + ByteEncode → c[1568]（T06 契约）。
 * 同时写 ws 内 OFF_C 与独立 cOut。
 */
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

    // c₁：逐 poly Compress₁₁ + ByteEncode₁₁
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

    // c₂：Compress₅ + ByteEncode₅
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
 * MIX 核入口：KERNEL_TYPE_MIX_AIC_1_2；blockDim=1。
 * @param out    [out] 握手成功魔数（AIV0 写 4B）
 * @param cOut   [out] 密文 c[1568]（pack 段写出）
 * @param ws     [in/out] 拓扑预喂 + mat + TRACE；设备写 û/v̂/u/v/c
 * @param tiling [in]  占位
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
        rb_t11::LightCube cube;
        cube.Init();

        // ---------- 段1 NTT：Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_NTT, MAGIC_AIC_POST_WAIT1_NTT);

        cube.Process(matCNtt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_NTT, MAGIC_AIC_PRE_SET3_NTT);
        CrossSet(kFlagAicDone);

        // ---------- 段2 GATE：Wait(4) ----------
        CrossWait(kFlagGate);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT4, MAGIC_AIC_POST_WAIT4);

        // ---------- 段3 INTT：复用 Wait(1) → Cube → Set(3) ----------
        CrossWait(kFlagAivReady);
        TraceMark(traceGm, SLOT_AIC_POST_WAIT1_INTT, MAGIC_AIC_POST_WAIT1_INTT);

        cube.Process(matCIntt, matA, matB);
        AscendC::PipeBarrier<PIPE_ALL>();

        TraceMark(traceGm, SLOT_AIC_PRE_SET3_INTT, MAGIC_AIC_PRE_SET3_INTT);
        CrossSet(kFlagAicDone);
    } else {
        // ---------- AIV 段1 NTT：Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_NTT, MAGIC_AIV0_PRE_SET1_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_NTT, MAGIC_AIV1_PRE_SET1_NTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_NTT, MAGIC_AIV0_POST_WAIT3_NTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_POST_WAIT3_NTT, MAGIC_AIV1_POST_WAIT3_NTT);
        }

        // ---------- AIV0：NTT 域 MultiplyNTTs（真拓扑；非 Host 预喂 u,v）----------
        if (subIdx == 0) {
            rb_t11::ComputeNttDomainUv(ws);
            TraceMark(traceGm, SLOT_AIV0_MUL_DONE, MAGIC_AIV0_MUL_DONE);
        }

        // ---------- AIV 段2 GATE：Set(4) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET4, MAGIC_AIV0_PRE_SET4);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET4, MAGIC_AIV1_PRE_SET4);
        }
        CrossSet(kFlagGate);

        // ---------- AIV 段3 INTT：复用 Set(1) → Wait(3) ----------
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_PRE_SET1_INTT, MAGIC_AIV0_PRE_SET1_INTT);
        } else {
            TraceMark(traceGm, SLOT_AIV1_PRE_SET1_INTT, MAGIC_AIV1_PRE_SET1_INTT);
        }
        CrossSet(kFlagAivReady);

        CrossWait(kFlagAicDone);
        if (subIdx == 0) {
            TraceMark(traceGm, SLOT_AIV0_POST_WAIT3_INTT, MAGIC_AIV0_POST_WAIT3_INTT);
            // ---------- INTT+加噪 → u,v ----------
            rb_t11::ComputeInttAddNoise(ws);
            TraceMark(traceGm, SLOT_AIV0_UV_DONE, MAGIC_AIV0_UV_DONE);

            // ---------- T06 pack：设备 u,v → c ----------
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
