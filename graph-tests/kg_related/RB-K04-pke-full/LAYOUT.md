# LAYOUT — RB-K04-pke-full

## 拓扑（KB §B2）

| Launch | 核 basename | 类型 | 输入 | 输出 |
|--------|-------------|------|------|------|
| L1 | `kg_prep_custom.cpp`（链自 RB-K01） | AIV-only | `seed_d` | `Â`/`ŝ`/`ê` |
| mid-sync | Host | — | — | — |
| L2a | `kg_ntt_custom.cpp` | MIX | `ŝ`/`ê`+ζ/mat | `ŝ̂`/`ê̂` |
| sync | Host | — | — | — |
| L2b | `kg_dot_encode_custom.cpp` | MIX | `Â`/`ŝ̂`/`ê̂`+γ/ρ/mat | `ek_pke`/`dk_pke` |

Flag：L2a/L2b 各自 1/3(+4)；禁 5/7 SoftSync。X12：业务写出经 DataCopy。

## 头文件隔离

- `k02_inc/`：L2a tiling / light_cube / ntt math（自 K02 契约复制，仅改 include）
- `k03_inc/`：L2b tiling / light_cube / dot_encode math（自 K03）
- 避免同 binary 内 `tiling.h` basename 撞车。

## Host 胶水

- L1→L2a：拷贝设备 `ŝ`/`ê` 入 L2a ws；读 `zetas`/`mat_*`
- L2a→L2b：拷贝 `Â` + `ŝ̂`/`ê̂`；读 `gammas`/`rho`/`mat_*`（ρ 与 prep 内 G 同 Derand）

## Oracle

**权威** `scripts/liboqs_pke_ref keygen`（`SEED_D=20260619` Derand）；缺库 → BLOCKED。
