/**
 * @file aiv_encaps.cpp
 * @brief AE-P：单 AIV 单 launch 全 AscendC Encaps = FO + CBD + SampleNTT + ByteDecode + Encrypt。
 *
 * 流水线：
 *   1) H(ek)/G(m‖H)→K；coins→SHAKE256+CBD η=2 → y/e₁/e₂
 *   2) ρ→SampleNTT×16→Â；ek[:1536]→ByteDecode₁₂→t̂
 *   3) NTT/matvec/INTT/pack → c；K 设备写
 *
 * Host 仅喂 ek|m + NTT 表；workspace 由设备写。禁 Cube/CrossCore。
 */
#include "kernel_operator.h"
#include "constants.hpp"
#include "tables.hpp"
#include "fips203_device_sha3.hpp"
#include "f203_byte_codec/byte_decode12_vec.hpp"

using namespace AscendC;
using namespace aiv_kem;

namespace {
constexpr int K = 4;
constexpr int N = 256;
constexpr int kQ = 3329;
constexpr int CT_BYTES = 1568;
constexpr int U_BYTES = 1408; // 4 * 352
constexpr int DU = 11;
constexpr int DV = 5;
constexpr int EK_BYTES = 1568;
constexpr int K_BYTES = 32;
constexpr int XOF_BYTES = 672;
constexpr int CAND_PAIRS = 224;
}  // namespace

/**
 * 单 AIV Encrypt 核：UB 驻留单 poly 工作区，中间 ŷ/累加经 GM 周转。
 */
