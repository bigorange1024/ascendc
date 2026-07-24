# 自包含与设备全链约束 — stable-fips203-mlkem-kem-decaps-k4

customspec：`stable-fips203-mlkem-kem-decaps-k4-实现方案-customspec.tex`  
registry：`docs/specs/fips203-mlkem1024-kem-decaps-baseline-registry.md`

## 密码学契约

- **Alg.21 / Alg.18**：`dk_kem` + `c` → Decrypt → `G(m'‖h)` → Encrypt → FO → **仅** `K`
- Host **禁止**代算 `J` / memcmp 冒充设备 FO；**禁止**预填生产 `$r'$`
- `$m'$`/`$h$`/`$z$`/`$K'$`/`$r'$`/`$c'$` **不**作为生产输出落盘

## 工程

| 项 | 约定 |
|----|------|
| Decrypt 源码 | 本目录 `pke_decrypt/` **vendored**（自 stable Decrypt 一次性复制） |
| Encrypt 源码 | 本目录 `prep/` + `compute/` **vendored**（自 stable Encrypt） |
| KEM 增量 | 本目录 `kem/`（对照 pass-fix device；本地 `l18_l19` 覆盖） |
| SIM | 默认 `ASCENDC_SIM_HOST_MODE=decaps_2session`；`prepare_dec_shim` 合单库 |
| 外部依赖 | **仅** `library/shared/` |

## 禁止

- 编译期 `#include` `ascendc-tests/` / 其它 `examples/` / `frozen/`
- 从 correctness / frozen 抄实现
- 子进程调探针 `run.sh` 冒充 Decaps
