#ifndef TOY_E18_L18_FSM_SEP56_TILING_H
#define TOY_E18_L18_FSM_SEP56_TILING_H

/**
 * @file tiling.h
 * @brief E18 stub：单 launch MIX；伪 NTT(1/3)→GATE(4/8)→伪 INTT 独立(5/6)。
 *
 * 相对 E17 单因子：INTT 不再复用 1/3，改用独立 CrossCore 5/6。
 * 无业务 tiling 字段；Host 仅占位写入。magic 证明 kernel 跑完。
 */

#include <cstddef>
#include <cstdint>

/** Host 传入 kernel：本 toy 无 phase（单 launch）；保留字段占位。 */
struct TilingData {
    int32_t reserved0; /**< 占位 */
    int32_t reserved1; /**< 占位 */
};

namespace tiling {

constexpr size_t kSrcBytes = 64;
constexpr size_t kOutBytes = 64;
/** 极小 workspace：本 stub 不写业务，仅满足 launch 形参。 */
constexpr size_t kWsBytes = 256;
constexpr size_t wssize = kWsBytes;

/** magic：out[0..7]="E18TOY01"；out[8]=0xE8；其余 0xA5。 */
constexpr char kMagicPrefix[8] = {'E', '1', '8', 'T', 'O', 'Y', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicMark = 0xE8;

} // namespace tiling

#endif
