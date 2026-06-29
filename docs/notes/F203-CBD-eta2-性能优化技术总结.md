# F203 Alg.8 CBD η=2 — 性能优化技术总结

**读者**：KeyGen prep 中 PRF+CBD 段、或 `pass-fix-f203-alg8-cbd-eta2-k4` 探针维护者  
**案例锚点**：[`ascendc-tests/pass-fix-f203-alg8-cbd-eta2-k4/`](../../ascendc-tests/pass-fix-f203-alg8-cbd-eta2-k4/)（2026-06-28 **`pass-` 终态**；CPU+SIM PASS；P2 **18048** / P1b **33311** tick）  
**讨论**：[`qa/2026-06/2026-06-22-Alg8-CBD-eta2-性能优化讨论.md`](../../qa/2026-06/2026-06-22-Alg8-CBD-eta2-性能优化讨论.md) · [`2026-06-25 KeyGen prep 路线图`](../../qa/2026-06/2026-06-25-KeyGen-prep优化路线图.md) §Opt-5  
**Pipe 同步**：[`F203-KeyGen-prep-Pipe细同步技术总结.md`](F203-KeyGen-prep-Pipe细同步技术总结.md)

---

## 1. 路线（P0–P2）

| 阶段 | 要点 | 状态 |
|------|------|------|
| P0 | SWAR / LUT 基线 | 探针 `build_bench_P0` |
| P1 | DataCopy 布局、单/双 AIV | P1a/P1b 目录 |
| P2 | 双 AIV + MTE2 向量 IO | SIM **18048** tick（2026-06-28 复跑；P1b **33311**） |

**不变量**：无 `PipeBarrier` 的 SIM tick 可能**虚低**但 golden **FAIL** — 优化须 CPU/SIM 对拍后再采 tick。

---

## 2. 与 KeyGen prep 关系

- 生产 prep 使用 `prep/alg8/f203_cbd_eta2_ub_io.hpp`（vendored 于 exp / 探针）。  
- Opt-5 **Pipe 细同步**在 KeyGen 探针 [`PIPE_SYNC_EVAL.md`](../../ascendc-tests/pass-fix-f203-alg13-device-keygen-k4/PIPE_SYNC_EVAL.md) 逐轮验证；未证前勿删 barrier。

---

## 3. 维护

探针晋级 `pass-` 或 tick 变更 → 更新本 note §1 与 `ascendc-tests/INDEX.md`。
