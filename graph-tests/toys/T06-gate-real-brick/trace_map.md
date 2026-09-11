# T06 TRACE 编号映射（KB §6）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> **本刀 GATE 段 AIV 为真 Vec MAC 积木**（int32[64]×8 轮 Mul+Add），非 T04 假循环加码。

## NTT 段（第一轮，flag 1/3）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **101** | Host | launch 前 | 必见 | Host 未进到 launch |
| **201** | AIV0 | SET(1) 前 | 核跑通应见 | AIV0 未到 NTT SET1 |
| **301** | AIV1 | SET(1) 前 | 核跑通应见 | AIV1 未到 NTT SET1 |
| **401** | AIC | WAIT(1) 之后 | 核跑通应见 | AIC 卡在 NTT WAIT(1) 或 TRACE 假空（X13） |
| **402** | AIC | SET(3) 之前 | 核跑通应见 | Cube 后未到 NTT SET3 |
| **203** | AIV0 | WAIT(3) 之后 | 核跑通应见 | AIV0 卡在 NTT WAIT(3) |
| **303** | AIV1 | WAIT(3) 之后 | 核跑通应见 | AIV1 卡在 NTT WAIT(3) |

## GATE 段（生产时序，flag 4/8；AIV 真 Vec MAC）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **403** | AIC | **进入 WAIT(4) 前** | 核跑通应见 | AIC 未到 GATE 占坑 |
| **204** | AIV0 | **真 Vec MAC 后**、SET(4) 前 | 核跑通应见 | AIV0 MAC 未完成或未 SET4 |
| **304** | AIV1 | **真 Vec MAC 后**、SET(4) 前 | 核跑通应见 | AIV1 MAC 未完成或未 SET4 |
| **404** | AIC | WAIT(4) 返回后、SET(8) 前 | 核跑通应见 | AIC 卡在 WAIT(4) |
| **205** | AIV0 | WAIT(8) 之后 | 核跑通应见 | AIV0 卡在 WAIT(8) |
| **305** | AIV1 | WAIT(8) 之后 | 核跑通应见 | AIV1 卡在 WAIT(8) |

## INTT 段（第二轮，**复用 flag 1/3，禁 5/7**，KB X1）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **206** | AIV0 | INTT SET(1) 前 | 核跑通应见 | AIV0 未到 INTT SET1 |
| **306** | AIV1 | INTT SET(1) 前 | 核跑通应见 | AIV1 未到 INTT SET1 |
| **405** | AIC | INTT WAIT(1) 之后 | 核跑通应见 | AIC 卡在 INTT WAIT(1) 或 TRACE 假空（X13） |
| **406** | AIC | INTT SET(3) 之前 | 核跑通应见 | INTT Cube 后未到 SET3 |
| **207** | AIV0 | INTT WAIT(3) 之后 | 核跑通应见 | AIV0 卡在 INTT WAIT(3) |
| **307** | AIV1 | INTT WAIT(3) 之后 | 核跑通应见 | AIV1 卡在 INTT WAIT(3) |
| **199** | Host | Sync 返回后 | 必见才算跑完 | Sync 挂死 / 未返回 |

## 时序语义（本刀核心）

```
NTT：  AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
GATE： AIC SET(3) 后 TRACE(403) → WAIT(4)；AIV 真 Vec MAC → SET(4)；AIC SET(8)；AIV WAIT(8)
INTT： AIC SET(8) 后 WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
```

**GATE 真积木**：`AivGateRealBrickMac` — int32 a/b/acc 各 [64]，8 轮 `Muls→Mul→Add`，GM↔UB DataCopy。

## 验收日志最小序列（A5）

至少可见：`101`、NTT（`203`/`303`）、GATE（`403`/`404`/`205`/`305`）、INTT（`206`/`306`/`207`/`307` 或 `406`）、`199`。

理想完整：
`101 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`
