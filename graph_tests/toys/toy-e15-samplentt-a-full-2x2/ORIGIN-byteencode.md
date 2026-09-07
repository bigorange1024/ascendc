# ORIGIN — ByteEncode_d（自包含拷贝）

| 项 | 值 |
|----|----|
| 只读参考 | `ascendc-tests/ml-kem/ml-kem-1024/pass-f203-byteencode-d-vec-k4/`（未改原目录） |
| 本目录 | `vendor/byteencode_d/`：`byte_encode_d_*.hpp` / `f203_mlkem_params.h` / `byte_encode_d_ref.c/.h` |
| 用法 | L2 Compress 后 AIV0 整 poly ByteEncode；默认 `F203_BYTE_ENCODE_D=4` → 128B |
| 壳封装 | `byteencode_l2_ub.hpp`（EncodeFullPoly；非整图 Encrypt tail） |
| 未采用 | 抄 Encrypt；独立 launch byte_encode_d_custom；假 ByteEncode TRACE stub |
