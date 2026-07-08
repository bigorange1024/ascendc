/**
 * @file byte_encode_d_custom.cpp
 * @brief ByteEncode_d 单 launch；宏默认 BYTE_ENCODE_D_VEC=1（见 byte_encode_d_config.hpp）。
 * 选型定稿：docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md
 */
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
#if BYTE_ENCODE_D_VEC >= 2
    // VEC=2 真·向量 pack 需更大 scratch（Gather 8 lane + byte-lane + 整字拼装，见 vp_* 分区，≥792 int32）。
    constexpr uint32_t scratchInt32 = 1024U;
#else
    const uint32_t scratchInt32 = n;
#endif
    pipe.InitBuffer(tmp_buf, scratchInt32 * sizeof(int32_t));

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
