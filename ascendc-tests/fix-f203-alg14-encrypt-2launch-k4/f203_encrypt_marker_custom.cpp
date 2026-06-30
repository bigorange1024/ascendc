/**
 * @file f203_encrypt_marker_custom.cpp
 * @brief Phase 0 占位 AIV kernel：写 GM 标记，证明设备 launch 路径可用。
 */
#include "f203_encrypt_layout.h"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_encrypt_marker_custom(GM_ADDR marker_out, int32_t marker_val)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer((__gm__ int32_t *)marker_out, 1);
    AscendC::PipeBarrier<PIPE_ALL>();
    gm.SetValue(0, marker_val);
    AscendC::PipeBarrier<PIPE_ALL>();
}
