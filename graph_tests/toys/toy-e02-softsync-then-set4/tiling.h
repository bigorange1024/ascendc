#ifndef TOY_E02_SOFTSYNC_THEN_SET4_TILING_H
#define TOY_E02_SOFTSYNC_THEN_SET4_TILING_H

/**
 * @file tiling.h
 * @brief E02 极简 2-launch toy：L1 stub + L2 SoftSyncArrive → SET(4)；无 Encrypt 业务。
 *
 * Host 通过 TilingData.phase 区分两趟 MIX launch；同进程连续 ≥3 轮。
 * SoftSync 哨兵在 softSyncGm（int32[2]），与 decrypt skel 同构；本核仅用 slot0。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：phase 选择 L1 / L2。 */
struct TilingData {
    int32_t phase;    /**< 0=L1 stub；1=L2 SoftSync→SET(4) */
    int32_t reserved; /**< 保留 */
};

namespace tiling {

constexpr size_t kSrcBytes = 64;
constexpr size_t kOutBytes = 64;
/** 极小 workspace：占位（本核业务不用）。 */
constexpr size_t kWsBytes = 256;
constexpr size_t wssize = kWsBytes;
/** SoftSync 哨兵：int32[2]；slot0=本核 Arrive；slot1 预留。 */
constexpr size_t kSoftSyncBytes = 8;

constexpr int32_t kPhaseLaunch1 = 0;
constexpr int32_t kPhaseLaunch2 = 1;

/** magic：out[0..7]="E02TOY01"；out[8]=0xE2；其余 0xA5。 */
constexpr char kMagicPrefix[8] = {'E', '0', '2', 'T', 'O', 'Y', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicMark = 0xE2;

} // namespace tiling

#endif
