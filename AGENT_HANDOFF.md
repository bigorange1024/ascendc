# Agent 交接 — 自 `v0.1_20260626191947` 起的进展

> **读者**：公司侧 Agent / 接手同事。  
> **你的备份基线**：[`backup/v0.1_20260626191947/`](backup/v0.1_20260626191947/)（2026-06-26 19:19:47）。  
> **当前快照**：[`backup/v0.1_20260629091128/`](backup/v0.1_20260629091128/)（含本文）。  
> 解压/同步到 WSL 后，先读本文 §2 增量，再按 §5 验收关键探针。

---

## 1. 备份对照

| 项 | 基线（你手里的） | 当前 |
|----|------------------|------|
| 路径 | `backup/v0.1_20260626191947/` | `backup/v0.1_20260629091128/` |
| 时间 | 2026-06-26 19:19:47 | 2026-06-29 09:11:31 |
| 规模 | 旧版白名单（**无** `scripts/`、**无** `HOME-KEYGEN-DEBUG.md`、**无** `.cursor/`） | 扩展白名单（见 [`BACKUP_README.txt`](backup/v0.1_20260629091128/BACKUP_README.txt)） |
| 命令 | — | 工程根 `bash backup-project.sh` |

**基线里仍存在的旧目录名**（活跃树已改名，对比 diff 时勿混淆）：

| 基线中的名字 | 当前活跃名 |
|--------------|------------|
| `pass-fix-f203-alg7-single-poly-sample-ntt-d12-vec-k4` | `pass-fix-f203-alg7-sample-ntt-k4` |
| `pass-fix-f203-alg13-lines3-7-a-hat-16poly-k4` | `pass-fix-f203-alg13-lines3-7-a-hat-k4` |
| `pass-fix-f203-alg13-device-presample-k4` | `pass-fix-f203-alg13-lines8-15-se-k4` |
| `fix-f203-alg13-device-keygen-k4` | **`pass-fix-f203-alg13-device-keygen-k4`** |
| `fix-f203-alg8-cbd-eta2-k4` | **`pass-fix-f203-alg8-cbd-eta2-k4`** |
| `fix-f203-alg13-device-presample-a-hat-k4` | **`frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4`**（已冻结） |

---

## 2. 自基线以来的进展（按主题）

### 2.1 KeyGen SIM prep `a_hat` workaround（2026-06-26，基线当日末）

**问题**：prep 融合路径下双 AIV 并行写 `a_hat` 行 8–15 在 SIM 上错误（`max_abs_diff≈3300`）；CPU 孪生不覆盖该路径。

**做法**：`F203_AHAT16_BLOCK_DIM==2` 时 block0 **串行**两片 `BuildAHat16ShardWithUb`；已合入 exp 与 keygen 探针。

**文档**：根目录 [`HOME-KEYGEN-DEBUG.md`](HOME-KEYGEN-DEBUG.md)（基线备份**没有**此文件，必读）。

**待办（T13h）**：恢复双 AIV **并行** Â，对照 `pass-fix-f203-alg13-lines3-7-a-hat-k4` 修 UB/PIPE — 非阻塞 workaround 移除。

### 2.2 标量探针冻结 + 共享库抽取（2026-06-26）

- `frozen-fix-f203-alg13-se-device-scalar-k4`、`frozen-fix-f203-alg13-host-scalar-fullchain-k4` 迁入 `frozen/`
- `golden_se_sampling.py` → `library/shared/fips203_se_sample/`
- 纪要：[`qa/2026-06/2026-06-26-标量探针冻结.md`](qa/2026-06/2026-06-26-标量探针冻结.md)

### 2.3 KeyGen 全设备 ρ + ek‖ρ（2026-06-26 前后）

- 共享 `library/shared/keccak_f1600_kernel/fips203_device_sha3.hpp`
- prep 设备写 `rho.bin`；G4 设备 `f203_keygen_ek_append`（去掉 Host `golden_rho` 拷贝）
- exp 与 keygen 探针同步

### 2.4 KeyGen 探针 pass + 生产 I/O（2026-06-28）

**目录**：`fix-f203-alg13-device-keygen-k4` → **`pass-fix-f203-alg13-device-keygen-k4`**

| 项 | 内容 |
|----|------|
| 生产 I/O | `input/` 仅 seed+LUT；`output/` 仅 ek/dk；单入口 `ascendc_keygen_bbit` |
| 验收 | CPU + SIM + liboqs KAT；SIM total tick **886801** |
| 注释 | `scripts/inject_probe_code_comments.py` → 120 文件 `@probe` 头 |
| 实现方案 | `pass-fix-f203-alg13-device-keygen-k4-实现方案-customspec.tex` |

**SIM 规则**（勿回退）：`build → sim_env_export → camodel → kernel`；勿用 CPU SUCCESS 日志推断 AI Core 用量。

纪要 §1–§5：[`qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md`](qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md)

### 2.5 交付示例 exp 自包含化（2026-06-28）

**目录**：[`examples/incubating/exp-mlkem-f203-pke-keygen-k4/`](examples/incubating/exp-mlkem-f203-pke-keygen-k4/)

