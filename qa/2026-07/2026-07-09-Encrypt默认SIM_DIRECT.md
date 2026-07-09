# 2026-07-09 — Encrypt 探针默认 `SIM_DIRECT` + 工程债 1–3 + 路线 11 关闭

## 1. 用户约束：默认即最优路径，勿再要求手动编译/运行选项

用户明确：测试阶段可用编译/运行选项切代码段；**代码稳定后**须改成默认跑最优最正确路径，**不要让用户再手动输入更多选项**（含 `SIM_DIRECT=1`）。

说明：`SIM_DIRECT` 本身不是编译宏，而是 sim 是否走 msprof/`OPPROF_*` 的运行开关。成熟探针（keygen/encaps）早已在 `run.sh` 的 sim 分支内 `export SIM_DIRECT=1`；Encrypt 系列文档却仍写 `SIM_DIRECT=1 bash run.sh …`，违反「默认 = 生产全量」口径。

## 2. 已改（2026-07-09）— 默认 `SIM_DIRECT=1`

下列 PASS 探针 `run.sh` 在 `RUN_MODE=sim` 时自动 `export SIM_DIRECT="${SIM_DIRECT:-1}"`；Usage 注释改为：

```bash
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4   # 无需手动 SIM_DIRECT
```

| 探针 | 改动 |
|------|------|
| `pass-fix-f203-alg14-pke-encrypt-device-k4` | run.sh + STATUS + INTEGRATION_PLAN |
| `pass-fix-f203-alg14-lines3-15-encrypt-prep-k4` | run.sh |
| `pass-fix-f203-alg14-lines2-18-19-21-encrypt-compute-k4` | run.sh |
| `pass-fix-f203-alg14-lines2-24-encrypt-compute-tail-k4` | run.sh |
| `pass-fix-f203-alg14-lines20-22-23-24-encrypt-pack-k4` | run.sh |

`AGENT_HANDOFF.md` smoke 命令同步去掉手动 `SIM_DIRECT=1`。调试采性能仍可显式 `SIM_DIRECT=0`（非默认）。

## 3. 全链 Encrypt 现状（承接 07-08）

- 探针：`pass-fix-f203-alg14-pke-encrypt-device-k4`
- I/O：in `ek+m+coins` → **out 仅密文 c**（u/v 不落盘）
- 验收：CPU+SIM `c` max=0；SIM ~626k tick；`SEED_D=20260619`
- 家里续测：`git pull` 后直接 `bash run.sh -r cpu|sim -v Ascend910B4`

## 4. 高价值工程债 1–3（**保留**）

用户确认先做改进清单高价值 1–3（中等暂缓）。**已落地，与路线 11 无关，回滚路线 11 时须保留**：

1. **`run.sh` 资源友好化（T7b 合入全链）**：默认 `ENCRYPT_SKIP_REBUILD=1`、`CMAKE_BUILD_JOBS=2`；stamp 含 RUN_MODE+主要宏；`ENCRYPT_FORCE_REBUILD=1` 强制全量。对齐 alg20。
2. **`gen_data.py` 自包含**：优先复用 correctness 产物；缺失时本目录 `gen_ek_pke(SEED_D)` + `rng(SEED_D+991)`→m/coins + `golden_encrypt`→c；**仍写** `lut_*.bin`（供 host `ReadFile`→ws）。
3. **文档口径**：STATUS/PLAN/run.sh/main/layout 统一为 **FIPS 行 1–22 完整 Encrypt**；删「vendor compute 待建」等过时结构图。

correctness 探针的 T7b 对齐仍待。

## 5. 性能路线 11（LUT H2D→ROM）— **关闭**

曾尝试去掉 `lut_*.bin` / 设备直读常量区；用户裁定不值得继续（改后数分钟无输出，不应靠拉长 900s 预算硬等）。

| 子方案 | 结果 | 说明 |
|--------|------|------|
| A. host 静态表 → `memcpy` ws → H2D | **放弃** | SIM 数分钟无输出；保守段还误引入 `l18_l19` **重复 `FsmWait(ST_NTT_AIV_SPLIT)`** → 死锁 |
| B. AIC 直读 `__gm__ const` 作 MMAD B | **证伪** | SIM launch 2 挂死 ~900s timeout + core dump |
| 回滚 | **仅回滚路线 11** | 删除 `f203_lut_*.inc` / ROM host 头；恢复 `ReadFile(lut_*.bin)`；**保留 §4 的 1–3** |

**实验纪律**：基线 SIM **~626k tick、墙钟 ~100s 内**；若改后需拉长防挂死预算或长时间无日志，即视为退化。

**当前 LUT 路径**：host `ReadFile(lut_*.bin)` → ws → H2D（与 PASS 基线一致）。LUT H2D→ROM **不再作为本探针优化项**。

## 6. examples 晋级：Alg.14 Encrypt customspec（同日）

新建 [`examples/incubating/exp-mlkem-f203-pke-encrypt-k4/`](../../examples/incubating/exp-mlkem-f203-pke-encrypt-k4/)：

- **规格**：[`exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex`](../../examples/incubating/exp-mlkem-f203-pke-encrypt-k4/exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex)（+ PDF）
- **I/O**：`ek_pke`+`m`+`coins` → **仅** `c`；禁止 Â/y/u/v 等中间态落盘
- **Launch**：SIM 2 / CPU 5；基线探针 `pass-fix-f203-alg14-pke-encrypt-device-k4`
- **状态**：`$写规格$` 已闭环；**【预研】写码**已落地 — CPU+SIM `c max=0`（SIM tick **627614**）；`output/` 仅 `c.bin`