class AivEncaps {
public:
    /**
     * @param ekGm    ek_kem[1568] uint8 — 设备 FO 输入
     * @param kGm     输出 K[32]（设备 G 前半）
     * @param yGm..cGm 同 AE-E Encrypt 变换 I/O（采样可仍 Host 预喂）
     */
    __aicore__ inline void Run(GM_ADDR ekGm, GM_ADDR kGm, GM_ADDR yGm, GM_ADDR e1Gm, GM_ADDR e2Gm,
                               GM_ADDR aHatGm, GM_ADDR tHatGm, GM_ADDR mGm, GM_ADDR rootsGm,
                               GM_ADDR mapGm, GM_ADDR invRootsGm, GM_ADDR invMapGm,
                               GM_ADDR gammaGm, GM_ADDR yHatGm, GM_ADDR cGm)
    {
        pipe.InitBuffer(dataBuf, 1024);
        pipe.InitBuffer(packBuf, 1024);
        pipe.InitBuffer(tBuf, 1024);
        pipe.InitBuffer(sBuf, 1024);
        pipe.InitBuffer(a0Buf, 512);
        pipe.InitBuffer(a1Buf, 512);
        pipe.InitBuffer(b0Buf, 512);
        pipe.InitBuffer(b1Buf, 512);
        pipe.InitBuffer(h0Buf, 512);
        pipe.InitBuffer(h1Buf, 512);
        pipe.InitBuffer(rootBuf, kKemRootWords * 4);
        pipe.InitBuffer(invRootBuf, kKemRootWords * 4);
        pipe.InitBuffer(mapBuf, kMapWords * 4);
        pipe.InitBuffer(invMapBuf, kMapWords * 4);
        pipe.InitBuffer(gammaBuf, 512);
        pipe.InitBuffer(evenMapBuf, 512);
        pipe.InitBuffer(oddMapBuf, 512);
        pipe.InitBuffer(packMapBuf, 1024);
        pipe.InitBuffer(accBuf, 1024);
        pipe.InitBuffer(tmpBuf, 1024);
        pipe.InitBuffer(msgBuf, 32);
        pipe.InitBuffer(cPackBuf, 352); // 单块 ByteEncode 输出
        pipe.InitBuffer(ekBuf, EK_BYTES);
        pipe.InitBuffer(kOutBuf, 64); // K‖coins 暂存（写回仅 K[32]）

        auto data = dataBuf.Get<int32_t>();
        auto spare = packBuf.Get<int32_t>();
        auto rt = rootBuf.Get<int32_t>();
        auto irt = invRootBuf.Get<int32_t>();
        auto map = mapBuf.Get<uint32_t>();
        auto imap = invMapBuf.Get<uint32_t>();
        auto gamma = gammaBuf.Get<int32_t>();
        auto ge = evenMapBuf.Get<uint32_t>();
        auto go = oddMapBuf.Get<uint32_t>();
        auto gp = packMapBuf.Get<uint32_t>();
        auto acc = accBuf.Get<int32_t>();
        auto tmp = tmpBuf.Get<int32_t>();
        auto msg = msgBuf.Get<uint8_t>();

        GlobalTensor<int32_t> yG, e1G, e2G, aG, tG, yHatG, rootsG, invRootsG, gammaG;
        GlobalTensor<uint32_t> mapG, invMapG;
        GlobalTensor<uint8_t> mG, cG;

        yG.SetGlobalBuffer((__gm__ int32_t *)yGm, K * N);
        e1G.SetGlobalBuffer((__gm__ int32_t *)e1Gm, K * N);
        e2G.SetGlobalBuffer((__gm__ int32_t *)e2Gm, N);
        aG.SetGlobalBuffer((__gm__ int32_t *)aHatGm, K * K * N);
        tG.SetGlobalBuffer((__gm__ int32_t *)tHatGm, K * N);
        yHatG.SetGlobalBuffer((__gm__ int32_t *)yHatGm, K * N);
        rootsG.SetGlobalBuffer((__gm__ int32_t *)rootsGm, kKemRootWords);
        invRootsG.SetGlobalBuffer((__gm__ int32_t *)invRootsGm, kKemRootWords);
        mapG.SetGlobalBuffer((__gm__ uint32_t *)mapGm, kMapWords);
        invMapG.SetGlobalBuffer((__gm__ uint32_t *)invMapGm, kMapWords);
        gammaG.SetGlobalBuffer((__gm__ int32_t *)gammaGm, 128);
        mG.SetGlobalBuffer((__gm__ uint8_t *)mGm, 32);
        cG.SetGlobalBuffer((__gm__ uint8_t *)cGm, CT_BYTES);
        GlobalTensor<uint8_t> ekG, kG;
        ekG.SetGlobalBuffer((__gm__ uint8_t *)ekGm, EK_BYTES);
        kG.SetGlobalBuffer((__gm__ uint8_t *)kGm, K_BYTES);

        DataCopy(rt, rootsG, kKemRootWords);
        DataCopy(irt, invRootsG, kKemRootWords);
        DataCopy(map, mapG, kMapWords);
        DataCopy(imap, invMapG, kMapWords);
        DataCopy(gamma, gammaG, 128);
        DataCopy(msg, mG, 32);
        // 常量表：偶/奇 Gather 与 even|odd→自然序 Pack（字节偏移）
        for (int i = 0; i < 128; ++i) {
            ge.SetValue(i, static_cast<uint32_t>(8 * i));
            go.SetValue(i, static_cast<uint32_t>(8 * i + 4));
        }
        for (int i = 0; i < 256; ++i) {
            if ((i & 1) == 0) {
                gp.SetValue(i, static_cast<uint32_t>(4 * (i / 2)));
            } else {
                gp.SetValue(i, static_cast<uint32_t>(4 * (128 + i / 2)));
            }
        }
        PipeBarrier<PIPE_ALL>();

        // —— Alg.20 FO：H(ek)、G(m‖H) → K；并 CBD(η=2) 自 coins 写 y/e₁/e₂（P1 部分）——
        {
            auto ekLocal = ekBuf.Get<uint8_t>();
            auto kLocal = kOutBuf.Get<uint8_t>();
            DataCopy(ekLocal, ekG, EK_BYTES);
            PipeBarrier<PIPE_ALL>();
            uint8_t ekStack[EK_BYTES];
            for (int i = 0; i < EK_BYTES; ++i) {
                ekStack[i] = ekLocal.GetValue(i);
            }
            uint8_t hStack[32];
            F203SeDeviceKeccak::Sha3OneShot(hStack, 32, ekStack, static_cast<uint32_t>(EK_BYTES));
            uint8_t mh[64];
            for (int i = 0; i < 32; ++i) {
                mh[i] = msg.GetValue(i);
                mh[32 + i] = hStack[i];
            }
            uint8_t gOut[64];
            F203SeDeviceKeccak::Sha3OneShot(gOut, 64, mh, 64);
            for (int i = 0; i < 32; ++i) {
                kLocal.SetValue(i, gOut[i]);
            }
            PipeBarrier<PIPE_ALL>();
            DataCopy(kG, kLocal, K_BYTES);
            PipeBarrier<PIPE_ALL>();

            // coins = G[32:64)；PRF=SHAKE256(coins‖nonce)→128B → CBD η=2
            uint8_t coins[32];
            for (int i = 0; i < 32; ++i) {
                coins[i] = gOut[32 + i];
            }
            for (int nonce = 0; nonce < 9; ++nonce) {
                uint8_t seed[33];
                for (int i = 0; i < 32; ++i) {
                    seed[i] = coins[i];
                }
                seed[32] = static_cast<uint8_t>(nonce & 0xFF);
                uint8_t prf[128];
                F203SeDeviceKeccak::Shake256OneShot(prf, 128, seed, 33);
                // Alg.8 CBD η=2 → data[256]
                for (int i = 0; i < (N / 8); ++i) {
                    uint32_t t = static_cast<uint32_t>(prf[4 * i]) |
                                 (static_cast<uint32_t>(prf[4 * i + 1]) << 8) |
                                 (static_cast<uint32_t>(prf[4 * i + 2]) << 16) |
                                 (static_cast<uint32_t>(prf[4 * i + 3]) << 24);
                    uint32_t d = (t & 0x55555555u) + ((t >> 1) & 0x55555555u);
                    for (int j = 0; j < 8; ++j) {
                        int a = static_cast<int>((d >> (4 * j + 0)) & 0x3u);
                        int b = static_cast<int>((d >> (4 * j + 2)) & 0x3u);
                        int c = a - b;
                        if (c < 0) {
                            c += kQ;
                        }
                        data.SetValue(8 * i + j, c);
                    }
                }
                PipeBarrier<PIPE_ALL>();
                if (nonce < K) {
                    DataCopy(yG[nonce * N], data, N);
                } else if (nonce < 2 * K) {
                    DataCopy(e1G[(nonce - K) * N], data, N);
                } else {
                    DataCopy(e2G, data, N);
                }
                PipeBarrier<PIPE_ALL>();
            }

            // —— SampleNTT(Â)×16：ρ=ek[1536:1568]；种子 ρ‖j‖p（与 FIPS/liboqs 一致）——
            uint8_t rho[32];
            for (int i = 0; i < 32; ++i) {
                rho[i] = ekStack[1536 + i];
            }
            for (int p = 0; p < K; ++p) {
                for (int j = 0; j < K; ++j) {
                    uint8_t nttSeed[34];
                    for (int i = 0; i < 32; ++i) {
                        nttSeed[i] = rho[i];
                    }
                    nttSeed[32] = static_cast<uint8_t>(j & 0xFF);
                    nttSeed[33] = static_cast<uint8_t>(p & 0xFF);
                    uint8_t xof[XOF_BYTES];
                    F203SeDeviceKeccak::Shake128OneShot(xof, static_cast<uint32_t>(XOF_BYTES),
                                                       nttSeed, 34);
                    int nOut = 0;
                    for (int t = 0; t < CAND_PAIRS && nOut < N; ++t) {
                        const uint32_t c0 = static_cast<uint32_t>(xof[3 * t + 0]);
                        const uint32_t c1 = static_cast<uint32_t>(xof[3 * t + 1]);
                        const uint32_t c2 = static_cast<uint32_t>(xof[3 * t + 2]);
                        const uint32_t d1 = c0 + 256U * (c1 & 0x0FU);
                        const uint32_t d2 = (c1 >> 4) + 16U * c2;
                        if (d1 < static_cast<uint32_t>(kQ) && nOut < N) {
                            data.SetValue(nOut, static_cast<int32_t>(d1));
                            ++nOut;
                        }
                        if (d2 < static_cast<uint32_t>(kQ) && nOut < N) {
                            data.SetValue(nOut, static_cast<int32_t>(d2));
                            ++nOut;
                        }
                    }
                    while (nOut < N) {
                        data.SetValue(nOut, 0);
                        ++nOut;
                    }
                    PipeBarrier<PIPE_ALL>();
                    DataCopy(aG[(p * K + j) * N], data, N);
                    PipeBarrier<PIPE_ALL>();
                }
            }

            // —— ByteDecode₁₂(t̂)×4：ek[0:1536] ——
            const __gm__ uint8_t *ekGmBase = (__gm__ uint8_t *)ekGm;
            for (int poly = 0; poly < K; ++poly) {
                f203_byte_codec::poly_byte_decode12_scalar_gm(data, ekGmBase + poly * 384, N);
                PipeBarrier<PIPE_ALL>();
                DataCopy(tG[poly * N], data, N);
                PipeBarrier<PIPE_ALL>();
            }
        }

        // —— 行 16：ŷ ← NTT(r)（k=4 串行，同 launch；r 已由设备 CBD 写入）——
        for (int j = 0; j < K; ++j) {
            DataCopy(data, yG[j * N], N);
            PipeBarrier<PIPE_ALL>();
            FwdNtt(data, spare, rt, map);
            PipeBarrier<PIPE_ALL>();
            DataCopy(yHatG[j * N], data, N);
            PipeBarrier<PIPE_ALL>();
        }

        // —— 行 17：û ← Âᵀ ∘ ŷ；INTT；+e₁；Compress₁₁+ByteEncode ——
        for (int p = 0; p < K; ++p) {
            Duplicate(acc, 0, N);
            PipeBarrier<PIPE_V>();
            for (int j = 0; j < K; ++j) {
                // Âᵀ[p,j] = Â[j,p]，平坦偏移 (j*K+p)*N
                DataCopy(data, aG[(j * K + p) * N], N);
                DataCopy(tmp, yHatG[j * N], N);
                PipeBarrier<PIPE_ALL>();
                BasemulAdd(acc, data, tmp, gamma, ge, go, gp, spare);
                PipeBarrier<PIPE_V>();
            }
            BarrettCanonical(acc);
            PipeBarrier<PIPE_V>();
            InvNtt(acc, spare, irt, imap);
            PipeBarrier<PIPE_V>();
            DataCopy(tmp, e1G[p * N], N);
            PipeBarrier<PIPE_ALL>();
            Add(acc, acc, tmp, N);
            BarrettCanonical(acc);
            PipeBarrier<PIPE_V>();
            PackCompressU(cG, p, acc, spare, tBuf.Get<int32_t>());
        }

        // —— 行 18–19：v ← INTT(⟨t̂,ŷ⟩)+e₂+μ(m)；Compress₅+ByteEncode ——
        Duplicate(acc, 0, N);
        PipeBarrier<PIPE_V>();
        for (int j = 0; j < K; ++j) {
            DataCopy(data, tG[j * N], N);
            DataCopy(tmp, yHatG[j * N], N);
            PipeBarrier<PIPE_ALL>();
            BasemulAdd(acc, data, tmp, gamma, ge, go, gp, spare);
            PipeBarrier<PIPE_V>();
        }
        BarrettCanonical(acc);
        PipeBarrier<PIPE_V>();
        InvNtt(acc, spare, irt, imap);
        PipeBarrier<PIPE_V>();
        DataCopy(tmp, e2G, N);
        PipeBarrier<PIPE_ALL>();
        Add(acc, acc, tmp, N);
        PipeBarrier<PIPE_V>(); // V→S：随后 GetValue 读 acc
        // μ(m)：比特→{0,⌈q/2⌉}，标量嵌一次（32B）；主算术仍为向量
        constexpr int halfQ = (kQ + 1) / 2;
        for (int i = 0; i < 32; ++i) {
            uint8_t byte = msg.GetValue(i);
            for (int b = 0; b < 8; ++b) {
                if ((byte >> b) & 1) {
                    int32_t v = acc.GetValue(8 * i + b);
                    acc.SetValue(8 * i + b, v + halfQ);
                }
            }
        }
        PipeBarrier<PIPE_ALL>();
        BarrettCanonical(acc);
        PipeBarrier<PIPE_V>();
        PackCompressV(cG, acc, spare, tBuf.Get<int32_t>());
        PipeBarrier<PIPE_ALL>();
    }

private:
    /** Montgomery 蝶形一层（与 AV01 同构）：t=mont(b·ζ)；b←a-t；a←a+t。 */
    __aicore__ inline void Butterfly(LocalTensor<int32_t> a, LocalTensor<int32_t> b,
                                     LocalTensor<int32_t> rt, int stage)
    {
        auto t = tBuf.Get<int32_t>();
        auto s = sBuf.Get<int32_t>();
        Mul(t, b, rt[stage * 128], 128);
        Muls(s, t, -3327, 128);
        ShiftLeft(s, s, 16, 128);
        ShiftRight(s, s, 16, 128);
        Muls(s, s, 3329, 128);
        Sub(t, t, s, 128);
        ShiftRight(t, t, 16, 128);
        Sub(b, a, t, 128);
        Add(a, a, t, 128);
    }

