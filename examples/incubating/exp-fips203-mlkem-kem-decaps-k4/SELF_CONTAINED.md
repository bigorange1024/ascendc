# 自包含与设备全链约束 — exp-fips203-mlkem-kem-decaps-k4

customspec：[`exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`](exp-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex)

## 密码学契约

- **Alg.18**：`dk_kem` + `c` → device Phase-D Decrypt → Phase-E `G`+Encrypt → FO → **仅** `K`
- Host **禁止**预算 `G`/`J`/密文比对选路；**禁止**预填生产输入 `r'`
- `m'`/`K'`/`r'`/`c'` **不**作为生产输出落盘（调试 dump 标非默认）

## 工程

| 项 | 约定 |
|----|------|
| Decrypt | 本目录 `decrypt/` **vendored**（自 stable Decrypt 一次性复制） |
| Encrypt | 本目录 `prep/` + `compute/` **vendored**（自 stable Encrypt / encaps exp） |
| KEM 增量 | 本目录 `kem/`（`G` 并入 prep；FO 过渡 `fo_only`） |
| SIM 合库 | `scripts/prepare_dec_shim.sh` → `shim/pke_decrypt/`（`dec_*` 头隔离） |
| Launch | SIM **4** / CPU **6**；默认 `decaps_1session` |
| 外部依赖 | **仅** `library/shared/`；golden 脚本经 `scripts/host_golden` 软链 |

## 禁止

- 编译期 `#include` `ascendc-tests/` / 其它 `examples/` / `frozen/`
- 子进程调探针 `run.sh` 冒充 Decaps
- Host FO / 预填 `r'` 作生产路径
