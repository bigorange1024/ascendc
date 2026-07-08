/**
 * @file byte_decode_d_custom.cpp
 * @brief ByteDecode_d 单 launch；d=5/11 标量 unpack（BYTE_DECODE_D_VEC 默认 1，与 0 同体）。
 * 选型：docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md
 */
#include "byte_decode_d_vec.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void byte_decode_d_custom(GM_ADDR encoded_in, GM_ADDR comp_out, int32_t coeff_n)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    AscendC::GlobalTensor<uint8_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ uint8_t *)encoded_in, byte_decode_d::kInBytes);
    gm_out.SetGlobalBuffer((__gm__ int32_t *)comp_out, n);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> hi_buf;

    pipe.InitBuffer(que_in, 1, byte_decode_d::kInBytes);
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, (byte_decode_d::kPolyLen / 2U) * sizeof(int32_t));
    pipe.InitBuffer(hi_buf, (byte_decode_d::kPolyLen / 2U) * sizeof(int32_t));

    AscendC::LocalTensor<uint8_t> in_local = que_in.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();
    AscendC::LocalTensor<int32_t> hi_local = hi_buf.Get<int32_t>();

    AscendC::DataCopy(in_local, gm_in, byte_decode_d::kInBytes);
    AscendC::PipeBarrier<PIPE_ALL>();

    byte_decode_d::poly_byte_decode_local(out_local, in_local, tmp_local, hi_local);

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
