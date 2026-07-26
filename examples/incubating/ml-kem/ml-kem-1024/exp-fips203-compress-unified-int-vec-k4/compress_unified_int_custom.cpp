/**
 * @file compress_unified_int_custom.cpp
 * @brief 统一整数舍入 Compress_d 单 launch 设备核（AIV-only，N=256）。
 *
 * 流水线位置：本目录 exp 探针 kernel；生产路径在 stable Encrypt tail 内联同公式 limb 宽乘。
 * 与 golden：I/O 对拍 compress_unified_int_ref.c（同 C=41285357 与 bias/shift）。
 * 规格：exp-fips203-compress-unified-int-vec-k4-实现方案-customspec.pdf
 *
 * @param poly_in  GM int32[N] 输入系数 u∈[0,q)
 * @param comp_out GM int32[N] 输出压缩域值（末步 mask 到 d bit 由共享头完成）
 * @param coeff_n  系数个数（固定 256，host 传入 F203_MLKEM_N）
 */
#include "compress_unified_int_config.hpp"
#include "f203_mlkem_params.h"
#include "f203_unified_round/f203_unified_compress_vec.hpp"
#include "kernel_operator.h"

extern "C" __global__ __aicore__ void compress_unified_int_custom(GM_ADDR poly_in, GM_ADDR comp_out, int32_t coeff_n)
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
#if COMPRESS_UNIFIED_INT_VEC >= 1
    // 向量路径：4 路独立 int32 scratch（lo/hi/carry/tmp），禁止与 out 别名
    AscendC::TBuf<AscendC::TPosition::VECCALC> scratch_buf;
    pipe.InitBuffer(scratch_buf, f203_unified_round::kPolyLen * sizeof(int32_t) * 4U);
#endif

    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();

    // GM → UB：整 poly 一次 DataCopy（探针单核握满 256 lane）
    AscendC::DataCopy(in_local, gm_in, n);
    AscendC::PipeBarrier<PIPE_ALL>();

#if COMPRESS_UNIFIED_INT_VEC >= 1
    const uint32_t plen = f203_unified_round::kPolyLen;
    const uint32_t w = plen * sizeof(int32_t);
    AscendC::LocalTensor<int32_t> lo_local = scratch_buf.GetWithOffset<int32_t>(plen, 0U);
    AscendC::LocalTensor<int32_t> hi_local = scratch_buf.GetWithOffset<int32_t>(plen, w);
    AscendC::LocalTensor<int32_t> carry_local = scratch_buf.GetWithOffset<int32_t>(plen, w * 2U);
    AscendC::LocalTensor<int32_t> tmp_local = scratch_buf.GetWithOffset<int32_t>(plen, w * 3U);
    // 统一 limb 宽乘：见 library/shared/f203_unified_round/f203_unified_compress_vec.hpp
    f203_unified_round::poly_compress_unified_local(out_local, in_local, lo_local, hi_local, carry_local,
                                                    tmp_local);
#else
    // 标量对照：逐 lane int64 宽乘（COMPRESS_UNIFIED_INT_VEC=0）
    AscendC::LocalTensor<int32_t> dummy = out_local;
    f203_unified_round::poly_compress_unified_local(out_local, in_local, dummy, dummy, dummy, dummy);
#endif

    AscendC::PipeBarrier<PIPE_ALL>();
    AscendC::DataCopy(gm_out, out_local, n);

    que_in.FreeTensor(in_local);
    que_out.FreeTensor(out_local);
}
