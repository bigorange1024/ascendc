/**
 * @file decaps_fo_custom.cpp
 * @brief RB-D06-decaps-fo / DRW-K03：单 AIV launch 设备侧 FO（隐式拒绝选路）写出 K[32]。
 *
 * 本文件在流水线中的位置：Decaps 重建战役第三刀（K03）。前置 K01 产 K'、K02 产 c'；
 * 本刀可独立喂中间量 (c, c', K', z)，在设备上完成：
 *   cmp ← (c ≟ c')；相等 → K ← K'；否则 → K ← J(z ‖ c)（SHAKE-256，32B）。
 * golden / 权威对拍仅 Host 侧 liboqs Decaps；本核不调用任何子进程。
 *
 * 契约（S0B FEEDBACK §2–§3 / Decrypt KB §A2）：
 *   - 输入：c[1568]、c'[1568]、K'[32]、z[32]（z 语义 = dk_kem[3136:3168) 切片）
 *   - 输出：K[32]；UB + DataCopy → GM（X12）；禁 GlobalTensor::SetValue 写业务 GM
 *   - 形态：KERNEL_TYPE_AIV_ONLY、blockDim=1；无 CrossCore / 无 flag 5/7
 *
 * 背景：T27 判决「SetValue 写 K → 错非挂」——本刀强制 DataCopy；未抄 T25–T27 /
 * alg21 / examples decrypt|decaps / frozen 源码。设备原语：shared
 * F203SeDeviceKeccak::Shake256OneShot（J）。
 */

#include "kernel_operator.h"
#include "fips203_device_sha3.hpp"

namespace {
/** ML-KEM-1024 密文长度（ByteEncode 打包后） */
constexpr uint32_t kCtBytes = 1568U;
/** 共享密钥 / z / K' 长度 */
constexpr uint32_t kHashHalf = 32U;
/** J 输入 z‖c = 32 + 1568 */
constexpr uint32_t kJInBytes = kHashHalf + kCtBytes;  // 1600
}  // namespace

/**
 * AIV-only：FO(c, c', K', z) → K[32]。
 *
 * @param cGm       输入密文 c[1568]（FO 比对原件；拒绝路径可为篡改向量）
 * @param cPrimeGm  输入重加密 c'[1568]（合法路径应与 c 逐字节相等）
 * @param kPrimeGm  输入 K'[32]（K01 G 输出前 32B；合法选路源）
 * @param zGm       输入 z[32]；契约语义 = dk_kem[3136:3168)
 * @param kOutGm    输出 K[32]；Host 清零后由本核 UB+DataCopy 写出
 * 前置条件：blockDim=1；KERNEL_TYPE_AIV_ONLY；无 MIX / CrossCore。
 *
 * 数学（FIPS 203 Alg.18 尾）：equal(c,c') ? K' : SHAKE256(z‖c, 32)。
 */
extern "C" __global__ __aicore__ void decaps_fo_custom(GM_ADDR cGm, GM_ADDR cPrimeGm,
                                                       GM_ADDR kPrimeGm, GM_ADDR zGm,
                                                       GM_ADDR kOutGm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    AscendC::GlobalTensor<uint8_t> gmC;
    AscendC::GlobalTensor<uint8_t> gmCp;
    AscendC::GlobalTensor<uint8_t> gmKp;
    AscendC::GlobalTensor<uint8_t> gmZ;
    AscendC::GlobalTensor<uint8_t> gmK;
    gmC.SetGlobalBuffer((__gm__ uint8_t *)cGm, kCtBytes);
    gmCp.SetGlobalBuffer((__gm__ uint8_t *)cPrimeGm, kCtBytes);
    gmKp.SetGlobalBuffer((__gm__ uint8_t *)kPrimeGm, kHashHalf);
    gmZ.SetGlobalBuffer((__gm__ uint8_t *)zGm, kHashHalf);
    gmK.SetGlobalBuffer((__gm__ uint8_t *)kOutGm, kHashHalf);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queC;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queCp;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queKp;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> queZ;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> queK;
    // DataCopy 按字节；1568=49×32、32 均满足 32B 对齐块。
    pipe.InitBuffer(queC, 1, kCtBytes);
    pipe.InitBuffer(queCp, 1, kCtBytes);
    pipe.InitBuffer(queKp, 1, kHashHalf);
    pipe.InitBuffer(queZ, 1, kHashHalf);
    pipe.InitBuffer(queK, 1, kHashHalf);

    AscendC::LocalTensor<uint8_t> cLocal = queC.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> cpLocal = queCp.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> kpLocal = queKp.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> zLocal = queZ.AllocTensor<uint8_t>();
    AscendC::LocalTensor<uint8_t> kLocal = queK.AllocTensor<uint8_t>();

    // ---- 1) GM → UB：四路只读输入 ----
    AscendC::DataCopy(cLocal, gmC, kCtBytes);
    AscendC::DataCopy(cpLocal, gmCp, kCtBytes);
    AscendC::DataCopy(kpLocal, gmKp, kHashHalf);
    AscendC::DataCopy(zLocal, gmZ, kHashHalf);
    AscendC::PipeBarrier<PIPE_ALL>();

    // ---- 2) 标量逐字节 c ≟ c'（1568B；无矢量 Compare，避免假绿掩码语义）----
    // 背景：FO 只关心全等布尔；未采用 Compares bit 打包读法（易误判）。
    bool equal = true;
    for (uint32_t i = 0; i < kCtBytes; ++i) {
        if (cLocal.GetValue(i) != cpLocal.GetValue(i)) {
            equal = false;
            break;
        }
    }

    if (equal) {
        // ---- 3a) 合法选路：K ← K'（拷到出队 UB）----
        for (uint32_t i = 0; i < kHashHalf; ++i) {
            kLocal.SetValue(i, kpLocal.GetValue(i));
        }
    } else {
        // ---- 3b) 拒绝选路：K ← J(z‖c) = SHAKE256(z‖c, 32) ----
        // 结论：J 输入必须在设备侧拼装；禁 Host 预喂最终 K。
        // 未采用项：用恒输出 K' 冒充拒绝（假绿）。
        uint8_t zc[kJInBytes];
        uint8_t jOut[kHashHalf];
        for (uint32_t i = 0; i < kHashHalf; ++i) {
            zc[i] = zLocal.GetValue(i);
        }
        for (uint32_t i = 0; i < kCtBytes; ++i) {
            zc[kHashHalf + i] = cLocal.GetValue(i);
        }
        F203SeDeviceKeccak::Shake256OneShot(jOut, kHashHalf, zc, kJInBytes);
        for (uint32_t i = 0; i < kHashHalf; ++i) {
            kLocal.SetValue(i, jOut[i]);
        }
    }

    // ---- 4) UB → GM：写出 K（X12；禁 GM SetValue）----
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gmK, kLocal, kHashHalf);

    queC.FreeTensor(cLocal);
    queCp.FreeTensor(cpLocal);
    queKp.FreeTensor(kpLocal);
    queZ.FreeTensor(zLocal);
    queK.FreeTensor(kLocal);
}