基线仍为多门禁（G0–G4、分段 cmake、`compute_io/` staging 等）；当前已收敛为：

| 项 | 内容 |
|----|------|
| vendored | `prep/`、`compute/`、`cmake/keygen/`、`scripts/prep|compute/`、内嵌 Host LUT |
| 唯一入口 | `bash run.sh` → 2 launch（prep \| compute+ek‖ρ） |
| 已删 | G0–G4 门禁、`main_keygen_prep`、`f203_keygen_ek_append` 旁路、分段 cpu_lib 等 |
| SIM | total_tick ≈ **884532**（prep ≈801491 + compute ≈83041） |
| KAT | `kat_liboqs_vs_ascendc.sh`：10×CPU + 1×SIM；`KEYGEN_KERNEL_BUDGET_SEC=1200` 防 SIM 超时 124 |

定稿 note：[`docs/notes/F203-KeyGen-exp交付示例技术总结.md`](docs/notes/F203-KeyGen-exp交付示例技术总结.md)

### 2.6 KeyGen 子轨重命名 + Phase A 冻结（2026-06-28）

- 三节 KeyGen 子轨目录语义化（见 §1 对照表）
- [`fix-f203-alg13-device-presample-a-hat-k4`](ascendc-tests/frozen/frozen-fix-f203-alg13-device-presample-a-hat-k4/) → **frozen**（Phase A benchmark 只读；tick 见 `SIM_BENCHMARK.md`）
- Alg.8 CBD：`fix-f203-alg8-cbd-eta2-k4` → **`pass-fix-f203-alg8-cbd-eta2-k4`**（P2 双 AIV SIM **18048** tick）

纪要 §6–§7：同上 2026-06-28 文档。

### 2.7 Encrypt 侧 fix 探针（2026-06-27～28 起，进行中）

基线**无**以下目录；当前为 **fix-**（未 pass）：

| 目录 | 语义 |
|------|------|
| `fix-f203-compress-d-vec-k4` | §4.2.1 Compress_d（d=4 SIM **3247**） |
| `fix-f203-decompress-d-vec-k4` | Decompress_d |
| `fix-f203-byteencode-d-vec-k4` | Alg.5 ByteEncode_d（d=4 SIM **5435**） |
| `fix-f203-alg6-bytedecode-d-vec-k4` | Alg.6 ByteDecode_d |

KeyGen 侧 ByteEncode₁₂ 仍为 [`pass-fix-f203-2s1e-byteencode12-vec-k4`](ascendc-tests/pass-fix-f203-2s1e-byteencode12-vec-k4/)（与 Encrypt d=4/10 分离）。

### 2.8 8-poly 批 NTT/INTT pass 探针（2026-06-28 周末，基线后最大新增）

**目录**：[`ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`](ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)（基线**不存在**）

| 要点 | 内容 |
|------|------|
| 语义 | **8×256 polyvec** batch NTT/INTT（`polyvec8`）；与 ML-KEM **k 无关** |
| 构图 | Tag5T S1→S2→S3；Stage1 紧凑 `[HI₈,LO₈]` |
| 切换 | **仅换 LUT**：`F203_NTT_MODE=ntt\|intt` |
| 资源 | `blockDim=1`；MIX **1×AIC + 2×AIV**；**1 launch** 全链 |
| 验收 | CPU/SIM **max=0**；NTT SIM **30347** / INTT **30340** tick |
| C 对拍 | `scripts/cross_check_ntt_study_c.py --regen`（不接入 run.sh） |

**曾修复**（勿回退）：SIM launch 传 `TilingData*`；`gen_data.py` 传 `STAGE123_POLYVEC8_MIX_PASS`；cross_check 避免 stale `dst.bin`。

定稿 note：[`docs/notes/F203-polyvec8-stage123-NTT-INTT技术总结.md`](docs/notes/F203-polyvec8-stage123-NTT-INTT技术总结.md)

### 2.9 工程基础设施

| 项 | 说明 |
|----|------|
| [`backup-project.sh`](backup-project.sh) | 2026-06-28 重写：含 `scripts/`、`prep/`、`compute/` vendored 树；命名 `v0.1_*` |
| [`scripts/sim_env.sh`](scripts/sim_env.sh) 等 | 基线备份缺失；SIM 依赖 `out/lib/libascend_dump.so` stub |
| [`scripts/inject_probe_code_comments.py`](scripts/inject_probe_code_comments.py) | 探针 `@probe` 注释批量注入 |
| 新增 notes | KeyGen exp、CBD η=2、ByteEncode prefetch、polyvec8 等 — 见 [`docs/notes/INDEX.md`](docs/notes/INDEX.md) |
| 本文 | `AGENT_HANDOFF.md` — 已纳入备份白名单 |

---

## 3. 当前工程状态（相对基线）

