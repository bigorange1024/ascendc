# 2026-07-15 — TODO、SIM tick 登记与 T19a Encaps device PASS

## 1. TODO

- **最近刷新**改为本日；主线曾指向 **T19a Encaps device**，本日已 **PASS** → 下一 **T19b/c**。
- **T19f** 保持完成（`stable-…-kem-keygen-k4`，tick **706633**）。
- 记下 07-14 已落地：**PKE/KEM 默认哈希 RNG**、`add_custom -r/-v`、勿手写 `SIM_DIRECT=1` 口径。
- **T20**（中文注释补课 Wave1–5）迁入**已关闭**。

## 2. `qa/active_sim_regress_summary.md`

旧 `.txt` 是残缺 regress dump；本日改为 **Markdown 分节表格**（目录可点进用例）：

- 范围：`examples/stable/*`、`examples/incubating/exp-*`、`ascendc-tests` 活跃探针（**不含** `frozen/`）。
- 值优先取各 `STATUS.md` / `INDEX.md` 默认配置验收 tick。
- stub / 未记载：`n/a`；统一整数 compress/decompress 探针壳标 `→exp`。

交付侧速查：PKE KeyGen **542393** · Encrypt **627590** · Decrypt **283290** · KEM KeyGen **706633**。

## 3. T19a Encaps device — 实现与验收 PASS

锁定（未改）：

- `m` 为 **GM 输入**（Alg.17）；SHA3 头 **并入** stable Encrypt prep 前段，不另 launch。
- Encrypt **编译期引用** stable；禁止 frozen G5 vendor。

落地：[`INTEGRATION_PLAN.md`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/INTEGRATION_PLAN.md) + `kem/` + `main_kem_encaps.cpp` + `run.sh` / `gen_data` / `verify`。

**排障**：`run.sh` 不得在 `source env.sh/setenv` 前 `set -e`（`grep -q` 非零会静默退出）；二进制须在用例根 cwd 跑（`ReadFile("./input/…")`）。

**验收证据**：

```text
bash run.sh -r cpu -v Ascend910B4  → verify c/K max=0 PASS
bash run.sh -r sim -v Ascend910B4  → verify c/K max=0 PASS；Total tick 721010
```

后续可选：更名 `pass-fix-…`（Encaps 分项 kat 已默认指本探针）。

## 4. 恢复 `docs/research/`

用户要求新建（恢复）[`docs/research/`](../../docs/research/INDEX.md)：调研草稿区，与 `notes/` 定稿分离。已写 INDEX + T19a 要点摘要；同步 `docs/INDEX.md`、`README.md`、Rule 子目录表、`AGENT_HANDOFF.md`。

## 5. 已验证能力 DAG 预研方法论（专题草稿）

Cloud 同步至 `origin/main` @ `5a63ae4` 后，将此前关于「数学模型约束预研写码 / 降低幻觉」的讨论要点写入：

- TeX / PDF：[`docs/research/2026-07-15-已验证能力DAG预研方法论要点.tex`](../../docs/research/2026-07-15-已验证能力DAG预研方法论要点.tex)（`xelatex-clean` 已编译）
- 内容：节点/边/证书、Agent 门禁、形式语言与自动机草图、kem.keygen 校准、Encaps/Decaps 验证计划、M0–M5；**未**记题外保护策略
- 后续数日专题讨论；可穿插测试与写码；结论稳定后再迁 `notes/`

## 6. liboqs Encaps 分项 KAT（device）

- 脚本默认 `ENCAPS_DIR` → [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)；生产路径 `M_FILE` 喂随机 `m`（无需 `KEM_ENC_EXT_SEED`）。
- 验收：`bash scripts/liboqs_kem_encaps_batch.sh` → **CPU×10 + SIM×3 PASS**（固定 stash `ek`，每轮 `os.urandom` m ↔ liboqs `encaps_derand` 逐字节 `c`/`K`）。
- 墙钟约 20min（SIM×3）；quiet log：`output/liboqs_kem_encaps/kat.log`。

## 7. Encaps device 更名 `pass-fix-…` + scripts 默认切换

- 目录：`fix-f203-alg20-kem-encaps-device-k4` → [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)
- `ENCAPS_DIR` 默认改指新目录：`roundtrip_kem_encaps.sh`、`liboqs_kem_vs_ascendc.sh`、`roundtrip_kem_keygen_encaps_decaps.sh`、`liboqs_kem_encaps_batch.sh`、`kat_liboqs_kem_encaps.py`
- INDEX：Encaps 从「规划中」挪入活跃 `pass-fix` 表；Decaps device 仍 T19b/c
- 其后 `#验收#` 默认再改指 stable（见 §11）

