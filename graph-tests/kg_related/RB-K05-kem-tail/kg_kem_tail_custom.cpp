/**
 * @file kg_kem_tail_custom.cpp
 * @brief RB-K05 / KGR-K01：KEM KeyGen L3 尾段（AIV-only）— H(ek)+z → dk_kem。
 *
 * 本文件在流水线中的位置：PKE KeyGen（RB-K04）产出 ek_pke[1568]+dk_pke[1536] 之后，
 * 本核完成 Alg.19 / Alg.16 增量尾段，写出完整 dk_kem[3168]（liboqs 布局）。
 * golden 仅 Host oracle 对拍，不参与设备路径。
 *
 * 布局契约（docs/notes/F203-KEM-Alg19…；禁抄 KeyGen 算子树）：
 *   dk_kem(3168) = dk_pke(1536) ‖ ek(1568) ‖ H(ek)(32) ‖ z(32)
 *   H(ek) = SHA3-256(ek)
 *   z     = SHA3-256("exp-mlkem-f203-kem-k4:SEED_Z=<SEED_D>")
 *         （对齐 scripts/liboqs_kem_fixture.py::derand_z_from_seed；默认 SEED_D=20260619）
 *
 * 工程约束：
 *   - basename：kg_kem_tail_custom（唯一）
 *   - KERNEL_TYPE_AIV_ONLY；blockDim=1；禁止与 L2b 深 CrossCore 融合
 *   - 写出：UB + DataCopy → GM（X12）；禁 GlobalTensor::SetValue 写业务 GM
 *   - 设备原语：library/shared Keccak → F203SeDeviceKeccak::Sha3OneShot(mdlen=32)
 *   - 哈希思路可参考 Encaps/Decrypt（T22/D04）；禁整核 fork / 禁抄 *keygen* / frozen
 */

#include "kernel_operator.h"
#include "fips203_device_sha3.hpp"

namespace {
/** ML-KEM-1024 PKE 公钥字节数（ByteEncode₁₂(t̂)‖ρ） */
constexpr uint32_t kEkBytes = 1568U;
/** ML-KEM-1024 PKE 私钥字节数（ByteEncode₁₂(ŝ)） */
constexpr uint32_t kDkPkeBytes = 1536U;
/** H / z 摘要长度 */
constexpr uint32_t kHashBytes = 32U;
/** liboqs 布局 dk_kem 总长 */
constexpr uint32_t kDkKemBytes = 3168U; // 1536+1568+32+32
/** seed_d 以 uint32 LE 落盘；DataCopy 按 32B 对齐块读 */
constexpr uint32_t kSeedPadBytes = 32U;
/** SHA3-256 mdlen */
constexpr int kSha3_256MdLen = 32;
/** z 域分离前缀（不含十进制 SEED_D） */
constexpr uint32_t kZPrefixLen = 29U; // strlen("exp-mlkem-f203-kem-k4:SEED_Z=")
/** 域分离消息最大长度：前缀 29 + uint32 最多 10 位十进制 */
constexpr uint32_t kZMsgMax = 40U;

/**
 * 将 uint32 无前导零写入十进制 ASCII（至少 1 位）。
 * @param dst 输出缓冲
 * @param v   无符号整数（本刀 SEED_D=20260619）
 * @return 写入字节数
 */
__aicore__ inline uint32_t U32ToDecimal(uint8_t *dst, uint32_t v)
{
    // 先写入临时倒序位，再反转到 dst（设备无 sprintf）
    uint8_t tmp[10];
    uint32_t n = 0;
    if (v == 0U) {
        dst[0] = static_cast<uint8_t>('0');
        return 1U;
    }
    while (v > 0U && n < 10U) {
        tmp[n++] = static_cast<uint8_t>('0' + (v % 10U));
        v /= 10U;
    }
    for (uint32_t i = 0; i < n; ++i) {
        dst[i] = tmp[n - 1U - i];
    }
    return n;
}

/**
 * 构造 z 域分离消息并 SHA3-256 → z[32]。
 * 背景：仓内约定与 liboqs fixture 一致；未采用「Host 预喂 z」路径。
 * @param seedD SEED_D（uint32）
 * @param zOut  输出 z[32]
 */
__aicore__ inline void DerandZFromSeedD(uint32_t seedD, uint8_t *zOut)
{
    // 前缀常量：与 scripts/liboqs_kem_fixture.py k==4 分支逐字节一致。
    // 背景：NPU/device 侧字符串字面量类型为 __gm__ char[]，不可赋给 const char*（CPU 孪生可过、真机编不过）。
    // 结论：用 UB 侧 constexpr uint8_t 表；未采用 const char* / reinterpret_cast。
    constexpr uint8_t kZPrefixBytes[kZPrefixLen] = {
        'e', 'x', 'p', '-', 'm', 'l', 'k', 'e', 'm', '-', 'f', '2', '0', '3', '-',
        'k', 'e', 'm', '-', 'k', '4', ':', 'S', 'E', 'E', 'D', '_', 'Z', '='};
    uint8_t msg[kZMsgMax];
    for (uint32_t i = 0; i < kZPrefixLen; ++i) {
        msg[i] = kZPrefixBytes[i];
    }
    const uint32_t digLen = U32ToDecimal(msg + kZPrefixLen, seedD);
    const uint32_t msgLen = kZPrefixLen + digLen;
    F203SeDeviceKeccak::Sha3OneShot(zOut, kSha3_256MdLen, msg, msgLen);
}
}  // namespace

