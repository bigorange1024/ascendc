# STATUS — exp-mlkem-f203-pke-encrypt-k4

**阶段**：【预研】**有条件完成**（CPU + SIM 对拍通过）  
**customspec**：[`exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex`](exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex)（+ PDF）

**目标**：FIPS 203 Alg.14 **完整 K-PKE.Encrypt（行 1–22）** — `ek_pke` + `m` + `coins` → **仅** `c`（1568B）。中间态 Â/y/u/v **不落盘**。

**实现来源**：vendor 自 [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../../../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)；自包含化（REPO_ROOT、`compute/` 内 alg11/multiply、本目录 LUT 头）。

## 验收证据（2026-07-09）

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | `[cmp] c max=0` → `[SUCCESS] … (cpu)` |
| SIM | `bash run.sh -r sim -v Ascend910B4` | `[cmp] c max=0`；Total tick **627614**；根目录 **0 stray dump** |

产物：`output/c.bin` 1568B 与 `golden/c.bin` 逐字节相等；`output/` **仅** `c.bin`。

```bash
cd examples/incubating/exp-mlkem-f203-pke-encrypt-k4
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```

## 参数（锁定）

| 项 | 值 |
|----|-----|
| `SEED_D` | `20260619` |
| SIM launch | 2（prep → l18_l19） |
| CPU launch | 5 |
| prep `blockDim` | 2（AIV_ONLY） |
| compute `blockDim` | 1（MIX_AIC_1_2） |

## 未做（非本轮）

- 晋级 `examples/stable/`（须 `#交付#`）
- liboqs KAT 脚本（可选）
- 去掉静态 `lut_*.bin`（路线 11 已关闭）
