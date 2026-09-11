# TRACE map — RB-T15

| slot | 名 | magic | 硬/软 |
|------|----|-------|-------|
| 0 | HOST_PRE | 0x484F5354 | 硬 |
| 1 | PREP_DONE | 0x50524550 | 硬 |
| 2 | AIV0_PRE_SET1_NTT | 0xA1010001 | 硬 |
| 6 | AIV0_POST_WAIT3_NTT | 0xA1030003 | 硬 |
| 22 | AIV0_AHAT_DONE | 0x41484154 | 硬 |
| 23 | AIV0_YHAT_DONE | 0x59484154 | 硬 |
| 8 | AIV0_PRE_SET4 | 0xA1040004 | 硬 |
| 10 | HOST_MID | 0x484F4D49 | 硬 |
| 11 | AIV0_PRE_SET1_INTT | 0xA1011001 | 硬 |
| 14 | AIV0_POST_WAIT3_INTT | 0xA1031003 | 硬 |
| 16 | PACK_DONE | 0x5041434B | 硬 |
| 17 | HOST_POST | 0x484F5355 | 硬 |
| 7 | MUL_DONE | 0x4D554C44 | 软 |
| 15 | UV_DONE | 0x5556444E | 软 |
| 其余 AIC/AIV1 | — | — | 软（SIM 常空） |

挂点：无 POST_WAIT3→握手；有 WAIT3 无 AHAT→SampleNTT；有 AHAT 无 YHAT→NTT；有 YHAT 无 MUL→拓扑；有 SET4 无 INTT→GATE；有 UV 无 PACK→pack。
