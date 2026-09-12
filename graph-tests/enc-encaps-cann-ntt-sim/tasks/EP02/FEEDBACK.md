# EP02 FEEDBACK — Encaps 真调 Encrypt（SIM-only）

| 项 | 值 |
|----|-----|
| 状态 | **PASS** |
| 目录 | `graph-tests/enc_cann_ntt/EP02-encaps-call-encrypt/` |

| 模式 | wall | tick | c/K |
|------|------|------|-----|
| CPU | 1.836s | — | max=0 |
| SIM | 160.502s | **812592** | max=0 |

Host H/G→(K̄,r)；设备 EN13 形 Encrypt；禁 npu。  
日志：`ep02-cpu.log` / `ep02-sim.log`。
