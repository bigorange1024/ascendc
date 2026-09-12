# QUEUE · 少 launch SIM

| 序 | 刀 | 状态 | Owner |
|----|----|------|-------|
| A0 | 锁定 launch：AIV=1 / cann-ntt=2 | **DONE** | main |
| A1 | AIV Encrypt：审计 1 launch + cpu+SIM | **DONE** | main+subagent |
| A2 | AIV Encaps：审计 1 launch + cpu+SIM | **DONE** | main+subagent |
| C1 | cann-ntt Encrypt：EN15 **2 launch** + cpu+SIM | **DONE** | subagent-cann+main |
| C2 | cann-ntt Encaps：EP06 **2 launch** + cpu+SIM | **DONE** | subagent-cann |
| D0 | 回写 STATUS / qa / HANDOFF | **DONE** | main |

## 验收口令（本战役已满足）

- AIV：Host launch=**1**；liboqs max=0（AE-E c；AE-P c/K）  
- cann-ntt：Host launch=**2**；liboqs max=0（EN15 c；EP06 c/K）  
- 仅 cpu+sim；日志 `/opt/cursor/artifacts/low-launch-sim/`

## 证据摘要

| 用例 | launch | cpu | sim |
|------|--------|-----|-----|
| AE-E-encrypt | 1 | PASS | PASS |
| AE-P-encaps | 1 | PASS | PASS |
| EN15-encrypt-2launch | 2 | PASS | PASS |
| EP06-encaps-2launch | 2 | PASS | PASS |
