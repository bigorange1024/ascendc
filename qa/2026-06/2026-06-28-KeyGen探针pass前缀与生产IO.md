# 2026-06-28 — KeyGen 探针 pass 前缀、生产 I/O 与注释体系

## 背景

`fix-f203-alg13-device-keygen-k4` 已完成：

1. **生产 I/O** 对齐 `exp-fips203-mlkem-pke-keygen-k4`（`input/` 仅 seed+LUT；`output/` 仅 ek/dk；单入口 `ascendc_keygen_bbit`）。
2. **CPU + SIM + liboqs KAT** 验收通过（SIM Total tick **886801**）。
3. 用户要求：**全源码详细注释**、目录改 **`pass-` 前缀**、文档/实现方案与当前探针一致。

## 变更摘要

| 项 | 内容 |
|---|---|
| 目录重命名 | `fix-f203-alg13-device-keygen-k4` → **`pass-fix-f203-alg13-device-keygen-k4`** |
| 源码注释 | `scripts/inject_probe_code_comments.py` 为 **120** 个源文件注入 `@probe` 文件头（layer/role/I/O/launch/ai\_core/depends/verify） |
| 实现方案 | 新增 `pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex` |
| 文档 | 刷新 `STATUS.md`、`INTEGRATION_PLAN.md`、`BUILD_OPTIONS.md`；更新 `ascendc-tests/INDEX.md` 与 exp/qa 交叉引用 |

## 经验教训

### 1. 生产 run.sh 必须完整走 SIM 环境

**问题**：合并单入口后 SIM 启动 FPE（`libascend_dump`）。

**根因**：

- 漏 `scripts/sim_env.sh`（WSL 需 `out/lib/libascend_dump.so` stub）。
- **`sim_env_export` 若在 `_keygen_build` 之前**：build 的 `rm -rf out/` 删掉 stub → 仍 FPE。

**规则**：SIM 路径固定为 **build → sim_env_export → camodel_sim_log → kernel**。

### 2. 不要用 CPU SUCCESS 日志推断 AI Core 用量

**现象**：tikicpu 打印 `[SUCCESS][AIC_0][AIC_1][AIV_0..3]`，易误判为 2 颗 AI Core。

**事实**：

- prep 为 **AIV_ONLY**；SIM `profile_aic_log0.toml` **无 task0 条目**（prep 不启 AIC）。
- `blockDim=2` 起 2 个 **AIV block**，但 Â 当前由 block0 串行完成；block1 cycle 很短（同步）。
- 两次 launch 串行在同一颗 AI Core（`core_list=[0]`）。

**规则**：核占用以 **`sim_log/profile_subtask_log0.toml`**、**`profile_aiv_log0.toml`** 为准。

### 3. 探针 I/O 即契约，staging 只能留在 dev 脚本

旧路径（`compute_io/`、`output/a_hat.bin`）易误导后续用例。默认 `run.sh` 必须：

- `prepare_production_input.py` 只写 seed+LUT；
- `_keygen_scrub_output` 只保留 ek/dk；
- 分段 `cmake/prep|compute` 文档标明 **非生产**。

### 4. 大规模注释用脚本 + 标记幂等

120 文件手改不可维护。`@probe pass-fix-…` 标记 + `inject_probe_code_comments.py`：

- 按子目录（prep/ahat、compute/…）定制 `@role`；
- shebang 后插入；
- 二次运行 skip（`files_already_marked`）。

改逻辑后应同步更新 `@role` 或重跑脚本（删标记可强制重写）。

### 5. pass- 前缀时机

按 `ascendc-tests/INDEX.md` 规则：**CPU+SIM 均已验收的终态** 才加 `pass-`。本次在 production I/O + KAT 通过后重命名，并从「规划中」迁入当前探针表。

## 验收

```bash
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh
python3 scripts/inject_probe_code_comments.py  # files_updated=0
```

## 关联

