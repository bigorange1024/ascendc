/**
 * @file f203_decrypt_marker_custom.cpp
 * @brief Decrypt 流水线 G0 占位 AIV kernel（冒烟 / 工具链连通性）。
 *
 * 非 Alg.15 密码学步骤：仅向 GM 写一个 int32 标记值，验证 AIV launch 与 GM 可见性。
 * 生产 1-kernel fused 路径不调用本入口；golden I/O 不依赖本文件。
 */
#include "f203_decrypt_layout.h"
#include "kernel_operator.h"

/**
 * 占位 kernel：marker_out[0] ← marker_val。
 * @param marker_out 输出 GM（至少 1×int32）；@param marker_val 写入值
 */
extern "C" __global__ __aicore__ void f203_decrypt_marker_custom(GM_ADDR marker_out, int32_t marker_val)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);
    AscendC::GlobalTensor<int32_t> gm;
    gm.SetGlobalBuffer((__gm__ int32_t *)marker_out, 1);
    AscendC::PipeBarrier<PIPE_ALL>();
    gm.SetValue(0, marker_val);
    AscendC::PipeBarrier<PIPE_ALL>();
}