    /**
     * 正向 NTT：7 层 + 末次 Gather + Barrett → 标准 bit-rev 偶奇对序。
     * 结果写回传入的 data 缓冲。
     */
    __aicore__ inline void FwdNtt(LocalTensor<int32_t> data, LocalTensor<int32_t> spare,
                                  LocalTensor<int32_t> rt, LocalTensor<uint32_t> map)
    {
        // ping=0 → 结果在 data；ping=1 → 结果在 spare
        int ping = 0;
        for (int stage = 0; stage < 7; ++stage) {
            if (stage == 0) {
                Butterfly(data, data[128], rt, stage);
            } else {
                LocalTensor<int32_t> src = (ping == 0) ? data : spare;
                LocalTensor<int32_t> dst = (ping == 0) ? spare : data;
                Gather(dst, src, map[(stage - 1) * 256], 0U, 256);
                Butterfly(dst, dst[128], rt, stage);
                ping ^= 1;
            }
        }
        LocalTensor<int32_t> src = (ping == 0) ? data : spare;
        LocalTensor<int32_t> dst = (ping == 0) ? spare : data;
        Gather(dst, src, map[6 * 256], 0U, 256);
        ping ^= 1;
        LocalTensor<int32_t> cur = (ping == 0) ? data : spare;
        BarrettCanonical(cur);
        if (ping != 0) {
            DataCopy(data, spare, 256);
        }
    }

