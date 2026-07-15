# 自包含与设备全链约束 — pass-fix-f203-alg20-kem-encaps-device-k4

## 密码学契约

- **Alg.17 形态**：`ek` + **`m`（GM 输入）** → device `H`/`G` → Encrypt → `c`/`K`
- Host **禁止**预算 `H(ek)` / `G(m‖h)` / 预填 `coins.bin`
- `m`/`r`/`h` **不**作为生产输出落盘（`VERIFY=1` 可对拍中间量）

## 工程（对齐 KeyGen device-k4）

| 项 | 约定 |
|----|------|
| Encrypt 源码 | **编译期引用** [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) |
| KEM 增量 | 本目录 `kem/` + `f203_kem_enc_prep_entry.cpp` |
| Launch | = stable Encrypt（SIM 2 / CPU 5） |
| `vendor/` | **禁止**；禁止 frozen G5 sync |

日后若要求单目录自包含：仅 copy stable 树进本目录，**不改变** I/O / launch 契约。

## 禁止

- 子进程调其它探针 `run.sh` 冒充 Encaps
- 从 `frozen/` 或 correctness `vendor/` **抄实现**
- 为 KEM 头增加第 3 次独立 launch
