/**
 * @file f203_decrypt_trace.hpp
 * @brief Decrypt fused kernel 分段 TRACE（DataCopy 写 GM 槽；调试 env `F203_DECRYPT_TRACE=1`）。
 *
 * 背景：NPU prod input-only 路径可能卡在 CrossCore FSM 或 SoftSync 忙等；
 * 写槽与 Encrypt `FusedTraceMark` 同模式（标量）；AIC 可见性限制见 qa Encrypt trace 纪要。
 * 未采用：mark 内局部 TPipe+DataCopy（会破坏 fused 全链正确性）。
 */
#ifndef F203_DECRYPT_TRACE_HPP
#define F203_DECRYPT_TRACE_HPP

#include "f203_decrypt_trace_layout.h"
#include "kernel_operator.h"

namespace f203_decrypt_trace {

/** 逻辑槽编号；Host 轮询时打印已置位下标。 */
enum DecryptTraceSlot : int32_t {
    TR_AIV_PREP = 0,           /**< AIV0 unpack+decode 完成 */
    TR_AIV_SOFT0 = 1,          /**< SoftSync slot0（prep）双 AIV 齐 */
    TR_AIV_POST_PREP_GATE = 2, /**< prep GATE 4/8 后，NTT 段开始前 */
    TR_AIV_NTT_SPLIT = 3,      /**< NTT Stage1 split 完成 */
    TR_AIV_NTT_PACK = 4,       /**< NTT Stage3 pack 完成 */
    TR_AIV_NTT_MERGE = 5,      /**< NTT RouteA merge → û 完成 */
    TR_AIV_SU_DOT = 6,         /**< su_dot+pad 完成（AIV0） */
    TR_AIV_SOFT1 = 7,          /**< SoftSync slot1（su_dot）双 AIV 齐 */
    TR_AIV_POST_SU_GATE = 8,   /**< su GATE 4/8 后，INTT 段开始前 */
    TR_AIV_INTT_SPLIT = 9,     /**< INTT Stage1 split 完成 */
    TR_AIV_INTT_PACK = 10,     /**< INTT Stage3 pack 完成 */
    TR_AIV_INTT_MERGE = 11,    /**< INTT RouteA merge → w 完成 */
    TR_AIV_EXTRACT = 12,       /**< Compress₁/ByteEncode₁ → m 完成 */
    TR_AIC_PREP_GATE = 13,     /**< AIC：prep 段 WAIT(4) 进入 */
    TR_AIC_NTT_MMAD = 14,      /**< AIC：NTT MMAD 完成 */
    TR_AIC_SU_GATE = 15,       /**< AIC：su 段 WAIT(4) 进入 */
    TR_AIC_INTT_MMAD = 16,     /**< AIC：INTT MMAD 完成 */
};

/**
 * TRACE 写槽（AIV0 / AIC 标量写 GM；与 Encrypt `FusedTraceMark` 同模式）。
 *
 * 背景：GT-4 DataCopy+局部 TPipe 在 fused 全链内会破坏 AIV 向量流水（SIM 对拍 FAIL）；
 * 结论：先标量写槽保正确性；AIC 槽在部分环境 Host 可能假空（同 Encrypt 已知限制）。
 * 未采用：每次 mark 内 `TPipe`+`DataCopy`（仅 toy 尺度安全）。
 *
 * @param traceGm  TRACE GM；nullptr 时 no-op
 * @param slot     逻辑槽 0..16
 * @param aic      true=AIC
 * @param subBlockID AIV 子块（仅 0 写）
 */
__aicore__ inline void DecryptTraceMark(GM_ADDR traceGm, DecryptTraceSlot slot, const bool aic, int32_t subBlockID)
{
    if (traceGm == nullptr) {
        return;
    }
    if (!aic && subBlockID != 0) {
        return;
    }
    auto *trace = reinterpret_cast<__gm__ int32_t *>(traceGm);
    trace[static_cast<int32_t>(slot) * static_cast<int32_t>(kAlignInts)] = 1;
}

} // namespace f203_decrypt_trace

#endif
