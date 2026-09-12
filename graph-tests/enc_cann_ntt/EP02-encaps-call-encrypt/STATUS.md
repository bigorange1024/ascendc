# EP02-encaps-call-encrypt · STATUS

> 日期：2026-09-11  
> 结论：**PASS**（CPU + SIM；`c`/`K` ≡ encaps-ref max=0）

| 模式 | exit | wall | tick | c/K max |
|------|------|------|------|---------|
| CPU | 0 | **1.836s** | — | **0** |
| SIM | 0 | **160.502s** | **812592** | **0** |

Host：H/G→(K̄,r)；设备：EN13 形 Encrypt(ek,m,r)→c；K=K̄。  
日志：`/opt/cursor/artifacts/enc-encaps-sim/ep02-cpu.log`、`ep02-sim.log`。  
sync_audit：核同 EN13，红线 0。
