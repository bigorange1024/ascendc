# EP05 FEEDBACK — Encaps sticky SIM R≥16

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 日期 | 2026-09-11 |
| 目录 | `graph-tests/enc_cann_ntt/EP05-encaps-sticky-sim/` |

| 模式 | wall | tick | c/K vs liboqs |
|------|------|------|---------------|
| CPU | **18.676s** | — | max=0 ×16 |
| SIM | **2050.245s** | **12897130** | max=0 ×16 |

编排对齐 EN14 sticky；核同 EP04/EN13。`EP05_ROUNDS=16`。禁 npu。  
sync_audit 红线=0。日志：`ep05-cpu.log` / `ep05-sim.log`。
