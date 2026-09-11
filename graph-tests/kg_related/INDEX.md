# kg_related — KeyGen 重建实现目录

> **前缀**：`RB-K*`  
> **运营**：[`../keygen-rebuild-ops/`](../keygen-rebuild-ops/INDEX.md)  
> **禁抄**：KeyGen 算子级代码（stable / pass-fix / incubating / frozen）

| 目录 | 刀 | 状态 |
|------|-----|------|
| [`RB-K01-kg-prep`](RB-K01-kg-prep/) | KGR-P01 | **PASS_CPU**（2026-09-09） |
| [`RB-K02-kg-ntt`](RB-K02-kg-ntt/) | KGR-P02 | **PASS_CPU**（2026-09-09） |
| [`RB-K03-kg-dot-encode`](RB-K03-kg-dot-encode/) | KGR-P03 | **PASS_CPU**（2026-09-09） |
| [`RB-K04-pke-full`](RB-K04-pke-full/) | KGR-P04 | **PASS_CPU + NPU×30**（2026-09-09） |
| [`RB-K05-kem-tail`](RB-K05-kem-tail/) | KGR-K01 | **PASS_CPU+SIM + NPU×30**（2026-09-09；`kZPrefixBytes`） |
| [`RB-K06-kem-full`](RB-K06-kem-full/) | KGR-K02 | **PASS_CPU + NPU×30**（2026-09-09） |
| [`RB-K07-pke-2launch`](RB-K07-pke-2launch/) | LR-KG-F1 | **PASS_NPU×30**（Host 2；Σ **1127µs** · [NPU表](../../qa/active_npu_perf_summary.md)） |
| [`RB-K08-kem-3launch`](RB-K08-kem-3launch/) | LR-KG-F2b | **PASS_NPU×30**（KEM 4→3） |
| [`RB-K09-kem-2launch`](RB-K09-kem-2launch/) | LR-KG-F2 | **PASS_NPU×30**（Host 2；Σ **1227µs** · [NPU表](../../qa/active_npu_perf_summary.md)） |
