/**
 * @file decompress_d_custom.cpp
 * @brief Decompress_d 单 launch；默认 DECOMPRESS_D_VEC=1 向量 per-lane。
 * 见 docs/notes/F203-Compress-Decompress-向量实现指南.md
 *
 * 本文件在流水线中的位置：本探针唯一的 AscendC kernel 入口（AIV-only），由 main.cpp 通过
 * ICPU_RUN_KF（CPU 孪生）或 ACLRT_LAUNCH_KERNEL（NPU/SIM）调起；具体解压算法委托给
 * decompress_d_vec.hpp 中的 poly_decompress_local，本文件只负责 GM↔UB 搬运、buffer 申请
 * 与流水编排。与 golden 的关系：输出 poly_out 须与 scripts/gen_data.py 生成的
 * output/golden_poly.bin 逐系数一致（由 verify_result.py 对拍）。
 */
#include "decompress_d_vec.hpp"
#include "kernel_operator.h"

/**
 * Decompress_d 单核 kernel。
 * @param comp_in GM 指针，输入压缩域系数，dtype int32，形状 [coeff_n]（本探针恒为 [256]），
 *                值 ∈ [0, 2^d-1]。
 * @param poly_out GM 指针，输出解压后的多项式系数，dtype int32，形状 [coeff_n]，
 *                 值 ∈ [0, q-1]（canonical mod q）。
 * @param coeff_n 参与计算的系数个数（=F203_MLKEM_N=256，由 host 传入）。
 * 前置条件：仅使用 AIV（向量核），无需 AIC 参与（KERNEL_TYPE_AIV_ONLY）；单 block（blockDim=1），
 * 无跨核同步/跨核数据交换需求，属于纯 per-lane 线性运算探针。
 */
extern "C" __global__ __aicore__ void decompress_d_custom(GM_ADDR comp_in, GM_ADDR poly_out, int32_t coeff_n)
{
    // 声明本 kernel 仅需 AIV（向量计算单元），不启用 AIC（矩阵单元）。
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    // 将 host 传入的 GM 裸指针包装为带长度信息的 GlobalTensor，供 DataCopy 使用。
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ int32_t *)comp_in, n);
    gm_out.SetGlobalBuffer((__gm__ int32_t *)poly_out, n);

    // TPipe 管理本 kernel 的 UB 分配；que_in/que_out 为搬入/搬出队列，
    // tmp_buf 是 poly_decompress_local 向量路径中乘加运算的中间结果 scratch。
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    // GM → UB：一次性搬入整段压缩域系数；PipeBarrier<PIPE_ALL> 是本 kernel 唯一的
    // 搬运/计算同步点，确保数据落地后才进入解压计算（数据量小，无需分块流水）。
    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

    decompress_d::poly_decompress_local(out_local, in_local, tmp_local);

    // 计算 → UB 就绪后再搬出，避免与 DataCopy(gm_out,...) 产生读写竞争。
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
