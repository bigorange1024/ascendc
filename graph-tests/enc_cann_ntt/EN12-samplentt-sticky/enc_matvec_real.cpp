/**
 * EN04 · L3 Matvec 真积木（NTT 域 4×4×1 内积）
 *
 * 作用：独立 AIV launch 计算 t̂[p] = mod_q(Σ_j Â[p,j] ∘ ŝ[j])（无 ê）。
 * ∘ = FIPS 203 Alg.11/12 paired basemul（γ_i = ζ^{2·BitRev7(i)+1} mod q）。
 * 布局（行主序，与 F203-innerproduct-k4 笔记契约一致）：
 *   flat_A(p,j,c) = (p·K + j)·N + c；flat_s(j,c) = j·N + c；flat_t(p,c) = p·N + c。
 * 输入：a_hat [K·K·N] int32、s_hat [K·N] int32、gammas [N/2] int32；K=4,N=256,q=3329。
 * 输出：t_hat [K·N] int32。
 * 背景：KB A2/X32 — CPU=`AIV_ONLY`，SIM=无握手 MIX 占位（AIC 即 return）；禁 GATE 4/8。
 * 未采用：抄 Encrypt/alg14/ER 核；粘贴 innerproduct 探针整文件；AIC 陪跑空等。
 */
#include "enc_aiv_stub_common.hpp"
#include "f203_mod_q/mod_q_vec.hpp"

namespace {

/** int64 有符号取模到 [0,q)。 */
__aicore__ inline int32_t ModQ64(int64_t x, int32_t q)
{
    const int64_t q64 = static_cast<int64_t>(q);
    int64_t r = x % q64;
    if (r < 0) {
        r += q64;
    }
    return static_cast<int32_t>(r);
}

/**
 * Alg.11 MultiplyNTTs：对 UB 内整 poly 做 128 对 BaseCaseMultiply，写回 dst。
 * a/b/gamma 已在 LocalTensor；dst 可与 a 同缓冲（先读后写按对）。
 */
__aicore__ inline void MultiplyNttsOnUb(AscendC::LocalTensor<int32_t> &dst,
                                        AscendC::LocalTensor<int32_t> &a,
                                        AscendC::LocalTensor<int32_t> &b,
                                        AscendC::LocalTensor<int32_t> &gamma, int32_t n,
                                        int32_t q)
{
    const int32_t pairs = n / 2;
    for (int32_t i = 0; i < pairs; ++i) {
        const uint32_t i0 = static_cast<uint32_t>(2 * i);
        const uint32_t i1 = static_cast<uint32_t>(2 * i + 1);
        const int64_t a0 = static_cast<int64_t>(a.GetValue(i0));
        const int64_t a1 = static_cast<int64_t>(a.GetValue(i1));
        const int64_t b0 = static_cast<int64_t>(b.GetValue(i0));
        const int64_t b1 = static_cast<int64_t>(b.GetValue(i1));
        const int64_t g = static_cast<int64_t>(gamma.GetValue(static_cast<uint32_t>(i)));
        // Alg.12：(c0,c1) = (a0·b0 + a1·b1·γ, a0·b1 + a1·b0) mod q
        const int32_t c0 = ModQ64(a0 * b0 + a1 * b1 * g, q);
        const int32_t c1 = ModQ64(a0 * b1 + a1 * b0, q);
        dst.SetValue(i0, c0);
        dst.SetValue(i1, c1);
    }
}

} // namespace

