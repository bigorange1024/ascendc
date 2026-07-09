/**
 * @file f203_decrypt_marker_custom.cpp
 * @brief G0 占位 AIV kernel。
 */
#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void f203_decrypt_marker_custom(GM_ADDR marker_out, int32_t marker_val)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer((__gm__ int32_t *)marker_out, 1);
    AscendC::PipeBarrier<PIPE_ALL>();
    gm.SetValue(0, marker_val);
    AscendC::PipeBarrier<PIPE_ALL>();
}
