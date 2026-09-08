/**
 * EN06 · L5 Pack 真 Compress + ByteEncode（ML-KEM-1024 密文外形）
 *
 * 作用：独立 AIV launch 完成 Alg.14 行 22–24 外形：
 *   c = ByteEncode_11(Compress_11(u)) ‖ ByteEncode_5(Compress_5(v))
 * 输入：u [K·N] int32、v [N] int32，系数 ∈ [0,q)；已锁 K=4、N=256、q=3329、d_u=11、d_v=5。
 * 输出：c [1568] uint8（4×352 + 160）。
 * 背景：KB A2/X32 — CPU=`AIV_ONLY`，SIM=无握手 MIX 占位；禁 GATE 4/8 / 融 NTT。
 * Compress：统一整数舍入 C=⌊2^37/q⌋（笔记 P-UCOMP-1）；ByteEncode：Alg.5 标量逐组 pack（VEC=1）。
 * 未采用：抄 Encrypt/alg14/ER/pack 探针整文件；ByteEncode VEC=2 Gather。
 */
#include "enc_aiv_stub_common.hpp"

namespace {

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

} // namespace

/**
 * L5 入口：u/v → c（Compress₁₁/₅ + ByteEncode）。
 * @param c_out 密文 GM，长度 K*(N*11/8)+N*5/8
 * @param u_in  u 多项式向量 [K·N]
 * @param v_in  v 多项式 [N]
 */
extern "C" __global__ __aicore__ void enc_pack_compress_real(GM_ADDR c_out, GM_ADDR u_in,
                                                             GM_ADDR v_in, int32_t k, int32_t n,
                                                             int32_t q)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

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
        PackOnePoly(out_local, in_local, comp, n, 11);
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
        PackOnePoly(out_local, in_local, comp, n, 5);
        que_in.FreeTensor(in_local);

        que_out.EnQue(out_local);
        out_local = que_out.DeQue<uint8_t>();
        AscendC::DataCopy(gm_c[static_cast<uint32_t>(k) * c1PolyBytes], out_local, c2Bytes);
        que_out.FreeTensor(out_local);
    }
}
