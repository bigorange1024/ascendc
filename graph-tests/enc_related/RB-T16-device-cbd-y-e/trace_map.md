# TRACE map — RB-T16

| slot | 符号 | 魔数 | 硬/软 |
|------|------|------|-------|
| 0 | HOST_PRE | `0x484F5354` | 硬 |
| 1 | AIV0_PRE_SET1 | `0xA1010001` | 硬（与 AIV1 成对） |
| 2 | AIV1_PRE_SET1 | `0xA1110001` | 硬（与 AIV0 成对） |
| 3 | AIC_POST_WAIT1 | `0xC1010001` | 软 |
| 4 | AIC_PRE_SET3 | `0xC1030003` | 软 |
| 5 | AIV0_POST_WAIT3 | `0xA1030003` | 硬（与 AIV1 成对） |
| 6 | HOST_POST_SYNC | `0x484F5355` | 硬 |
| 7 | AIV1_POST_WAIT3 | `0xA1130003` | 硬（与 AIV0 成对） |
| 8 | AIV0_CBD_DONE | `0x43424439` | 软 |

`out.bin` 魔数：`0x543A0010`（T16）。
