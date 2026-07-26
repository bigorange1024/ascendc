/**
 * @file byte_decode_d_custom.cpp
 * @brief ByteDecode_d 单 launch kernel 入口；d=5/11 标量 unpack（BYTE_DECODE_D_VEC 默认 1，与 0 同体）。
 * 在流水线中的位置：由 main.cpp 通过 ICPU_RUN_KF（CPU 孪生）或 ACLRT_LAUNCH_KERNEL（NPU/SIM）
 * 调起，是本探针唯一的 __global__ kernel；核心比特解包算法在 byte_decode_d_vec.hpp 中实现，
 * 本文件只负责 GM↔UB 搬运与 TPipe/TQue/TBuf 资源编排。
 * 选型：docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md
 */
#include "byte_decode_d_vec.hpp"
#include "kernel_operator.h"

/**
 * ByteDecode_d kernel：把 GM 上的 d-bit 打包比特流解码为 GM 上的系数数组。
 * @param encoded_in GM 输入指针，语义为 uint8[byte_decode_d::kInBytes] 的打包比特流
 *                   （FIPS 203 Alg.5 ByteEncode_d 的输出格式）
 * @param comp_out   GM 输出指针，语义为 int32[coeff_n] 的还原系数（每元素落在 [0, 2^d)）
 * @param coeff_n    系数个数（本探针恒为 F203_MLKEM_N=256，由 host 侧传入）
 * 前置条件：单核（blockDim=1）AIV-only 任务，无需跨核同步；GM 缓冲区大小需与
 *          main.cpp 侧按 F203_BYTE_DECODE_POLY_BYTES 分配的一致。
 */
extern "C" __global__ __aicore__ void byte_decode_d_custom(GM_ADDR encoded_in, GM_ADDR comp_out, int32_t coeff_n)
{
    /* 本 kernel 只做向量搬运与比特解包，不涉及矩阵计算，故声明为纯 AIV 任务。 */
    KERNEL_TASK_TYPE_DEFAULT(KERNEL_TYPE_AIV_ONLY);

    const uint32_t n = static_cast<uint32_t>(coeff_n);
    /* gm_in/gm_out：把裸 GM 指针包装为带元素类型与长度的 GlobalTensor 视图，
     * 长度分别为 kInBytes 个 uint8（打包输入）与 n 个 int32（还原系数）。 */
    AscendC::GlobalTensor<uint8_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer((__gm__ uint8_t *)encoded_in, byte_decode_d::kInBytes);
    gm_out.SetGlobalBuffer((__gm__ int32_t *)comp_out, n);

    AscendC::TPipe pipe;
    /* que_in/que_out：VECIN/VECOUT 队列各配 1 块缓冲，供 AllocTensor 分配 UB 空间搬入/搬出；
     * tmp_buf/hi_buf：VECCALC 标量域中间缓冲，仅 d=4 向量 nibble 路径使用
     *   （tmp 存低 4bit 掩码结果，hi 存 mask_low_bits_i32 内部移位中间量），
     *   d=5/10/11 标量 unpack 路径不使用但仍需分配（poly_byte_decode_local 统一签名）。 */
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;
    AscendC::TBuf<AscendC::TPosition::VECCALC> hi_buf;

    pipe.InitBuffer(que_in, 1, byte_decode_d::kInBytes);
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    /* tmp_buf/hi_buf 大小按 d=4 场景的 kPolyLen/2（=128 对）个 int32 分配，足够覆盖其余 d
     * 场景（不使用时对应缓冲区未被读写，只是占位）。 */
    pipe.InitBuffer(tmp_buf, (byte_decode_d::kPolyLen / 2U) * sizeof(int32_t));
    pipe.InitBuffer(hi_buf, (byte_decode_d::kPolyLen / 2U) * sizeof(int32_t));

    /* 从队列/TBuf 取出实际可读写的 LocalTensor（UB 上的向量视图）。 */
    AscendC::LocalTensor<uint8_t> in_local = que_in.AllocTensor<uint8_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();
    AscendC::LocalTensor<int32_t> hi_local = hi_buf.Get<int32_t>();

    /* GM → UB：把打包比特流搬入向量计算单元可访问的 UB 空间。 */
    AscendC::DataCopy(in_local, gm_in, byte_decode_d::kInBytes);
    /* 同步点：确保搬入完成后才能开始下面的比特解包运算（避免读到未搬完的脏数据）。 */
    AscendC::PipeBarrier<PIPE_ALL>();

    /* 核心比特解包：按当前 F203_BYTE_DECODE_D 选择 d4/d5/d10/d11 对应实现（见 byte_decode_d_vec.hpp）。 */
    byte_decode_d::poly_byte_decode_local(out_local, in_local, tmp_local, hi_local);

    /* 同步点：确保解包运算全部完成后才能把结果搬出，避免搬出未写完的数据。 */
    AscendC::PipeBarrier<PIPE_ALL>();
    /* UB → GM：把还原出的系数数组写回 GM，供 host 侧取回。 */
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
