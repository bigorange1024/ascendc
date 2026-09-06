# ORIGIN — CBD η=2（自包含拷贝）

- 来源（只读参考，未改原目录）：`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg8-cbd-eta2-k4/`
- 本目录 `vendor/cbd_eta2/`：`f203_cbd_eta2*.hpp` + `cbd2_ab_lut.h` + `f203_cbd_eta2_config.h`
- 用法：L1 在真 SHAKE 之后调用单 poly `SamplePolyCbd2OneRowUb`（η=2；PRF 128B→256 int32）
- 编译：`F203_CBD_BLOCK_DIM=1`（与 E08 MIX `blockDim=1` 对齐；非整图 P2 batch8）
