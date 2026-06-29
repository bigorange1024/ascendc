# Pipe 同步评估方案 — KeyGen G4 prep（Opt-5）

**探针**：`pass-fix-f203-alg13-device-keygen-k4`  
**范围**：`f203_keygen_prep` 单内核（行 3–15）；**不含** vec-k4-v2 compute（MIX）、`f203_keygen_ek_append`（G1）  
**基线**（910B4 SIM，双 AIV 默认）：prep tick **454170**；G4 CPU+SIM golden **PASS**  
**原理参考**：[`docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md`](../../docs/notes/F203-KeyGen-prep双AIV与SHAKE内嵌技术总结.md) · [`qa/2026-06/2026-06-22-Alg8-CBD-eta2-性能优化讨论.md`](../../qa/2026-06/2026-06-22-Alg8-CBD-eta2-性能优化讨论.md)  
**状态**：**Phase 1+5 已合入**（2026-06-26）；Phase 2–4 窄化 SIM 失败或 tick 回退，保持 `PIPE_ALL`

---

## 0. 目标与原则

| 目标 | 说明 |
|------|------|
| **窄化** | `PipeBarrier<PIPE_ALL>` → 定向 `PIPE_MTE2` / `PIPE_V` / `PIPE_MTE3`（或手册 event） |
| **删减** | 仅在 Phase 1–4 全 PASS 后，尝试去掉与下游重复的 barrier |
| **禁止** | 仅凭 CPU 通过删 barrier；**SIM golden** 为裁判 |

**生产默认配置**（本评估假定不变）：

- `F203_AHAT16_BLOCK_DIM=2`
- `F203_ALG7_REJ_IMPL=1`（Mins 剔除 + 标量 compact）
- `F203_ALG7_D12_GATHER=0`（标量解交织）
- `F203_AHAT16_BATCH_SHAKE=0`
- KeyGen CBD 路径：`f203_cbd_eta2_ub_io.hpp`（非 presample `f203_se_vector_cbd_ub.hpp`）

**建议实现开关**（后续编码时引入，默认 0 = 现行为）：

```text
F203_PREP_PIPE_FINE=0   # 0=PIPE_ALL（基线）；1=按本方案 Phase 已启用项窄化
```

---

## 1. 实验 Phase 与修改范围

| Phase | 修改文件（优先） | 验收 |
|-------|------------------|------|
| **1** | `pass-fix-f203-alg8-cbd-eta2-k4/f203_cbd_eta2_ub_io.hpp` | G4 CPU+SIM；prep tick |
| **2** | `pass-fix-f203-alg13-lines8-15-se-k4/f203_se_vector_prf.hpp` | 同上 |
| **3** | `pass-fix-f203-alg13-lines3-7-a-hat-k4/f203_a_hat16_ub.hpp`（循环 GM 边界） | 同上 |
| **4** | `pass-fix-f203-alg7-sample-ntt-k4/f203_alg7_{d12_vec,rej_vec,rej_filter}.hpp` | 同上；可先单 poly 探针再 G4 |
| **5** | 跨段重复 barrier 删减；`f203_keygen_prep_ub.hpp` 段间 | 同上 |
| **—** | `f203_keygen_prep_ub.hpp` L84/L97 | **Phase 5 前不动**（段间 + 双 block） |

**每 Phase 命令**：

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

---

## 2. 符号说明（修改建议列）

| 标记 | 含义 |
|------|------|
| **保留 ALL** | 维持 `PIPE_ALL`；跨段/跨 block/强依赖 |
| **→ MTE2** | CopyIn（GM→UB）后：`PipeBarrier<PIPE_MTE2>()` |
| **→ V** | Vector 写 UB 后、CopyOut 前：`PipeBarrier<PIPE_V>()` |
| **→ MTE3** | CopyOut（UB→GM）后：`PipeBarrier<PIPE_MTE3>()` |
| **→ V+MTE2** | 标量/Keccak 写 UB 后、Vector 读前：先 `PIPE_V` 或保守 `PIPE_ALL`（需 SIM 证） |
| **Phase N** | 建议在第 N 轮实验 |
| **删减?** | Phase 5 再试删除（常与下游 barrier 重叠） |
| **不热** | 当前 G4 prep 默认路径不执行 |

---

## 3. 编排层 — `f203_keygen_prep_ub.hpp`

