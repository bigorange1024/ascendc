// @probe pass-fix-f203-alg14-pke-encrypt-device-k2
// @file prep/presample/f203_se_device_keccak.hpp
// @layer prep
// @role prep/presample：SHAKE/PRF/CBD 预采样与 NTT17 链入口；从 seed 派生设备侧中间量供 alg7/alg8/ahat。 / Presample + Keccak/PRF device vector entry. 本文件 `f203_se_device_keccak.hpp` 为该子模块组件。 / Component: f203_se_device_keccak.hpp.
// @production_io D14 默认 I/O：input/ ek_pke.bin(800B)+m.bin+coins.bin；output/c.bin(768B)；中间 GM 不落盘。
// @launch prep launch: blockDim=2, AIV_ONLY（双 AIV 分担 presample/alg7/alg8/ahat 链）
// @ai_core SIM 剖面：prep 0×AIC+2×AIV；双 AIV 并行 Â（blockIdx 分片）；block0 独占 PRF+CBD；CPU AIC_* 为 tikicpu 伪影。
// @depends #include: fips203_device_sha3.hpp
// @verify 经 main_keygen 或 split main_* + run.sh；SIM/CPU golden 或生产 cmp。



// @encrypt_probe_note 本文件在 D14 prep 中提供设备侧 Keccak/SHAKE 基础能力；生产入口为 f203_encrypt_prep_entry.cpp。
/**
 * @file f203_se_device_keccak.hpp
 * @brief 兼容头：实现已迁至 library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp
 */
#pragma once
#include "fips203_device_sha3.hpp"
