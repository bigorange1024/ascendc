#ifndef TOY_E01_2LAUNCH_SET4_TRACE_REPEAT_TILING_H
#define TOY_E01_2LAUNCH_SET4_TRACE_REPEAT_TILING_H

/**
 * @file tiling.h
 * @brief E01 极简 2-launch toy：L1 stub + L2 Wait(4)/SET(4)；无 Encrypt 业务。
 *
 * Host 通过 TilingData.phase 区分两趟 MIX launch；同进程连续 8 轮。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2。 */
struct TilingData {
    int32_t phase;    /**< 0=L1 stub；1=L2 Wait/SET(4) */
    int32_t reserved; /**< 保留 */
};

namespace tiling {

constexpr size_t kSrcBytes = 64;
constexpr size_t kOutBytes = 64;
/** 极小 workspace：仅供 L2 写一字节完成标记（非业务）。 */
constexpr size_t kWsBytes = 256;
constexpr size_t wssize = kWsBytes;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

/** magic：out[0..7]="E01TOY01"；out[8]=0xE1；其余 0xA5。 */
constexpr char kMagicPrefix[8] = {'E', '0', '1', 'T', 'O', 'Y', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicMark = 0xE1;

} // namespace tiling

#endif
