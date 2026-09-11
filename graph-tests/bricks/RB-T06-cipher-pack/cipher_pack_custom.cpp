/**
 * @file cipher_pack_custom.cpp
 * @brief RB-T06：密文 pack 壳 c₁‖c₂ —— Compress₁₁(u)∥Compress₅(v) → ByteEncode → c[1568]。
 *
 * 本文件在流水线中的位置：Encrypt 重建积木 G5 薄壳；Host 读 input/u.bin、v.bin，
 * 本核写出 output/c.bin（1568B），与 golden 对拍。对齐 FIPS 203 ML-KEM-1024：
 *   c₁ = ByteEncode₁₁(Compress₁₁(u))，k=4 poly → 4×352 = 1408 B，偏移 [0,1408)
 *   c₂ = ByteEncode₅(Compress₅(v))，1 poly → 160 B，偏移 [1408,1568)
 *   c = c₁ ‖ c₂，总长 1568。
 *
 * 背景：inventory G5；契约参考 pass-f203-compress-d / byteencode-d（d=5/11），勿抄 encrypt。
 * 结论：AIV-only 标量 Compress + ByteEncode；无 CrossCore。
 * 未采用：设备侧向量宽乘（薄壳验收优先 I/O 等价）。
 */
#include "kernel_operator.h"

namespace {
constexpr int32_t kK = 4;
constexpr int32_t kPolyN = 256;
constexpr int32_t kQ = 3329;
constexpr int32_t kDu = 11;
constexpr int32_t kDv = 5;
constexpr int32_t kC1PolyBytes = (kPolyN * kDu) / 8; // 352
constexpr int32_t kC2PolyBytes = (kPolyN * kDv) / 8; // 160
constexpr int32_t kC1Bytes = kK * kC1PolyBytes;      // 1408
constexpr int32_t kC2Bytes = kC2PolyBytes;           // 160
constexpr int32_t kCBytes = kC1Bytes + kC2Bytes;     // 1568
constexpr int32_t kUCoeffs = kK * kPolyN;            // 1024
}  // namespace

/** Compress_5：Barrett 标量（与 compress_d_ref.c 同式，仅作设备 I/O）。 */
__aicore__ inline int32_t Compress5(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(kQ)) {
        x = static_cast<uint32_t>(kQ) - 1u;
    }
    const uint32_t d0 = x * 1290176u;
    return static_cast<int32_t>(((d0 + (1u << 26)) >> 27) & 0x1fu);
}

/** Compress_11：u64 Barrett 标量。 */
__aicore__ inline int32_t Compress11(int32_t u)
{
    uint32_t x = static_cast<uint32_t>(u);
    if (x >= static_cast<uint32_t>(kQ)) {
        x = static_cast<uint32_t>(kQ) - 1u;
    }
    uint64_t d0 = static_cast<uint64_t>(x) * 5284526080ull;
    d0 = (d0 + (static_cast<uint64_t>(1) << 32)) >> 33;
    return static_cast<int32_t>(d0 & 0x7ffu);
}

/**
 * ByteEncode_5：8 系数×5bit → 5B（FIPS Alg.5）。
 * @param out 输出字节缓冲（至少 5B×n/8）
 * @param in  压缩系数 LocalTensor
 * @param n   系数个数（256）
 */
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

/**
 * ByteEncode_11：8 系数×11bit → 11B。
 */
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
 * AIV-only：u[1024] int32、v[256] int32 → c[1568] uint8。
 * @param uGm  域元素 u（k·256，canonical mod q）
 * @param vGm  域元素 v（256）
 * @param cGm  密文 c = c₁‖c₂
 * 前置：blockDim=1；无 CrossCore。
 */
extern "C" __global__ __aicore__ void cipher_pack_custom(GM_ADDR uGm, GM_ADDR vGm, GM_ADDR cGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    AscendC::GlobalTensor<int32_t> gmU;
    AscendC::GlobalTensor<int32_t> gmV;
    AscendC::GlobalTensor<uint8_t> gmC;
    gmU.SetGlobalBuffer((__gm__ int32_t *)uGm, static_cast<uint32_t>(kUCoeffs));
    gmV.SetGlobalBuffer((__gm__ int32_t *)vGm, static_cast<uint32_t>(kPolyN));
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cGm, static_cast<uint32_t>(kCBytes));

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queIn;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queComp;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queBytes;
    // 单 poly 系数 1024B；c₁ poly 352B、c₂ 160B 均已 32B 对齐，可精确 DataCopy。
    constexpr uint32_t kPolyBytes = static_cast<uint32_t>(kPolyN) * sizeof(int32_t);
    pipe.InitBuffer(queIn, 1, kPolyBytes);
    pipe.InitBuffer(queComp, 1, kPolyBytes);
    pipe.InitBuffer(queBytes, 1, static_cast<uint32_t>(kC1PolyBytes)); // 352 ≥ 160

    AscendC::LocalTensor<int32_t> inLocal = queIn.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> compLocal = queComp.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> encLocal = queBytes.AllocTensor<uint8_t>();

    // ---- c₁：逐 poly Compress₁₁ + ByteEncode₁₁，写入 c[0 .. 1408) ----
    for (int32_t p = 0; p < kK; ++p) {
        AscendC::DataCopy(inLocal, gmU[static_cast<uint32_t>(p * kPolyN)],
                          static_cast<uint32_t>(kPolyN));
        AscendC::PipeBarrier<PIPE_ALL>();
        for (int32_t i = 0; i < kPolyN; ++i) {
            const int32_t u = inLocal.GetValue(static_cast<uint32_t>(i));
            compLocal.SetValue(static_cast<uint32_t>(i), Compress11(u));
        }
        AscendC::PipeBarrier<PIPE_ALL>();
        ByteEncode11(encLocal, compLocal, kPolyN);
        AscendC::PipeBarrier<PIPE_ALL>();
        AscendC::DataCopy(gmC[static_cast<uint32_t>(p * kC1PolyBytes)], encLocal,
                          static_cast<uint32_t>(kC1PolyBytes));
        AscendC::PipeBarrier<PIPE_ALL>();
    }

    // ---- c₂：Compress₅ + ByteEncode₅ → c[1408 .. 1568) ----
    AscendC::DataCopy(inLocal, gmV, static_cast<uint32_t>(kPolyN));
    AscendC::PipeBarrier<PIPE_ALL>();
    for (int32_t i = 0; i < kPolyN; ++i) {
        const int32_t v = inLocal.GetValue(static_cast<uint32_t>(i));
        compLocal.SetValue(static_cast<uint32_t>(i), Compress5(v));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    ByteEncode5(encLocal, compLocal, kPolyN);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmC[static_cast<uint32_t>(kC1Bytes)], encLocal,
                      static_cast<uint32_t>(kC2Bytes));
    AscendC::PipeBarrier<PIPE_ALL>();

    queIn.FreeTensor(inLocal);
    queComp.FreeTensor(compLocal);
    queBytes.FreeTensor(encLocal);
}
