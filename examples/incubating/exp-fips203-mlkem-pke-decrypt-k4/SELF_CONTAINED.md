# 自包含与设备全链约束 — exp-fips203-mlkem-pke-decrypt-k4

**customspec**：[`exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex`](exp-fips203-mlkem-pke-decrypt-k4-实现方案-customspec.tex)  
**基线探针**：[`pass-fix-f203-alg15-pke-decrypt-device-k4`](../../../ascendc-tests/pass-fix-f203-alg15-pke-decrypt-device-k4/)（一次性 vendor）  
**验收权重**：SIM 主参考 / CPU 辅助（对齐 Encrypt 交付口径）

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `compute/`、`unpack/`、`prep/`、`cmake/`、`scripts/` | `#include` / Python import **其它探针或 example** 源码 |
| 编译期 `#include` **`library/shared/`** | 运行时依赖 `ascendc-tests/pass-*`、其它 `examples/*` |
| 仓库级 `scripts/sim_env.sh`、`kernel-run-timeout.sh`、`camodel_sim_log.sh` | liboqs 接入**生产** `run.sh` |

**规则**：实现自 PASS 探针 **一次性 vendor**；`run.sh` **不**默认调用 `scripts/vendor_sync.sh`（该脚本仅维护刷新，会跨探针 `cp`）。

## 2. 设备全链（Alg.15 I/O；无中间态落盘）

**生产主路径 = SIM**（默认 `bash run.sh -r sim`）：

```
input/  dk_pke.bin + c.bin + lut_*_stacked.bin（静态 LUT）
   → Launch1 f203_decrypt_device_fused（prep|NTT|su_dot|INTT|尾）
output/ m.bin（32B）仅消息
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| 写 `dk_pke`/`c`、生成**静态** NTT/INTT LUT | 向 `input/`/`output/` 写 u/v/ŝ/û/ŵ/w 等中间态 bin |
| Host oracle 对拍 `golden_m.bin` | Host 算 m 写回 `output/m.bin` 代替设备 |
| | Host D2H 中间态再 H2D 冒充全链 |

**CPU**：辅助正确性孪生；无 NPU 前交付主参考为 **SIM**。

## 3. 审查

```bash
ls input/    # dk_pke c lut_*
ls output/   # 仅 m.bin（及 golden_m 对拍用）
rg '#include.*ascendc-tests/(pass|fix)-' compute unpack prep *.cpp || true
```
