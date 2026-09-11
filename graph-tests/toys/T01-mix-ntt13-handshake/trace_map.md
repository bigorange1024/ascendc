# T01 TRACE 编号映射（KB §6）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> 设备侧写 TRACE 槽（DataCopy）；Host 在 Sync 后按槽打印对应编号（一行一个）。

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **101** | Host | `ACLRT_LAUNCH` 前 | 必见 | Host 未进到 launch |
| **201** | AIV0 | SET(1) 前 | 核跑通应见 | AIV0 未到 SET1（卡更早或未写 TRACE） |
| **301** | AIV1 | SET(1) 前 | 核跑通应见 | AIV1 未到 SET1 |
| **401** | AIC | WAIT(1) 之后 | 核跑通应见 | AIC 卡在 WAIT(1) 或 TRACE 假空 |
| **402** | AIC | SET(3) 之前 | 核跑通应见 | Cube 后未到 SET3 |
| **203** | AIV0 | WAIT(3) 之后 | 核跑通应见 | AIV0 卡在 WAIT(3) |
| **303** | AIV1 | WAIT(3) 之后 | 核跑通应见 | AIV1 卡在 WAIT(3) |
| **199** | Host | `aclrtSynchronizeStream` 返回后 | 必见才算跑完 | Sync 挂死 / 未返回 |

## 验收日志最小序列（A4）

至少可见：`101`、AIV SET1（`201` 或 `301`）、`401`、`402`、`199`。

理想完整：`101 201 301 401 402 203 303 199`（设备号顺序由 Host 按槽序打印，非严格实时序）。

## 槽布局

| 槽下标 | 编号 | 文件位点 |
|--------|------|----------|
| 0 | 201 | `mmad_custom.cpp` AIV0 SET1 前 `ToyTraceMark` |
| 1 | 301 | `mmad_custom.cpp` AIV1 SET1 前 |
| 2 | 401 | `mmad_custom.cpp` AIC WAIT1 后 |
| 3 | 402 | `mmad_custom.cpp` AIC SET3 前 |
| 4 | 203 | `mmad_custom.cpp` AIV0 WAIT3 后 |
| 5 | 303 | `mmad_custom.cpp` AIV1 WAIT3 后 |
| Host | 101 / 199 | `main.cpp` printf |
