#ifndef TOY_E03_STAGE_SKEL_2LAUNCH_TILING_H
#define TOY_E03_STAGE_SKEL_2LAUNCH_TILING_H

/**
 * @file tiling.h
 * @brief E03 Encrypt 形态骨架：L1 采样 stub TRACE + L2 代数 stub TRACE + SET(4)。
 *
 * Host 通过 TilingData.phase 区分两趟 MIX launch；同进程默认 ≥3 轮。
 * 无真 SHAKE/NTT/点积；无 SoftSync（E02 已证极简非必要）。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2。 */
struct TilingData {
    int32_t phase;    /**< 0=L1 采样 stub；1=L2 代数 stub + Wait/SET(4) */
    int32_t reserved; /**< 保留 */
};

namespace tiling {

constexpr size_t kSrcBytes = 64;
constexpr size_t kOutBytes = 64;
/** 极小 workspace：占位（非业务）。 */
constexpr size_t kWsBytes = 256;
constexpr size_t wssize = kWsBytes;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

/** magic：out[0..7]="E03TOY01"；out[8]=0xE3；其余 0xA5。 */
constexpr char kMagicPrefix[8] = {'E', '0', '3', 'T', 'O', 'Y', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicMark = 0xE3;

} // namespace tiling

#endif
