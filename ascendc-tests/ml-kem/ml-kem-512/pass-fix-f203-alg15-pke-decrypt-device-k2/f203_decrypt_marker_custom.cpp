/**
 * @file f203_decrypt_marker_custom.cpp
 * @brief G0 占位 AIV kernel：写单个 int32 标记到 GM（冒烟 / 工具链探针）。
 *
 * 流水线位置：非 Alg.15 生产路径；用于确认 AIV-only launch 与 GM 写通。
 * 与 golden：无密码学对拍。
 */
#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

/**
 * 在 marker_out[0] 写入 marker_val。
 * @param marker_out GM int32×1
 * @param marker_val 任意哨兵值
 * 前置：AIV-only；本核不做 AIC。
 */
extern "C" __global__ __aicore__ void f203_decrypt_marker_custom(GM_ADDR marker_out, int32_t marker_val)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer((__gm__ int32_t *)marker_out, 1);
    AscendC::PipeBarrier<PIPE_ALL>();
    // 标量写 GM：仅冒烟，非生产数据面
    gm.SetValue(0, marker_val);
    AscendC::PipeBarrier<PIPE_ALL>();
}
