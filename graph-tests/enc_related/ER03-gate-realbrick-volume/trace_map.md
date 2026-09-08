# ER03 TRACE 编号映射（KB §6）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> **本刀**：Launch1 prep（SAMPLE）+ Launch2 全 FSM（NTT/GATE/INTT）；GATE 为近生产体量真 Vec MAC（**256×32**）。

## Host（双 launch）

| 编号 | 含义 | 出现 | 缺失含义 |
|------|------|------|----------|
| **110** | prep launch 前 | 必见 | Host 未进 Launch1 |
| **120** | compute launch 前 | 必见 | Host 未进 Launch2 / prep Sync 未返 |
| **199** | 末次 Sync 返回后 | 必见 | compute 挂死或 Host 早退 |

## Launch1 SAMPLE / prep（210–219 区）

| 编号 | 谁 | 含义 |
|------|----|------|
| **211/311** | AIV0/1 | SAMPLE mixing 前 |
| **212/312** | AIV0/1 | SAMPLE 写 GM 后 |

## Launch2 NTT（第一轮 flag 1/3）

| 编号 | 谁 | 含义 |
|------|----|------|
| **201/301** | AIV0/1 | SET(1) 前 |
| **401/402** | AIC | WAIT(1) 后 / SET(3) 前 |
| **203/303** | AIV0/1 | WAIT(3) 后 |

## Launch2 GATE（生产时序 4/8；真 Vec MAC 256×32）

| 编号 | 谁 | 含义 |
|------|----|------|
| **403** | AIC | 进入 WAIT(4) 前 |
| **204/304** | AIV0/1 | Vec MAC 后、SET(4) 前 |
| **404** | AIC | WAIT(4) 返回后 |
| **205/305** | AIV0/1 | WAIT(8) 后 |

## Launch2 INTT（复用 1/3，禁 5/7）

| 编号 | 谁 | 含义 |
|------|----|------|
| **206/306** | AIV0/1 | INTT SET(1) 前 |
| **405/406** | AIC | INTT WAIT(1) 后 / SET(3) 前 |
| **207/307** | AIV0/1 | INTT WAIT(3) 后 |

## 时序语义

```
Host 110 → Launch1 PREP：AIV SAMPLE → 211/212/311/312；AIC 立即返回
Host 120 → Launch2 COMPUTE：
  NTT：  SAMPLE→S0；AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
  GATE： AIC TRACE(403)+WAIT(4)；AIV 真 Vec MAC(256×32) → SET(4)；AIC SET(8)；AIV WAIT(8)
  INTT： AIC WAIT(1)+Cube → SET(3)；AIV SET(1) → WAIT(3) → 完成标记
Host 199
```

## 验收最小序列

至少可见：`110`、`211`/`212`（或 `311`/`312`）、`120`、GATE（`403`/`404`）、INTT（`207`/`307` 或 `406`）、`199`。

理想：
`110 211 311 212 312 120 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`
