/**
 * @file byte_encode_d_custom.cpp
 * @brief ByteEncode_d 单 launch kernel 入口；宏默认 BYTE_ENCODE_D_VEC=1（见 byte_encode_d_config.hpp）。
 * 在流水线中的位置：由 main.cpp 通过 ICPU_RUN_KF（CPU 孪生）或 ACLRT_LAUNCH_KERNEL（NPU/SIM）
 * 调起，是本探针唯一的 __global__ kernel；核心比特打包算法在 byte_encode_d_vec.hpp 中实现，
 * 本文件只负责 GM↔UB 搬运与 TPipe/TQue/TBuf 资源编排。
 * 选型定稿：docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md
 */
#include "byte_encode_d_vec.hpp"
#include "kernel_operator.h"

/**
 * ByteEncode_d kernel：把 GM 上的压缩系数数组编码为 GM 上的 d-bit 打包比特流。
 * @param comp_in    GM 输入指针，语义为 int32[coeff_n] 的 Compress_d 输出（每元素 < 2^d）
 * @param encoded_out GM 输出指针，语义为 uint8[byte_encode_d::kOutBytes] 的打包比特流
 * @param coeff_n    系数个数（本探针恒为 F203_MLKEM_N=256，由 host 侧传入）
 * 前置条件：单核（blockDim=1）AIV-only 任务，无需跨核同步；GM 缓冲区大小需与
 *          main.cpp 侧按 F203_BYTE_ENCODE_POLY_BYTES 分配的一致。
 */
extern "C" __global__ __aicore__ void byte_encode_d_custom(GM_ADDR comp_in, GM_ADDR encoded_out, int32_t coeff_n)
{
    /* 本 kernel 只做向量搬运与比特打包，不涉及矩阵计算，故声明为纯 AIV 任务。 */
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    /* gm_in/gm_out：把裸 GM 指针包装为带元素类型与长度的 GlobalTensor 视图，
     * 长度分别为 n 个 int32（压缩系数）与 kOutBytes 个 uint8（打包输出）。 */
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<uint8_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ int32_t *)comp_in, n);
    gm_out.SetGlobalBuffer((__gm__ uint8_t *)encoded_out, byte_encode_d::kOutBytes);

    AscendC::TPipe pipe;
    /* que_in/que_out：VECIN/VECOUT 队列各配 1 块缓冲，供 AllocTensor 分配 UB 空间搬入/搬出；
     * tmp_buf：VECCALC 标量域中间缓冲，供 mask_low_bits_i32 等就地向量运算使用。 */
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, byte_encode_d::kOutBytes);
#if BYTE_ENCODE_D_VEC >= 2
    // VEC=2 真·向量 pack 需更大 scratch（Gather 8 lane + byte-lane + 整字拼装，见 vp_* 分区，≥792 int32）。
    constexpr uint32_t scratchInt32 = 1024U;
#else
    /* VEC∈{0,1}：tmp_buf 仅用于 mask_low_bits_i32 的中间结果，与输入系数数组同长 n 即可。 */
    const uint32_t scratchInt32 = n;
#endif
    pipe.InitBuffer(tmp_buf, scratchInt32 * sizeof(int32_t));

    /* 从队列/TBuf 取出实际可读写的 LocalTensor（UB 上的向量视图）。 */
    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<uint8_t> out_local = que_out.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    /* GM → UB：把压缩系数搬入向量计算单元可访问的 UB 空间。 */
    AscendC::DataCopy(in_local, gm_in, n);
    /* 同步点：确保搬入完成后才能开始下面的比特打包运算（避免读到未搬完的脏数据）。 */
    AscendC::PipeBarrier<PIPE_ALL>();

    /* 核心比特打包：按当前 F203_BYTE_ENCODE_D 选择 d4/d5/d10/d11 对应实现（见 byte_encode_d_vec.hpp）。 */
    byte_encode_d::poly_byte_encode_local(out_local, in_local, tmp_local);

    /* 同步点：确保打包运算全部完成后才能把结果搬出，避免搬出未写完的数据。 */
    AscendC::PipeBarrier<PIPE_ALL>();
    /* UB → GM：把打包好的比特流写回 GM，供 host 侧取回。 */
    AscendC::DataCopy(gm_out, out_local, byte_encode_d::kOutBytes);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
