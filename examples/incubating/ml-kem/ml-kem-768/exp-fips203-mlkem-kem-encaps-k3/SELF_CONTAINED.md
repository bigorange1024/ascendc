# 自包含与设备全链约束 — exp-fips203-mlkem-kem-encaps-k3

## 密码学契约

- **Alg.17 形态**：`ek_kem(1184)` + **`m(32)`（GM 输入）** → device `H`/`G` → 本目录 E14 k3 Encrypt 自包含副本 → `c(1088)`/`K(32)`。
- Host 只生成 golden；生产路径中 `H(ek)`、`G(m‖H(ek))`、`coins/r` 均由 `f203_kem_enc_prep` 设备头段计算。
- `m`/`r`/`H(ek)` 不作为生产输出落盘；`input/coins.bin` 仅服务 CPU 分段 golden 注入与对拍。

## 工程（对齐 E14 k3 Encrypt 自包含副本）

| 项 | 约定 |
|----|------|
| Encrypt 源码 | 本目录 vendored `compute/`、`prep/`、`multiply/`、`scripts/host_golden/`，编译/生成 golden 不依赖其它用例 |
| KEM 增量 | 本目录 `kem/` + `f203_kem_enc_prep_entry.cpp` |
| Launch | SIM **2**（prep_kem → l18_l19）；CPU **5**（prep_kem +本地分段 compute） |
| 参数 | ML-KEM-768：`k=3`、`du=10`、`dv=4`、INTT batch4 |

## 禁止

- 子进程调其它探针 `run.sh` 冒充 Encaps。
- 为 KEM 头增加第 3 次独立 launch。
- 把 `ek/c` 填到 k4 尺寸或用 0 pad 凑齐 4/8。
