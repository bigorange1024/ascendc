#ifndef TOY_E19_EARLY_ENTRY_TRACE_TILING_H
#define TOY_E19_EARLY_ENTRY_TRACE_TILING_H

/**
 * @file tiling.h
 * @brief E19 stub：入口 fused-trace 槽 + flag1 桥 + SET4。
 *
 * fused-trace：int32[16]；入口槽 0=AIC（见 TRACE.md）、15=AIV0。
 */

#include <cstddef>
#include <cstdint>

struct TilingData {
    int32_t reserved0; /**< 占位 */
    int32_t reserved1; /**< 占位 */
};

namespace tiling {

constexpr size_t kSrcBytes = 64;
constexpr size_t kOutBytes = 64;
constexpr size_t kWsBytes = 256;
constexpr size_t wssize = kWsBytes;

constexpr int kTraceStages = 16;
constexpr size_t kTraceBytes = static_cast<size_t>(kTraceStages) * sizeof(int32_t);

/**
 * 入口槽语义（EARLY E19；非 Encaps 业务号）：
 * - 15：AIV0 在任何 CrossCore Wait 之前写 1。
 * - 0：AIC 入口标。AIC 在 Wait 前自写（NPU）；SIM 上 Host 靠 AIV0 在 Wait(1) 后桥写同槽。
 * TASK 允许「约定 entry 槽」；未用 14（与 15 同半区且 AIC 直写 SIM 不可见）。
 */
constexpr int kTraceSlotAicEntry = 0;
constexpr int kTraceSlotAiv0Entry = 15;

constexpr char kMagicPrefix[8] = {'E', '1', '9', 'T', 'O', 'Y', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicMark = 0xE9;

} // namespace tiling

#endif
