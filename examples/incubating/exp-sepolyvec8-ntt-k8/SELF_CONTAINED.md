# 自包含约束 — exp-sepolyvec8-ntt-k8

全仓通则：[docs/engineering/用例自包含与设备全链约束.md](../../../docs/engineering/用例自包含与设备全链约束.md)

| 允许 | 禁止 |
|------|------|
| 本 example 目录 + `thirdparty/ntt_study/` + `scripts/mlkem_ref.py` | `parents[4]` 指仓库根、`examples/incubating/*` 互引 |
| `library/shared/`（编译期） | 从 `ascendc-tests/` 探针 `#include` 源码 |

**说明**：8-poly NTT 预研对照；活跃探针继任见 `ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/`。
