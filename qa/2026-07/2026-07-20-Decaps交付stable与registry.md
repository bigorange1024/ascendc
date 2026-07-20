# 2026-07-20 — Decaps `#交付#` stable · registry · 六算子齐

## 结论

[`exp-fips203-mlkem-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/) 按 customspec 落地自包含实现：**CPU+SIM 全链绿**。

| 项 | 证据 |
|----|------|
| I/O | `dk_kem`+`c`+LUT → 仅 `K`；`K` max=0 |
| Launch | SIM 4 / CPU 6；单库；`decaps_1session` |
| tick | D **286999** + E **745790**（对标 pass-fix） |
| vendor | `decrypt/` + `prep/`/`compute/` + `kem/`；`prepare_dec_shim.sh` |

## 验收（预研阶段；同日上午）

```bash
cd examples/incubating/exp-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## Round-trip（同日续；中间态 → 终态）

| 阶段 | `DECAPS_DIR` 默认 | 证据 |
|------|-------------------|------|
| 预研绿后 | incubating `exp-…-kem-decaps-k4` | roundtrip CPU+SIM agreement+reject **PASS** |
| `#交付#` 后 | **stable** `stable-…-kem-decaps-k4` | 同脚本复验 **PASS**（见下「复验证据」） |

涉及脚本：`roundtrip_kem_decaps.sh` / `roundtrip_kem_keygen_encaps_decaps.sh` / `liboqs_kem_vs_ascendc.sh` / `kat_liboqs_kem_decaps.py`。

## 下一刀（交付后仍开）

- ✅ `#交付#` → stable（见下节）
- **T19i**：`fo_only` 内联（SIM 4→3）；**T2-npu**：NPU 实机

## 登记表

已刷新 [`qa/active_sim_regress_summary.md`](../active_sim_regress_summary.md)：stable Decaps **1032762**；incubating 同期 D**286999**+E**745790**；`scripts/` Decaps 默认终态 → **stable**。

## liboqs 分项 KAT（同日续）

`KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh`（当时 `DECAPS_DIR`→exp；交付后默认同脚本指 stable 复验绿）：

| 模式 | 结果 |
|------|------|
| CPU×10 | **PASS** |
| SIM×3 | **PASS** |

固定 stash `dk`；每轮 liboqs encaps→`c`；device `K` 逐字节一致。

## baseline-registry（同日续）

新增 [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md)：登记合法/拒绝路径 golden 块（liboqs encaps/decaps、`J`/`G` Host 对照、LUT、CPU `golden_v`）；晋级后「适用」已改指 stable。

## Skill + 历史缺表补登记（同日续）

约定：**registry 硬卡点放在 Skill，不改 Rule** — `#交付#`/`#验收#` 晋级前必须定稿；`$规格$`/【预研】允许缺表。

| 变更 | 路径 |
|------|------|
| Skill | [`ascendc-delivery`](../../.cursor/skills/ascendc-delivery/SKILL.md) 门禁§3 + 清单；[`pre-research`](../../.cursor/skills/pre-research/SKILL.md)「收敛 toward delivery」提示 |
| 补登记 | [`pke-keygen-baseline-registry`](../../docs/specs/fips203-mlkem1024-pke-keygen-baseline-registry.md)、[`pke-decrypt-baseline-registry`](../../docs/specs/fips203-mlkem1024-pke-decrypt-baseline-registry.md) |
| 索引 / STATUS | `docs/specs/INDEX.md`；stable PKE KeyGen / Decrypt `STATUS.md` 链到 registry |

当前六表齐：PKE KeyGen / Encrypt / Decrypt · KEM KeyGen / Encaps / Decaps。

## `#交付#` Decaps → stable（同日续）

| 项 | 内容 |
|----|------|
| 晋级 | 复制 [`exp-…-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/) → [`stable-…-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/) |
| 默认路径 | 仓库 `DECAPS_DIR` → stable；registry「适用」→ stable |
| 复验 | stable CPU / SIM / KAT×10+3 / roundtrip（含拒绝）— 见 STATUS |


## `#交付#` stable 复验证据（同日续）

目录：[`stable-fips203-mlkem-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/)

| 项 | 结果 |
|----|------|
| `run.sh` CPU | **PASS**（`K` max=0） |
| `run.sh` SIM | **PASS** tick **1032762**=D**286896**+E**745866**；无 stray dump |
| liboqs KAT | **PASS** CPU×10 + SIM×3（CPU 曾一轮 flake，复测绿） |
| roundtrip | **PASS** CPU+SIM（agreement + reject） |

`DECAPS_DIR` 默认已指向 stable。


## 收尾：纪要刷新 · 备份 · 合入 main（同日续）

| 项 | 说明 |
|----|------|
| 纪要 / 索引 | 本日文件更名关键词；`qa/INDEX` · `qa/2026-07/INDEX` · `qa/TODO` · `AGENT_HANDOFF` · `AGENTS` · `README` · 各 `examples/*/INDEX` |
| 近期核对 | 07-15 Encaps `#验收#`、07-17 T2、07-18 pass-probe/`$规格$` 已有日纪要；本文件补齐当日【预研】→registry→`#交付#`全链；07-18「未提交」已标注闭合 |
| 本地备份 | `bash backup-project.sh` → **`backup/v0.1_20260720053001`**；收尾再刷一版见下 |
| 合入 | feature `cursor/stable-kem-decaps-delivery-8244` → **`main`**（`42d4f24`）并推送 |
| 里程碑 | PKE 三段 + KEM KeyGen/Encaps/**Decaps** **六算子 stable 齐** |

## 再收尾：缺口补记 · 二次备份 · 推 main（同日续）

| 项 | 说明 |
|----|------|
| 补记 | `qa/TODO` 正式挂 **T19i**；07-18 纪要「未提交」→ 已合入说明；本文件验收/DECAPS 路径按时间线澄清 |
| 二次备份 | `bash backup-project.sh` → **`backup/v0.1_20260720053530`**（4035 文件） |
| 推送 | 文档补丁直接落 **`main`** |


## T19i pass-probe/`pass-fix`：`fo_only`→`l18_l19`（同日续）

目录：[`pass-fix-f203-alg21-kem-decaps-device-k4`](../../ascendc-tests/pass-fix-f203-alg21-kem-decaps-device-k4/)（用户口中的 pass-probe 已更名）。

| 项 | 内容 |
|----|------|
| 方案 | `INTEGRATION_PLAN` §7：探针本地覆盖 `l18_l19`；禁止改共享 Encrypt |
| 实现 | pack 后 `SyncAll<isAIVOnly>`；AIV0 `KemDecFo`；Host 删 `fo_only` |
| CPU | **PASS** |
| SIM | **PASS** D**287037**+E**763886**；3 launch |
| 拒绝 | CPU+SIM **PASS** |
| 未做 | stable/exp Decaps 镜像（仍 SIM 4） |


## T19i incubating customspec 修订（同日续；仅规格）

路径：[`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex/.pdf`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/)

| 项 | 内容 |
|----|------|
| 锁定 | 生产 SIM **3** launch；FO 并入 `l18_l19` 尾（`SyncAll` + AIV0 `KemDecFo`） |
| CPU | 仍 6（`pack_fo`） |
| 禁止 | 默认 `fo_only`；回写共享 PKE Encrypt `compute/` |
| 基线 tick | pass-fix T19i D**287037**+E**763886** |
| 未做 | 本目录【迭代】写码；stable customspec/实现 |

本轮按 ascendc-impl-spec **只改规格**；待用户确认并说「可以写代码」/【迭代】后再改实现。


## T19i incubating【迭代】落地（同日续）

目录：[`exp-fips203-mlkem-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/)

| 项 | 结果 |
|----|------|
| 规格 | 已确认 customspec SIM 3 |
| 实现 | `kem/f203_encrypt_l18_l19_kernel.cpp` 覆盖；Host 删 `fo_only` |
| CPU | **PASS** |
| SIM | **PASS** D**286846**+E**763935** |
| 拒绝 | CPU+SIM **PASS** |
| 未做 | stable Decaps 镜像 |


## T19i stable `#修改#`（同日续）

目录：[`stable-fips203-mlkem-kem-decaps-k4`](../../examples/stable/stable-fips203-mlkem-kem-decaps-k4/)

| 项 | 结果 |
|----|------|
| customspec | SIM **3** 锁定；验收含 KAT + roundtrip |
| 实现 | 镜像 exp：`kem/l18_l19` 覆盖；Host 删 `fo_only` |
| CPU / SIM | **PASS**；D**286851**+E**763769** |
| 拒绝 | CPU+SIM **PASS** |
| liboqs KAT | **PASS** CPU×10 + SIM×3 |
| roundtrip | **PASS** cpu+sim（agreement + reject） |
| T19i | **关闭**（pass-fix + exp + stable 齐） |