- [`STATUS.md`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/STATUS.md)
- [`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/INTEGRATION_PLAN.md)
- [`exp-fips203-mlkem-pke-keygen-k4`](../../examples/incubating/exp-fips203-mlkem-pke-keygen-k4/)（交付 I/O 对照）

---

## 6. 交付示例 `exp-fips203-mlkem-pke-keygen-k4` 自包含化与唯一路径

**背景**：探针验证通过后，将 KeyGen **交付示例**从「多门禁 + 外链探针」收敛为 **自包含、唯一向量化全链**，便于复制为 Encrypt 等后续用例模板。

### 6.1 变更摘要

| 项 | 内容 |
|---|---|
| 源码 vendored | `prep/`、`compute/`、`cmake/keygen/`、`scripts/prep/`、`scripts/compute/`、`thirdparty/ntt_study/`（Host LUT golden，非 AscendC 编译依赖） |
| 唯一入口 | `bash run.sh` → `main_keygen.cpp` → 2 launch（prep \| compute+ek‖ρ） |
| 已删除旁路 | G0–G4 门禁、`main_keygen_prep`、`f203_keygen_ek_append`、分段 `cmake/cpu_lib*`、`compute_io/` staging、`kat_liboqs_staged.sh` 等 |
| SIM 指标 | `scripts/parse_keygen_sim_metrics.py`：两次 launch tick + wall_ms 加总打印 |
| KAT | `kat_liboqs_vs_ascendc.sh`：默认 **10×CPU + 1×SIM**，同 `SEED_D` 对拍 liboqs `ml_kem_1024`；`KEYGEN_KAT=1` 静默日志 |
| 文档 | `STATUS.md`、`exp-fips203-mlkem-pke-keygen-k4-实现方案-customspec.tex`（§唯一交付路径 + 锁定 CMake 表） |

### 6.2 验收（示例目录）

```bash
cd examples/incubating/exp-fips203-mlkem-pke-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4          # SIM summary 示例：total_tick≈884532
bash kat_liboqs_vs_ascendc.sh              # 可选
KEYGEN_VERIFY=1 bash run.sh -r cpu -v Ascend910B4
```

| 指标 | 典型值（Ascend910B4 SIM） |
|------|---------------------------|
| Launch1 prep tick | ≈ **801491** |
| Launch2 compute tick | ≈ **83041** |
| **total_tick** | ≈ **884532** |
| 探针对照 total | ≈ **886801**（同 I/O 契约） |

### 6.3 经验教训（交付示例专用）

1. **自包含 ≠ 复制探针目录名**：exp 的 `REPO_ROOT` 比探针多一层 `examples/incubating/`；`run.sh` / `cmake/keygen/CMakeLists.txt` 须单独核对，不可硬链探针路径。
2. **唯一路径从第一天写进 customspec**：禁止保留 `mixPass≠0` 调试路径进默认 `run.sh`；验收只认 seed+LUT→ek/dk。
3. **KAT SIM 超时预算**：全链 SIM 单轮可 >900s；`KEYGEN_KERNEL_BUDGET_SEC=1200`（`run.sh` 默认），否则 exit **124** 误判失败。
4. **SIM tick 拆分**：单进程 SIM 可能只有一条 `Total tick`；优先读 `sim_log/profile_task_log0.toml`，否则按 launch 墙钟比例分摊。
5. **备份须含 vendored 树**：旧快照缺 `prep/`/`compute/` 会导致「从 backup 恢复后半天找不到源码」——见 [`backup-project.sh`](../../backup-project.sh) §用例树。
6. **ntt_study 边界**：`thirdparty/ntt_study/` 在 exp 内仅 vendored **Host LUT**；其 docs/specs 为**另一研究线**，CPU/设备切分**不得**默认当作本仓规范（Encrypt 规划须写独立 customspec）。

### 6.4 教学总结（如何复制为下一用例）

| 步骤 | 动作 |
|------|------|
| 1 | fork exp 骨架（`run.sh`、双 launch Host、`scripts/prepare_production_input.py`、SIM metrics 脚本） |
| 2 | 写 **新 customspec**：生产 I/O、Launch 表、锁定 CMake 宏；**唯一路径**一节与 STATUS 对齐 |
| 3 | vendored `prep/`/`compute/`，仅允许 `library/shared` 外链 |
| 4 | Host golden + liboqs KAT；CPU → SIM → 可选 KAT |
| 5 | 定稿 note → `docs/notes/`；当日 qa **追加章节**（不另开同日第二篇） |

定稿 note：[F203-KeyGen-exp交付示例技术总结.md](../../docs/notes/F203-KeyGen-exp交付示例技术总结.md)

---

## 7. KeyGen 子轨探针重命名 + Phase A 全链冻结

### 7.1 子轨目录语义化（2026-06-28）

| 旧目录 | 新目录 | 语义 |
|--------|--------|------|
| `pass-fix-f203-alg7-single-poly-sample-ntt-d12-vec-k4` | [`pass-fix-f203-alg7-sample-ntt-k4`](../../ascendc-tests/pass-fix-f203-alg7-sample-ntt-k4/) | Alg.7 SampleNTT 单 poly 模块 |
| `pass-fix-f203-alg13-lines3-7-a-hat-16poly-k4` | [`pass-fix-f203-alg13-lines3-7-a-hat-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines3-7-a-hat-k4/) | Alg.13 行 3–7 → `a_hat[16,256]` |
| `pass-fix-f203-alg13-device-presample-k4` | [`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/) | Alg.13 行 8–15 → `src[8,256]` |

活跃树内路径与 `ascendc-tests/INDEX.md` §KeyGen 子轨命名 已同步；`backup/` 历史快照仍含旧名。

### 7.2 Phase A 全链 benchmark 冻结

[`fix-f203-alg13-device-presample-a-hat-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/)（G+A+P+C 单 launch Phase A 实验）→ **`frozen-fix-f203-alg13-device-presample-a-hat-k4`**（2026-06-28）。

