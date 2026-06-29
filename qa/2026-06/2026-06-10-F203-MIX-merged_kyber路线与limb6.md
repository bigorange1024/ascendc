# 2026-06-10 — F203 MIX：merged_kyber 壳、limb6 探针、放弃融合模板

> **6/11 修订**：NTT 内 `Matmul<>` / `int8-matmul-cube-*` 已 **废弃冻结** → [2026-06-11-…#NTT-Matmul](2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)。下文「纯 AIC Matmul」「暂缓」等表述以该文为准。

## 结论（拍板）

1. **三段式 NTT 本质**：分解 → 矩阵乘 → 合并取模（D 与 F203 同构，差别在 limb 位宽、LUT 切块、mod 阶段位置）。
2. **MIX 开发壳**：**放弃** `examples/` 融合算子模板（LeakyRelu：`Matmul<>` + auto_gen 单趟 MIX）；**一律**以 `merged_kyber` 手写 FSM（`MachineState` + `WAIT/SET` + Split/Mmad/Merge）为参考。
3. **7bit→6bit（仅 D）**：架构不变；改 `split` mask/shift、`Merge` 移位、`gen_data.py` 的 M 切片；golden 仍为 `f @ M mod 3329`。
4. **FIPS 203**：$R_q$ 系数规范表示为 $[0,q-1]$，非负；实现中可用有符号中间量，入库前规约。
5. **SIM/NPU 同步**：AIV↔AIC 边界及关键 API 后加 `PipeBarrier<PIPE_ALL>`（CPU 可不强制）。

归档：
- [docs/notes/F203-merged-kyber-MIX路线技术总结.md](../../docs/notes/F203-merged-kyber-MIX路线技术总结.md)
- [docs/notes/merged-kyber-poly-batch-NTT技术总结.md](../../docs/notes/merged-kyber-poly-batch-NTT技术总结.md)

---

## 当日产出（代码）

| 探针 | CPU | SIM | 说明 |
|------|-----|-----|------|
| `frozen/frozen-merged-kyber-ntt256-limb6/` | ✓ | ✓ | D @ 6bit；F203 fork 基线 |
| `frozen/frozen-merged-kyber-ntt256/` | ✓ | ✓ | D @ 7bit 对照 |
| `frozen/frozen-merged-kyber-ntt256-limb6-poly2-s12/` | ✓ | ✓ | poly2 Stage1+2 only；`dst` 为 `A@M0` `[4,256]` |
| `frozen/frozen-fix-merged-kyber-ntt256-limb6-poly2-s12/` | ✓ | ✓ | 上者 + **单 TPipe batch Split**（纠正循环 `new AivSplit`） |
| `frozen/frozen-fix-merged-kyber-ntt256-limb6-poly2-s123/` | ✓ | — | **全链路** batch Split+Mmad+Merge；`dst` `[2,256]` NTT ≡ limb6 |
| `frozen/frozen-fix-merged-kyber-ntt256-limb6-poly8-s123/` | ✓ | ✓ | k=8 全链路；8×同 poly |
| `examples/incubating/exp-sepolyvec8-ntt-k8/` | ✓ | ✓ | F203 预研；k=8 **互异随机** poly；`seed=20260610` |
| `f203-ntt-phase-a-fsm/` | ✓ | ✗ | **2026-06-19 归档** → [`frozen/frozen-f203-ntt-phase-a-fsm/`](../../ascendc-tests/frozen/frozen-f203-ntt-phase-a-fsm/)（任务完成，非路线否决） |
| `int8-matmul-cube-128x512x512/` | ✓ | ✗ | 纯 AIC tiling（非 MIX） |
| ~~`f203-kyber6-stage12-mix/`~~ | ✗ | — | **已删**（CPU 挂死，非 merged_kyber 壳） |
| ~~batch8 扩展示例~~ | ✗ | — | **放弃**（pipe/event 耗尽 + Merge 读址错误） |

验收（limb6）：

```bash
cd ascendc-tests/frozen/frozen-merged-kyber-ntt256-limb6
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

---

## 讨论要点

### 为何不用 LeakyRelu 融合模板

- auto_gen / workspace / 3×GM 与 `Matmul<>` 绑在一起，AIV→AIC→AIV **阶段边界不透明**。
- Phase A：CPU 通过、sim 挂死；`f203-kyber6-stage12-mix` CPU 亦挂死，目录已删。
- merged_kyber 同构 FSM：CPU+sim 秒级通过。
- **纯 AIC** Stage2（`Matmul<>`）仍可用于隔离验证；禁止的是 **当 MIX 三段壳**。

### merged_kyber 应复制的结构

- `KERNEL_TYPE_MIX_AIC_1_2`
- AIC：`WAIT(AIV_SPLIT)` → `AicMmad×2` → `SET`
- AIV：`Split` → `SET` → `WAIT(AIC_MMAD)` → `Merge` + Barrett
- Host：`dst, src, ws, TilingData`；LUT @ `ws+M0`

### 6bit 改造清单（相对 7bit D）

| 文件 | 改动 |
|------|------|
| `ntt_vec.hpp` | `&0x3f`，`>>6` |
| `aiv_func.hpp` | Merge `ShiftLeft` 6/12/18 |
| `scripts/gen_data.py` | M 按 6bit 切片 → `M4.bin` |

---

## 晚间：poly2 批量化 NTT（batch=2）— 教训与调通

**专题文档**：[docs/notes/merged-kyber-poly-batch-NTT技术总结.md](../../docs/notes/merged-kyber-poly-batch-NTT技术总结.md)

### 怎么调通的（一句话）

**每个 AIV 子核、每个向量阶段只有一个 `TPipe`**；Split/Merge 用 `tileLength = kPolys×(n/2)` 一趟向量核，GM 上 `for (p)` 只做搬数；Stage2 一次 `AicMmad(4,256,256)`；Stage3 按 poly 从紧凑 `A0/A1` 取行 **`2p`/`2p+1`**，merge 槽 2/3 置 0（等价单 poly 读未写行）。

### 之前为何失败 / 卡死

| 现象 | 根因 | 纠正 |
|------|------|------|
| `AllocEventID … max 8`、仿真挂死 | `for (p) { AivSplit split; }` 每次新建 `TPipe`/`InitBuffer` | 单算子实例 + 单 `Init`；循环仅 CopyIn/Out |
| batch8 对拍错 / Merge 乱 | 误以为要 8×16 行 padding；Merge 仍按 `i=0..3` 读行 | 紧凑 `[2k,256]` + 按 `2p/2p+1` 取行 |
| 「batch」但 Compute 仍是单 poly | 语义是多次单 poly，不是扩大 tile 的 SIMD | `split_vec(..., kPolys*n/2)` + Cube `m=2k` |
| SIM 根目录一堆 `core*.dump` | 未设 `CAMODEL_LOG_PATH` | `source camodel_sim_log.sh` |
| poly2-s12 在 SIM 编不过 | debug 打印 `const char*` vs `__gm__` 字面量 | 非 CPU 用宏空操作 |

### 固定输入与 golden

- 输入：`library/shared/merged_kyber_fixed_poly.py`（`seed=42`），limb6 与 poly2 共用。
- poly2 s123：`dst[0]` 与 `dst[1]` 均须与 `frozen/frozen-merged-kyber-ntt256-limb6/output/golden.bin` **逐元素一致**（当前为同一 poly 复制 2 份）。

### 验收命令

```bash
cd ascendc-tests/frozen/frozen-fix-merged-kyber-ntt256-limb6-poly2-s123
bash run.sh -r cpu -v Ascend910B4
```

---

## 深夜：exp-sepolyvec8-ntt-k8 落地（F203 k=8 互异随机）

**目录**：`examples/incubating/exp-sepolyvec8-ntt-k8/`  
**规格 PDF**：同目录 `exp-sepolyvec8-ntt-k8-实现方案.pdf`

### 做了什么

- 从 `fix-merged-kyber-ntt256-limb6-poly8-s123` **整架构迁移**（merged_kyber FSM + 单 TPipe batch Split/Merge + `AicMmad(16,256,256)×2`）。
- `gen_data.py`：`seed=20260610`，生成 **8 条互不相同** 的 $Z_q$ 系数向量；golden 为逐行 `ntt_forward`。
- I/O 按规格命名：`se_polyvec_gm.bin`、`mat_b_lut_gm.bin`、`output.bin`（非探针的 `src.bin`/`M4.bin`/`dst.bin`）。
- **废弃**旧 `Matmul<>` 融合稿与 `exp-mlkem-f203-stage12-encode-matmul-mix`。

### 编译陷阱（当晚踩坑）

`thirdparty/merged_kyber/aiv_func.hpp` 仍是 **单 poly** API。exp 必须在本地放置 batch 版 `aiv_func.hpp`（及 `ntt_vec.hpp`、`kyber_limb6.hpp`），且 CMake `include_directories` 把 `TEST_ROOT` 放在 `MERGED_KYBER_ROOT` **之前**。否则 `sepolyvec8_ntt_custom.cpp` 报 `AivSplit` 参数个数不匹配。

### 验收

```bash
cd examples/incubating/exp-sepolyvec8-ntt-k8
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
# max_abs_diff=0；output.bin == golden.bin
```

---

## 后续

- **NPU 实机**：在 A2 上补 `bash run.sh -r npu`。
- **merged_kyber 上游**：考虑将 batch 版 `aiv_func.hpp` 回灌 `thirdparty/merged_kyber`，避免 exp/探针各维护一份。
- Stage2 MIX 内 **固定复用 D 的 `AicMmad`**；`exp-mlkem-f203-stage2-int8-matmul-cube` 仅 golden 对照（`[16,512]` 右 LUT 路径，与 limb6 紧凑 `[16,256]` 不同）。
- WSL 开发：**禁止**在资源管理器里复制整个用例目录；备份用 `cp -a` / `tar`。
