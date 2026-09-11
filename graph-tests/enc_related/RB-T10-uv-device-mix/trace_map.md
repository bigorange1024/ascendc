# TRACE 编号分区 — RB-T10-uv-device-mix

> 设备写魔数到 `ws+OFF_TRACE`；Host 在 launch 前 / SynchronizeStream 后写槽 0/8。

| 槽 | 分区 | 时机 | 魔数 |
|----|------|------|------|
| 0 | Host | launch 前 | `0x484F5354` |
| 1/2 | AIV0/1 | NTT SET(1) 前 | `0xA1010001` / `0xA1110001` |
| 3/4 | AIC | NTT WAIT1 后 / SET3 前 | 软 |
| 5/16 | AIV0/1 | NTT WAIT(3) 后 | 成对硬 |
| 17 | AIV0 | MultiplyNTTs 完成 | `0x4D554C44` 软硬均可 |
| 6/14 | AIV0/1 | GATE SET(4) 前 | 成对硬 |
| 7 | AIC | GATE WAIT(4) 后 | 软 |
| 8 | Host | sync 后 | `0x484F5355` |
| 9/15 | AIV0/1 | INTT SET(1) 前 | 成对硬 |
| 10/11 | AIC | INTT WAIT1 / SET3 | 软 |
| 12/13 | AIV0/1 | INTT WAIT(3) 后 | 成对硬 |
| 18 | AIV0 | u,v 写完 | `0x5556444E` |

**验收**：AIV 逻辑事件取 AIV0\|AIV1 成对槽；Host + mat_c + **u,v vs golden** 硬。

## CrossCore flag（永禁 5/7）

同 T03：1/3 复用，4=GATE。
