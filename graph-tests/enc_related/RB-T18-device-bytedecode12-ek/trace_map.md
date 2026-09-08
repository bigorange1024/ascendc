# TRACE map — RB-T18

| slot | 含义 | magic |
|------|------|-------|
| 0 | HOST_PRE | `0x484F5354` |
| 1 | AIV0_PRE_SET1 | `0xA1010001` |
| 2 | AIV1_PRE_SET1 | `0xA1110001` |
| 3 | AIC_POST_WAIT1 | `0xC1010001` |
| 4 | AIC_PRE_SET3 | `0xC1030003` |
| 5 | AIV0_POST_WAIT3 | `0xA1030003` |
| 6 | HOST_POST_SYNC | `0x484F5355` |
| 7 | AIV1_POST_WAIT3 | `0xA1130003` |
| 8 | AIV0_BD12_DONE | `0x42443132` |

硬验收：Host 0/6 + AIV 成对 1‖2、5‖7；Cube `mat_c` 非零。  
软：AIC 3/4、BD12_DONE。
