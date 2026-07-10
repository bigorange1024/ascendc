/**
 * @file decompress_unified_int_custom.cpp
 * @brief 统一整数舍入 Decompress_d 单 launch（AIV-only）；默认 int32 全向量。
 * @see docs/notes/F203-Compress-Decompress-统一整数舍入技术总结.md
 */
#include "decompress_unified_int_config.hpp"
#include "f203_mlkem_params.h"
#include "f203_unified_round/f203_unified_decompress_vec.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void decompress_unified_int_custom(GM_ADDR comp_in, GM_ADDR poly_out,
                                                                    int32_t coeff_n)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ int32_t *)comp_in, n);
    gm_out.SetGlobalBuffer((__gm__ int32_t *)poly_out, n);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, f203_unified_round::kPolyLen * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

    f203_unified_round::poly_decompress_unified_local(out_local, in_local, tmp_local);

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
