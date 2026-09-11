# STATUS — RB-T04-mu-embed

| 字段 | 值 |
|------|-----|
| 刀 | T04 · D-EXP-T04 / G-MU |
| 状态 | **PASS**（CPU + `SIM_DIRECT=1` sim） |
| 日期 | 2026-09-08 |
| 墙钟 | ~6 min（编码+双跑） |

## 目标达成

1. `KERNEL_TYPE_AIV_ONLY`，`blockDim=1`；无 MIX / CrossCore
2. I/O：`m[32]` uint8 → `μ[256]` int32；FIPS ByteDecode₁（LSB-first）+ Decompress₁ `(c·q+1)>>1` → {0,1665}
3. Host golden（`scripts/gen_data.py`）与设备/CPU 孪生 `cmp` max=0
4. 未抄 `f203_mu_embed` / alg14；未改 KB/DAG

## 验收证据

| 项 | 结果 |
|----|------|
| `bash run.sh -r cpu -v Ascend910B4` | SUCCESS；verify max=0 |
| `SIM_DIRECT=1 bash run.sh -r sim …` | SUCCESS；verify max=0；totalTick≈10168 |
| sync_audit | **N/A**（无 CrossCore / 设备同步 API） |
| 用例根 stray dump | 无（camodel 已收拢） |

运营回报：[`../../encrypt-rebuild-ops/tasks/T04-mu-embed-decompress1/FEEDBACK.md`](../../encrypt-rebuild-ops/tasks/T04-mu-embed-decompress1/FEEDBACK.md)

## 建议（仅建议）

薄壳契约清晰，可日后晋级 `library/shared` 头；**勿擅自大迁**。