## 8. `$写规格$` — Alg.20 Encaps incubating customspec

- 新建目录（**仅规格，无实现码**）：[`examples/incubating/exp-fips203-mlkem-kem-encaps-k4/`](../../examples/incubating/exp-fips203-mlkem-kem-encaps-k4/)
- 规格：[`exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex`](../../examples/incubating/exp-fips203-mlkem-kem-encaps-k4/exp-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex) + PDF（`xelatex-clean` OK）
- 契约对齐 [`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg20-kem-encaps-device-k4/)：Alg.17；$m$ GM 入；$H$/$G$ 并入 prep；SIM 2 / CPU 5；$c$+$K$；incubating **vendored**（非 `STABLE_ENCRYPT_ROOT`）
- 已登记 [`examples/incubating/INDEX.md`](../../examples/incubating/INDEX.md)、API 查阅索引
- **下一步**：用户确认 customspec 后，明确【预研】/「可以写代码」再落地实现

## 9. 【预研】Encaps incubating 写码 + CPU/SIM PASS

- 目录：[`exp-fips203-mlkem-kem-encaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-encaps-k4/)（vendored Encrypt + `kem/`；变量用 FIPS `$m$`/`$r$`/`$K$`/`$c$`）
- 验收：CPU `c`/`K` max=0（含随机 `m`×3）；SIM max=0，tick **721211** / **721033**（对标 device 721010）
- 详情：[`STATUS.md`](../../examples/incubating/exp-fips203-mlkem-kem-encaps-k4/STATUS.md)
- `qa/active_sim_regress_summary.md`：**保留** pass-fix device **721010**，**另加** incubating **721211**
- 下一刀：Encaps `#交付#` → stable，或 T19b/c Decaps device

## 10. incubating Encaps liboqs 分项 KAT（对齐 device）

同历史口径（`kat_liboqs_kem_encaps.py` / device STATUS）：

1. KeyGen CPU 一次 → `kem_keypair_stash_bootstrap.sh`（固定 `ek`）
2. `ENCAPS_DIR=…/exp-fips203-mlkem-kem-encaps-k4 bash scripts/liboqs_kem_encaps_batch.sh`

**结果（2026-07-15）**：**CPU×10 + SIM×3 PASS**（每轮 `os.urandom` m ↔ liboqs `encaps_derand`，`c`/`K` 逐字节一致）。  
quiet log：`output/liboqs_kem_encaps/exp_encaps_kat.log`。

## 11. `#验收#` Encaps 晋级 stable

- 复制来源：[`exp-fips203-mlkem-kem-encaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-encaps-k4/)
- 目标：[`stable-fips203-mlkem-kem-encaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-encaps-k4/)（customspec 更名 + PDF；registry；notes）
- 注释：强化 `kem/`、`main`、`gen_data`/`verify` 中文说明
- 验收：CPU **PASS**；SIM tick **721119**；liboqs KAT **CPU×10+SIM×3 PASS**（复跑；首轮 SIM 曾一次 exit 139 flake）
- `scripts/` 默认 `ENCAPS_DIR` → stable；device 仍可 `ENCAPS_DIR=` 覆盖
- 索引：`examples/{INDEX,stable,incubating}` · `README` · `AGENTS` · `AGENT_HANDOFF` · `qa/TODO` **T19g** · `active_sim_regress_summary`
- 定稿笔记：[`docs/notes/F203-KEM-Alg20-Encaps设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg20-Encaps设备全链技术总结.md)
- 提交推送：见本轮 commit（用户指令）

## 12. Encaps `run.sh` WSL/三环境对齐

用户要求：SIM 运行脚本须考虑 WSL。已改 stable + incubating Encaps `run.sh`：

| 点 | 处理 |
|----|------|
| cwd | 脚本入口 `cd` 到用例根（Host `ReadFile("./input")` + `kernel-run-timeout` 绑 `$(pwd)`）；仓库根绝对路径调用也可 |
| Usage | 与 Encrypt/KeyGen 对齐：` -r sim` 默认 `SIM_DIRECT=1`，**勿手写**；`-r auto/verify`；WSL 拒 `-r npu` |
| SIM env | 先 `sim_env_export`（WSL dump 桩 / Cloud 不装桩）再 `camodel_sim_log` |
| 二进制 | `cd` 后相对路径 `./ascendc_kem_encaps_bbit` |

文档 Smoke 已去掉「手写 `SIM_DIRECT=1`」写法。