| ID | 行 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|--------|------|-------|----------|
| P-01 | 20 | macro `F203_PREP_PIPE_ALL` | — | 5+ | 改为分档 macro：`PREP_SYNC_MTE2/V/ALL`；默认 ALL |
| P-02 | 84 | `BuildAHat16ShardWithUb` 返回后；PRF 复用 `shakeXBuf`/`aHatQue`/`scratchBuf` | 每核 1× | **5** | **保留 ALL**（Â→PRF 段间 + 双 AIV 汇合）；窄化风险高 |
| P-03 | 92 | `RunShakePrfBatchUbWithUb` 后 → CBD | block0 1× | **5** | 若 PRF 末 barrier 已保证 GM 写完成，可 **删减?**；否则 **→ MTE3** |
| P-04 | 97 | block0 PRF+CBD 结束；block1 须等待才能 return | 每核 1× | **—** | **保留 ALL**（跨 block 生命周期；不可删） |

---

## 4. CBD — `pass-fix-f203-alg8-cbd-eta2-k4/f203_cbd_eta2_ub_io.hpp`

KeyGen 经 `SamplePolyCbd2Batch8WithUb`（8 行 × 每行 3 barrier）。

| ID | 行 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|--------|------|-------|----------|
| C-01 | 27 | macro `F203_CBD_PIPE_ALL` | — | 1 | 拆为 `CBD_SYNC_AFTER_COPYIN`（MTE2）、`CBD_SYNC_BEFORE_COPYOUT`（V） |
| C-02 | 63 | `DataCopy` prf_gm→UB 后 → `SamplePolyCbd2RowSwLutUb` | 8×/prep | **1** | **→ MTE2**（qa 已证：无 barrier 则 SIM 对拍 FAIL） |
| C-03 | 67 | Vector CBD 后 → `EnQue`/准备 `DataCopy` 写 GM | 8× | **1** | **→ V** |
| C-04 | 72 | `DataCopy` src 写 GM 后 | 8× | **1** | **→ MTE3** 或 **删减?**（下一行若立即下一 row CopyIn，由 C-02 覆盖） |

---

## 5. PRF — `pass-fix-f203-alg13-lines8-15-se-k4/f203_se_vector_prf.hpp`

KeyGen 调用 `RunShakePrfBatchUbWithUb`（`PipeAll` ≡ `PIPE_ALL`）。

| ID | 行 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|--------|------|-------|----------|
| R-01 | 16 | macro `F203_SE_PRF_PIPE_ALL` → `ShakeXofUb::PipeAll()` | — | 2 | 与 shake 胶水统一命名 |
| R-02 | 55 | `FillPrfMessagesUb` 标量 SetValue 循环后 → SHAKE | 1×/prep | **2** | **→ V** 或保留 ALL；标量填 xUb 后 Keccak 读 xUb |
| R-03 | 73 | `RunKernelShakeGeneralUb`（SHAKE 写 yUb）后 → `DataCopy` 写 prf_out GM | 1× | **2** | **→ V** 再 CopyOut；或 **→ V** + CopyOut 前不再 ALL |
| R-04 | 78 | PRF 写 GM 完成后 | 1× | **2** | **→ MTE3** 或 **删减?**（CBD C-02 若已 MTE2 读 GM 可吸收） |

---

## 6. Â 16 poly — `pass-fix-f203-alg13-lines3-7-a-hat-k4/f203_a_hat16_ub.hpp`

G4 prep 走 **`BuildAHat16ShardWithUb`**（`F203_AHAT16_BATCH_SHAKE=0`）。下列 **A-06～A-08** 为每 poly 热路径（×8×2 核）。

| ID | 行 | 函数 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|------|--------|------|-------|----------|
| A-01 | 28 | — | macro | — | 3 | 同 CBD，拆 MTE2/V macro |
| A-02 | 50 | `CopyXofRowFromBatchUb` | batch 行拷贝后 | — | — | **不热**（BATCH_SHAKE=0） |
| A-03 | 76 | `FillBatchSampleSeedsRangeUb` | 填 batch 消息后 | — | — | **不热**（WithUb 路径不用） |
| A-04 | 132 | `SampleNttVecOnePolyFromXofUb` | rej 完成、`FreeTensor` d1/d2 后 | 8×/核 | **4** | 紧接 EnQue/DataCopy；**→ V**；**不可删**（scratch 下 poly 复用） |
| A-05 | 184 | `BuildAHat16ShardWithUb` | `InitAlg7InterleaveRomUb` 后、poly 循环前 | 1×/核 | **4** | Init 标量 SetValue；**→ V** 或保留 ALL |
| A-06 | 193 | `BuildAHat16ShardWithUb` | `RunShake128SampleNttUb` 后 → d12/rej | 8×/核 | **3** | Keccak 标量写 xofUb → Vector 读；**→ V+MTE2**（先 SIM 证；勿删） |
| A-07 | 201 | `BuildAHat16ShardWithUb` | `SampleNttVecOnePoly` 后、`DataCopy` â→GM 前 | 8×/核 | **3** | **→ V** |
| A-08 | 203 | `BuildAHat16ShardWithUb` | `DataCopy` â→GM 后、下一 poly | 8×/核 | **3** | **→ MTE3** 或 **删减?**（与 A-06 合并评估） |
| A-09 | 271 | `BuildAHat16ShardFromSeedD` | batch SHAKE 后 | — | — | **不热**（prep 不用此入口） |
| A-10 | 289 | 同上 | ROM init 后 | — | — | **不热** |
| A-11 | 305–307 | 同上 | batch 路径循环内 | — | — | **不热** |