| 轨道 | 基线 (06-26) | 现在 |
|------|--------------|------|
| KeyGen 探针 | `fix-f203-alg13-device-keygen-k4`，多路径/staging | **`pass-`**，生产 I/O + KAT ✓ |
| KeyGen exp | G0–G4 门禁、非自包含 | **自包含唯一路径** + KAT ✓ |
| KeyGen prep SIM | 刚合入 a_hat workaround | workaround 仍在；**T13h 并行 Â 未修** |
| Alg.13 2s1e 向量全链 | `vec-k4-v2` 已有 | 仍 **77958** tick；文档/注释刷新 |
| Phase A presample | `fix-f203-alg13-device-presample-a-hat-k4` 活跃 | **已 frozen** |
| 8-poly NTT/INTT | 无 | **pass 探针** ✓ |
| Encrypt 编解码 | 无 fix 探针 | **4 个 fix- 探针**（未 pass） |
| 实机 NPU | 未测 | **仍未测** |

---

## 4. 建议下一步（按优先级）

完整打开项：**[`qa/TODO.md`](qa/TODO.md)**

1. **T13b — vec-k4-v3**：fork `vec-k4-v2`，接入 V3 预采样 + 设备 `a_hat`（T13a-v ✅、T13g ✅）
2. **T13h — 恢复 prep 双 AIV 并行 Â**：[`HOME-KEYGEN-DEBUG.md`](HOME-KEYGEN-DEBUG.md) + [`PIPE_SYNC_EVAL.md`](ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md)
3. **Encrypt fix 探针**：compress/decompress/byteencode/bytedecode 推至 pass（若主线转 Encaps）
4. **polyvec8 可选**：mixPass 0/1/2 分段 tick；或评估并入 KeyGen/Encrypt 链
5. **T2a/T2b**：`docs/specs/` KeyGen plan + baseline-registry

---

## 5. Agent 快速上手

### 5.1 阅读顺序

1. [`README.md`](README.md)
2. **本文**（基线 → 当前增量）
3. [`HOME-KEYGEN-DEBUG.md`](HOME-KEYGEN-DEBUG.md)
4. [`qa/TODO.md`](qa/TODO.md) + [`qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md`](qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md)
5. [`.cursor/rules/ascendc-development.mdc`](.cursor/rules/ascendc-development.mdc) — **frozen 禁止抄码**

### 5.2 环境

- WSL2 + Ubuntu 22.04；无 NPU → CPU 孪生 + CAModel SIM
- `source scripts/sim_env.sh`（或各探针 `run.sh` 自动处理）
- 详见 [`docs/engineering/环境复现与开发指南.md`](docs/engineering/环境复现与开发指南.md)

### 5.3 运行约定

```bash
# 必须带 -v Ascend910B4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

**从基线升级后建议跑的 smoke**：

```bash
# KeyGen 探针（基线时为 fix-，现为 pass-）
cd ascendc-tests/pass-fix-f203-alg13-device-keygen-k4
bash run.sh -r cpu -v Ascend910B4
bash kat_liboqs_vs_ascendc.sh

# KeyGen exp（结构已与基线不同，勿按 G0–G4 旧文档跑）
cd examples/incubating/exp-mlkem-f203-pke-keygen-k4
bash run.sh -r sim -v Ascend910B4

# 基线后新增：8-poly NTT/INTT
cd ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec
bash run.sh -r sim -v Ascend910B4
F203_NTT_MODE=intt bash run.sh -r sim -v Ascend910B4
python3 scripts/cross_check_ntt_study_c.py --regen
```

### 5.4 从基线 diff 时的注意

- 旧备份 `pass-fix-f203-alg13-device-presample-k4` = 现 `pass-fix-f203-alg13-lines8-15-se-k4`
- 旧 `fix-f203-alg13-device-presample-a-hat-k4` = 现 `frozen/...`（勿继续开发）
- exp KeyGen **不要**恢复 `main_keygen_prep`、G4 分段、`compute_io/` — 已删 intentionally
- `frozen/` 内旧名快照只读；**禁止**把 frozen 路线抄回活跃目录

---

## 6. 本次备份（相对基线的终点）

- **路径**：[`backup/v0.1_20260629091128/`](backup/v0.1_20260629091128/)
- **说明**：[`backup/v0.1_20260629091128/BACKUP_README.txt`](backup/v0.1_20260629091128/BACKUP_README.txt)
- **含**：`AGENT_HANDOFF.md`、`scripts/`、`HOME-KEYGEN-DEBUG.md`、全部 pass/fix 探针 vendored 树

---

## 7. 索引

| 主题 | 链接 |
|------|------|
| 06-26 标量冻结 + prep workaround | [`qa/2026-06/2026-06-26-标量探针冻结.md`](qa/2026-06/2026-06-26-标量探针冻结.md) |
| 06-28 KeyGen pass / exp / 子轨 / polyvec8 | [`qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md`](qa/2026-06/2026-06-28-KeyGen探针pass前缀与生产IO.md) |
| 活跃探针表 | [`ascendc-tests/INDEX.md`](ascendc-tests/INDEX.md) |
| NTT 向量指南 | [`docs/notes/MLKEM-NTT-向量与标量实现指南.md`](docs/notes/MLKEM-NTT-向量与标量实现指南.md) |
