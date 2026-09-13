/**
 * EN15 · L2 compute 融合核（1 Host launch）
 *
 * 设备内串级（禁 GATE 4/8；阶段间 SyncAll 在 Wait 环外）：
 *   NTT(y) → Matvec(读A[j,p]≡Âᵀ∘ŷ，无物化转置) → Dot(⟨t̂,ŷ⟩) → INTT(û) → INTT(v̂ pad) → +e1/e2 + μ←m → Pack→c
 * CrossCore 仅 cann-ntt 阶段复用 flag 1/2/3；AIV 积木段 AIC/AIV1 空等 SyncAll。
 * 背景：用户锁定 Host launch=2；否决 EN13 的 8×ACLRT_LAUNCH_KERNEL 交付形态。
 * 未采用：自研 SoftSync；把 SampleNTT 融进本 MIX。
 */
#include "kernel_operator.h"
#include "tiling.h"
#include "enc_mmad_phase.hpp"
#include "enc_aiv_bodies.hpp"

namespace {

constexpr int32_t kQ = 3329;
constexpr int32_t kK = 4;
constexpr int32_t kN = 256;

/**
 * 经 UB 做逐系数 (x+y) mod q，写回 dst（可与 x 同缓冲）。
 * @param elems 系数个数（u: K·N；v: N）
 */
__aicore__ inline void EncAddModQGm(GM_ADDR dst, GM_ADDR x, GM_ADDR y, int32_t elems, int32_t q)
{
    const uint32_t nU = static_cast<uint32_t>(elems);
    AscendC::GlobalTensor<int32_t> gmD;
    AscendC::GlobalTensor<int32_t> gmX;
    AscendC::GlobalTensor<int32_t> gmY;
    gmD.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst), nU);
    gmX.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(x), nU);
    gmY.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(y), nU);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufX;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufY;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    pipe.InitBuffer(queIn, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(queOut, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(bufX, nU * sizeof(int32_t));
    pipe.InitBuffer(bufY, nU * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> xL = bufX.Get<int32_t>();
    AscendC::LocalTensor<int32_t> yL = bufY.Get<int32_t>();
    {
        AscendC::LocalTensor<int32_t> inL = queIn.AllocTensor<int32_t>();
        AscendC::DataCopy(inL, gmX, nU);
        queIn.EnQue(inL);
        inL = queIn.DeQue<int32_t>();
        AscendC::DataCopy(xL, inL, nU);
        queIn.FreeTensor(inL);
    }
    {
        AscendC::LocalTensor<int32_t> inL = queIn.AllocTensor<int32_t>();
        AscendC::DataCopy(inL, gmY, nU);
        queIn.EnQue(inL);
        inL = queIn.DeQue<int32_t>();
        AscendC::DataCopy(yL, inL, nU);
        queIn.FreeTensor(inL);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t i = 0; i < nU; ++i) {
        int64_t s = static_cast<int64_t>(xL.GetValue(i)) + static_cast<int64_t>(yL.GetValue(i));
        s %= q;
        if (s < 0) {
            s += q;
        }
        xL.SetValue(i, static_cast<int32_t>(s));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::LocalTensor<int32_t> outL = queOut.AllocTensor<int32_t>();
    AscendC::DataCopy(outL, xL, nU);
    queOut.EnQue(outL);
    outL = queOut.DeQue<int32_t>();
    AscendC::DataCopy(gmD, outL, nU);
    queOut.FreeTensor(outL);
}

/**
 * 将 src[n] 拷到 dst 首 poly，其余 (k-1)·n 清零（INTT(v̂) 垫成 bench=k）。
 */
__aicore__ inline void EncPadVHatGm(GM_ADDR dstPad, GM_ADDR vHat, int32_t k, int32_t n)
{
    const uint32_t nU = static_cast<uint32_t>(n);
    const uint32_t kn = static_cast<uint32_t>(k) * nU;
    AscendC::GlobalTensor<int32_t> gmD;
    AscendC::GlobalTensor<int32_t> gmV;
    gmD.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dstPad), kn);
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(vHat), nU);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf;
    pipe.InitBuffer(queIn, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(queOut, 1, kn * sizeof(int32_t));
    pipe.InitBuffer(buf, kn * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> pad = buf.Get<int32_t>();
    AscendC::Duplicate(pad, static_cast<int32_t>(0), kn);
    AscendC::PipeBarrier<PIPE_V>();
    {
        AscendC::LocalTensor<int32_t> inL = queIn.AllocTensor<int32_t>();
        AscendC::DataCopy(inL, gmV, nU);
        queIn.EnQue(inL);
        inL = queIn.DeQue<int32_t>();
        AscendC::DataCopy(pad, inL, nU);
        queIn.FreeTensor(inL);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::LocalTensor<int32_t> outL = queOut.AllocTensor<int32_t>();
    AscendC::DataCopy(outL, pad, kn);
    queOut.EnQue(outL);
    outL = queOut.DeQue<int32_t>();
    AscendC::DataCopy(gmD, outL, kn);
    queOut.FreeTensor(outL);
}


/**
 * Alg.14：m[32] → Decompress₁ → 就地加到 v[n]（half_q=(q+1)/2）。
 * 禁止 Host 预解压 μ 文件作为输入。
 */
__aicore__ inline void EncMuEmbedAddGm(GM_ADDR v, GM_ADDR m, int32_t n, int32_t q)
{
    const uint32_t nU = static_cast<uint32_t>(n);
    const int32_t halfQ = (q + 1) / 2;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmM;
    gmV.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(v), nU);
    gmM.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(m), 32U);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufV;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufM;
    pipe.InitBuffer(queIn, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(queOut, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(bufV, nU * sizeof(int32_t));
    pipe.InitBuffer(bufM, 32);

    AscendC::LocalTensor<int32_t> vL = bufV.Get<int32_t>();
    AscendC::LocalTensor<uint8_t> mL = bufM.Get<uint8_t>();
    {
        AscendC::LocalTensor<int32_t> inL = queIn.AllocTensor<int32_t>();
        AscendC::DataCopy(inL, gmV, nU);
        queIn.EnQue(inL);
        inL = queIn.DeQue<int32_t>();
        AscendC::DataCopy(vL, inL, nU);
        queIn.FreeTensor(inL);
    }
    AscendC::DataCopy(mL, gmM, 32);
    AscendC::PipeBarrier<PIPE_ALL>();
    for (uint32_t i = 0; i < nU; ++i) {
        const uint32_t byte = static_cast<uint32_t>(mL.GetValue(i / 8U));
        const int32_t bit = static_cast<int32_t>((byte >> (i % 8U)) & 1U);
        int64_t s = static_cast<int64_t>(vL.GetValue(i)) + static_cast<int64_t>(bit * halfQ);
        s %= q;
        if (s < 0) {
            s += q;
        }
        vL.SetValue(i, static_cast<int32_t>(s));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::LocalTensor<int32_t> outL = queOut.AllocTensor<int32_t>();
    AscendC::DataCopy(outL, vL, nU);
    queOut.EnQue(outL);
    outL = queOut.DeQue<int32_t>();
    AscendC::DataCopy(gmV, outL, nU);
    queOut.FreeTensor(outL);
}

/** 拷贝 GM 首 poly：src[0:n) → dst[0:n)。 */
__aicore__ inline void EncCopyPoly0Gm(GM_ADDR dst, GM_ADDR src, int32_t n)
{
    const uint32_t nU = static_cast<uint32_t>(n);
    AscendC::GlobalTensor<int32_t> gmD;
    AscendC::GlobalTensor<int32_t> gmS;
    gmD.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst), nU);
    gmS.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(src), nU);
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOut;
    pipe.InitBuffer(queIn, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(queOut, 1, nU * sizeof(int32_t));
    AscendC::LocalTensor<int32_t> inL = queIn.AllocTensor<int32_t>();
    AscendC::DataCopy(inL, gmS, nU);
    queIn.EnQue(inL);
    inL = queIn.DeQue<int32_t>();
    AscendC::LocalTensor<int32_t> outL = queOut.AllocTensor<int32_t>();
    AscendC::DataCopy(outL, inL, nU);
    queIn.FreeTensor(inL);
    queOut.EnQue(outL);
    outL = queOut.DeQue<int32_t>();
    AscendC::DataCopy(gmD, outL, nU);
    queOut.FreeTensor(outL);
}

} // namespace

/**
 * L2 入口：NTT→matvec→dot→INTT×2→加噪→pack。
 *
 * scratch 布局（字节，与 main 一致）：
 *   [0, vec)           sHat / ŷ
 *   [vec, 2vec)        uHat（INTT_u 后可复用为 vInttDst）
 *   [2vec, 2vec+poly)  vHat
 *   [2vec+poly, 3vec+poly) vPad（INTT_v src）
 *   [3vec+poly, 4vec+poly) uOut
 *   [4vec+poly, 4vec+2poly) vOut
 */
extern "C" __global__ __aicore__ void enc_compute_l2(
    GM_ADDR c_out, GM_ADDR y_in, GM_ADDR a_hat, GM_ADDR t_pub, GM_ADDR gammas, GM_ADDR e1,
    GM_ADDR e2, GM_ADDR m, GM_ADDR ws_ntt, GM_ADDR ws_intt, GM_ADDR scratch, TilingData tiling)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_MIX_AIC_1_2);

    // CPU 孪生与 SIM/NPU：均按 MIX 子块区分 AIC / AIV（与 mmad_custom 同构）
    const bool AIC = AscendC::GetSubBlockNum() == 1;
    const int32_t subBlockID = (int32_t)AscendC::GetSubBlockIdx();

    const int32_t k = kK;
    const int32_t n = kN;
    const int32_t q = kQ;
    const size_t polyBytes = static_cast<size_t>(n) * sizeof(int32_t);
    const size_t vecBytes = static_cast<size_t>(k) * polyBytes;

    GM_ADDR sHat = scratch;
    GM_ADDR uHat = scratch + vecBytes;
    GM_ADDR vHat = scratch + 2 * vecBytes;
    GM_ADDR vPad = scratch + 2 * vecBytes + polyBytes;
    GM_ADDR uOut = scratch + 2 * vecBytes + polyBytes + vecBytes;
    GM_ADDR vOut = scratch + 2 * vecBytes + polyBytes + vecBytes + vecBytes;
    // INTT(v) 的 dst：复用 uHat（此时 û 已写入 uOut）
    GM_ADDR vInttDst = uHat;

    // -------- Phase NTT(y)→ŷ --------
    enc_mmad_phase::RunCannNttPhase(sHat, y_in, ws_ntt, tiling);
    AscendC::SyncAll();

    // -------- Phase Matvec + Dot（仅 AIV0）--------
    if (!AIC && subBlockID == 0) {
        EncMatvecRealImpl(uHat, a_hat, sHat, gammas, k, n, q);
        EncDotRealImpl(vHat, t_pub, sHat, gammas, k, n, q);
    }
    AscendC::SyncAll();

    // -------- Phase INTT(û)→u --------
    enc_mmad_phase::RunCannNttPhase(uOut, uHat, ws_intt, tiling);
    AscendC::SyncAll();

    // -------- Phase pad v̂ --------
    if (!AIC && subBlockID == 0) {
        EncPadVHatGm(vPad, vHat, k, n);
    }
    AscendC::SyncAll();

    // -------- Phase INTT(v̂ pad)→vInttDst（src/dst 分离）--------
    enc_mmad_phase::RunCannNttPhase(vInttDst, vPad, ws_intt, tiling);
    AscendC::SyncAll();

    // -------- Phase 取 poly0 + 加噪 + Pack --------
    if (!AIC && subBlockID == 0) {
        EncCopyPoly0Gm(vOut, vInttDst, n);
        EncAddModQGm(uOut, uOut, e1, k * n, q);
        EncAddModQGm(vOut, vOut, e2, n, q);
        EncMuEmbedAddGm(vOut, m, n, q);
        EncPackCompressRealImpl(c_out, uOut, vOut, k, n, q);
    }
}
