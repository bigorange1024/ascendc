# FIPS 203 Alg.14 PKE Encrypt — 交付口径（CPU 辅助 / SIM 主参考）

**交付算子**：[`examples/stable/stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/)  
**预研副本**：[`examples/incubating/exp-fips203-mlkem-pke-encrypt-k4`](../../examples/incubating/exp-fips203-mlkem-pke-encrypt-k4/)  
**探针基线**：[`pass-fix-f203-alg14-pke-encrypt-device-k4`](../../ascendc-tests/pass-fix-f203-alg14-pke-encrypt-device-k4/)  
**日期**：2026-07-09  
**讨论**：[qa/2026-07-09](../../qa/2026-07/2026-07-09-Encrypt默认SIM_DIRECT.md) §7–§8

---

## 1. 验收权重（无 NPU 实机时）

| 模式 | 角色 | 说明 |
|------|------|------|
| **SIM（CaModel）** | **主参考点** | 与设备同构的全链路径（prep → l18_l19 内联 pack）；性能看 Total tick；门禁与交付结论以 SIM 为准 |
| **CPU（tikicpu）** | **辅助正确性** | 多 launch 孪生，便于快速回归与 gdb；**非**与 SIM 同构的完整设备 Encrypt |
| **NPU 实机** | 未测 | 上机前不得把 CPU/SIM 等同于实机最终正确性 |

**一句话**：没上 NPU 前，**SIM 绿才算交付主证据**；CPU 绿是辅助，不能单独宣称「设备全链已证」。

---

## 2. 为何 CPU ≠ SIM 同构

| 项 | SIM | CPU |
|----|-----|-----|
| Launch | **2**（prep → fused compute+pack） | **5**（prep + ntt_y/at_jp/intt_e1 + pack） |
| `v` | 设备 INTT(tr̂)+e₂' 产出 | **`input/golden_v.bin` 注入**（tikicpu 无 k=8 融合 INTT） |
| 数学 I/O | `ek+m+coins` → **仅** `c` | 同左（对拍仍看 `c`） |

`golden_v` **不是** Alg.14 产物，禁止当作生产输出或 SIM 依赖。

---

## 3. 交付门禁（已通过 2026-07-09）

| 门禁 | 默认 | 主证据面 |
|------|------|----------|
| `run.sh -r sim` | 全量 | **SIM** `c` max=0（stable 复验 tick **627590**） |
| `run.sh -r cpu` | 全量 | 辅助：`c` max=0 |
| `kat_liboqs_vs_ascendc.sh` | CPU×10 + **SIM×1** | 批测以多轮 CPU 扫种子；**至少 1 轮 SIM** 对 liboqs |
| `scripts/roundtrip_pke_batch.sh` | 同上 | Encrypt→Decrypt 闭环；SIM 轮次为闭环主参考 |

---

## 4. 相关文档

| 文档 | 用途 |
|------|------|
| [baseline-registry](../specs/fips203-mlkem1024-pke-encrypt-baseline-registry.md) | golden / KAT 计算块 |
| [CPU/SIM 分叉指南](AscendC-CPU与SIM实现分叉开发指南.md) | 为何必须分叉 launch |
| [compute+tail PASS](F203-Alg14-Encrypt-compute-tail-PASS技术总结.md) | 内联 pack 基线 |
| [UB 驻留](F203-Encrypt-compute-行18-19-UB驻留技术总结.md) | SIM 融合前提 |

---

## 维护

变更验收权重或 CPU/SIM 拓扑 → 同步本笔记、`STATUS.md`、当日 `qa/`、`AGENT_HANDOFF.md`。
