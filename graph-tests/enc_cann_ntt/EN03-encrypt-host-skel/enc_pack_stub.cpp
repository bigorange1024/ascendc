/**
 * EN03 · L5 Pack AIV 轻桩（压缩编码外形）
 *
 * 作用：对 int32 系数做有界右移外形（模拟 compress 截断），再写回 GM。
 * 输入：src [n_elem] int32；输出：dst [n_elem] int32；n_elem 须 8 对齐。
 * 禁止：抄 ByteEncode/Compress 生产核；禁 CrossCore。
 */
#include "enc_aiv_stub_common.hpp"

extern "C" __global__ __aicore__ void enc_pack_stub(GM_ADDR dst, GM_ADDR src, int32_t n_elem,
                                                    int32_t shift_bits)
{
    ENC_STUB_KERNEL_TASK_TYPE();
    if (EncStubSkipIfAicPlaceholder()) {
        return;
    }
    if (!EncStubIsWorkerAiv()) {
        return;
    }

    const uint32_t n = static_cast<uint32_t>(n_elem);
    // 外形：shift 夹到 [0,8]，避免非法移位
    int32_t sh = shift_bits;
    if (sh < 0) {
        sh = 0;
    }
    if (sh > 8) {
        sh = 8;
    }

    AscendC::GlobalTensor<int32_t> gm_in;
    AscendC::GlobalTensor<int32_t> gm_out;
    gm_in.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(src), n);
    gm_out.SetGlobalBuffer(reinterpret_cast<__gm__ int32_t *>(dst), n);

    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::TPosition::VECIN, 1> que_in;
    AscendC::TQue<AscendC::TPosition::VECOUT, 1> que_out;
    pipe.InitBuffer(que_in, 1, n * sizeof(int32_t));
    pipe.InitBuffer(que_out, 1, n * sizeof(int32_t));

    AscendC::LocalTensor<int32_t> in_local = que_in.AllocTensor<int32_t>();
    AscendC::DataCopy(in_local, gm_in, n);
    que_in.EnQue(in_local);
    in_local = que_in.DeQue<int32_t>();

    AscendC::LocalTensor<int32_t> out_local = que_out.AllocTensor<int32_t>();
    // 压缩外形：算术右移（ShiftRight 对 int32 标量为 int32）；shift=0 时等价恒等
    AscendC::ShiftRight(out_local, in_local, sh, n);
    AscendC::PipeBarrier<PIPE_V>();
    que_in.FreeTensor(in_local);

    que_out.EnQue(out_local);
    out_local = que_out.DeQue<int32_t>();
    AscendC::DataCopy(gm_out, out_local, n);
    que_out.FreeTensor(out_local);
}
