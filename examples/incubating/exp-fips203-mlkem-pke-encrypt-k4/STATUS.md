# STATUS — exp-fips203-mlkem-pke-encrypt-k4

**阶段**：【预研】**完成** → 已 **`#交付#` 复制晋级** [`stable-fips203-mlkem-pke-encrypt-k4`](../../stable/stable-fips203-mlkem-pke-encrypt-k4/)（2026-07-09）  
**customspec**：[`exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.tex`](exp-fips203-mlkem-pke-encrypt-k4-实现方案-customspec.tex)（+ PDF）  
**交付以 stable 为准**；本目录为预研副本。  
**验收权重**：[CPU 辅助 / SIM 主参考](../../../docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)

**目标**：FIPS 203 Alg.14 **完整 K-PKE.Encrypt（行 1–22）** — `ek_pke` + `m` + `coins` → **仅** `c`（1568B）。中间态 Â/y/u/v **不落盘**。

## 验收证据（2026-07-09）

| 模式 | 权重 | 命令 | 结果 |
|------|------|------|------|
| **SIM** | **主参考** | `bash run.sh -r sim -v Ascend910B4` | `[cmp] c max=0`；Total tick **627614**；0 stray dump |
| CPU | 辅助 | `bash run.sh -r cpu -v Ascend910B4` | `[cmp] c max=0`（`golden_v` 注入，非与 SIM 同构） |
| liboqs KAT | 批测 | `bash kat_liboqs_vs_ascendc.sh` | **CPU×10 + SIM×1 PASS** |
| round-trip | 批测 | `bash scripts/roundtrip_pke_batch.sh` | **CPU×10 + SIM×1 PASS** |

```bash
cd examples/incubating/exp-fips203-mlkem-pke-encrypt-k4
bash run.sh -r sim -v Ascend910B4   # 主参考
bash run.sh -r cpu -v Ascend910B4   # 辅助
bash kat_liboqs_vs_ascendc.sh
```

## 参数（锁定）

| 项 | 值 |
|----|-----|
| `SEED_D` | `20260619`（日常 golden；KAT/roundtrip 可随机） |
| SIM launch | 2（prep → l18_l19）= 生产主路径 |
| CPU launch | 5 = 辅助孪生 |
| prep `blockDim` | 2（AIV_ONLY） |
| compute `blockDim` | 1（MIX_AIC_1_2） |

## 门禁脚本

| 脚本 | 说明 |
|------|------|
| `kat_liboqs_vs_ascendc.sh` | liboqs fixture → prepare → AscendC `c` 对拍 |
| `scripts/prepare_kat_input.py` | 外部 ek/m/coins → input + golden_v + golden/c |
| 仓库 `scripts/roundtrip_pke_batch.sh` | KeyGen 密钥 → Encrypt → Decrypt 闭环批跑 |
