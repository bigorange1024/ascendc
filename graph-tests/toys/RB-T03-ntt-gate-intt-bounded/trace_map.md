# TRACE 编号分区 — RB-T03-ntt-gate-intt-bounded

> 设备写魔数到 `ws+TRACE`；Host 在 launch 前 / `SynchronizeStream` 后写槽 0/8 并打印。

| 槽 | 分区 | 时机 | 魔数 | 备注 |
|----|------|------|------|------|
| 0 | **Host** | launch 前 | `0x484F5354` | 硬（单槽） |
| 1 | **AIV0** | NTT SET(1) 前 | `0xA1010001` | 与槽 2 成对硬：任一侧命中 |
| 2 | **AIV1** | NTT SET(1) 前 | `0xA1110001` | 与槽 1 成对 |
| 3 | **AIC** | NTT WAIT(1) 后 | `0xC1010001` | 软 |
| 4 | **AIC** | NTT SET(3) 前 | `0xC1030003` | 软 |
| 5 | **AIV0** | NTT WAIT(3) 后 | `0xA1030003` | 与槽 16 成对硬 |
| 6 | **AIV0** | GATE SET(4) 前 | `0xA1040004` | 与槽 14 成对硬 |
| 7 | **AIC** | GATE WAIT(4) 后 | `0xC1040004` | 软 |
| 8 | **Host** | SynchronizeStream 后 | `0x484F5355` | 硬（单槽） |
| 9 | **AIV0** | INTT SET(1) 前 | `0xA1011001` | 与槽 15 成对硬 |
| 10 | **AIC** | INTT WAIT(1) 后 | `0xC1011001` | 软 |
| 11 | **AIC** | INTT SET(3) 前 | `0xC1031003` | 软 |
| 12 | **AIV0** | INTT WAIT(3) 后 | `0xA1031003` | 与槽 13 成对硬 |
| 13 | **AIV1** | INTT WAIT(3) 后 | `0xA1131003` | 与槽 12 成对 |
| 14 | **AIV1** | GATE SET(4) 前 | `0xA1140004` | 与槽 6 成对 |
| 15 | **AIV1** | INTT SET(1) 前 | `0xA1111001` | 与槽 9 成对 |
| 16 | **AIV1** | NTT WAIT(3) 后 | `0xA1130003` | 与槽 5 成对 |

**验收**：AIV 逻辑事件取 **AIV0|AIV1 成对槽任一魔数匹配**（NPU TRACE 可能落 AIV1，勿硬绑 AIV0 / KB X7/X9）；空侧 soft WARN。Host + `mat_c_*` 仍硬。

**因果**：NTT SET1+WAIT3 + SET4 + INTT SET1+WAIT3 ⇒ 三段均完成；两份 `mat_c_*.bin` 非全 0 证有界 Cube。

## CrossCore flag 表（永禁 5/7）

| flagId | 方向 | 含义 |
|--------|------|------|
| **1** | AIV → AIC | NTT / INTT 就绪（**复用**：NTT 的 Wait(3) 完成后才用于 INTT） |
| **3** | AIC → AIV | 该段 Cube 完成（**同上复用**） |
| **4** | AIV → AIC | GATE：插在 NTT 与 INTT 之间 |
| 禁 | — | **5 / 7**；SoftSync；AIC Wait 环内 SyncAll |

## 时序（预期）

```text
AIC:  Wait(1) → CubeNTT → Set(3) → Wait(4) → Wait(1) → CubeINTT → Set(3)
AIV:  Set(1)  → Wait(3)          → Set(4)  → Set(1)  → Wait(3)
```

## 若挂，卡在哪段 TRACE（假设）

| 现象 | 假设卡点 |
|------|----------|
| 有 `PRE_SET1_NTT`（AIV0 或 AIV1），无 `POST_WAIT3_NTT` 成对 | NTT：Wait(1)/Cube/Set(3) |
| 有 `POST_WAIT3_NTT`+`PRE_SET4`，无 INTT 槽 | GATE：Wait(4) 未醒或 AIV 未到 Set(1) INTT |
| 有 `PRE_SET1_INTT`，无 `POST_WAIT3_INTT` 成对 | INTT：复用 1/3 断 |

## 日志关键字

`PRE_SET1_NTT` / `POST_WAIT3_NTT` / `PRE_SET4` / `PRE_SET1_INTT` / `POST_WAIT3_INTT` / `HOST_POST_SYNC` / `phases: NTT…GATE…INTT`
