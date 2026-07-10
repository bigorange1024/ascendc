/**
 * @file decompress_unified_int_custom.cpp
 * @brief 统一整数舍入 Decompress_d 单 launch 设备核（AIV-only，N=256）。
 *
 * 流水线位置：本目录 exp 探针 kernel；生产路径在 stable Decrypt unpack 内联同公式。
 * 与 golden：I/O 对拍 decompress_unified_int_ref.c（(c·q + 2^(d-1)) >> d）。
 * 规格：exp-fips203-decompress-unified-int-vec-k4-实现方案-customspec.pdf
 *
 * @param comp_in  GM int32[N] 压缩域系数 c∈[0,2^d-1]
 * @param poly_out GM int32[N] 解压后 Z_q 代表元
 * @param coeff_n  系数个数（固定 256）
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
    // tmp：承载 Muls(q)+Adds(bias) 中间结果，与 out 不可别名
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, f203_unified_round::kPolyLen * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

    // 默认向量三条指令；DECOMPRESS_UNIFIED_INT_VEC=0 时走标量 GetValue 对照
    f203_unified_round::poly_decompress_unified_local(out_local, in_local, tmp_local);

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
