# ORIGIN — Compress_d（自包含拷贝）

| 项 | 值 |
|----|----|
| 只读参考 | `ascendc-tests/ml-kem/ml-kem-1024/pass-f203-compress-d-vec-k4/`（未改原目录） |
| 本目录 | `vendor/compress_d/`：`compress_d_*.hpp` / `f203_*` / `compress_d_ref.c/.h` |
| 用法 | L2 INTT 后双 AIV 各压 half；默认 `F203_COMPRESS_D=4` 向量 Barrett |
| 壳封装 | `compress_l2_ub.hpp`（halfLen=128；非整图 Encrypt tail） |
| 未采用 | 抄 Encrypt；unified-int 外置 `f203_unified_round` 依赖；假 Compress TRACE stub |