| 项 | 说明 |
|----|------|
| 冻结原因 | Phase 1 + A-v1~v4b 对拍完成；行 3–7 / 8–15 已拆至上述 pass 子轨；A-v5 不阻塞 KeyGen |
| 只读用途 | [`SIM_BENCHMARK.md`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/SIM_BENCHMARK.md) tick 表、A-v4 反模式 |
| 定稿 note | [F203-Alg7-PhaseA-向量化技术总结.md](../../docs/notes/F203-Alg7-PhaseA-向量化技术总结.md) |

---

## 8. 8-poly 批 NTT/INTT 向量探针 pass（polyvec8-vec）

### 8.1 背景

独立 AscendC 探针，验证 **8×256 polyvec** 上 Tag5T 三段式 **NTT/INTT** 批量向量化（与 ML-KEM 方案参数 **k** 无关；仅表示 **8 个 poly**）。构图对齐 ntt_study 交付 [`sepolyvec8_ntt_f203`](../../thirdparty/ntt_study/examples/mlkem/deliverables/sepolyvec8_ntt_f203/)：紧凑 Stage1 `[HI₈,LO₈]`、**同一计算图仅换 LUT**。

### 8.2 变更摘要

| 项 | 内容 |
|---|---|
| 目录 | `fix-f203-stage123-ntt-intt-k8-vec-k4` → `fix-f203-stage123-ntt-intt-polyvec8-vec` → **`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`** |
| 验收 | NTT/INTT 各 **CPU+SIM PASS**（max=0）；vs ntt_study C **max=0** |
| 资源 | **`blockDim=1`** · **MIX 1×AIC+2×AIV** · **1 launch** 全链（mixPass=3） |
| SIM tick | NTT **30347** · INTT **30340**（910B4） |
| 独立脚本 | `scripts/cross_check_ntt_study_c.py`（C 参考对拍，不接入 run.sh） |

### 8.3 命名约定

- **`polyvec8`**：8 个 poly 的 batch 口径（对齐 `sepolyvec8`），**不**表示 ML-KEM 的 k。
- **`-vec`**：AscendC 向量实现（fork 自 vec-k4-v2 的 S1/S2/S3 原语）。

### 8.4 验收

```bash
cd ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
F203_NTT_MODE=intt bash run.sh -r sim -v Ascend910B4
python3 scripts/cross_check_ntt_study_c.py --regen
```

### 8.5 关联

- [`STATUS.md`](../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/STATUS.md)
- 定稿 note：[F203-polyvec8-stage123-NTT-INTT技术总结.md](../../docs/notes/F203-polyvec8-stage123-NTT-INTT技术总结.md)
- 全链路集成（4-poly 2s1e）：[`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../ascendc-tests/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)
