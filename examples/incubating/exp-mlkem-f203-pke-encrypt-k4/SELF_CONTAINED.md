# 自包含与设备全链约束 — exp-mlkem-f203-pke-encrypt-k4

**customspec**：[`exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex`](exp-mlkem-f203-pke-encrypt-k4-实现方案-customspec.tex)  
**交付副本**：[`stable-mlkem-f203-pke-encrypt-k4`](../../stable/stable-mlkem-f203-pke-encrypt-k4/)  
**验收权重**：[CPU 辅助 / SIM 主参考](../../../docs/notes/F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md)

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`compute/`、`cmake/`、`scripts/`、`thirdparty/`（vendored LUT 头） | `#include` / Python import **其它探针或 example** 源码 |
| 编译期 `#include` **`library/shared/`** | 运行时依赖 `ascendc-tests/pass-*`、其它 `examples/*` |
| 仓库级 `scripts/sim_env.sh`、`kernel-run-timeout.sh`、`camodel_sim_log.sh` | liboqs 接入**生产** `run.sh` |

**规则**：实现自 [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../../../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/) **一次性 vendor**；写码后切断外链。`alg11_*` / `multiply_ntts_*` 已抄入 `compute/`。

## 2. 设备全链（Alg.14 I/O；无中间态落盘）

**生产主路径 = SIM**（默认 `bash run.sh -r sim`）：

```
input/  ek_pke.bin + m.bin + coins.bin + lut_*_stacked.bin（静态 LUT）
   → Launch1 f203_encrypt_prep（设备：Â + re；仅 GM）
   → Launch2 f203_encrypt_l18_l19（设备：NTT/内积/INTT/pack → c）
output/ c.bin（1568B）仅密文
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| 写 `ek_pke`/`m`/`coins`、生成**静态** NTT/INTT LUT | 向 `input/`/`output/` 写 Â、y、u、v、t̂ 等中间态 bin |
| `golden_v.bin`：**仅** CPU **辅助**路径 pack 注入（非产物；SIM 禁止依赖） | Host D2H 中间态再 H2D 冒充全链 |
| Host oracle 对拍 `golden/c.bin` | Host 算 c 写回 `output/c.bin` 代替设备 |

**CPU**：辅助正确性孪生；无 NPU 前交付主参考为 **SIM**。

## 3. 审查

```bash
ls input/    # ek_pke m coins lut_* [golden_v 仅 CPU]
ls output/   # 仅 c.bin
rg '#include.*ascendc-tests/(pass|fix)-' prep compute *.cpp || true
```
