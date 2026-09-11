# DS-W1 · 静态热点清单（离线，基于 D09 源码）

> 日期：2026-09-11  
> 范围：`graph-tests/dec_related/RB-D09-decrypt-1launch/`  
> 目的：给 H1–H4 排出**唯一**上板序；不替代 W0 上机复测。

## 1. 握手 / 屏障计数（`d09_decrypt_fused_custom.cpp`）

| 项 | 次数 / 位置 | 备注 |
|----|-------------|------|
| `SyncAll` | **1**（Prep 后，CrossCore Wait 环外） | 合法位置；成本未知 |
| NTT 轮 CrossSet/Wait | AIV Set(1)→AIC Wait(1)→AIC Set(3)→AIV Wait(3) | 完整 1 轮 |
| INTT 轮 CrossSet/Wait | 同上再来 1 轮 | 共 **2** 轮 flag{1,3} |
| `CrossWait`/`CrossSet` 外包 | 每次前后各 `PipeBarrier<PIPE_ALL>` | **每端点 2×ALL**，气泡嫌疑高 |
| AIV1 | 仅参与 Set/Wait + Trace | **零数学**；H4 候选 |

**热点档 Q3（握手气泡）**：**中高**（轮次不多，但每端点 ALL 屏障密；且 msopprof 见大量 wait_id）。

## 2. Prep / 解码（`prep_device_math.hpp`）

| 项 | 观察 | 档 |
|----|------|----|
| `poly_byte_decode12_scalar_gm` | 按 poly 调用；GM 标量读字节 | **高** |
| 逐系数 for 环 | 解包 / 组装 ŝ,u,v | **高** |
| 仅 AIV0 | Prep 段 AIC/AIV1 空转 + 随后 SyncAll | 放大 prep 墙钟 |

**热点档 Q1（Prep）**：**高**。

## 3. NTT / INTT + dot/extract（`ntt_device_math.hpp` / `intt_device_math.hpp`）

| 项 | 观察 | 档 |
|----|------|----|
| 蝶形三重 for | length×start×j 标量 | **高** |
| 逐系数环 | twiddle / 乘加 / extract | **高** |
| 位置 | 均在 AIV0、位于 Wait(3) **之后** | 直接计入 vector0 scalar |

**热点档 Q2（NTT/INTT 标量）**：**高**。

## 4. Cube 侧

| 项 | 观察 |
|----|------|
| `LightCube::Process` | 两轮 NTT/INTT 矩阵 | 
| PipeUtilization | cube≈0%，大量等 CrossCore | 

**结论**：Cube 不是算力墙；优化 Cube 微内核 **非本战役优先**。

## 5. 假说优先级（唯一推荐上板序）

| 序 | 假说 | 理由 |
|----|------|------|
| 1 | **H2 Prep/解码向量化** | 源码标量 GM 解码+解包档=高；与「vector0 scalar」同核同段；风险低于改蝶形 |
| 2 | **H3 NTT/INTT 向量化** | 蝶形环档=高；改动面大，放 H2 后 |
| 3 | **H1 握手屏障减薄** | 握手档=中高；改动小、易回归挂死——在已有正确融合上**先打数学热环更稳**；若 H2/H3 后 scalar% 仍高再打 H1 |
| 4 | **H4 双 AIV** | 仅当前序收益不足时；同步风险最高 |

> 说明：H1「改动小」不等于「应最先上板」。本战役主证据是 **scalar%**，优先砍 AIV0 数学标量环；握手刀作补刀。

## 6. 回写图谱

- Q1/Q2 倾向 → **高**  
- Q3 倾向 → **中高**  
- Q4 → 仍 open（依赖前刀）  
- `D-EXEC-ORDER` 具体序 → **H2 → H3 → H1 → H4**
