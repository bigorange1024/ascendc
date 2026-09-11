/**
 * @file f203_decrypt_trace_layout.h
 * @brief Decrypt TRACE 布局常量（Host / 设备共用；不含 AscendC API）。
 */
#ifndef F203_DECRYPT_TRACE_LAYOUT_H
#define F203_DECRYPT_TRACE_LAYOUT_H

#include <cstddef>
#include <cstdint>

namespace f203_decrypt_trace {

constexpr uint32_t kAlignInts = 8U;
constexpr size_t kTraceSlots = 17U;
constexpr size_t kTraceBytes = kTraceSlots * static_cast<size_t>(kAlignInts) * sizeof(int32_t);
constexpr size_t kOnesOffBytes = kTraceBytes;
constexpr size_t kTraceDevBytes = kTraceBytes + static_cast<size_t>(kAlignInts) * sizeof(int32_t);

} // namespace f203_decrypt_trace

#endif
