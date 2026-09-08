/**
 * EN03 · L1 Prep AIV 轻桩（采样/解码外形）
 *
 * 作用：把 Host 喂入的 int32 系数块 GM→UB→GM 恒等拷贝，模拟 prep 外形体量。
 * 输入：src [n_elem] int32；输出：dst [n_elem] int32；n_elem 须 8 对齐（32B）。
 * 禁止：抄 Encrypt prep / alg7 / CBD；禁 CrossCore / GATE 4/8。
 */
#include "enc_aiv_stub_common.hpp"

extern "C" __global__ __aicore__ void enc_prep_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem)
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
    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));

    // GM → UB：整块 DataCopy（恒等拷贝级体量）
    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::DataCopy(in_local, gm_in, n);
    que_in.EnQue(in_local);
    in_local = que_in.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    // 轻量 Vec：Adds(+0) 保持外形，避免纯搬运被优化掉语义讨论
    AscendC::Adds(out_local, in_local, static_cast<int32_t>(0), n);
    AscendC::PipeBarrier<PIPE_V>();
    que_in.FreeTensor(in_local);

    que_out.EnQue(out_local);
    out_local = que_out.DeQue<int32_t>();
    AscendC::DataCopy(gm_out, out_local, n);
    que_out.FreeTensor(out_local);
}
