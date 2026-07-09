# 固定测试向量 — ek_pke.bin

| 项 | 值 |
|---|---|
| 文件 | `ek_pke.bin`（1568B） |
| 来源 | `examples/stable/stable-fips203-mlkem-pke-keygen-k4` 一次 KeyGen 产出 |
| 参数 | `SEED_D=20260619`（stable 默认） |
| 用途 | `run.sh` / `gen_data.py` 复制到 `input/ek_pke.bin`；设备读 `ρ = ek[1536:1568]` |

**禁止**运行时从 stable 或其它探针路径 fallback 读取；缺失本文件则 `gen_data.py` 失败。
