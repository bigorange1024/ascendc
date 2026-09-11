# graph-tests/dec_related

> Decrypt/Decaps cannbot 重建实现目录（`RB-D*`）。  
> 运营：[`../decrypt-rebuild-ops/`](../decrypt-rebuild-ops/INDEX.md)。  
> **禁抄** alg15/21、T25–T27 源码。

| 目录 | 刀 | 状态 |
|------|-----|------|
| `RB-D01-decrypt-prep/` | DRW-D01 | **PASS_CPU**（ŝ/u/v max=0；非 liboqs） |
| `RB-D02-decrypt-ntt-dot/` | DRW-D02 | **PASS_CPU**（û/ŵ max=0；非 liboqs） |
| `RB-D03-decrypt-intt-extract/` | DRW-D03 | **PASS_CPU**（m[32] max=0；非 liboqs） |
| `RB-D04-decrypt-full/` | DRW-D04 | **PASS_CPU + PASS_NPU**（m≡liboqs_pke_ref） |
| `RB-D04-decaps-G/` | DRW-K01 | **PASS_CPU + PASS_NPU**（K'/r' max=0） |
| `RB-D05-decaps-reenc/` | DRW-K02 | **PASS_CPU + PASS_NPU**（c'≡liboqs_pke_ref） |
| `RB-D06-decaps-fo/` | DRW-K03 | **PASS_CPU + PASS_NPU**（FO 合法+拒绝≡liboqs） |
| `RB-D07-enc-decaps-rt/` | DRW-K04 | **PASS_CPU**（K_dec≡K_enc≡liboqs；**wait_npu×30**） |
| `RB-D08-decrypt-2launch/` | LR-DC-F1 | **PASS_NPU×30**（Decrypt 2 launch） |
| `RB-D09-decrypt-1launch/` | LR-DC-F2 | **PASS_NPU×30**（Decrypt 1；Σ **456µs** · [NPU表](../../qa/active_npu_perf_summary.md)） |
| （性能后续） | [`../decrypt-scalar-opt/`](../decrypt-scalar-opt/INDEX.md) | **计划已锁**：scalar/握手优化；上板序 H2→H3→H1→H4；禁改本表 D08/D09 源码，新树 `RB-D10*` |
