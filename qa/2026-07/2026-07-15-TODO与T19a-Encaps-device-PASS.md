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

落地：[`INTEGRATION_PLAN.md`](../../ascendc-tests/fix-f203-alg20-kem-encaps-device-k4/INTEGRATION_PLAN.md) + `kem/` + `main_kem_encaps.cpp` + `run.sh` / `gen_data` / `verify`。

**排障**：`run.sh` 不得在 `source env.sh/setenv` 前 `set -e`（`grep -q` 非零会静默退出）；二进制须在用例根 cwd 跑（`ReadFile("./input/…")`）。

**验收证据**：

```text
bash run.sh -r cpu -v Ascend910B4  → verify c/K max=0 PASS
bash run.sh -r sim -v Ascend910B4  → verify c/K max=0 PASS；Total tick 721010
```

后续可选：更名 `pass-fix-…`；仓库 `ENCAPS_DIR` / 分项 kat 改指本探针。
## 4. 恢复 `docs/research/`

用户要求新建（恢复）[`docs/research/`](../../docs/research/INDEX.md)：调研草稿区，与 `notes/` 定稿分离。已写 INDEX + T19a 要点摘要；同步 `docs/INDEX.md`、`README.md`、Rule 子目录表、`AGENT_HANDOFF.md`。
