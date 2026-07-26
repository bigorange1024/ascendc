/**
 * @file f203_decrypt_decode_s_hat_entry.cpp
 * @brief Alg.15 行 5 独立入口壳：仅 #include 实现 TU（decode_s_hat）。
 *
 * 流水线位置：历史多 launch / 单测编译单元；生产 1-kernel fused
 * 直接调 decrypt_g4::decode_s_hat_impl，本文件保证符号可链。
 * 与 golden：Host decode_s_hat 对拍中间态 ŝ（门控 G2）。
 */
#include "f203_decrypt_decode_s_hat.cpp"