extern "C" __global__ __aicore__ void enc_matvec_real(GM_ADDR t_hat, GM_ADDR a_hat, GM_ADDR s_hat,
                                                      GM_ADDR gammas, int32_t k, int32_t n,
                                                      int32_t q)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

    // 已锁：ML-KEM-1024 K=4、N=256、q=3329；非法参数直接空返回（不挂）
    if (k <= 0 || n <= 0 || (n & 7) != 0 || q <= 0) {
        return;
    }

    const uint32_t nU = static_cast<uint32_t>(n);
    const uint32_t pairs = nU / 2U;
    const uint32_t kn = static_cast<uint32_t>(k) * nU;

    AscendC::GlobalTensor<int32_t> gm_a;
    AscendC::GlobalTensor<int32_t> gm_s;
    AscendC::GlobalTensor<int32_t> gm_t;
    AscendC::GlobalTensor<int32_t> gm_g;
    gm_a.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(a_hat),
                         static_cast<uint32_t>(k) * static_cast<uint32_t>(k) * nU);
    gm_s.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(s_hat), kn);
    gm_t.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(t_hat), kn);
    gm_g.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(gammas), pairs);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_a;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_s;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_g;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_prod;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_acc;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_t1;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_t2;

    // VECIN 仅作 GM→UB 握手（SYNC-02）；计算落在独立 TBuf
    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(buf_a, n * sizeof(int32_t));
    pipe.InitBuffer(buf_s, n * sizeof(int32_t));
    pipe.InitBuffer(buf_g, static_cast<int32_t>(pairs) * static_cast<int32_t>(sizeof(int32_t)));
    pipe.InitBuffer(buf_prod, n * sizeof(int32_t));
    pipe.InitBuffer(buf_acc, static_cast<int32_t>(kn) * static_cast<int32_t>(sizeof(int32_t)));
    pipe.InitBuffer(buf_t1, n * sizeof(int32_t));
    pipe.InitBuffer(buf_t2, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> a_local = buf_a.Get<int32_t>();
    AscendC::LocalTensor<int32_t> s_local = buf_s.Get<int32_t>();
    AscendC::LocalTensor<int32_t> g_local = buf_g.Get<int32_t>();
    AscendC::LocalTensor<int32_t> prod_local = buf_prod.Get<int32_t>();
    AscendC::LocalTensor<int32_t> acc_local = buf_acc.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t1 = buf_t1.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t2 = buf_t2.Get<int32_t>();

    // 累加器清零（P-inner-1：先 Σ 再一次 mod）
    AscendC::Duplicate(acc_local, static_cast<int32_t>(0), kn);
    AscendC::PipeBarrier<PIPE_V>();

    // 一次装入 γ[N/2]
    {
        AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
        AscendC::DataCopy(in_local, gm_g, pairs);
        que_in.EnQue(in_local);
        in_local = que_in.DeQue<int32_t>();
        AscendC::DataCopy(g_local, in_local, pairs);
        AscendC::PipeBarrier<PIPE_V>();
        que_in.FreeTensor(in_local);
    }

    // 外层 j、内层 p：每次 basemul 整 poly（笔记 §3.1）
    for (int32_t j = 0; j < k; ++j) {
        {
            AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
            AscendC::DataCopy(in_local, gm_s[static_cast<uint32_t>(j) * nU], nU);
            que_in.EnQue(in_local);
            in_local = que_in.DeQue<int32_t>();
            AscendC::DataCopy(s_local, in_local, nU);
            AscendC::PipeBarrier<PIPE_V>();
            que_in.FreeTensor(in_local);
        }

        for (int32_t p = 0; p < k; ++p) {
            const uint32_t a_off =
                (static_cast<uint32_t>(p) * static_cast<uint32_t>(k) + static_cast<uint32_t>(j)) * nU;
            {
                AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
                AscendC::DataCopy(in_local, gm_a[a_off], nU);
                que_in.EnQue(in_local);
                in_local = que_in.DeQue<int32_t>();
                AscendC::DataCopy(a_local, in_local, nU);
                AscendC::PipeBarrier<PIPE_V>();
                que_in.FreeTensor(in_local);
            }

            // 标量 GetValue 前须 V→S；与 SYNC-02 / EN03 桩同构
            AscendC::PipeBarrier<PIPE_ALL>();
            MultiplyNttsOnUb(prod_local, a_local, s_local, g_local, n, q);
            AscendC::PipeBarrier<PIPE_ALL>();

            // acc[p] += prod（int32；S=4 且每项已 mod q → 和 < 4q）
            AscendC::LocalTensor<int32_t> acc_row = acc_local[static_cast<uint32_t>(p) * nU];
            AscendC::Add(acc_row, acc_row, prod_local, nU);
            AscendC::PipeBarrier<PIPE_V>();
        }
    }

    // 每行一次 final mod，再写回 t_hat
    for (int32_t p = 0; p < k; ++p) {
        AscendC::LocalTensor<int32_t> acc_row = acc_local[static_cast<uint32_t>(p) * nU];
#if defined(ASCENDC_CPU_DEBUG)
        f203_mod_q::mod_q_final_vec(acc_row, q, n);
#else
        f203_mod_q::mod_q_final_vec(acc_row, q, t1, t2, n);
#endif
        AscendC::PipeBarrier<PIPE_V>();

        AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
        AscendC::DataCopy(out_local, acc_row, nU);
        AscendC::PipeBarrier<PIPE_V>();
        que_out.EnQue(out_local);
        out_local = que_out.DeQue<int32_t>();
        AscendC::DataCopy(gm_t[static_cast<uint32_t>(p) * nU], out_local, nU);
        que_out.FreeTensor(out_local);
    }
}
