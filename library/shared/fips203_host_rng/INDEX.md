# fips203_host_rng — Host 侧 SEED_D / m / coins 哈希派生（定点可覆盖）

| 文件 | 说明 |
|------|------|
| [`host_rng.py`](host_rng.py) | `resolve_seed_d(case_tag)`、`expand_bytes(case_tag, field, seed_d, n)` |

用法：`sys.path` 加入 `library/shared/fips203_host_rng` 后 `from host_rng import resolve_seed_d, expand_bytes`。
