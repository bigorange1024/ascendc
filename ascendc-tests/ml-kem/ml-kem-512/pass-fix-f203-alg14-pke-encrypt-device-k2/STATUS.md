# STATUS — pass-fix-f203-alg14-pke-encrypt-device-k2

**状态**：**PASS**（2026-07-27）；ML-KEM-512 D14 Encrypt device 探针 CPU + `SIM_DIRECT=1` sim 双绿。

**目标**：FIPS 203 Alg.14 **完整 K-PKE.Encrypt** — `ek_pke[800] + m[32] + coins[32] → c[768]`，其中 `c1=640 (d_u=10)`、`c2=128 (d_v=4)`。

**锁定参数**：见 [`docs/specs/fips203-mlkem512-parameter-card.md`](../../../docs/specs/fips203-mlkem512-parameter-card.md) §3.2；本探针禁止把噪声或 INTT 输入零垫到 6/8。

## 验收证据（2026-07-27）

| 模式 | 命令 | 结果 |
|------|------|------|
| CPU | `bash run.sh -r cpu -v Ascend910B4` | `[cmp] c max=0` → `[SUCCESS] full encrypt: c matches golden` |
| SIM | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` | `[cmp] c max=0`；Total tick **365995**（glue-c 后）；2 launch；用例根目录 **0 stray dump** |

## 生产路径

| 段 | 拓扑 | 语义 |
|----|------|------|
| prep | AIV_ONLY `blockDim=2` | 从 `ek_pke[768:800]` 取 `rho` 生成 `Â[4,256]`；从 `coins` 生成 `r‖e1‖e2` 共 **5** poly |
| compute+tail | MIX `blockDim=1` | NTT(`r`) k=2 → `Â∘r̂` / `t̂∘r̂` → **INTT batch=4** (`u0,u1,v,空槽`) → Compress/ByteEncode d=10/4 → `c` |

CPU 仍保留分段调试路径（prep + ntt_y + at_jp + intt_e1 + pack），不作为性能基线；SIM/生产路径为 2 launch。

## 关键不变量

- `a_hat` 为 **4** poly；`re` 为 **5** poly；`ek_pke` 为 **800B**；`c` 为 **768B**。
- Encrypt noise：**r←η1=3**（2 poly）、**e₁‖e₂←η2=2**（3 poly）；禁止补到 6/8 行。
  （2026-07-27 glue-c：曾误 5 行全 η2，已按参数卡补缺。）
- INTT 采用 polyvec4 物理几何：S0 `[8,256] int8`、mat_c `[32,128]`、AIV 连续 **2+2**；语义槽为 `u0,u1,v,空槽`。
- golden 由本目录 `scripts/host_golden/` 本地生成，derand 前缀为 k2 参数卡锁定串；只验 I/O 等价。
- `output/` 仅保留 `c.bin`；`u/v/a_hat/re` 为设备内部中间态，不作为交付 I/O。

## 复现

```bash
cd ascendc-tests/ml-kem/ml-kem-512/pass-fix-f203-alg14-pke-encrypt-device-k2
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

## 后续

- D19/D20/D21/D21ct KEM device-k2 仍待补；D20 可复用本 D14 的 `c=768B` 与 d=10/4 布局。
- 本探针不晋级 `examples/stable`，也不写 `examples/incubating`；PKE exp 需另走 customspec。
