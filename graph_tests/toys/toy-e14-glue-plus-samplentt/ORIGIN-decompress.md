# ORIGIN — Decompress_1(μ)（自包含拷贝 + d=1 扩展）

| 项 | 值 |
|----|----|
| 只读参考 | `ascendc-tests/ml-kem/ml-kem-1024/pass-f203-decompress-d-vec-k4/`（未改原目录） |
| 本目录 | `vendor/decompress_d/`：探针 d∈{4,5,10,11} 文件 + **d=1 消息嵌入**扩展 |
| d=1 语义 | FIPS Alg.14 行 20：μ[32B]→256 系数 {0,⌊(q+1)/2⌋}；INTT 后 mod-q 累加（≠公式 Decompress_d） |
| d=1 设备 | `decompress_d1_mu_embed.hpp`（对齐 Encrypt `f203_mu_embed.hpp`） |
| d=1 golden | `decompress_d1_ref.c` → `embed_message_ref` |
| 壳封装 | `decompress_l2_ub.hpp`（DecompressMuAddHalfInPlace；双 AIV 各 128 系数） |
| μ 输入 | Host `input/mu.bin`（32B，SEED_D+1 固定随机）→ ws[MU0] |
| 未采用 | 抄 Encrypt 整图；独立 launch decompress_d_custom；TRACE stub |
