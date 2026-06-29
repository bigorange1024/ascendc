#include "byte_encode_d_vec.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void byte_encode_d_custom(GM_ADDR comp_in, GM_ADDR encoded_out, int32_t coeff_n)
{
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<uint8_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ int32_t *)comp_in, n);
    gm_out.SetGlobalBuffer((__gm__ uint8_t *)encoded_out, byte_encode_d::kOutBytes);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, byte_encode_d::kOutBytes);
    pipe.InitBuffer(tmp_buf, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> out_local = que_out.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

    byte_encode_d::poly_byte_encode_local(out_local, in_local, tmp_local);

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, byte_encode_d::kOutBytes);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
