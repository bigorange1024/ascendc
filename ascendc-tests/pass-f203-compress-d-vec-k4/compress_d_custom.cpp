/**
 * @file compress_d_custom.cpp
 * @brief Compress_d 单 launch 入口；d 由 CMake F203_COMPRESS_D 选定。
 * @see docs/notes/F203-Compress-Decompress-向量实现指南.md
 */
#include "compress_d_config.hpp"
#include "compress_d_vec.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void compress_d_custom(GM_ADDR poly_in, GM_ADDR comp_out, int32_t coeff_n)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ int32_t *)poly_in, n);
    gm_out.SetGlobalBuffer((__gm__ int32_t *)comp_out, n);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, compress_d::kPolyLen * sizeof(int32_t));

#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    AscendC::TBuf<AscendC::TPosition::VECCALC> f_buf;
    pipe.InitBuffer(f_buf, compress_d::kPolyLen * sizeof(float) * 3U);
#endif

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

#if F203_COMPRESS_USE_CAST_DIV && COMPRESS_D_VEC >= 1
    AscendC::LocalTensor<float> f_raw = f_buf.GetWithOffset<float>(compress_d::kPolyLen, 0U);
    AscendC::LocalTensor<float> f_tmp =
        f_buf.GetWithOffset<float>(compress_d::kPolyLen, compress_d::kPolyLen * sizeof(float));
    AscendC::LocalTensor<float> f_quot =
        f_buf.GetWithOffset<float>(compress_d::kPolyLen, compress_d::kPolyLen * 2U * sizeof(float));
    compress_d::poly_compress_cast_div_dispatch(out_local, in_local, tmp_local, f_raw, f_tmp, f_quot);
#else
    compress_d::poly_compress_local(out_local, in_local, tmp_local);
#endif

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
