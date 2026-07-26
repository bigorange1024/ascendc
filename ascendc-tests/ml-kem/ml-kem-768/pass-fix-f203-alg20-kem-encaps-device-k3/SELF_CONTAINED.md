# 自包含与设备全链约束 — pass-fix-f203-alg20-kem-encaps-device-k3

## 密码学契约

- **Alg.17 形态**：`ek_kem(1184)` + **`m(32)`（GM 输入）** → device `H`/`G` → D14 k3 Encrypt → `c(1088)`/`K(32)`。
- Host 只生成 golden；生产路径中 `H(ek)`、`G(m‖H(ek))`、`coins/r` 均由 `f203_kem_enc_prep` 设备头段计算。
- `m`/`r`/`H(ek)` 不作为生产输出落盘；`input/coins.bin` 仅服务 CPU 分段 golden 注入与对拍。

## 工程（对齐 D14 k3 Encrypt）

| 项 | 约定 |
|----|------|
| Encrypt 源码 | 编译期引用活跃 [`pass-fix-f203-alg14-pke-encrypt-device-k3`](../pass-fix-f203-alg14-pke-encrypt-device-k3/) |
| KEM 增量 | 本目录 `kem/` + `f203_kem_enc_prep_entry.cpp` |
| Launch | SIM **2**（prep_kem → l18_l19）；CPU **5**（prep_kem + D14 分段 compute） |
| 参数 | ML-KEM-768：`k=3`、`du=10`、`dv=4`、D14 INTT batch4 |

## 禁止

- 子进程调其它探针 `run.sh` 冒充 Encaps。
- 为 KEM 头增加第 3 次独立 launch。
- 把 `ek/c` 填到 k4 尺寸或用 0 pad 凑齐 4/8。
