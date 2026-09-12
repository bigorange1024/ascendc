/**
 * EN13 · L3b NTT 域点积 v̂ = Σ_i MultiplyNTTs(t̂[i], ŷ[i])
 *
 * 作用：独立 AIV launch，计算 Encrypt 行 19 前半 ⟨t̂, ŷ⟩（单 poly 输出）。
 * 为何不能 k=1 冒充 matvec：既有 enc_matvec_real 用同一 k 作 Â 的行列维，
 *   会读 A[k×k×N]；点积左侧是 t̂[k×N]，几何不同。
 * 输入：t_hat [K·N] int32、s_hat/ŷ [K·N] int32、gammas [N/2] int32；K=4,N=256,q=3329。
 * 输出：v_hat [N] int32。
 * API：复用 EN04 已查记录的 DataCopy/TPipe/TQue/TBuf/Duplicate/Add/GetValue/SetValue/PipeBarrier；
 *       无新增 AscendC API。
 * 背景：禁抄 Encrypt/alg14/ER 核；同步禁 GATE 4/8。
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
 * Alg.11 MultiplyNTTs：UB 内整 poly 128 对 BaseCaseMultiply。
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
        const int32_t c0 = ModQ64(a0 * b0 + a1 * b1 * g, q);
        const int32_t c1 = ModQ64(a0 * b1 + a1 * b0, q);
        dst.SetValue(i0, c0);
        dst.SetValue(i1, c1);
    }
}

} // namespace

/**
 * @param v_hat  输出单 poly [N]
 * @param t_hat  公钥 NTT 域 t̂ [K·N]
 * @param s_hat  ŷ = NTT(y) [K·N]
 * @param gammas γ[N/2]
 */
extern "C" __global__ __aicore__ void enc_dot_real(GM_ADDR v_hat, GM_ADDR t_hat, GM_ADDR s_hat,
                                                   GM_ADDR gammas, int32_t k, int32_t n, int32_t q)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }
    if (k <= 0 || n <= 0 || (n & 7) != 0 || q <= 0) {
        return;
    }

    const uint32_t nU = static_cast<uint32_t>(n);
    const uint32_t pairs = nU / 2U;
    const uint32_t kn = static_cast<uint32_t>(k) * nU;

    AscendC::GlobalTensor<int32_t> gm_t;
    AscendC::GlobalTensor<int32_t> gm_s;
    AscendC::GlobalTensor<int32_t> gm_v;
    AscendC::GlobalTensor<int32_t> gm_g;
    gm_t.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(t_hat), kn);
    gm_s.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(s_hat), kn);
    gm_v.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(v_hat), nU);
    gm_g.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(gammas), pairs);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_a;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_b;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_g;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_prod;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_acc;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_t1;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_t2;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(buf_a, n * sizeof(int32_t));
    pipe.InitBuffer(buf_b, n * sizeof(int32_t));
    pipe.InitBuffer(buf_g, static_cast<int32_t>(pairs) * static_cast<int32_t>(sizeof(int32_t)));
    pipe.InitBuffer(buf_prod, n * sizeof(int32_t));
    pipe.InitBuffer(buf_acc, n * sizeof(int32_t));
    pipe.InitBuffer(buf_t1, n * sizeof(int32_t));
    pipe.InitBuffer(buf_t2, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> a_local = buf_a.Get<int32_t>();
    AscendC::LocalTensor<int32_t> b_local = buf_b.Get<int32_t>();
    AscendC::LocalTensor<int32_t> g_local = buf_g.Get<int32_t>();
    AscendC::LocalTensor<int32_t> prod_local = buf_prod.Get<int32_t>();
    AscendC::LocalTensor<int32_t> acc_local = buf_acc.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t1 = buf_t1.Get<int32_t>();
    AscendC::LocalTensor<int32_t> t2 = buf_t2.Get<int32_t>();

    AscendC::Duplicate(acc_local, static_cast<int32_t>(0), nU);
    AscendC::PipeBarrier<PIPE_V>();

    // γ[N/2]
    {
        AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
        AscendC::DataCopy(in_local, gm_g, pairs);
        que_in.EnQue(in_local);
        in_local = que_in.DeQue<int32_t>();
        AscendC::DataCopy(g_local, in_local, pairs);
        AscendC::PipeBarrier<PIPE_V>();
        que_in.FreeTensor(in_local);
    }

    // Σ_i t̂[i] ∘ ŷ[i]
    for (int32_t i = 0; i < k; ++i) {
        const uint32_t off = static_cast<uint32_t>(i) * nU;
        {
            AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
            AscendC::DataCopy(in_local, gm_t[off], nU);
            que_in.EnQue(in_local);
            in_local = que_in.DeQue<int32_t>();
            AscendC::DataCopy(a_local, in_local, nU);
            AscendC::PipeBarrier<PIPE_V>();
            que_in.FreeTensor(in_local);
        }
        {
            AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
            AscendC::DataCopy(in_local, gm_s[off], nU);
            que_in.EnQue(in_local);
            in_local = que_in.DeQue<int32_t>();
            AscendC::DataCopy(b_local, in_local, nU);
            AscendC::PipeBarrier<PIPE_V>();
            que_in.FreeTensor(in_local);
        }

        AscendC::PipeBarrier<PIPE_ALL>();
        MultiplyNttsOnUb(prod_local, a_local, b_local, g_local, n, q);
        AscendC::PipeBarrier<PIPE_ALL>();

        AscendC::Add(acc_local, acc_local, prod_local, nU);
        AscendC::PipeBarrier<PIPE_V>();
    }

#if defined(ASCENDC_CPU_DEBUG)
    f203_mod_q::mod_q_final_vec(acc_local, q, n);
#else
    f203_mod_q::mod_q_final_vec(acc_local, q, t1, t2, n);
#endif
    AscendC::PipeBarrier<PIPE_V>();

    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::DataCopy(out_local, acc_local, nU);
    AscendC::PipeBarrier<PIPE_V>();
    que_out.EnQue(out_local);
    out_local = que_out.DeQue<int32_t>();
    AscendC::DataCopy(gm_v, out_local, nU);
    que_out.FreeTensor(out_local);
}
