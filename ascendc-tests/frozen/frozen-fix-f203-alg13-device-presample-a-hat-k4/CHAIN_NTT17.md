# 链式探针 — Alg.13 行 8–17（1×AIV 预采样 → MIX NTT）

**状态**：✅ **CPU + SIM 均 PASS**（2026-06-22）；**无实现异常** — tick 与两段 standalone 之和一致。

**目的**：在 **保留 V3 单 AIV 预采样** 前提下，验证 **Launch1（1×AIV）→ Launch2（1×AIC + 2×AIV）** 双 launch、**同一 GM `src`** 交接是否产生意料之外问题。

**范围**：至 **行 17**（`mixPass=5`：S1+S2+S3）；**不含**行 18–20。

---

## 数据流

```text
Launch 1  f203_se_vector_k4     blockDim=1   SEED_D → src[8,256] GM
          （G 标量 + P 向量 + C P1b-single）

Launch 2  mmad_custom MIX        blockDim=1   src GM → dst[12,256]
          AIV_SPLIT → AIC_MMAD → AIV_PACK → S3 merge/mod
          （1×AIC + 2×AIV，CrossCore 握手）
```

**关键**：`input/` **无** `src.bin`；`src` 仅由 Launch1 写入，Launch2 直读同一 GM 缓冲。

---

## 与 vec-k4-v2 的差异

| 项 | vec-k4-v2 默认 | 本链式探针 |
|----|----------------|------------|
| `src` 来源 | Host `build_src_se()`（4×相同 s） | Device **真实 CBD**（4×**不同** $s_i$） |
| launch 数 | 1（或 CPU 5→4 分段） | **2**（SE + NTT） |
| 验收范围 | 行 16–20 | **行 8–17** |

---

## 关注的风险点（本探针要回答的）

| 风险 | 说明 | 预期 |
|------|------|------|
| **src 布局** | 8 行 int32 行主序；0–3=$s_i$，4–7=$e_i$ | 与 `Aiv2s1eSplit` 读法一致 |
| **四行 s 不同** | v2 用例多为 `np.tile` 同一 s；本链用真实 Alg.13 | 双 AIV 仍各读 **完整** `src[0:3]` 复制到两套 S0 bank；`dst[0:3]` 应等于 `dst[4:7]` |
| **GM 生命周期** | Launch1 写 / Launch2 读同一缓冲 | CPU：同进程 GM；SIM：分阶段经 `src.bin` 注入 Launch2 |
| **MIX 模式切换** | Launch1 纯 AIV；Launch2 需 `SetKernelMode(MIX_MODE)` | Host 在 Launch2 前设置 |
| **CrossCore 同步** | S1→AIC→AIV PACK→S3 | 沿用 v2 已验收握手；新变量是 **src 内容** 非 tiled |
| **workspace 污染** | 两 launch 用不同 ws 缓冲 | 已隔离（`seWs` / `nttWs`） |

### 验收结论（已测）

| 风险点 | 结果 |
|--------|------|
| `src` 布局 / 四行 $s_i$ 不同 | ✅ `golden_src` / `dst` / `s0` / `mat_c` 全 `max_abs_diff=0` |
| 双 AIV 读完整 `src[0:3]` | ✅ `dst[0:3]` 与 `dst[4:7]` 一致（`verify_chain_ntt17.py` ŝ 检查） |
| GM 生命周期 | ✅ CPU 同进程同 GM；SIM 分阶段 `src.bin` 数值等价 |
| MIX / CrossCore | ✅ 与 v2 `mixPass=5` 行为一致；真实 CBD `src` 不改变 tick 量级 |
| 链式额外开销 | ✅ **无** — 分阶段 tick ≈ Launch1 + Launch2 standalone |

---

## 运行

```bash
cd ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4
bash run_chain_ntt17.sh -r cpu -v Ascend910B4   # 单进程同 GM
bash run_chain_ntt17.sh -r sim -v Ascend910B4   # 分阶段 SIM（见下）
```

**CPU**：单进程 `f203_se_chain_ntt17_cpu`，Launch1/2 共享 GM `src`（tikicpu `GmAlloc`）。

**SIM（分阶段，已验收 PASS）**：

1. `f203_se_vector_npu` — Launch1（~133k tick）
2. `cp output/src.bin → input/src.bin`
3. `build/vec_k4_ntt/ascendc_kernels_bbit` — Launch2 mixPass=5（~44k tick）

