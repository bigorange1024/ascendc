/**
 * EN15 · AIV 积木可调用体（Matvec / Dot / Pack）
 *
 * 从 EN13 独立 AIV 核抽出函数体，供 enc_compute_l2 在 MIX 串级中由 AIV0 顺序调用。
 * 每个 Impl 自建 TPipe；阶段间由外层 SyncAll 隔离。禁 GATE 4/8；写出 UB+DataCopy。
 */
#ifndef ENC_AIV_BODIES_HPP
#define ENC_AIV_BODIES_HPP

#include "kernel_operator.h"
#include "f203_mod_q/mod_q_vec.hpp"

namespace enc_aiv_bodies {

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

} // namespace enc_aiv_bodies

namespace enc_aiv_bodies {

/** 统一 Compress 乘数 C = ⌊2^37 / 3329⌋（见 F203-Compress 统一整数舍入笔记）。 */
constexpr int64_t kUnifiedC = 41285357LL;

/**
 * Compress_d(u) = round(u·2^d/q) mod 2^d
 * 公式：(C·u + 2^(36-d)) >> (37-d)，再 mask 低 d bit。
 */
__aicore__ inline int32_t CompressUnified(int32_t u, int32_t d)
{
    const int32_t shift = 37 - d;
    const int64_t bias = 1LL << (36 - d);
    const int64_t s = kUnifiedC * static_cast<int64_t>(u) + bias;
    const int32_t y = static_cast<int32_t>(s >> shift);
    const int32_t mask = (1 << d) - 1;
    return y & mask;
}

/**
 * Alg.5 ByteEncode_d=5：8 系数 × 5bit → 5B（组内 bit 跨字节）。
 * @param out 输出字节 LocalTensor；写 out[base+0..4]
 * @param t   已压缩系数（低 5 bit 有效）
 * @param base 本组输出字节偏移
 * @param coeff0 本组首系数在 t 中的下标
 */
__aicore__ inline void ByteEncodeD5Group(AscendC::LocalTensor<uint8_t> &out,
                                         AscendC::LocalTensor<int32_t> &t, uint32_t base,
                                         uint32_t coeff0)
{
    uint8_t a[8];
    for (int32_t j = 0; j < 8; ++j) {
        a[j] = static_cast<uint8_t>(t.GetValue(coeff0 + static_cast<uint32_t>(j)) & 0x1F);
    }
    out.SetValue(base + 0U, static_cast<uint8_t>((a[0] >> 0) | (a[1] << 5)));
    out.SetValue(base + 1U, static_cast<uint8_t>((a[1] >> 3) | (a[2] << 2) | (a[3] << 7)));
    out.SetValue(base + 2U, static_cast<uint8_t>((a[3] >> 1) | (a[4] << 4)));
    out.SetValue(base + 3U, static_cast<uint8_t>((a[4] >> 4) | (a[5] << 1) | (a[6] << 6)));
    out.SetValue(base + 4U, static_cast<uint8_t>((a[6] >> 2) | (a[7] << 3)));
}

/**
 * Alg.5 ByteEncode_d=11：8 系数 × 11bit → 11B。
 */
__aicore__ inline void ByteEncodeD11Group(AscendC::LocalTensor<uint8_t> &out,
                                          AscendC::LocalTensor<int32_t> &t, uint32_t base,
                                          uint32_t coeff0)
{
    uint16_t a[8];
    for (int32_t j = 0; j < 8; ++j) {
        a[j] = static_cast<uint16_t>(t.GetValue(coeff0 + static_cast<uint32_t>(j)) & 0x7FF);
    }
    out.SetValue(base + 0U, static_cast<uint8_t>((a[0] >> 0) & 0xFF));
    out.SetValue(base + 1U, static_cast<uint8_t>((a[0] >> 8) | ((a[1] << 3) & 0xFF)));
    out.SetValue(base + 2U, static_cast<uint8_t>((a[1] >> 5) | ((a[2] << 6) & 0xFF)));
    out.SetValue(base + 3U, static_cast<uint8_t>((a[2] >> 2) & 0xFF));
    out.SetValue(base + 4U, static_cast<uint8_t>((a[2] >> 10) | ((a[3] << 1) & 0xFF)));
    out.SetValue(base + 5U, static_cast<uint8_t>((a[3] >> 7) | ((a[4] << 4) & 0xFF)));
    out.SetValue(base + 6U, static_cast<uint8_t>((a[4] >> 4) | ((a[5] << 7) & 0xFF)));
    out.SetValue(base + 7U, static_cast<uint8_t>((a[5] >> 1) & 0xFF));
    out.SetValue(base + 8U, static_cast<uint8_t>((a[5] >> 9) | ((a[6] << 2) & 0xFF)));
    out.SetValue(base + 9U, static_cast<uint8_t>((a[6] >> 6) | ((a[7] << 5) & 0xFF)));
    out.SetValue(base + 10U, static_cast<uint8_t>(a[7] >> 3));
}

/**
 * 对 UB 内一整 poly：Compress_d → ByteEncode_d，写到 out 字节缓冲。
 * @param d 5 或 11；n 须为 8 的倍数
 */
__aicore__ inline void PackOnePoly(AscendC::LocalTensor<uint8_t> &out,
                                   AscendC::LocalTensor<int32_t> &poly,
                                   AscendC::LocalTensor<int32_t> &comp, int32_t n, int32_t d)
{
    const uint32_t nU = static_cast<uint32_t>(n);
    for (uint32_t i = 0; i < nU; ++i) {
        const int32_t u = poly.GetValue(i);
        comp.SetValue(i, CompressUnified(u, d));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    if (d == 11) {
        // 32 组 × 11B = 352B
        for (uint32_t g = 0; g < nU / 8U; ++g) {
            ByteEncodeD11Group(out, comp, g * 11U, g * 8U);
        }
    } else {
        // d=5：32 组 × 5B = 160B
        for (uint32_t g = 0; g < nU / 8U; ++g) {
            ByteEncodeD5Group(out, comp, g * 5U, g * 8U);
        }
    }
    AscendC::PipeBarrier<PIPE_ALL>();
}

} // namespace enc_aiv_bodies

__aicore__ inline void EncMatvecRealImpl(GM_ADDR t_hat, GM_ADDR a_hat, GM_ADDR s_hat,
                                                      GM_ADDR gammas, int32_t k, int32_t n,
                                                      int32_t q)
{
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
            enc_aiv_bodies::MultiplyNttsOnUb(prod_local, a_local, s_local, g_local, n, q);
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

__aicore__ inline void EncDotRealImpl(GM_ADDR v_hat, GM_ADDR t_hat, GM_ADDR s_hat,
                                                   GM_ADDR gammas, int32_t k, int32_t n, int32_t q)
{
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
        enc_aiv_bodies::MultiplyNttsOnUb(prod_local, a_local, b_local, g_local, n, q);
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

__aicore__ inline void EncPackCompressRealImpl(GM_ADDR c_out, GM_ADDR u_in,
                                                             GM_ADDR v_in, int32_t k, int32_t n,
                                                             int32_t q)
{
// 已锁参数；非法则空返回（不挂）
    (void)q;
    if (k <= 0 || n <= 0 || (n & 7) != 0) {
        return;
    }

    const uint32_t nU = static_cast<uint32_t>(n);
    const uint32_t c1PolyBytes = nU * 11U / 8U; // 352
    const uint32_t c2Bytes = nU * 5U / 8U;       // 160
    const uint32_t cBytes = static_cast<uint32_t>(k) * c1PolyBytes + c2Bytes;
    const uint32_t kn = static_cast<uint32_t>(k) * nU;

    AscendC::GlobalTensor<int32_t> gm_u;
    AscendC::GlobalTensor<int32_t> gm_v;
    AscendC::GlobalTensor<uint8_t> gm_c;
    gm_u.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(u_in), kn);
    gm_v.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(v_in), nU);
    gm_c.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(c_out), cBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> buf_comp;

    // VECIN：一 poly；VECOUT：最大单 poly 编码 352B（32B 对齐）；comp 同 poly
    pipe.InitBuffer(que_in, 1, nU * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, c1PolyBytes); // 352 ≥ 160
    pipe.InitBuffer(buf_comp, nU * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> comp = buf_comp.Get<int32_t>();

    // ---- c1：K 个 poly，d_u=11 ----
    for (int32_t p = 0; p < k; ++p) {
        AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
        AscendC::DataCopy(in_local, gm_u[static_cast<uint32_t>(p) * nU], nU);
        que_in.EnQue(in_local);
        in_local = que_in.DeQue<int32_t>();

        AscendC::LocalTensor<uint8_t> out_local = que_out.AllocTensor<uint8_t>();
        enc_aiv_bodies::PackOnePoly(out_local, in_local, comp, n, 11);
        que_in.FreeTensor(in_local);

        que_out.EnQue(out_local);
        out_local = que_out.DeQue<uint8_t>();
        AscendC::DataCopy(gm_c[static_cast<uint32_t>(p) * c1PolyBytes], out_local, c1PolyBytes);
        que_out.FreeTensor(out_local);
    }

    // ---- c2：v，d_v=5 ----
    {
        AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
        AscendC::DataCopy(in_local, gm_v, nU);
        que_in.EnQue(in_local);
        in_local = que_in.DeQue<int32_t>();

        AscendC::LocalTensor<uint8_t> out_local = que_out.AllocTensor<uint8_t>();
        // out 缓冲按 352 分配；仅写前 160B
        enc_aiv_bodies::PackOnePoly(out_local, in_local, comp, n, 5);
        que_in.FreeTensor(in_local);

        que_out.EnQue(out_local);
        out_local = que_out.DeQue<uint8_t>();
        AscendC::DataCopy(gm_c[static_cast<uint32_t>(k) * c1PolyBytes], out_local, c2Bytes);
        que_out.FreeTensor(out_local);
    }
}

#endif // ENC_AIV_BODIES_HPP
