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
