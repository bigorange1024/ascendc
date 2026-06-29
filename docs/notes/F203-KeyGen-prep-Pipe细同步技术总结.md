# F203 KeyGen prep Pipe 细同步 — 技术总结

**读者**：在 KeyGen prep 热路径上优化 `PipeBarrier`、或从 CBD/PRF/Alg7 子模块做 MTE↔V 窄化的实现者  
**目的**：说明 **哪些 barrier 可窄化/删减**、**哪些必须保留 `PIPE_ALL`**，以及 **为何 SIM golden 是裁判**  
**案例锚点**：`ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md`（Opt-5 Phase 1→5）  
**讨论**：`qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md` §Opt-5  
**实现方案**：`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`（经 `ALG8_INC` 链入 CBD 窄化）

---

## 0. 本文怎么读

| 章节 | 内容 |
|------|------|
| §1 | 流水线与 barrier 角色 |
| §2 | 工程不变量：可窄化 vs 须 ALL |
| §3 | Opt-5 合入结论（Phase 1+5） |
| §4 | 验证方法论 |
| §5 | 可复用模式 |
| §6 | 案例附录 |

---

## 1. 流水线与 barrier 角色

KeyGen prep 单 TPipe（行 3–15）热路径：

1. **Â**：16×（SHAKE → 标量解交织 → Vector d12 → Vector rej）→ GM `a_hat`
2. **PRF**：batch SHAKE 写 `prf_out` GM
3. **CBD**：8 行 ×（GM→UB → Vector CBD → UB→GM）→ `src`

每段典型三段式：**CopyIn（MTE2）→ Vector 计算（V）→ CopyOut（MTE3）**。  
`PipeBarrier<PIPE_ALL>` 同步全管道，**正确但贵**；窄化目标是只等**生产者→消费者**所跨的 pipe stage。

---

## 2. 工程不变量

### 2.1 可安全窄化的模式

| 模式 | 窄化 | 前提 |
|------|------|------|
| MTE `DataCopy` GM→UB 后 Vector 读 UB | `PIPE_MTE2` | 无其它 pipe 异步写同一 UB |
| Vector 写 UB 后 MTE CopyOut | `PIPE_V` | CopyOut 前无跨 pipe 读 |
| 连续 Vector 链（同 UB、无 MTE 插入） | 段末一次 `PIPE_V` 或保留 ALL | 须 SIM 证；中间 barrier 删减风险高 |
| CopyOut 后**立即**下一行 CopyIn（不同 GM 区） | **可删** CopyOut 后 barrier | 下一 CopyIn 的 MTE2 足够；见 CBD C-04 |

### 2.2 必须保留 `PIPE_ALL` 的模式

| 模式 | 原因 |
|------|------|
| **标量/Keccak 写 UB** → Vector 读（SHAKE 后 xofUb、FillPrf、解交织 SetValue） | `PIPE_V`  alone 不足；SIM 出现 `a_hat` 大面积错（≈3272 max_abs） |
| **Alg7 d12/rej** 全链窄化 | 标量↔Vector↔Gather↔Mins 交错；窄化即 FAIL |
| **prep 段间** Â→PRF（P-02） | TPipe 复用 `shakeXBuf`/`scratchBuf`；窄化风险高 |
| **双 AIV block1 等待 block0**（P-04） | 跨 block 生命周期；不可删 |
| PRF→CBD 段间（P-03） | 删减 PASS 但 tick 变差；保留 ALL |

**红旗**：CPU PASS + SIM FAIL → barrier 不足；tick 下降 + golden FAIL → 以 golden 为准（CBD 无 barrier 曾 SIM 虚低）。

---

## 3. Opt-5 合入结论

**合入（Phase 1 + Phase 5 部分）** — `f203_cbd_eta2_ub_io.hpp`：

```cpp
#define F203_CBD_SYNC_AFTER_COPYIN()  AscendC::PipeBarrier<PIPE_MTE2>()
#define F203_CBD_SYNC_BEFORE_COPYOUT() AscendC::PipeBarrier<PIPE_V>()
// CopyOut 后：无 barrier（C-04 删减）
```

**回滚**：Phase 2 PRF、Phase 3 Â GM 边界、Phase 4 Alg7；Phase 5 中 P-03 删减。

| 指标 | Opt-4 基线 | Opt-5 后 |
|------|------------|----------|
| prep SIM tick | 454170 | **447061**（−1.6%） |
| 全链 SIM tick | 532074 | **524986** |
| G4 golden | PASS | PASS |

---

## 4. 验证方法论

1. **每 Phase** 改一项 → G4 CPU + `SIM_DIRECT=1` SIM → 记录 prep tick  
2. **失败即回滚**该 Phase，不叠加  
3. **exp 交付**：prep 经 `ALG8_INC` 引用 CBD 头文件，**重建即继承**窄化；无需在 exp 目录复制 CBD 逻辑  
4. **禁止**仅 CPU 通过就删 barrier  

---

## 5. 可复用模式 P-pipe-cbd-1

MTE↔Vector **行流水线**（PRF 行、CBD 行、小 batch CopyIn/Out）：

1. CopyIn 后 **`PIPE_MTE2`**
2. Vector 后、CopyOut 前 **`PIPE_V`**
3. CopyOut 后：若下一操作是**下一行 CopyIn** 且读**不同 GM**，可试删 CopyOut barrier（Phase 5）
4. 标量填 UB / Keccak 后：**不要**假设 `PIPE_V` 够用，先 SIM  

---

## 6. 案例附录

| 项 | 值 |
|----|-----|
| 评估清单 | `ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md` |
| 合入文件 | `pass-fix-f203-alg8-cbd-eta2-k4/f203_cbd_eta2_ub_io.hpp`（canonical）；KeyGen vendored 于 `prep/alg8/` |
| 未改 | PRF、Â、Alg7、`f203_keygen_prep_ub.hpp` P-02/P-03/P-04 |
| exp 路径 | `examples/incubating/exp-mlkem-f203-pke-keygen-k4/` |

**验收命令**：

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```
