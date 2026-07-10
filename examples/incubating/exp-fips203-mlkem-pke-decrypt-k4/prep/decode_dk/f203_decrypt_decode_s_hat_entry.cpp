/**
 * @file f203_decrypt_decode_s_hat_entry.cpp
 * @brief Decrypt 流水线 G2a 编译入口：直接包含独立 kernel 实现。
 *
 * 本 TU 仅作 CMake 目标源文件名；实现见同目录 f203_decrypt_decode_s_hat.cpp。
 * 生产 1-kernel fused 路径不 launch 本入口，而内联 decrypt_g4::decode_s_hat_impl。
 */
// Decrypt：dk → ŝ ByteDecode 设备入口。
// 流水线：Alg.15 准备段；输出 ŝ 供后续 NTT。
// 与 golden：中间态可不落盘，最终对拍 m。

#include "f203_decrypt_decode_s_hat.cpp"
