# Encrypt + Encaps × cann-ntt · SIM-only QUEUE

> 执行序 = 行号序。状态：`PENDING` / `IN_PROGRESS` / `PASS` / `FAIL` / `BLOCKED_UNTIL_USER`  
> **默认运行态：SIM-only**。标 `NPU` 的刀在用户授权前保持 `BLOCKED_UNTIL_USER`。

| # | ID | 状态 | 目录/动作 | 门禁 | 依赖 |
|---|-----|------|-----------|------|------|
| 1 | EE-W0a | **PASS** | 本战役 INDEX/PLAN/QUEUE + `docs/rg-enc-encaps-cann-ntt-sim.yaml` | `rg_validate` OK | — |
| 2 | EE-W0b | **PASS** | 复跑 `enc_cann_ntt/EN09` cpu+SIM_DIRECT | 不挂；无 stray | W0a |
| 3 | EN13 | **PASS** | `EN13-encrypt-liboqs-cross` | Encrypt `c`≡liboqs；cpu+SIM | W0b |
| 4 | EN14 | **PASS** | `EN14-encrypt-cross-sticky` | SIM sticky R=8；每轮 c≡liboqs | EN13 |
| 5 | EP01 | **PASS** | `EP01-encaps-host-skel` | Encaps Host 壳长度/不挂 | EN13 |
| 6 | EP02 | **PASS** | `EP02-encaps-call-encrypt` | 真调 Encrypt；自洽 c/K | EP01+EN13 |
| 7 | EP03 | **DEFERRED_HOST** | `EP03-encaps-device-hash` | 设备 H/G vs Host（可后置） | EP02 |
| 8 | EP04 | **PASS** | `EP04-encaps-liboqs-cross` | Encaps vs liboqs（强完成） | EP02 |
| 9 | EP05 | **PASS** | `EP05-encaps-sticky-sim` | SIM sticky R≥16 不挂；抽样交叉 | EP02；建议 EP04 后 |
| 10 | EN15 | **BLOCKED_UNTIL_USER** | Encrypt NPU 冒烟 | NPU | 用户授权 |
| 11 | EP10 | **BLOCKED_UNTIL_USER** | Encaps NPU sticky | NPU | 用户授权 |

## 下一可执行刀

**SIM 战役强完成**（EN14→EP05 全 PASS；EP03=DEFERRED_HOST）。  
下一刀仅 **EN15/EP10（NPU）**，须用户明确授权后开；在此之前 **停**。

## 空闲策略

- SIM 刀间隙：本机整理 FEEDBACK/图谱即可。  
- **禁止**为「占着板」开 SSH keepalive。  
- 用户未归：**停在 QUEUE 下一行 PENDING**，不要自行跳到 NPU。