数值对拍与 CPU 一致（`max_abs_diff=0`）。单进程 `ccec` 单体同时编入 Launch1+2 会在 Launch1 触发 SIM 段错误（与 `main.cpp` 单体 SE 对比已定位）；故 SIM 采用分阶段，待工具链/链接方案后再做同 GM 单进程。

V3 单段回归不受影响：

```bash
SE_VECTOR_STAGE=v3 bash run.sh -r cpu -v Ascend910B4
SE_VECTOR_STAGE=v3 bash run.sh -r sim -v Ascend910B4
```

---

## SIM 性能（Ascend910B4 CaModel，`SEED_D=20260619`）

tick 取自 camodel `[INFO] Total tick`；权威 Alg.13 行号对照见 [`ALG13_LINE_TICK.md`](ALG13_LINE_TICK.md) §5.1。

| Launch | 内核 | blockDim | SIM tick | 占链式 |
|--------|------|----------|----------|--------|
| **Launch1** | `f203_se_vector_k4`（行 8–15） | 1×AIV | **133153** | **75.0%** |
| **Launch2** | `mmad_custom` mixPass=5（行 16–17） | 1×AIC + 2×AIV | **44400** | **25.0%** |
| **链式合计** | 分阶段 SIM | — | **~177553** | 100% |

**对照**：

| 测法 | tick | 说明 |
|------|------|------|
| V3 standalone `run.sh -r sim` | 133153 | = Launch1 |
| vec-k4-v2 `NTTS2S1E_MIX_PASS=5`（Host tiled `src`） | 44648 | Launch2 定标 |
| 链式 Launch2（Device 真实 CBD `src`） | 44400 | 比定标低 **~0.6%**，噪声内 |
| vec-k4-v2 全链路 16–20 | 77958 | 本探针未跑行 18–20 |

Launch1 段内拆分（G/P/C）：[`SIM_BENCHMARK.md`](SIM_BENCHMARK.md) §2 — P **83478**、C **49675**、G **~3–8k** 粗估。

Launch2 OPPROF 核级 duration（`OPPROF_latest`，Launch2 任务）：

| 核 | duration (cycles) |
|----|-------------------|
| AIC_0 | 15028 |
| AIV_0 | 44207 |
| AIV_1 | 44172 |
| Task 总 | 44262 |

**解读**：链式至行 17 的瓶颈仍在 Launch1 向量 PRF（约六成）；Launch2 由双 AIV NTT/pack 主导。分阶段 SIM 的 Host `src.bin` 注入**不引入**可测设备 tick 开销。

**CPU**：tikicpu 无 CaModel `Total tick`；功能验收以 SIM tick 预算为准。

---

## 输出与对拍

| 输出 | golden | 阶段 |
|------|--------|------|
| `src.bin` | `golden_src.bin` | Launch1（8–15） |
| `dst.bin` | `golden_dst.bin` | Launch2（16–17） |
| `s0.bin` | `golden_s0.bin` | Stage1 |
| `mat_c.bin` | `golden_mat_c.bin` | Stage2 |

---

## 文件

| 文件 | 职责 |
|------|------|
| `gen_data_chain_ntt17.py` | SE golden_src + NTT golden（src 来自 `golden_se_sampling`） |
| `main_chain_ntt17.cpp` | 双 launch Host 驱动（CPU 同 GM；SIM 含 `RunChainSim` 分阶段路径） |
| `main_chain_ntt17_launch2.inc` | Launch2 代码块（`RunChainSim` include） |
| `chain_ntt17_layout.h` | 本地 tiling 常量（避免 ccec 单体链式构建拉入 `tiling.h`） |
| `verify_chain_ntt17.py` | 链式对拍（src / dst / s0 / mat_c / 双 AIV ŝ） |
| `run_chain_ntt17.sh` | 一键 CPU/SIM 构建 + 运行 + verify |
| `cmake/chain_sim_staged.cmake` | SIM 分阶段：V3 + vec-k4 子工程 |

NTT 核源码：[`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/) `mmad_custom.cpp`（`mixPass=5`）。

---

## 已知限制 / 后续

| 项 | 说明 |
|----|------|
| **SIM 同进程同 GM** | 单体 `ccec` 同时编入 Launch1+2 会在 Launch1 触发 SIM 段错误；当前用分阶段两二进制，数值已等价 |
| **行 18–20** | 未纳入本探针；接入 `vec-k4-v3` 时预计 **+~33k** tick（见 [`ALG13_LINE_TICK.md`](ALG13_LINE_TICK.md) §6） |
| **G-only 门控** | 行 1–2 Phase G 仍为粗估，待可选探针 |
