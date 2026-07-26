# STATUS — pass-fix-f203-alg14-pke-encrypt-device-k3

**状态**：**有条件完成**（2026-07-26）；ML-KEM-768 D14 Encrypt device 探针 CPU + `SIM_DIRECT=1` sim 双绿。

**目标**：FIPS 203 Alg.14 **完整 K-PKE.Encrypt** — `ek_pke[1184] + m[32] + coins[32] → c[1088]`，其中 `c1=960 (d_u=10)`、`c2=128 (d_v=4)`。

**锁定参数**：见 [`docs/specs/fips203-mlkem768-parameter-card.md`](../../../docs/specs/fips203-mlkem768-parameter-card.md) §3.2；本探针禁止把 INTT pad 到 6/8。

## 验收证据（2026-07-26）

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | `[cmp] c max=0` → `[SUCCESS] full encrypt: c matches golden` |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `[cmp] c max=0`；Total tick **507605**；2 launch；用例根目录 **0 stray dump** |

## 生产路径

| 段 | 拓扑 | 语义 |
|----|------|------|
| prep | AIV_ONLY `blockDim=2` | 从 `ek_pke[1152:1184]` 取 `rho` 生成 `Â[9,256]`；从 `coins` 生成 `r‖e1‖e2` 共 **7** poly |
| compute+tail | MIX `blockDim=1` | NTT(`r`) k=3 → `Â∘r̂` / `t̂∘r̂` → **INTT batch=4** (`u0,u1,u2,v`) → Compress/ByteEncode d=10/4 → `c` |

CPU 仍保留分段调试路径（prep + ntt_y + at_jp + intt_e1 + pack），不作为性能基线；SIM/生产路径为 2 launch。

## 关键不变量

- `a_hat` 为 **9** poly；`re` 为 **7** poly；`ek_pke` 为 **1184B**；`c` 为 **1088B**。
- INTT 采用真 polyvec4 几何：S0 `[8,256] int8`、mat_c `[32,128]`、AIV 连续 **2+2**。
- golden 由本目录 `scripts/host_golden/` 本地生成，derand 前缀为 k3 参数卡锁定串；只验 I/O 等价。
- `output/` 仅保留 `c.bin`；`u/v/a_hat/re` 为设备内部中间态，不作为交付 I/O。

## 复现

```bash
cd ascendc-tests/ml-kem/ml-kem-768/pass-fix-f203-alg14-pke-encrypt-device-k3
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 后续

- D15 Decrypt device-k3 仍待做，需复用本 D14 的 c 布局与 d=10/4 解码约束。
- 本探针不晋级 `examples/stable`，也不写 `examples/incubating`；PKE exp 需另走 customspec。
