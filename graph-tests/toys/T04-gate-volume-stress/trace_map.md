# T04 TRACE 编号映射（KB §6）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> 设备侧写 TRACE 槽（DataCopy）；Host 在 Sync 后按槽打印对应编号（一行一个）。  
> **本刀须能区分 NTT / GATE / INTT 三段**（A5）；GATE 段 AIV 体量相对 T03 ×10（40 轮 vs 4 轮）。

## 体量参数（相对 T03）

| 参数 | T03 | T04 |
|------|-----|-----|
| GATE AIV 类 | `AivLightWorkload` | `AivGateVolumeWorkload` |
| 循环轮数 `kRounds` | 4 | **40** |
| 每轮向量长 `kWorkPerAiv` | 256 int8 | 256 int8（不变） |
| 每 AIV 总 touch | 4×256=1024 | **40×256=10240** |

## NTT 段（第一轮，flag 1/3）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **101** | Host | `ACLRT_LAUNCH` 前 | 必见 | Host 未进到 launch |
| **201** | AIV0 | SET(1) 前 | 核跑通应见 | AIV0 未到 NTT SET1 |
| **301** | AIV1 | SET(1) 前 | 核跑通应见 | AIV1 未到 NTT SET1 |
| **401** | AIC | WAIT(1) 之后 | 核跑通应见 | AIC 卡在 NTT WAIT(1) 或 TRACE 假空（X13） |
| **402** | AIC | SET(3) 之前 | 核跑通应见 | Cube 后未到 NTT SET3 |
| **203** | AIV0 | WAIT(3) 之后 | 核跑通应见 | AIV0 卡在 NTT WAIT(3) |
| **303** | AIV1 | WAIT(3) 之后 | 核跑通应见 | AIV1 卡在 NTT WAIT(3) |

## GATE 段（生产时序，flag 4/8）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **403** | AIC | **进入 WAIT(4) 前** | 核跑通应见 | AIC 未到 GATE 占坑 |
| **204** | AIV0 | **40 轮体量后**、SET(4) 前 | 核跑通应见 | AIV0 未做体量或未 SET4 |
| **304** | AIV1 | **40 轮体量后**、SET(4) 前 | 核跑通应见 | AIV1 未做体量或未 SET4 |
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
| **199** | Host | `aclrtSynchronizeStream` 返回后 | 必见才算跑完 | Sync 挂死 / 未返回 |

## 时序语义（本刀核心）

```
NTT：  AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
GATE： AIC SET(3) 后 TRACE(403) → WAIT(4)；AIV **40 轮**体量 → SET(4)；AIC SET(8)；AIV WAIT(8)
INTT： AIC SET(8) 后 WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
```

**INTT 禁 5/7**：第二轮仍用 CrossCore flag **1/3**（与 NTT 同字面量，时序上在 GATE 4/8 之后）。

## 验收日志最小序列（A5）

至少可见：`101`、NTT（`203`/`303`）、GATE（`403`/`404`/`205`/`305`）、INTT（`206`/`306`/`207`/`307` 或 `406`）、`199`。

理想完整（按槽序打印，非严格实时序）：
`101 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`

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
| 7 | 204 | GATE | AIV0 SET4 前（40 轮体量后） |
| 8 | 304 | GATE | AIV1 SET4 前（40 轮体量后） |
| 9 | 404 | GATE | AIC SET8 前 |
| 10 | 205 | GATE | AIV0 WAIT8 后 |
| 11 | 305 | GATE | AIV1 WAIT8 后 |
| 12 | 206 | INTT | AIV0 INTT SET1 前 |
| 13 | 306 | INTT | AIV1 INTT SET1 前 |
| 14 | 405 | INTT | AIC INTT WAIT1 后 |
| 15 | 406 | INTT | AIC INTT SET3 前 |
| 16 | 207 | INTT | AIV0 INTT WAIT3 后 |
| 17 | 307 | INTT | AIV1 INTT WAIT3 后 |
| Host | 101 / 199 | — | `main.cpp` printf |
