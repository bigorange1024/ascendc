# 2026-07-20 — Decaps incubating 【预研】CPU+SIM PASS

## 结论

[`exp-fips203-mlkem-kem-decaps-k4`](../../examples/incubating/exp-fips203-mlkem-kem-decaps-k4/) 按 customspec 落地自包含实现：**CPU+SIM 全链绿**。

| 项 | 证据 |
|----|------|
| I/O | `dk_kem`+`c`+LUT → 仅 `K`；`K` max=0 |
| Launch | SIM 4 / CPU 6；单库；`decaps_1session` |
| tick | D **286999** + E **745790**（对标 pass-fix） |
| vendor | `decrypt/` + `prep/`/`compute/` + `kem/`；`prepare_dec_shim.sh` |

## 验收

```bash
cd examples/incubating/exp-fips203-mlkem-kem-decaps-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## Round-trip（同日续）

仓库 `scripts/` 默认 `DECAPS_DIR` 自 pass-probe 切到本 incubating exp（对齐 Encaps→stable 操作；交付后再切 stable）：

| 脚本 | 变更 |
|------|------|
| `roundtrip_kem_decaps.sh` / `roundtrip_kem_keygen_encaps_decaps.sh` | 默认 → exp |
| `liboqs_kem_vs_ascendc.sh` / `kat_liboqs_kem_decaps.py` | 同上 |

**证据**：`bash scripts/roundtrip_kem_keygen_encaps_decaps.sh -r cpu|sim` → agreement + reject **PASS**（KeyGen device + Encaps stable + 本 Decaps）。

## 下一刀

- `#交付#` → stable（用户点名；交付后 `DECAPS_DIR` 默认再切 stable）
- 可选：KAT 扩量、`fo_only` 内联

## 登记表

已刷新 [`qa/active_sim_regress_summary.md`](../active_sim_regress_summary.md)：incubating Decaps D**286999**+E**745790**；device 行注明 scripts 默认已切 exp。

## liboqs 分项 KAT（同日续）

`KEM_DEC_CPU_TRIALS=10 KEM_DEC_SIM_TRIALS=3 bash scripts/liboqs_kem_decaps_batch.sh`（`DECAPS_DIR`→exp）：

| 模式 | 结果 |
|------|------|
| CPU×10 | **PASS** |
| SIM×3 | **PASS** |

固定 stash `dk`；每轮 liboqs encaps→`c`；device `K` 逐字节一致。

## baseline-registry（同日续）

新增 [`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`](../../docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md)：登记合法/拒绝路径 golden 块（liboqs encaps/decaps、`J`/`G` Host 对照、LUT、CPU `golden_v`）；适用当前 incubating exp，交付后改指 stable。

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
