# STATUS — toy-e11-chain-plus-decompress-mu

| 项 | 值 |
|----|----|
| task | E11 / D-exp-e11 |
| verdict | **PASS**（CPU + SIM 3 轮） |
| 目录 | `graph_tests/toys/toy-e11-chain-plus-decompress-mu/` |
| 语义 | SHAKE→CBD→NTT→basemul→INTT→**+Decompress_1(μ)**→Compress_d=4→ByteEncode→SET(4) |
| μ | `input/mu.bin` 32B（SEED_D+1=20260620 固定随机）→ ws[MU0] |
| golden | dst 128B diffs=0；SHAKE 0/32；CBD 0/256 |
| SIM | 3 轮不挂；kernel wall ≈122s；Total tick **828718**；budget 900s |
| CPU | wall ≈2.6s；全绿 |

## 关键文件

- `mmad_custom.cpp` — L2 INTT 后 Decompress_1(μ)（TRACE 544–547）再 Compress/ByteEncode
- `decompress_l2_ub.hpp` — 壳封装
- `vendor/decompress_d/decompress_d1_*` — d=1 消息嵌入 + golden ref
- `TRACE.md` / `ORIGIN-decompress.md`

## 未改

E01–E10、Encrypt、原探针、图谱 yaml。
