# 自包含与设备全链约束 — pass-fix-f203-alg19-kem-keygen-device-k2

## 1. 生产路径

```text
input/ seed_d.bin + lut_even/odd_stacked.bin
  → Launch1 f203_keygen_prep（设备：ρ/σ/Â/ŝ/ê）
  → Launch2 mmad_custom + FuseEkPke + KemKgTailFused
output/ ek_kem.bin + dk_kem.bin
```

Host 仅写可复现 `seed_d` 与静态 LUT；`scripts/gen_data.py` 只生成 I/O golden，禁止作为设备实现规格。

## 2. 允许依赖

| 允许 | 说明 |
|------|------|
| 本目录 `prep/`、`compute/`、`kem/`、`cmake/`、`scripts/prep/` | 设备源码与本探针 ROM 生成 |
| `library/shared/` | SHAKE / Keccak 设备原语 |
| 运行时软链到 D13 k2 的 `scripts/compute` / `thirdparty` | 只用于静态 LUT 与 host oracle；D13 k2 是本轮指定 sibling green |
| 仓库级 `scripts/runtime_env.sh`、`sim_env.sh`、`kernel-run-timeout.sh` | CANN 编译/仿真壳，非密码学实现 |

## 3. 禁止

- 禁止引用或复制 `**/frozen/**` 源码。
- 禁止把 `a_hat` / `src` / `rho` 等中间态作为默认 input。
- 禁止以分段 `main_compute` / 磁盘 staging 作为 D19 默认验收。
