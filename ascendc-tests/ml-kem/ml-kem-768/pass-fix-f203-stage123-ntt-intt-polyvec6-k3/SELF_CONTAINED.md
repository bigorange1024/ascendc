# 自包含约束 — pass-fix-f203-stage123-ntt-intt-polyvec6-vec

全仓通则：[docs/engineering/用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md)

| 允许 | 禁止 |
|------|------|
| 本目录 + `thirdparty/ntt_onnx/` + `scripts/mlkem_ref.py` | 跨探针 / 跨 example 源码引用 |
| `library/shared/`（编译期） | 仓库根 `thirdparty/ntt_onnx`（`cross_check_ntt_study_c.py` 已改本目录） |
| `scripts/cross_check_ntt_study_c.py` 独立调试（非 `run.sh` 默认） | — |

**生产 I/O**：`src.bin` + LUT → 设备 Stage1–3 → `dst.bin` 对拍。
