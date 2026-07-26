# F203 ByteEncode₁₂ prefetch — 技术总结

**读者**：优化 Alg.13 行 19–20 编码、或对照 vec-k4-v2 全链路 tick 的实现者  
**目的**：记录 **prefetch 合入** 后的性能与输入契约；非独立交付算子  
**案例锚点**：[`pass-fix-f203-2s1e-byteencode12-vec-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-byteencode12-vec-k4/) · 全链路 [`pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/)  
**讨论**：[`qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md`](../../qa/2026-06/2026-06-19-ByteEncode12-only探针与prefetch实验.md)  
**SIM 表**：[`SIM_BENCHMARK.md`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-2s1e-alg13-16171820-vec-k4-v2/SIM_BENCHMARK.md)

---

## 1. 结论摘要

| 配置 | encode-only tick | 全链路 tick（v2 mixPass=0） |
|------|------------------|-----------------------------|
| `BYTE_ENCODE12_PREFETCH=1`（默认） | **17429** | **77958** |
| `BYTE_ENCODE12_PREFETCH=0`（tile32） | **25464** | **85991** |

**不变量**：prefetch 只改 **MTE/UB 预取策略**，不改变 ByteEncode₁₂ 数学语义；对拍仍要求 ek/sk max=0。

---

## 2. CMake 宏（生产默认）

- `BYTE_ENCODE12_VEC=1`
- `BYTE_ENCODE12_SCATTER_VEC=1`
- `BYTE_ENCODE12_PREFETCH=1`

KeyGen exp / 探针 compute 已锁定上述组合（见各目录 customspec §唯一路径）。

---

## 3. 维护

性能数字变更时同步：**独立探针 STATUS**、**v2 SIM_BENCHMARK.md**、本 note §1 表。