    /**
     * 逆向 NTT（互逆）：逆末次 Gather 后，stage=6..0 做 CT 逆蝶形（含 ×2^{-1}）+ 逆层间 Gather。
     * 每层 /2 → 无需末级 ×128^{-1}；结果写回 data。
     */
    __aicore__ inline void InvNtt(LocalTensor<int32_t> data, LocalTensor<int32_t> spare,
                                  LocalTensor<int32_t> irt, LocalTensor<uint32_t> imap)
    {
        auto t = tBuf.Get<int32_t>();
        auto s = sBuf.Get<int32_t>();
        auto u = a0Buf.Get<int32_t>();
        Gather(spare, data, imap[6 * 256], 0U, 256);
        int ping = 1;
        for (int stage = 6; stage >= 0; --stage) {
            LocalTensor<int32_t> cur = (ping == 0) ? data : spare;
            auto a = cur;
            auto b = cur[128];
            // CT 逆：a=(a'+b')/2；t=(a'-b')/2；b=mont(t·ζ^{-1})
            Add(t, a, b, 128);
            Sub(s, a, b, 128);
            Muls(t, t, kHalf, 128);
            Muls(s, s, kHalf, 128);
            PipeBarrier<PIPE_V>(); // V→S：随后 GetValue 读 t/s
            // 约减到较小范围再 mont（half*sum 可至 ~2q*1665）
            for (int i = 0; i < 128; ++i) {
                int64_t xv = static_cast<int64_t>(t.GetValue(i)) % kQ;
                if (xv < 0) {
                    xv += kQ;
                }
                t.SetValue(i, static_cast<int32_t>(xv));
                xv = static_cast<int64_t>(s.GetValue(i)) % kQ;
                if (xv < 0) {
                    xv += kQ;
                }
                // center for mont
                if (xv > kQ / 2) {
                    xv -= kQ;
                }
                s.SetValue(i, static_cast<int32_t>(xv));
            }
            Mul(s, s, irt[stage * 128], 128);
            Muls(u, s, -3327, 128);
            ShiftLeft(u, u, 16, 128);
            ShiftRight(u, u, 16, 128);
            Muls(u, u, 3329, 128);
            Sub(s, s, u, 128);
            ShiftRight(b, s, 16, 128);
            DataCopy(a, t, 128);
            if (stage > 0) {
                LocalTensor<int32_t> dst = (ping == 0) ? spare : data;
                Gather(dst, cur, imap[(stage - 1) * 256], 0U, 256);
                ping ^= 1;
            }
        }
        LocalTensor<int32_t> cur = (ping == 0) ? data : spare;
        PipeBarrier<PIPE_ALL>();
        BarrettCanonical(cur);
        if (ping != 0) {
            DataCopy(data, spare, 256);
            PipeBarrier<PIPE_ALL>();
        }
    }

