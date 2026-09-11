# ER05 TRACE 编号映射（KB §6）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> **本刀**：Launch1 prep（SAMPLE）+ Launch2 COMPUTE + Launch3 COMPUTE（同 workspace 粘性双 COMPUTE）；  
> 核内体量保持 ER04：GATE 真 Vec MAC（**256×32**）；NTT/INTT 各真 Cube **×16**。

## TRACE 区分两轮 COMPUTE（强制约定）

| 约定 | 说明 |
|------|------|
| **清槽** | 每轮 launch 前 Host `ClearTraceBuffer`（CPU 直清；SIM 清后 H2D） |
| **Host 锚点** | `120` = 第一轮 COMPUTE 前；`121` = 第二轮 COMPUTE 前 |
| **读法** | `120` 之后、`121` 之前的设备 TRACE 属第一轮；`121` 之后、`199` 之前属第二轮 |
| **不改核槽** | 核内仍写同一套 201–406 等槽；靠 Host 清槽 + `120`/`121` 分段，无需偏移槽位 |

## Host（三 launch）

| 编号 | 含义 | 出现 | 缺失含义 |
|------|------|------|----------|
| **110** | prep launch 前 | 必见 | Host 未进 Launch1 |
| **120** | 第一轮 compute launch 前 | 必见 | Host 未进 Launch2 / prep Sync 未返 |
| **121** | 第二轮 compute launch 前 | 必见 | Host 未进 Launch3 / 第一轮 COMPUTE Sync 未返 |
| **199** | 末次 Sync 返回后 | 必见 | 第二轮 compute 挂死或 Host 早退 |

## Launch1 SAMPLE / prep（210–219 区）

| 编号 | 谁 | 含义 |
|------|----|------|
| **211/311** | AIV0/1 | SAMPLE mixing 前 |
| **212/312** | AIV0/1 | SAMPLE 写 GM 后 |

## Launch2 / Launch3 NTT（第一轮 flag 1/3；两轮 COMPUTE 槽号相同）

| 编号 | 谁 | 含义 |
|------|----|------|
| **201/301** | AIV0/1 | SET(1) 前 |
| **401/402** | AIC | WAIT(1) 后 / SET(3) 前 |
| **203/303** | AIV0/1 | WAIT(3) 后 |

## Launch2 / Launch3 GATE（生产时序 4/8；真 Vec MAC 256×32）

| 编号 | 谁 | 含义 |
|------|----|------|
| **403** | AIC | 进入 WAIT(4) 前 |
| **204/304** | AIV0/1 | Vec MAC 后、SET(4) 前 |
| **404** | AIC | WAIT(4) 返回后 |
| **205/305** | AIV0/1 | WAIT(8) 后 |

## Launch2 / Launch3 INTT（复用 1/3，禁 5/7）

| 编号 | 谁 | 含义 |
|------|----|------|
| **206/306** | AIV0/1 | INTT SET(1) 前 |
| **405/406** | AIC | INTT WAIT(1) 后 / SET(3) 前 |
| **207/307** | AIV0/1 | INTT WAIT(3) 后 |

## 时序语义

```
Host 110 → Launch1 PREP：AIV SAMPLE → 211/212/311/312；AIC 立即返回
Host 120 → Launch2 COMPUTE#1（同 GM workspace）：
  NTT / GATE / INTT（与 ER04 相同核内 FSM）
Host 121 → Launch3 COMPUTE#2（不清 workspace、不重填 seed/LUT/MAC；仅清 TRACE）：
  再次跑完整 COMPUTE FSM（粘性：脏 workspace / CrossCore 残留 / 二次进 MIX）
Host 199
```

## 验收最小序列

至少可见：`110`、`211`/`212`（或 `311`/`312`）、`120`、GATE、`121`、GATE（第二轮）、`199`。

理想（两轮 COMPUTE 设备 TRACE 对称；SIM 可能缺 401/X13）：
`110 211 311 212 312 120 …COMPUTE… 121 …COMPUTE… 199`
