# TRACE map — RB-T11

| slot | 名称 | 硬/软 |
|------|------|-------|
| 0 | HOST_PRE | 硬 |
| 1 | PREP_DONE | 硬 |
| 2 / 3 | AIV0/1 PRE_SET1_NTT | 硬 AIV0 |
| 4 / 5 | AIC POST_WAIT1 / PRE_SET3 NTT | 软 |
| 6 | AIV0 POST_WAIT3_NTT | 硬 |
| 7 | AIV0 MUL_DONE | 软 |
| 8 | AIV0 PRE_SET4 | 硬 |
| 9 | AIC POST_WAIT4 | 软 |
| 10 | HOST_MID_SYNC | 硬 |
| 11 | AIV0 PRE_SET1_INTT | 硬 |
| 12 / 13 | AIC INTT | 软 |
| 14 | AIV0 POST_WAIT3_INTT | 硬 |
| 15 | AIV0 UV_DONE | 软 |
| 16 | AIV0 PACK_DONE | 硬 |
| 17 | HOST_POST | 硬 |
| 18–21 | AIV1 镜像 | 软 |

## 挂点假设

| 现象 | 假设 |
|------|------|
| 无 PREP_DONE | 卡 Launch1 |
| 无 POST_WAIT3_NTT | 卡 NTT 握手 |
| 有 POST_WAIT3 无 MUL/SET4 | 卡 MultiplyNTTs |
| 有 SET4 无 INTT | 卡 GATE |
| 有 PRE_SET1_INTT 无 POST_WAIT3_INTT | 卡 INTT 复用 |
| 有 UV 无 PACK | 卡 pack |
