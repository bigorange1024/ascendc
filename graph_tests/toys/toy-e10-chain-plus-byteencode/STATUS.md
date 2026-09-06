# STATUS — toy-e10-chain-plus-byteencode

| 项 | 值 |
|----|----|
| task | E10 / D-exp-e10 |
| 基线 | 自包含拷贝 E09 + `vendor/byteencode_d/` |
| L2 增量 | Compress 后 **真 ByteEncode_d(d=4)** → 128B；再 SET(4) |
| 验收 | `bash run.sh -r cpu` + `SIM_DIRECT=1 bash run.sh -r sim`（默认 3 轮） |
| golden | `dst.bin` vs `golden.bin`（128B uint8）；SHAKE/CBD 同 E09 |

## 目录要点

- `byteencode_l2_ub.hpp` — AIV0 整 poly EncodeFullPoly（vendor poly_byte_encode_local）
- `mmad_custom.cpp` — L2：…→Compress→ByteEncode(560/562)→SET(4)
- `tiling.h` — `kEncodeBytes=128`；GM `kOutBytes=1024`（L2 int32 中间态）

## 禁止（已遵守）

未改 E01–E09 / Encrypt / 原探针 / 图谱 yaml；未复测 retracted；未并行 SIM。
