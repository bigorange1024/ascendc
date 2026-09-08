# TRACE 编号分区 — RB-T12-device-ntt-y

> 设备写魔数到 `ws+OFF_TRACE`；Host 在 launch 前 / SynchronizeStream 后写槽 0/6。

| 槽 | 分区 | 时机 | 魔数 |
|----|------|------|------|
| 0 | Host | launch 前 | `0x484F5354` |
| 1/2 | AIV0/1 | NTT SET(1) 前 | `0xA1010001` / `0xA1110001` |
| 3/4 | AIC | NTT WAIT1 后 / SET3 前 | 软 |
| 5/7 | AIV0/1 | NTT WAIT(3) 后 | 成对硬 |
| 6 | Host | sync 后 | `0x484F5355` |
| 8 | AIV0 | ŷ 写完 | `0x59484154` 软 |

**验收**：AIV 逻辑事件取 AIV0\|AIV1 成对槽；Host + mat_c_ntt + **ŷ vs golden** 硬。

## CrossCore flag（永禁 5/7）

| flagId | 方向 | 含义 |
|--------|------|------|
| **1** | AIV → AIC | NTT 就绪 |
| **3** | AIC → AIV | Cube 完成 |
| 禁 | — | **5 / 7** |

## 时序

```text
AIC:  Wait(1) → CubeNTT → Set(3)
AIV:  Set(1)  → Wait(3) → NTT(y)→ŷ
```