    /** 向量 Barrett 风格收到 [0,q)（与 AV01 收尾同构）。 */
    __aicore__ inline void BarrettCanonical(LocalTensor<int32_t> data)
    {
        auto t = tBuf.Get<int32_t>();
        auto s = sBuf.Get<int32_t>();
        PipeBarrier<PIPE_ALL>(); // 调用方可能刚 DataCopy 写入 data
        Muls(s, data, 20159, 256);
        Adds(s, s, 1 << 25, 256);
        ShiftRight(s, s, 26, 256);
        Muls(s, s, 3329, 256);
        Sub(data, data, s, 256);
        ShiftRight(t, data, 31, 256);
        Muls(t, t, -3329, 256);
        Add(data, data, t, 256);
    }

    /**
     * 对 ≈q² 量级中间积约减到 [0,q)。
     * 背景：AV01 收尾 magic=20159 在 Mul 后 ~q² 上会 int32 溢出。
     */
    __aicore__ inline void BarrettRed128(LocalTensor<int32_t> data)
    {
        PipeBarrier<PIPE_V>(); // V→S：Mul 后标量约减
        for (int i = 0; i < 128; ++i) {
            int64_t x = static_cast<int64_t>(data.GetValue(i));
            x %= kQ;
            if (x < 0) {
                x += kQ;
            }
            data.SetValue(i, static_cast<int32_t>(x));
        }
        PipeBarrier<PIPE_ALL>(); // S→V：后续向量读
    }

