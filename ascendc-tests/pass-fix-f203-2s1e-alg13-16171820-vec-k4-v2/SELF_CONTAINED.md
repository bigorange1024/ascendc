# 自包含约束 — pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2

全仓通则：[docs/engineering/用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md)

| 允许 | 禁止 |
|------|------|
| 本目录源码 + `thirdparty/ntt_study/`（LUT 头） + `scripts/mlkem_ref.py` | Python/`#include` 引用其它探针或 `examples/` |
| `library/shared/`（编译期 merged_kyber 等） | 仓库根 `thirdparty/ntt_study` 未 vendored 路径 |
| Host golden（`gen_data.py`）仅验 I/O | Host 算 NTT 结果替代设备默认验收 |

**生产 I/O**：`input/` 契约 bin → 设备 NTT+Alg.13 → `output/dst.bin` 对拍 golden。
