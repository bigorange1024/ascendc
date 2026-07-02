/**
 * @file f203_encrypt_at_r5_tiling.h
 * @brief G3 合并核 at_r5 的常量与 scratch 偏移（kP=5，比原 at_r 多一列）。
 *
 * 背景（INTEGRATION_PLAN §2.3）：
 *   旧 G3 拆 4 核（g3_linear/g3_linear4/at_r/t_dot_r）+ 多 session 路径已证伪
 *   （[2026-06-30 funckey 病根纪要](../../../../qa/2026-06/2026-06-30-funckey-507000本地独立验证.md)）。
 *   新方案：单核 at_r5 收完 {û[0..3]+tr̂}，输入端 host 把 t̂ 当 Â 的「虚拟列 4」拼到 matM。
 *
 * 与 innerproduct_tiling 区别：
 *   - kP=5 而非 4（输出多 1 个 poly = tr̂）
 *   - matM 索引顺序为「先 j 后 p」：matM[(j*kP + p)*N + n]
 *   - kSVec（求和维度）保持 4
 *   - scratch 因此多 1*N int32（outLine 多 1 行）
 */
#pragma once

#include "innerproduct_tiling.h"

namespace at_r5_tiling {

constexpr int32_t kN = innerproduct_tiling::kN;            // 256
constexpr int32_t kK = innerproduct_tiling::kSVec;         // 4（求和维度）
constexpr int32_t kP = 5;                                  // 输出列数：û[0..3] + tr̂[4]
constexpr int32_t kBlockDim = innerproduct_tiling::kBlockDim;
constexpr int32_t kRomPairCount = innerproduct_tiling::kRomPairCount; // N/2
constexpr int32_t kVecWsInts = innerproduct_tiling::kVecWsInts;       // 8*N/2
constexpr int32_t kHatQ = innerproduct_tiling::kHatQ;                 // 3329

/** matM 字节数：[kK, kP, kN] int32（host 拼装；详见 layout.h::mat_offset） */
constexpr int32_t kMatBytes = kK * kP * kN * static_cast<int32_t>(sizeof(int32_t));
/** r̂ 字节数：[kK, kN] int32 */
constexpr int32_t kRBytes = kK * kN * static_cast<int32_t>(sizeof(int32_t));
/** uTr 字节数：[kP, kN] int32（uTr[0..3]=û、uTr[4]=tr̂） */
constexpr int32_t kOutBytes = kP * kN * static_cast<int32_t>(sizeof(int32_t));

/** scratch 偏移（int32 单位）—— 与 innerproduct 同构，仅 outLine 段 4N→5N。 */
constexpr int32_t kOffAcc = 0;                          // multiply_ntts UB 累加临时（N i32）
constexpr int32_t kOffRow = kN;                         // 单 poly basemul 结果（N i32）
constexpr int32_t kOffModT2 = 2 * kN;                   // mod_q_final_vec 临时（N i32）
constexpr int32_t kOffOutLine = 3 * kN;                 // 输出累加 line × kP（kP*N i32）
constexpr int32_t kScratchInts = kOffOutLine + kP * kN; // 3N + 5N = 8N = 2048 i32 = 8 KB

} // namespace at_r5_tiling