    /**
     * Alg.11：acc += f ⊙ g（NTT 域对乘）。偶/奇 Gather 后向量 Mul/Add，再 Pack 回自然序累加。
     * @param spare 长度 256 的 scratch，布局 [h0|h1] 后 Gather 成自然序。
     */
    __aicore__ inline void BasemulAdd(LocalTensor<int32_t> acc, LocalTensor<int32_t> f,
                                      LocalTensor<int32_t> g, LocalTensor<int32_t> gamma,
                                      LocalTensor<uint32_t> ge, LocalTensor<uint32_t> go,
                                      LocalTensor<uint32_t> gp, LocalTensor<int32_t> spare)
    {
        auto a0 = a0Buf.Get<int32_t>();
        auto a1 = a1Buf.Get<int32_t>();
        auto b0 = b0Buf.Get<int32_t>();
        auto b1 = b1Buf.Get<int32_t>();
        auto h0 = h0Buf.Get<int32_t>();
        auto h1 = h1Buf.Get<int32_t>();
        Gather(a0, f, ge[0], 0U, 128);
        Gather(a1, f, go[0], 0U, 128);
        Gather(b0, g, ge[0], 0U, 128);
        Gather(b1, g, go[0], 0U, 128);
        // h0 = a0*b0 + a1*b1*γ
        Mul(h0, a0, b0, 128);
        BarrettRed128(h0);
        Mul(h1, a1, b1, 128);
        BarrettRed128(h1);
        Mul(h1, h1, gamma, 128);
        BarrettRed128(h1);
        Add(h0, h0, h1, 128);
        BarrettRed128(h0);
        // h1 = a0*b1 + a1*b0
        Mul(h1, a0, b1, 128);
        BarrettRed128(h1);
        Mul(a0, a1, b0, 128);
        BarrettRed128(a0);
        Add(h1, h1, a0, 128);
        BarrettRed128(h1);
        // 拼 even|odd 到 spare，再 Pack 成自然序累加
        DataCopy(spare, h0, 128);
        DataCopy(spare[128], h1, 128);
        Gather(f, spare, gp[0], 0U, 256);
        Add(acc, acc, f, 256);
    }

