# 自包含与设备全链约束 — stable-fips203-mlkem-kem-encaps-k4

customspec：`stable-fips203-mlkem-kem-encaps-k4-实现方案-customspec.tex`  
registry：`docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md`

## 密码学契约

- **Alg.17**：`ek` + **`m`（GM 输入）** → device `H`/`G` → Encrypt → `c`/`K`
- Host **禁止**预算 `H(ek)` / `G(m‖h)` / 预填生产输入 `$r$`
- `$h$`/`$r$`/`$\hat{A}$`/`$y$`/`$e_1$`/`$e_2$` **不**作为生产输出落盘

## 工程

| 项 | 约定 |
|----|------|
| 晋级 | 自 `examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4/` **复制**（2026-07-15 `#验收#`）；exp 保留 |
| Encrypt 源码 | 本目录 `prep/` + `compute/` **vendored** |
| KEM 增量 | 本目录 `kem/` + `f203_kem_enc_prep_entry.cpp` |
| Launch | = Encrypt（SIM 2 / CPU 5） |
| 外部依赖 | **仅** `library/shared/` |

## 禁止

- 编译期 `#include` `ascendc-tests/` / 其它 `examples/` / `frozen/`
- 子进程调探针 `run.sh` 冒充 Encaps
- 为 KEM 头增加第 3 次独立 launch
