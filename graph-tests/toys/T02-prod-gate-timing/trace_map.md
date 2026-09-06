# T02 TRACE 编号映射（KB §6）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> 设备侧写 TRACE 槽（DataCopy）；Host 在 Sync 后按槽打印对应编号（一行一个）。  
> **本刀须能区分 NTT 段与 GATE 段**（A4）。

## NTT 段（同构 T01，flag 1/3）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **101** | Host | `ACLRT_LAUNCH` 前 | 必见 | Host 未进到 launch |
| **201** | AIV0 | SET(1) 前 | 核跑通应见 | AIV0 未到 SET1 |
| **301** | AIV1 | SET(1) 前 | 核跑通应见 | AIV1 未到 SET1 |
| **401** | AIC | WAIT(1) 之后 | 核跑通应见 | AIC 卡在 WAIT(1) 或 TRACE 假空（X13） |
| **402** | AIC | SET(3) 之前 | 核跑通应见 | Cube 后未到 SET3 |
| **203** | AIV0 | WAIT(3) 之后 | 核跑通应见 | AIV0 卡在 WAIT(3) |
| **303** | AIV1 | WAIT(3) 之后 | 核跑通应见 | AIV1 卡在 WAIT(3) |

## GATE 段（生产时序，flag 4/8）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **403** | AIC | **进入 WAIT(4) 前** | 核跑通应见 | AIC 未到 GATE 占坑；或未写 TRACE |
| **204** | AIV0 | 轻体量后、SET(4) 前 | 核跑通应见 | AIV0 未做体量或未 SET4 |
| **304** | AIV1 | 轻体量后、SET(4) 前 | 核跑通应见 | AIV1 未做体量或未 SET4 |
| **404** | AIC | WAIT(4) 返回后、SET(8) 前 | 核跑通应见 | AIC 卡在 WAIT(4) |
| **205** | AIV0 | WAIT(8) 之后 | 核跑通应见 | AIV0 卡在 WAIT(8) |
| **305** | AIV1 | WAIT(8) 之后 | 核跑通应见 | AIV1 卡在 WAIT(8) |
| **199** | Host | `aclrtSynchronizeStream` 返回后 | 必见才算跑完 | Sync 挂死 / 未返回 |

## 时序语义（本刀核心）

```
AIC：SET(3) → TRACE(403) → WAIT(4)     ← 先占坑，不等 AIV WAIT(3)/体量
AIV：WAIT(3) → 轻体量 → TRACE(204/304) → SET(4)  ← 解除 AIC WAIT(4)
AIC：WAIT(4) 返回 → TRACE(404) → SET(8)
AIV：WAIT(8) → TRACE(205/305) → 完成标记
```

**非对称 GATE**：禁止「双方齐到再 AIV SET4」对称捷径（TASK 禁令）。

## 验收日志最小序列（A5）

至少可见：`101`、NTT 完成（`203`/`303` 或 `402`）、`403`、`204`/`304`、`404`、`199`。

理想完整（按槽序打印，非严格实时序）：
`101 201 301 401 402 203 303 403 204 304 404 205 305 199`

## 槽布局

| 槽下标 | 编号 | 段 | 文件位点 |
|--------|------|-----|----------|
| 0 | 201 | NTT | `mmad_custom.cpp` AIV0 SET1 前 |
| 1 | 301 | NTT | AIV1 SET1 前 |
| 2 | 401 | NTT | AIC WAIT1 后 |
| 3 | 402 | NTT | AIC SET3 前 |
| 4 | 203 | NTT | AIV0 WAIT3 后 |
| 5 | 303 | NTT | AIV1 WAIT3 后 |
| 6 | 403 | GATE | AIC WAIT4 前 |
| 7 | 204 | GATE | AIV0 SET4 前 |
| 8 | 304 | GATE | AIV1 SET4 前 |
| 9 | 404 | GATE | AIC SET8 前 |
| 10 | 205 | GATE | AIV0 WAIT8 后 |
| 11 | 305 | GATE | AIV1 WAIT8 后 |
| Host | 101 / 199 | — | `main.cpp` printf |
