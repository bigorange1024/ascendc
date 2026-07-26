// @probe pass-fix-f203-alg19-kem-keygen-device-k3
// @file compute/f203_keygen_ek_pke_fuse.hpp
// @layer compute
// @role compute/：Tag5T NTT + Alg.11 basemul + 行18–20 UB 融合 MMAD 内核与 host 驱动；第二次 launch，读 prep 写 GM + LUT，写 ek/sk 与 ek_pke。 / Full keygen compute (mmad_custom) sources. 本文件 `f203_keygen_ek_pke_fuse.hpp` 为该子模块组件。 / Component: f203_keygen_ek_pke_fuse.hpp.
// @production_io 默认 run.sh 生产 I/O：input/ 仅 seed_d.bin + lut_even/odd_stacked.bin；output/ ek_kem.bin (1184B) + dk_kem.bin (2400B)；D13 PKE 中间 GM 不落盘。 / Default production I/O: seed+LUT in; ek_kem+dk_kem out; no intermediate GM dumps.
// @launch mmad launch: blockDim=1, MIX_AIC_1_2（1×AIC + 2×AIV 融合 NTT+Alg11+行18–20）
// @ai_core SIM 剖面：mmad 段 1×AIC + 2×AIV；CPU SUCCESS 中 AIC_x 为 tikicpu artifact。
// @depends #include: f203_keygen_layout.h, kernel_operator.h
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。


/**
 * @file f203_keygen_ek_pke_fuse.hpp
 * @brief Alg.13 行 21：ek_PKE = ByteEncode₁₂(t̂) ‖ ρ（设备侧融合）。
 *
 * ## 流水线位置
 * Launch 2（mmad_custom）末尾、ByteEncode 之后；仅 AIV0 且 F203_KEYGEN_EK_PKE=1。
 * ρ 来自 prep Launch 写入的 rho_gm。
 *
 * ## 对齐与 golden
 * ML-KEM-768（k=3）：1152B polyvec + 32B ρ = 1184B；与 golden_ek_pke.bin I/O 等价。
 */
#pragma once

#include "f203_keygen_layout.h"
#include "kernel_operator.h"

namespace F203KeygenEkPke {

using namespace F203Keygen;

/**
 * 将 ek_polyvec 与 ρ 拼接写入 ek_pke GM。
 * @param ek_polyvec_gm 输入：1152B 编码公钥多项式向量
 * @param rho_gm        输入：32B ρ（prep 产出）
 * @param ek_pke_gm     输出：1184B 生产公钥
 * 前置：仅 subBlockID==0 的 AIV 调用；ByteEncode 已写完 ek_polyvec。
 */
__aicore__ inline void FuseEkPke(GM_ADDR ek_polyvec_gm, GM_ADDR rho_gm, GM_ADDR ek_pke_gm)
{
    AscendC::GlobalTensor<uint8_t> ekSrc;
    ekSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_polyvec_gm), kEkPolyvecBytes);
    AscendC::GlobalTensor<uint8_t> rhoSrc;
    rhoSrc.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(rho_gm), kRhoBytes);
    AscendC::GlobalTensor<uint8_t> ekDst;
    ekDst.SetGlobalBuffer(reinterpret_cast<__gm__ uint8_t *>(ek_pke_gm), kEkPkeBytes);

    // 独立小 TPipe：ek 与 ρ 各一块 UB，避免与上游 UbPipeline 生命周期纠缠
    AscendC::TPipe pipe;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufEk;
    AscendC::TBuf<AscendC::TPosition::VECCALC> bufRho;
    pipe.InitBuffer(bufEk, kEkPolyvecBytes);
    pipe.InitBuffer(bufRho, 32U);

    AscendC::LocalTensor<uint8_t> ekUb = bufEk.Get<uint8_t>();
    AscendC::LocalTensor<uint8_t> rhoUb = bufRho.Get<uint8_t>();

    // GM→UB：先 ek 再 ρ
    AscendC::DataCopy(ekUb, ekSrc, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(rhoUb, rhoSrc, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    // UB→GM：ek 写偏移 0，ρ 紧随其后（偏移 kEkPolyvecBytes）
    AscendC::DataCopy(ekDst, ekUb, kEkPolyvecBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(ekDst[kEkPolyvecBytes], rhoUb, kRhoBytes);
    AscendC::PipeBarrier<PIPE_ALL>();
}

}  // namespace F203KeygenEkPke