    /** Compress₁₁ + ByteEncode₁₁ 写入 c 的第 p 个 352B 块。 */
    __aicore__ inline void PackCompressU(GlobalTensor<uint8_t> cG, int p,
                                         LocalTensor<int32_t> u, LocalTensor<int32_t> scratch,
                                         LocalTensor<int32_t> tmp)
    {
        (void)tmp;
        auto out = cPackBuf.Get<uint8_t>();
        for (int i = 0; i < N; ++i) {
            int64_t uu = static_cast<int64_t>(u.GetValue(i)) % kQ;
            if (uu < 0) {
                uu += kQ;
            }
            int64_t d0 = uu * 5284526080LL;
            d0 = (d0 + (1LL << 32)) >> 33;
            scratch.SetValue(i, static_cast<int32_t>(d0 & 0x7FF));
        }
        int bitPos = 0;
        uint8_t cur = 0;
        int curBits = 0;
        for (int i = 0; i < N; ++i) {
            int32_t a = scratch.GetValue(i) & 0x7FF;
            for (int j = 0; j < DU; ++j) {
                cur |= static_cast<uint8_t>((a & 1) << curBits);
                a >>= 1;
                curBits++;
                if (curBits == 8) {
                    out.SetValue(bitPos, cur);
                    bitPos++;
                    cur = 0;
                    curBits = 0;
                }
            }
        }
        PipeBarrier<PIPE_ALL>();
        DataCopy(cG[p * 352], out, 352);
        PipeBarrier<PIPE_ALL>();
    }

    /** Compress₅ + ByteEncode₅ → c[1408:1568]（160B）。 */
    __aicore__ inline void PackCompressV(GlobalTensor<uint8_t> cG, LocalTensor<int32_t> v,
                                         LocalTensor<int32_t> scratch, LocalTensor<int32_t> tmp)
    {
        (void)tmp;
        auto out = cPackBuf.Get<uint8_t>();
        for (int i = 0; i < N; ++i) {
            int64_t uu = static_cast<int64_t>(v.GetValue(i)) % kQ;
            if (uu < 0) {
                uu += kQ;
            }
            int64_t d0 = uu * 1290176LL;
            scratch.SetValue(i, static_cast<int32_t>(((d0 + (1LL << 26)) >> 27) & 0x1F));
        }
        int bitPos = 0;
        uint8_t cur = 0;
        int curBits = 0;
        for (int i = 0; i < N; ++i) {
            int32_t a = scratch.GetValue(i) & 0x1F;
            for (int j = 0; j < DV; ++j) {
                cur |= static_cast<uint8_t>((a & 1) << curBits);
                a >>= 1;
                curBits++;
                if (curBits == 8) {
                    out.SetValue(bitPos, cur);
                    bitPos++;
                    cur = 0;
                    curBits = 0;
                }
            }
        }
        PipeBarrier<PIPE_ALL>();
        DataCopy(cG[U_BYTES], out, 160);
        PipeBarrier<PIPE_ALL>();
    }

    TPipe pipe;
    TBuf<TPosition::VECCALC> dataBuf, packBuf, tBuf, sBuf;
    TBuf<TPosition::VECCALC> a0Buf, a1Buf, b0Buf, b1Buf, h0Buf, h1Buf;
    TBuf<TPosition::VECCALC> rootBuf, invRootBuf, mapBuf, invMapBuf, gammaBuf;
    TBuf<TPosition::VECCALC> evenMapBuf, oddMapBuf, packMapBuf, accBuf, tmpBuf, msgBuf, cPackBuf;
    TBuf<TPosition::VECCALC> ekBuf, kOutBuf;
};

/**
 * @brief 单 AIV Encaps 入口（设备 FO + Encrypt 变换）。
 */
extern "C" __global__ __aicore__ void aiv_encaps(
    GM_ADDR ekGm, GM_ADDR kGm, GM_ADDR yGm, GM_ADDR e1Gm, GM_ADDR e2Gm, GM_ADDR aHatGm,
    GM_ADDR tHatGm, GM_ADDR mGm, GM_ADDR rootsGm, GM_ADDR mapGm, GM_ADDR invRootsGm,
    GM_ADDR invMapGm, GM_ADDR gammaGm, GM_ADDR yHatGm, GM_ADDR cGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    AivEncaps kernel;
    kernel.Run(ekGm, kGm, yGm, e1Gm, e2Gm, aHatGm, tHatGm, mGm, rootsGm, mapGm, invRootsGm,
               invMapGm, gammaGm, yHatGm, cGm);
}
