# INTEGRATION_PLAN — pass-fix-f203-alg19-kem-keygen-device-k2

**讨论**：[`qa/TODO.md`](../../../../qa/TODO.md) T512。

## 1. 目标

| 项 | 锁定值 |
|----|--------|
| 参数组 | ML-KEM-512，`k=2` |
| 生产 I/O / launch | 2 launch，`seed_d`+LUT → `ek_kem=800B`、`dk_kem=1632B` |
| dk 布局 | `dk_pke(768)‖ek(800)‖H(ek)(32)‖z(32)` |
| prep/compute | 复用活跃 D13 k2 几何：Â `[4,256]`、`s‖e [4,256]`、S-1、禁零垫 |
| Derand | `d`: `exp-mlkem-f203-2s1e-k2:SEED_D=`；`z`: `exp-mlkem-f203-kem-k2:SEED_Z=` |

## 2. 代码差分

- 以活跃 D13 k2 探针为源码基底，仅在 `compute/mmad_custom.cpp` 行 21 后挂 `F203_KEM_KEYGEN_TAIL`。
- `kem/f203_kem_kg_tail_fuse.hpp` 在 AIV0 内完成 `H(ek)`、`z` 派生与 `dk_kem` 拼接，不新增第三次 kernel。
- Host `main_kem_keygen.cpp` 只写 `ek_kem.bin` / `dk_kem.bin`；D13 PKE 中间态默认不落盘。

## 3. 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

要求：`ek_kem` / `dk_kem` max=0，SIM tick 登记，用例根无 stray dump。

## 4. 边界

- 不建 `examples/`，不建 stable-512。
- 不从 `**/frozen/**` 复制源码；本轮只使用活跃 D13 k2 与活跃 D19 k3 模式。
