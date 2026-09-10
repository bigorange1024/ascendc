# LAYOUT — RB-D04-decrypt-full

## 拓扑（S0A）

| Launch | 核 basename | 类型 | 输入 | 输出 |
|--------|-------------|------|------|------|
| L1 | `dec_prep_custom.cpp` | AIV-only | `dk_pke`/`c` | `ŝ`/`u`/`v` |
| mid-sync | Host | — | — | — |
| L2a | `dec_ntt_dot_custom.cpp` | MIX | `u`/`ŝ`+LUT | `û`/`ŵ` |
| sync | Host | — | — | — |
| L2b | `dec_intt_extract_custom.cpp` | MIX | `ŵ`/`v`+LUT | `m[32]` |

Flag：L2a/L2b 各自 1/3(+4)；禁 5/7 SoftSync。X12：业务写出经 DataCopy。

## 头文件隔离

- `d02_inc/`：L2a tiling / light_cube / ntt_dot math
- `d03_inc/`：L2b tiling / light_cube / intt_extract math
- 避免同 binary 内 `tiling.h` basename 撞车导致错误布局。

## Oracle

优先 `scripts/liboqs_pke_ref`；否则 host FIPS 全链（`output/cross_backend.txt` 标明）。
