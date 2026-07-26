/**
 * @file compress_d_custom.cpp
 * @brief Compress_d 单 launch；默认 COMPRESS_D_VEC=1 向量 per-lane（Encrypt tail 抄此路径）。
 * @see docs/notes/F203-Compress-Decompress-向量实现指南.md
 *
 * 本文件在流水线中的位置：本探针唯一的 AscendC kernel 入口（AIV-only），由 main.cpp 通过
 * ICPU_RUN_KF（CPU 孪生）或 ACLRT_LAUNCH_KERNEL（NPU/SIM）调起；具体压缩算法委托给
 * compress_d_vec.hpp 中的 poly_compress_* 系列函数，本文件只负责 GM↔UB 搬运、buffer 申请
 * 与流水编排。与 golden 的关系：输出 comp_out 须与 scripts/gen_data.py 生成的
 * output/golden_comp.bin 逐系数一致（由 verify_result.py 对拍）。
 */
#include "compress_d_config.hpp"
#include "compress_d_vec.hpp"
#include "kernel_operator.h"

/**
 * Compress_d 单核 kernel。
 * @param poly_in GM 指针，输入多项式系数，dtype int32，形状 [coeff_n]（本探针恒为 [256]），
 *                要求已是 canonical mod q（[0, q-1]）。
 * @param comp_out GM 指针，输出压缩域结果，dtype int32，形状 [coeff_n]，值 ∈ [0, 2^d-1]。
 * @param coeff_n 参与计算的系数个数（=F203_MLKEM_N=256，由 host 传入）。
 * 前置条件：仅使用 AIV（向量核），无需 AIC 参与（KERNEL_TYPE_AIV_ONLY）；单 block（blockDim=1），
 * 无跨核同步/跨核数据交换需求，属于纯 per-lane 运算探针。
 */
extern "C" __global__ __aicore__ void compress_d_custom(GM_ADDR poly_in, GM_ADDR comp_out, int32_t coeff_n)
{
    // 声明本 kernel 仅需 AIV（向量计算单元），不启用 AIC（矩阵单元）。
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    // 将 host 传入的 GM 裸指针包装为带长度信息的 GlobalTensor，供 DataCopy 使用。
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ int32_t *)poly_in, n);
    gm_out.SetGlobalBuffer((__gm__ int32_t *)comp_out, n);

    // TPipe 管理本 kernel 的 UB（Unified Buffer）分配；que_in/que_out 为搬入/搬出双缓冲队列，
    // tmp_buf 是压缩算法内部使用的整型 scratch（mask_low_bits_i32 等函数所需）。
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, compress_d::kPolyLen * sizeof(int32_t));

    // 仅 d=10/11（cast_div 路径）且向量开启时才需要额外的 float scratch：
    // f_buf 一次性申请 3×kPolyLen 个 float，分别切给 fRaw/fTmp/fQuot（见下方 GetWithOffset）。
#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    AscendC::TBuf<AscendC::TPosition::VECCALC> f_buf;
    pipe.InitBuffer(f_buf, compress_d::kPolyLen * sizeof(float) * 3U);
#endif

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    // GM → UB：把整段输入多项式一次性搬入 UB；PipeBarrier<PIPE_ALL> 确保搬运完成后才进入计算，
    // 是本 kernel唯一的搬运/计算同步点（搬运与计算无法重叠，因数据量小、无需分块流水）。
    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    // 按偏移（单位：字节）从 f_buf 中切出三段不重叠的 float LocalTensor：
    // f_raw 存放分子（float 形式）、f_tmp 存放分母 q（float 形式）、f_quot 存放浮点商。
    AscendC::LocalTensor<float> f_raw = f_buf.GetWithOffset<float>(compress_d::kPolyLen, 0U);
    AscendC::LocalTensor<float> f_tmp =
        f_buf.GetWithOffset<float>(compress_d::kPolyLen, compress_d::kPolyLen * sizeof(float));
    AscendC::LocalTensor<float> f_quot =
        f_buf.GetWithOffset<float>(compress_d::kPolyLen, compress_d::kPolyLen * 2U * sizeof(float));
    compress_d::poly_compress_cast_div_dispatch(out_local, in_local, tmp_local, f_raw, f_tmp, f_quot);
#else
    // d=4/5：向量 Barrett 或（COMPRESS_D_VEC=0 时）标量 fallback，见 poly_compress_local 内部分派。
    compress_d::poly_compress_local(out_local, in_local, tmp_local);
#endif

    // 计算 → UB 就绪后再搬出，避免与 DataCopy(gm_out,...) 产生读写竞争。
    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
