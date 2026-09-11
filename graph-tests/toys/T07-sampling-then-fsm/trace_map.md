# T07 TRACE 编号映射（KB §6 + SAMPLE 段）

> 约定：Host 100–199 / AIV0 200–299 / AIV1 300–399 / AIC 400–499。  
> **本刀新增 SAMPLE 段（210–219 区）**；GATE 仍为 T06 真 Vec MAC。

## SAMPLE 段（FSM 之前；Host seed + AIV 向量 mixing）

| 编号 | 谁 | 含义 | 出现 | 缺失含义 |
|------|----|------|------|----------|
| **108** | Host | seed 装填完成、launch 前 | 必见 | Host 未装 seed |
| **211** | AIV0 | SAMPLE mixing 前 | 核跑通应见 | AIV0 未到 SAMPLE |
| **212** | AIV0 | SAMPLE 写 GM 后 | 核跑通应见 | AIV0 SAMPLE 未完成 |
| **311** | AIV1 | SAMPLE mixing 前 | 核跑通应见 | AIV1 未到 SAMPLE |
| **312** | AIV1 | SAMPLE 写 GM 后 | 核跑通应见 | AIV1 SAMPLE 未完成 |

**SAMPLE stub**：Host `seed.bin` 32B + `ref_sha3.bin`（hashlib.sha3_256，同 `fips203_se_sample`）；  
设备 `AivSampleStub`：seed 8×int32 → 16×int32，4 轮 Muls+Add → 64B/AIV 写 `SAMPLE_OUT`。

## NTT 段（第一轮，flag 1/3）

| 编号 | 谁 | 含义 |
|------|----|------|
| **101** | Host | launch 前 |
| **201/301** | AIV0/1 | SET(1) 前 |
| **401/402** | AIC | WAIT(1) 后 / SET(3) 前 |
| **203/303** | AIV0/1 | WAIT(3) 后 |

## GATE 段（生产时序 4/8；AIV 真 Vec MAC）

| 编号 | 谁 | 含义 |
|------|----|------|
| **403** | AIC | 进入 WAIT(4) 前 |
| **204/304** | AIV0/1 | Vec MAC 后、SET(4) 前 |
| **404** | AIC | WAIT(4) 返回后 |
| **205/305** | AIV0/1 | WAIT(8) 后 |

## INTT 段（第二轮，复用 flag 1/3，禁 5/7）

| 编号 | 谁 | 含义 |
|------|----|------|
| **206/306** | AIV0/1 | INTT SET(1) 前 |
| **405/406** | AIC | INTT WAIT(1) 后 / SET(3) 前 |
| **207/307** | AIV0/1 | INTT WAIT(3) 后 |
| **199** | Host | Sync 返回后 |

## 时序语义（本刀核心）

```
SAMPLE：Host seed → AIV 向量 mixing → SAMPLE_OUT → TRACE(211/212/311/312)
NTT：  SAMPLE→S0；AIV SET(1) → AIC WAIT(1)+Cube → SET(3) → AIV WAIT(3)
GATE： AIC TRACE(403)+WAIT(4)；AIV 真 Vec MAC → SET(4)；AIC SET(8)；AIV WAIT(8)
INTT： AIC WAIT(1)+Cube → SET(3)；AIV WAIT(8) 后 SET(1) → WAIT(3) → 完成标记
```

## 验收日志最小序列（A5）

至少可见：`108`、`211`/`212`/`311`/`312`、NTT（`203`/`303`）、GATE（`403`/`404`）、INTT（`207`/`307` 或 `406`）、`199`。

理想完整：
`108 101 211 311 212 312 201 301 401 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199`
