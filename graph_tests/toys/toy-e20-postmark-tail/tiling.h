#ifndef TOY_E20_POSTMARK_TAIL_TILING_H
#define TOY_E20_POSTMARK_TAIL_TILING_H

/**
 * @file tiling.h
 * @brief E20 stub：E17 CrossCore 全序后 AIC 早退 + 双 AIV 非对称 DataCopy 尾包。
 *
 * 无业务 tiling 字段；ws 仅作假数据 GM 区供尾包 stub 读写。
 * magic 证明「全 Mark 之后」AIV-only 尾包仍可达。
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

/**
 * workspace：双 AIV 各占独立假数据区，避免尾包 DataCopy 写冲突。
 * 每区 ≥ kStubChunkBytes × maxRounds；对齐 256B。
 */
constexpr size_t kStubChunkBytes = 256; /**< 单轮 GM↔UB 块长（32B 对齐） */
constexpr uint32_t kStubRoundsHeavy = 6; /**< AIV0：模拟 v+两 poly（多干） */
constexpr uint32_t kStubRoundsLight = 2; /**< AIV1：模拟两 poly（少干） */
constexpr size_t kAivWsStride = 2048;    /**< 每 AIV 假数据区字节数 */
constexpr size_t kWsBytes = kAivWsStride * 2;
constexpr size_t wssize = kWsBytes;

/** magic：out[0..7]="E20TOY01"；out[8]=0xE0；其余 0xA5。 */
constexpr char kMagicPrefix[8] = {'E', '2', '0', 'T', 'O', 'Y', '0', '1'};
constexpr uint8_t kMagicFill = 0xA5;
constexpr uint8_t kMagicMark = 0xE0;

} // namespace tiling

#endif
