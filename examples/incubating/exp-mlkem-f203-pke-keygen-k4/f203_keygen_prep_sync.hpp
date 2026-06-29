/**
 * @file f203_keygen_prep_sync.hpp
 * @brief KeyGen prep 编排层 Pipe 同步契约（对齐 customspec §数据与组件同步规划）。
 *
 * 范围：f203_keygen_prep 单 TPipe（行 3–15）；不含 compute/mmad_custom（独立 launch）。
 * 子模块（CBD/PRF/Â/Alg7）源码 vendored 于 prep/{alg8,presample,ahat,alg7}/。
 *
 * 规格 ID 对照：
 *   P-02  Â 结束 → PRF 复用 shakeXBuf/scratchBuf：PIPE_ALL
 *   P-03  PRF 结束 → CBD 读 prf_out GM：PIPE_ALL
 *   P-04  block0 PRF+CBD 结束；block1 汇合：PIPE_ALL
 *
 * CBD（f203_cbd_eta2_ub_io.hpp，ALG8_INC）：
 *   C-02  CopyIn 后 PIPE_MTE2；C-03  Vector 后 PIPE_V；C-04  CopyOut 后无 barrier
 *
 * 实验记录：docs/notes/F203-KeyGen-prep-Pipe细同步技术总结.md
 */
#pragma once

#include "kernel_operator.h"

namespace F203KeygenPrep {

/** prep 编排段间 / 双 AIV 汇合：保守 PIPE_ALL（P-02/P-03/P-04）。 */
#define F203_PREP_PIPE_ALL() AscendC::PipeBarrier<PIPE_ALL>()

}  // namespace F203KeygenPrep
