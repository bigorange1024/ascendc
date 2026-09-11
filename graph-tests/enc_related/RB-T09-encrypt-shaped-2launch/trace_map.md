# TRACE map — RB-T09

| slot | 符号 | 含义 |
|------|------|------|
| 0 | HOST_PRE | Host 装载后、Launch1 前 |
| 1 | PREP_DONE | Launch1 AIV 写完 y/ρ |
| 2/3 | AIV0/1 PRE_SET1_NTT | Launch2 NTT 前 Set(1) |
| 4/5 | AIC POST_WAIT1 / PRE_SET3 NTT | NTT Cube 前后 |
| 6 | AIV0 POST_WAIT3_NTT | NTT Wait(3) 返回 |
| 7/17 | AIV0/1 PRE_SET4 | GATE Set(4) 前 |
| 8 | AIC POST_WAIT4 | GATE Wait(4) 返回 |
| 9 | HOST_MID_SYNC | Launch1↔Launch2 之间 |
| 10/18 | AIV0/1 PRE_SET1_INTT | INTT 复用 Set(1) |
| 11/12 | AIC POST_WAIT1 / PRE_SET3 INTT | INTT Cube |
| 13/14 | AIV0/1 POST_WAIT3_INTT | INTT Wait(3) |
| 15 | AIV0 PACK_DONE | pack 完成 |
| 16 | HOST_POST_SYNC | 双 launch 全结束后 |

## 若挂假设

- 无 PREP_DONE → 卡 Launch1
- 有 PREP、无 POST_WAIT3_NTT → 卡 NTT
- 有 SET4、无 INTT → 卡 GATE
- 有 PRE_SET1_INTT、无 POST_WAIT3_INTT → 卡 INTT 复用
- 有 POST_WAIT3_INTT、无 PACK → 卡 pack
