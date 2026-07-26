# 自包含与设备全链约束 — stable-fips203-mlkem-pke-keygen-k4

## 1. 自包含（除 `library/shared`）

| 允许 | 禁止 |
|------|------|
| 本目录 `prep/`、`compute/`、`cmake/`、`scripts/`、`thirdparty/` | `#include` / Python import **其它探针或 example** 源码 |
| 编译期 `#include` **`library/shared/`**（SHAKE、Keccak 设备原语） | 运行时依赖 `ascendc-tests/pass-*`、`examples/*` 路径 |
| 仓库级 **`scripts/sim_env.sh`**、`kernel-run-timeout.sh`（CANN 仿真壳，非密码学） | `thirdparty/liboqs` 接入**生产** `run.sh`（仅 KAT 脚本可选） |

**规则**：其它包的测试代码会随目录改名/删除而失效；需要的能力必须 **vendored 到本目录**（抄也得抄过来）。

## 2. 设备全链（无 Host 辅助计算）

**生产路径**（默认 `bash run.sh`）：

```
input/  seed_d.bin + lut_even/odd_stacked.bin（静态 LUT，非 per-seed 密码中间量）
   → Launch1 f203_keygen_prep（设备：ρ/σ/Â/ŝ）
   → Launch2 mmad_custom + FuseEkPke（设备：行 16–21）
output/ ek_pke.bin + dk_pke.bin
```

| 允许（Host） | 禁止（生产路径） |
|--------------|------------------|
| 写 `seed_d`、缺省时生成 **静态 NTT LUT** | 向 `input/` 写 `a_hat`/`src`/`rho`/`tiling`（`prepare_production_input.py` 会删 stray） |
| `KEYGEN_VERIFY=1`：Host **oracle** 对拍 ek/dk | `golden_src` fallback、Host 算 Â/ŝ 再喂 mmad |
| `KEYGEN_DEBUG_DUMP=1`：D2H 调试落盘 | Host 拼接 ek‖ρ 冒充设备全链 |
| `kat_liboqs_vs_ascendc.sh`：外部 liboqs 对照 | 分段 `main_compute` + 磁盘 staging 作为**默认**验收 |

**Golden 脚本**（`keygen_golden.py`、`scripts/compute/gen_data.py`）仅用于 **VERIFY / 调试**；不得作为默认 `run.sh` 输入来源。

## 3. 已知 vendored 缺口（待补全）

| 路径 | 用途 | 状态 |
|------|------|------|
| `thirdparty/ntt_onnx/.../transpose_mlkem_luts_i8.h` | Host LUT golden | ✅ 已有 |
| `thirdparty/ntt_onnx/.../mlkem_ntt_tables.h` | 仅 `mlkem_ref.mlkem_ntt()` 调试 | ⚠️ 未 vendored（生产 golden 不调用） |

## 4. 审查命令

```bash
# 生产 input 不得含中间态
ls input/   # 仅 seed_d.bin lut_*.bin

# 不得引用其它探针源码（注释链接除外）
rg '#include.*ascendc-tests/(pass|fix)-' prep compute *.cpp
rg "ascendc-tests/pass-fix" scripts --glob '*.py' | rg -v '@probe|INTEGRATION|STATUS'
```