---

## 7. Alg7 d12 — `pass-fix-f203-alg7-sample-ntt-k4/f203_alg7_d12_vec.hpp`

经 `SampleNttVecOnePolyFromXofUb` / `DeinterleaveCandFromUb` / `ComputeD12Vec` 进入 prep 热路径。  
`BuildAlg7SampleNttFromSeedD` 整链（L318+）在 **a_hat16 探针独立跑** 时用；prep 复用子函数，barrier **仍执行**。

| ID | 行 | 函数 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|------|--------|------|-------|----------|
| D-01 | 40 | — | macro `F203_ALG7_PIPE_ALL` | — | 4 | 拆 ALG7_SYNC_* |
| D-02 | 135 | `DeinterleaveCandScalarFromUb` | 标量 GetValue 解交织后 → `ComputeD12Vec` | 8×/核 | **4** | 标量→Vector；**→ V** 或 ALL；**不可删** |
| D-03 | 150 | `InitAlg7DeinterleaveRomUb` | ROM Init（Gather 路径） | — | — | **不热**（D12_GATHER=0） |
| D-04 | 163 | `PackXofBytesToExpandedInt32` | — | — | — | **不热** |
| D-05 | 179 | `DeinterleaveCandGatherFromUb` | — | — | — | **不热** |
| D-06 | 208 | `DumpXofUbToGm` | xof DataCopy 后 | — | — | **不热**（DUMP_XOF=0） |
| D-07 | 234 | `ComputeD12Vec` | `Adds` 后 → `MaskLowBitsI32` | 8×/核 | **4** | 连续 Vector；可试 **删减?** 或 **→ PIPE_V** 仅段末 |
| D-08 | 240 | `ComputeD12Vec` | 第一段 d1 完成后 → d2 段 | 8×/核 | **4** | Vector 链中间；**删减?**（无 MTE）；SIM 证 |
| D-09 | 246 | `ComputeD12Vec` | d2 完成后 → rej | 8×/核 | **4** | **→ V**（接 rej DataCopy） |
| D-10 | 318 | `BuildAlg7SampleNttFromSeedD` | SHAKE 后 → 解交织 | — | 4 | 同 **A-06** 语义；单 poly 探针先验 |
| D-11 | 362 | 同上 | rej 后 EnQue → DataCopy â | — | 4 | 同 **A-07** |
| D-12 | 364 | 同上 | DataCopy â 后 | — | 4 | 同 **A-08** |
| D-13 | 376–385 | 同上 | d1/d2 落 GM 对拍 | — | — | **不热**（prep 不写 d1/d2 GM） |

---

## 8. Alg7 rej — `f203_alg7_rej_vec.hpp` / `f203_alg7_rej_filter.hpp`

生产路径：`RejVecBulkFromD12Ub` + `RejectFilterMinsUb`（IMPL=1）+ 标量 `RejScalarCompactStreamUb`（compact 内 **无** PIPE_ALL）。

| ID | 行 | 文件 | 函数 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|------|------|--------|------|-------|----------|
| J-01 | 28 | rej_vec | macro | — | 4 | 拆 REJ_SYNC_* |
| J-02 | 64 | rej_vec | `InitAlg7InterleaveRomUb` | ROM Init 后 | 1×/核 | **4** | 同 **A-05** |
| J-03 | 81 | rej_vec | `InterleaveD12GatherUb` | UB DataCopy d1/d2→scratch 后 → Gather | 8×/核 | **4** | **→ MTE2** 或 **→ V**（同 UB 内 Copy 后 Gather） |
| J-04 | 84 | rej_vec | `InterleaveD12GatherUb` | Gather 写 stream 后 → compact | 8×/核 | **4** | **→ V** |
| J-05 | 110 | rej_vec | `RejVecBulkFromD12Ub` | DataCopy d1/d2→work 后 → Mins | 8×/核 | **4** | **→ MTE2** |
| J-06 | 41 | rej_filter | `RejectFilterMinsUb` | `Mins` 后 | 16×/核 | **4** | 单 Vector op 后；**删减?** 或 **→ PIPE_V** |
| J-07 | 63–91 | rej_filter | Mask 路径 | Compares/Select | — | — | **不热**（REJ_IMPL=1） |
| J-08 | — | rej_compact | 向量 compact 循环 | — | — | **不热**（生产走标量 compact） |

