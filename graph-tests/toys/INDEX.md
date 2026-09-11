# graph-tests/toys

> Wave A 同步玩具实现目录（RB-T01…）。

| 目录 | 刀 | 说明 |
|------|----|------|
| [`RB-T01-mix-ntt13-handshake/`](RB-T01-mix-ntt13-handshake/) | T01 | MIX flag1/3 最短握手 + 极轻 Cube + TRACE |
| [`RB-T02-gate-timing-wait4/`](RB-T02-gate-timing-wait4/) | T02 | MIX GATE：AIC 先 Wait(4) + 极轻 Cube + flag1/3 |
| [`RB-T03-ntt-gate-intt-bounded/`](RB-T03-ntt-gate-intt-bounded/) | T03 | MIX NTT(1/3)→GATE(4)→INTT(1/3复用) + 两段极轻 Cube |

运营任务书：[`../encrypt-rebuild-ops/`](../encrypt-rebuild-ops/INDEX.md)。

| 刀 | 目录 | 状态 | 要点 |
|----|------|------|------|
| T01 | T01-mix-ntt13-handshake | **PASS** | 最短 1/3 |
| T02 | T02-prod-gate-timing | **PASS** | 生产 GATE |
| T03 | T03-full-fsm-ntt-gate-intt | **PASS** | 全 FSM |
| T04 | T04-gate-volume-stress | **PASS** | 假循环×10 → X14 |
| T05 | T05-multi-launch-rounds | **PASS** | 2×launch |
| T06 | T06-gate-real-brick | **PASS** | 真 Vec MAC |
| T07 | T07-sampling-then-fsm | **PASS** | SAMPLE 前置 |

**闸门（X15）**：同质 SIM toys 已穷尽。  
**继任（2026-09-07）**：开 [`../enc_related/`](../enc_related/INDEX.md)（ER01）；每刀用 cannbot skills。