/**
 * AIV-only：H(ek)+z → 拼 dk_kem；并写出中间量 h/z 供对拍。
 *
 * @param ekGm     输入 ek_pke[1568] uint8（PKE KeyGen 产出或同 SEED 自洽）
 * @param dkPkeGm  输入 dk_pke[1536] uint8
 * @param seedDGm  输入 seed_d：前 4B 为 uint32 LE（后垫至 32B）
 * @param hGm      输出 H(ek)[32]
 * @param zGm      输出 z[32]
 * @param dkKemGm  输出 dk_kem[3168] = dk_pke‖ek‖H(ek)‖z
 * 前置条件：blockDim=1；KERNEL_TYPE_AIV_ONLY；无 MIX / CrossCore。
 */
extern "C" __global__ __aicore__ void kg_kem_tail_custom(GM_ADDR ekGm, GM_ADDR dkPkeGm,
                                                         GM_ADDR seedDGm, GM_ADDR hGm,
                                                         GM_ADDR zGm, GM_ADDR dkKemGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    AscendC::GlobalTensor<uint8_t> gmEk;
    AscendC::GlobalTensor<uint8_t> gmDkPke;
    AscendC::GlobalTensor<uint8_t> gmSeed;
    AscendC::GlobalTensor<uint8_t> gmH;
    AscendC::GlobalTensor<uint8_t> gmZ;
    AscendC::GlobalTensor<uint8_t> gmDkKem;
    gmEk.SetGlobalBuffer((__gm__ uint8_t *)ekGm, kEkBytes);
    gmDkPke.SetGlobalBuffer((__gm__ uint8_t *)dkPkeGm, kDkPkeBytes);
    gmSeed.SetGlobalBuffer((__gm__ uint8_t *)seedDGm, kSeedPadBytes);
    gmH.SetGlobalBuffer((__gm__ uint8_t *)hGm, kHashBytes);
    gmZ.SetGlobalBuffer((__gm__ uint8_t *)zGm, kHashBytes);
    gmDkKem.SetGlobalBuffer((__gm__ uint8_t *)dkKemGm, kDkKemBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queInEk;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queInDk;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queInSeed;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOutH;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOutZ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queOutChunk;
    // DataCopy 按字节：ek/dk_pke 整段对齐 32B；seed/h/z 各 32B
    pipe.InitBuffer(queInEk, 1, kEkBytes);
    pipe.InitBuffer(queInDk, 1, kDkPkeBytes);
    pipe.InitBuffer(queInSeed, 1, kSeedPadBytes);
    pipe.InitBuffer(queOutH, 1, kHashBytes);
    pipe.InitBuffer(queOutZ, 1, kHashBytes);
    // 复用大块：先写 dk_pke 段（1536），再写 ek 段（1568）到 dk_kem
    pipe.InitBuffer(queOutChunk, 1, kEkBytes);

    AscendC::LocalTensor<uint8_t> ekLocal = queInEk.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> dkLocal = queInDk.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> seedLocal = queInSeed.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> hLocal = queOutH.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> zLocal = queOutZ.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> chunkLocal = queOutChunk.AllocTensor<uint8_t>();

    // ---- 1) GM → UB：ek / dk_pke / seed_d ----
    AscendC::DataCopy(ekLocal, gmEk, kEkBytes);
    AscendC::DataCopy(dkLocal, gmDkPke, kDkPkeBytes);
    AscendC::DataCopy(seedLocal, gmSeed, kSeedPadBytes);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 2) H(ek)=SHA3-256：Sha3OneShot 不接 __gm__*，先摊到栈 ----
    // 背景：与 Encaps 设备 H(ek) 同式栈缓冲；未 fork T22 整核。
    uint8_t ekStack[kEkBytes];
    for (uint32_t i = 0; i < kEkBytes; ++i) {
        ekStack[i] = ekLocal.GetValue(i);
    }
    uint8_t hStack[kHashBytes];
    F203SeDeviceKeccak::Sha3OneShot(hStack, kSha3_256MdLen, ekStack, kEkBytes);

    // ---- 3) z ← Derand(SEED_D) ----
    // seed_d.bin：小端 uint32 在前 4B
    const uint32_t seedD =
        static_cast<uint32_t>(seedLocal.GetValue(0)) |
        (static_cast<uint32_t>(seedLocal.GetValue(1)) << 8) |
        (static_cast<uint32_t>(seedLocal.GetValue(2)) << 16) |
        (static_cast<uint32_t>(seedLocal.GetValue(3)) << 24);
    uint8_t zStack[kHashBytes];
    DerandZFromSeedD(seedD, zStack);

    // ---- 4) h/z 写入 UB，再 DataCopy → 独立输出 GM（X12）----
    for (uint32_t i = 0; i < kHashBytes; ++i) {
        hLocal.SetValue(i, hStack[i]);
        zLocal.SetValue(i, zStack[i]);
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmH, hLocal, kHashBytes);
    AscendC::DataCopy(gmZ, zLocal, kHashBytes);

    // ---- 5) 拼 dk_kem：分段 UB+DataCopy（禁 GM SetValue）----
    // 5a) dk_kem[0:1536) ← dk_pke
    for (uint32_t i = 0; i < kDkPkeBytes; ++i) {
        chunkLocal.SetValue(i, dkLocal.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    {
        AscendC::GlobalTensor<uint8_t> gmSeg;
        gmSeg.SetGlobalBuffer((__gm__ uint8_t *)dkKemGm + 0, kDkPkeBytes);
        AscendC::DataCopy(gmSeg, chunkLocal, kDkPkeBytes);
    }

    // 5b) dk_kem[1536:3104) ← ek
    for (uint32_t i = 0; i < kEkBytes; ++i) {
        chunkLocal.SetValue(i, ekLocal.GetValue(i));
    }
    AscendC::PipeBarrier<PIPE_ALL>();
    {
        AscendC::GlobalTensor<uint8_t> gmSeg;
        gmSeg.SetGlobalBuffer((__gm__ uint8_t *)dkKemGm + kDkPkeBytes, kEkBytes);
        AscendC::DataCopy(gmSeg, chunkLocal, kEkBytes);
    }

    // 5c) dk_kem[3104:3136) ← H(ek)；[3136:3168) ← z
    AscendC::PipeBarrier<PIPE_ALL>();
    {
        AscendC::GlobalTensor<uint8_t> gmSegH;
        gmSegH.SetGlobalBuffer((__gm__ uint8_t *)dkKemGm + kDkPkeBytes + kEkBytes, kHashBytes);
        AscendC::DataCopy(gmSegH, hLocal, kHashBytes);
        AscendC::GlobalTensor<uint8_t> gmSegZ;
        gmSegZ.SetGlobalBuffer((__gm__ uint8_t *)dkKemGm + kDkPkeBytes + kEkBytes + kHashBytes,
                               kHashBytes);
        AscendC::DataCopy(gmSegZ, zLocal, kHashBytes);
    }

    queInEk.FreeTensor(ekLocal);
    queInDk.FreeTensor(dkLocal);
    queInSeed.FreeTensor(seedLocal);
    queOutH.FreeTensor(hLocal);
    queOutZ.FreeTensor(zLocal);
    queOutChunk.FreeTensor(chunkLocal);
}
