# T05 TRACE 编号映射（KB §6 + 多 launch 扩展）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> 设备侧写 TRACE 槽（DataCopy）；Host 在 Sync 后按槽打印对应编号（一行一个）。  
> **T05 扩展**：Host 用 **110/120** 区分 launch 轮次；**199** 仅在末轮 Sync 返回后打印一次。

## Host 多 launch 轮次（T05 核心）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **110** | Host | 第 **1** 轮 `ACLRT_LAUNCH` / `ICPU_RUN_KF` 前 | 必见 | 未进入 round-1 |
| **120** | Host | 第 **2** 轮 launch 前（round-1 Sync 返回后） | 必见 | 未进入 round-2 / 只 launch 1 次 |
| **199** | Host | **末轮** `aclrtSynchronizeStream` 返回后 | 必见才算跑完 | 末轮 Sync 挂死 |

每轮设备 TRACE 编号与 T03 相同（201–307）；**设备号可重复**，Host 110/120 区分轮次。

## NTT 段（每轮第一轮，flag 1/3）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **201** | AIV0 | SET(1) 前 | 每轮应见 | AIV0 未到 NTT SET1 |
| **301** | AIV1 | SET(1) 前 | 每轮应见 | AIV1 未到 NTT SET1 |
| **401** | AIC | WAIT(1) 之后 | 每轮应见 | AIC 卡在 NTT WAIT(1) 或 TRACE 假空（X13） |
| **402** | AIC | SET(3) 之前 | 每轮应见 | Cube 后未到 NTT SET3 |
| **203** | AIV0 | WAIT(3) 之后 | 每轮应见 | AIV0 卡在 NTT WAIT(3) |
| **303** | AIV1 | WAIT(3) 之后 | 每轮应见 | AIV1 卡在 NTT WAIT(3) |

## GATE 段（生产时序，flag 4/8）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **403** | AIC | **进入 WAIT(4) 前** | 每轮应见 | AIC 未到 GATE 占坑 |
| **204** | AIV0 | 轻体量后、SET(4) 前 | 每轮应见 | AIV0 未做体量或未 SET4 |
| **304** | AIV1 | 轻体量后、SET(4) 前 | 每轮应见 | AIV1 未做体量或未 SET4 |
| **404** | AIC | WAIT(4) 返回后、SET(8) 前 | 每轮应见 | AIC 卡在 WAIT(4) |
| **205** | AIV0 | WAIT(8) 之后 | 每轮应见 | AIV0 卡在 WAIT(8) |
| **305** | AIV1 | WAIT(8) 之后 | 每轮应见 | AIV1 卡在 WAIT(8) |

## INTT 段（每轮第二轮，**复用 flag 1/3，禁 5/7**，KB X1）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **206** | AIV0 | INTT SET(1) 前 | 每轮应见 | AIV0 未到 INTT SET1 |
| **306** | AIV1 | INTT SET(1) 前 | 每轮应见 | AIV1 未到 INTT SET1 |
| **405** | AIC | INTT WAIT(1) 之后 | 每轮应见 | AIC 卡在 INTT WAIT(1) 或 TRACE 假空（X13） |
| **406** | AIC | INTT SET(3) 之前 | 每轮应见 | INTT Cube 后未到 SET3 |
| **207** | AIV0 | INTT WAIT(3) 之后 | 每轮应见 | AIV0 卡在 INTT WAIT(3) |
| **307** | AIV1 | INTT WAIT(3) 之后 | 每轮应见 | AIV1 卡在 INTT WAIT(3) |

## 时序语义（每 launch 轮）

```
NTT：  AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
GATE： AIC SET(3) 后 TRACE(403) → WAIT(4)；AIV 体量 → SET(4)；AIC SET(8)；AIV WAIT(8)
INTT： AIC SET(8) 后 WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
```

**INTT 禁 5/7**：每轮 INTT 仍用 CrossCore flag **1/3**（与 NTT 同字面量，时序上在 GATE 4/8 之后）。

## Host 串行多 launch（T05 验收最小序列）

```
110 → [round-1 device trace] → 120 → [round-2 device trace] → 199
```

理想每轮设备 trace（按槽序，非严格实时序）：
`201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307`

完整 2 轮理想日志：
```
110
201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307
120
201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307
199
```

## 槽布局

与 T03 相同；见 `tiling.h` `ToyTraceSlot` 与 `mmad_custom.cpp`。

| 槽下标 | 编号 | 段 | 文件位点 |
|--------|------|-----|----------|
| 0–5 | 201–303 | NTT | `mmad_custom.cpp` AIV/AIC NTT 段 |
| 6–11 | 403–305 | GATE | GATE 段 |
| 12–17 | 206–307 | INTT | INTT 段（第二轮 1/3） |
| Host | 110/120/199 | — | `main.cpp` printf |
