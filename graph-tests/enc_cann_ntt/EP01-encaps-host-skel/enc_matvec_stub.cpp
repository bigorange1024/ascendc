/**
 * EN03 · L3 Matvec AIV 轻桩（域运算外形）
 *
 * 作用：有界真 Vec MAC — dst = src * scale + bias（scale/bias 标量，体量有界）。
 * 输入：src [n_elem] int32；输出：dst [n_elem] int32；n_elem 须 8 对齐。
 * 背景：KB A2 要求 Matvec 默认 AIV_ONLY，禁止 AIC 空等 GATE 4/8。
 * 未采用：Cube matmul、GATE 4/8、抄 MultiplyNTTs/Encrypt at_jp。
 */
#include "enc_aiv_stub_common.hpp"

extern "C" __global__ __aicore__ void enc_matvec_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem,
                                                      int32_t scale, int32_t bias)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

    const uint32_t n = static_cast<uint32_t>(n_elem);
    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(src), n);
    gm_out.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst), n);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    AscendC::TBuf<AscendC::TPosition::VECCALC> tmp_buf;
    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));
    pipe.InitBuffer(tmp_buf, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::DataCopy(in_local, gm_in, n);
    que_in.EnQue(in_local);
    in_local = que_in.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    AscendC::LocalTensor<int32_t> tmp_local = tmp_buf.Get<int32_t>();

    // 有界 Vec MAC：tmp = src * scale；dst = tmp + bias
    AscendC::Muls(tmp_local, in_local, scale, n);
    AscendC::PipeBarrier<PIPE_V>();
    AscendC::Adds(out_local, tmp_local, bias, n);
    AscendC::PipeBarrier<PIPE_V>();
    que_in.FreeTensor(in_local);

    que_out.EnQue(out_local);
    out_local = que_out.DeQue<int32_t>();
    AscendC::DataCopy(gm_out, out_local, n);
    que_out.FreeTensor(out_local);
}