---

## 9. SHAKE 胶水 — `library/shared/shake_xof_kernel/shake_ub_helpers.hpp`

| ID | 行 | 上下文 | 频率 | Phase | 修改建议 |
|----|-----|--------|------|-------|----------|
| S-01 | 23–25 | `PipeAll()` 定义 | 被 PRF/alg7 SHAKE 调用 | 多次 | **2/4** | Keccak 内核以标量+uint64 写 UB；调用点 **R-03/A-06** 处窄化即可；**暂不改** `PipeAll` 定义直至调用点证毕 |

---

## 10. 明确不在 G4 prep 评估范围

| 文件 | 原因 |
|------|------|
| `f203_keygen_ek_append_entry.cpp` | G1 门禁，非 prep |
| `f203_se_vector_cbd_ub.hpp` / `f203_se_vector.hpp` | KeyGen prep 用 alg8 CBD 路径 |
| `pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/*` | compute MIX；单独评估 |
| `f203_a_hat16_ub.hpp` BATCH_SHAKE 分支 | `F203_AHAT16_BATCH_SHAKE=0` |
| `f203_alg7_rej_compact.hpp` | 生产 compact 为标量 |

---

## 11. Phase 执行检查表（逐轮填写）

### Phase 1 — CBD（C-02～C-04）

- [x] 拆 macro：`F203_CBD_SYNC_AFTER_COPYIN`（MTE2）、`F203_CBD_SYNC_BEFORE_COPYOUT`（V）
- [x] 仅改 `f203_cbd_eta2_ub_io.hpp`
- [x] G4 CPU PASS
- [x] G4 SIM PASS，`max_abs_diff=0`
- [x] prep tick：基线 **454170** → Phase1 **451813**（−0.5%）
- [x] 结论：**合入**

### Phase 2 — PRF（R-02～R-04）

- [x] 窄化 V/MTE3 于 `f203_se_vector_prf.hpp`
- [x] G4 CPU PASS；G4 SIM PASS（与 Phase1+3 联测）
- [x] prep tick：Phase1+2 **469138**（**劣于** Phase1  alone）→ **回滚**，PRF 保持 `PipeAll()`

### Phase 3 — Â GM 边界（A-06～A-08）

- [x] 窄化于 `f203_a_hat16_ub.hpp` 热循环
- [x] G4 SIM PASS（Phase1+2+3 联测 **462628**，仍慢于 Phase1）
- [x] 结论：**回滚**，Â 循环保持 `F203_AHAT16_PIPE_ALL()`

### Phase 4 — Alg7 d12/rej（D/J 系列）

- [x] 全量窄化 → SIM `a_hat max_abs_diff≈3272` **FAIL**
- [x] 仅 rej MTE2/V（J-03～J-05）→ 仍 **FAIL**
- [x] 结论：**回滚**；Alg7 标量↔Vector 边界须 **PIPE_ALL**

### Phase 5 — 删减 + prep 段间

- [x] **C-04** CopyOut 后 barrier 删减 → SIM PASS，prep **447061**（−1.6% vs 基线）
- [x] **P-03** PRF→CBD 段间删减 → PASS 但 tick **449039**（劣于仅 C-04）→ **保留 P-03 ALL**
- [x] P-02 / P-04：**未动**（保留 ALL）
- [x] 最终 prep tick：**447061**；全链 total **524986**（910B4 SIM）

---

## 12. 风险红旗（出现即回滚）

1. SIM PASS 但 **仅部分输出** 错（如 poly8+ 全 0）→ 查 MTE/跨段，非单纯删 barrier  
2. tick **下降** 但 golden **FAIL** → CBD 先例；以 golden 为准  
3. CPU PASS、SIM FAIL → barrier 不足，恢复 ALL  
4. 动 **P-02/P-04** 前未充分理解双 AIV + TPipe 复用  

---

## 13. 交叉引用

- 集成计划 Opt-5 行：[`INTEGRATION_PLAN.md`](INTEGRATION_PLAN.md) §1.1  
- exp 交付：[`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`](../../examples/incubating/exp-mlkem-f203-pke-keygen-k4/)  
- qa 路线图：[`qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md`](../../qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md) §Opt-5  
- 平台通则：`.cursor/skills/ascendc-engineering-notes/SKILL.md` §6–§7  

---

*文档版本：2026-06-25；随 Phase 实验更新 §11 检查表与合入结论。*
