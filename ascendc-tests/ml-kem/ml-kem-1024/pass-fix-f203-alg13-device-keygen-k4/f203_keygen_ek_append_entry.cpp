// @probe pass-fix-f203-alg13-device-keygen-k4
// @file f203_keygen_ek_append_entry.cpp
// @layer host
// @role ek_pke 追加/融合 launch 入口（与 keygen 链衔接）。 / ek_pke append entry.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_pke.bin (1568B) + dk_pke.bin (1536B)；中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_pke+dk_pke out; no intermediate GM dumps.
// @launch N/A（host / 脚本 / CMake 不参与 device launch）
// @ai_core N/A（非 AI Core 内核源）
// @depends #include: f203_keygen_layout.h, kernel_operator.h
// @verify 随 run.sh 全链或子目录 run_orchestrated/sim_*.sh 验收。


/**
 * @file f203_keygen_ek_append_entry.cpp
 * @brief Alg.13 行 21：ek_PKE = ByteEncode₁₂(t̂) polyvec ‖ ρ（GM 拼接）。
 *
 * ## 流水线位置
 * 设备侧 AIV_ONLY 小核：把已编码的 `ek_polyvec` 与 `rho` 拼成 `ek_pke`。
 * 生产全链中行 21 多在 `mmad_custom`（`F203_KEYGEN_EK_PKE`）内融合；本 TU 供 G1/legacy 单独验收。
 *
 * ## 与 golden 关系
 * I/O：入 1536+32B，出 1568B；与 Host golden `ek_polyvec||rho` 字节一致即可。
 *
 * ## 输入 / 输出（GM）
 * - `ek_polyvec_gm`：ByteEncode₁₂(t̂)，k=4 → 4×384=1536B
 * - `rho_gm`：公钥种子 ρ，32B
 * - `ek_pke_gm`：拼接结果，1568B
 */
#include "f203_keygen_layout.h"

#include "kernel_operator.h"

using namespace F203Keygen;

/**
 * 设备核：仅 blockIdx==0 执行；UB 中转两段 DataCopy 拼到 ek_pke。
 * @param ek_polyvec_gm 已编码公钥多项式向量
 * @param rho_gm        ρ（32B）
 * @param ek_pke_gm     输出 ek_PKE
 * 前置：`GetBlockIdx()!=0` 直接返回，避免多核重复写。
 */
extern "C" __global__ __aicore__ void f203_keygen_ek_append(GM_ADDR ek_polyvec_gm, GM_ADDR rho_gm, GM_ADDR ek_pke_gm)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    if (AscendC::GetBlockIdx() != 0U) {
        return;
    }

    // 绑定三段 GM（字节视图）
    AscendC::GlobalTensor<uint8_t> ekSrc;
    ekSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_polyvec_gm), kEkPolyvecBytes);
    AscendC::GlobalTensor<uint8_t> rhoSrc;
    rhoSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho_gm), kRhoBytes);
    AscendC::GlobalTensor<uint8_t> ekDst;
    ekDst.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_pke_gm), kEkPkeBytes);

    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufEk;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufRho;
    pipe.InitBuffer(bufEk, kEkPolyvecBytes);
    pipe.InitBuffer(bufRho, 32U);

    AscendC::LocalTensor<uint8_t> ekUb = bufEk.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> rhoUb = bufRho.Get<uint8_t>();

    // GM→UB→GM：两段拼接，避免跨 32B 边界的一次大块写
    AscendC::DataCopy(ekUb, ekSrc, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(rhoUb, rhoSrc, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(ekDst, ekUb, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    // ρ 写在 ek_polyvec 之后（偏移 1536）
    AscendC::DataCopy(ekDst[kEkPolyvecBytes], rhoUb, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
}

#ifndef __CCE_KT_TEST__
/**
 * Host 侧 ACL 启动包装：<<<blockDim, l2ctrl, stream>>> 调设备核。
 * @param blockDim 通常为 1；仅 AIV0 干活
 */
extern "C" void f203_keygen_ek_append_do(uint32_t blockDim, void *l2ctrl, void *stream, uint8_t *ek_polyvec_gm,
                                         uint8_t *rho_gm, uint8_t *ek_pke_gm)
{
    f203_keygen_ek_append<<<blockDim, l2ctrl, stream>>>(ek_polyvec_gm, rho_gm, ek_pke_gm);
}
#endif
