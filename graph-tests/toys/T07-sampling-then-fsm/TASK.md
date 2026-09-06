# T07 — sampling/哈希前置 stub → 全 FSM

**状态**：PASS  
**图谱**：`D-EXP-T07`  
**目录**：`graph-tests/toys/T07-sampling-then-fsm/`  
**脚手架**：T03/T06 全 FSM；前置一段**轻量** sampling/哈希 stub（可用已验 SHA3/向量积木，**禁抄 Encrypt**）

---

## 开刀前遍历

| 项 | 结论 |
|----|------|
| T01–T06 | 全 PASS；假循环、真 Vec MAC、2×launch 均不挂 |
| 计划 S2 | Toy 应呈现 sampling → 代数；此缺口未补 |
| X14 | 勿用空转冒充 sampling |
| 判断 | SIM 持续绿 → 本刀补结构缺口；若再绿，主控将**停下来与用户讨论** NPU/下一战略 |

---

## 目标

1. **前置**：AIV（或 Host 填 GM + 设备轻处理）做有界 sampling/哈希 stub（例如短 SHAKE/SHA3 调用或已验哈希积木；输出写 GM）  
2. 再跑全 FSM：NTT `1/3` → 生产 GATE（AIC 先 WAIT4 + 真/轻积木）→ INTT `1/3`（禁 5/7）  
3. TRACE：前置段有独立号段（如 210–219 / 建议在 map 标明 `SAMPLE`）  
4. 不对 KAT/liboqs；SIM kernel <5min  

## 验收

| # | 标准 |
|---|------|
| A1 | 仅本目录 |
| A2/A3 | CPU + SIM_DIRECT 均 exit 0 |
| A4 | STATUS 写清 sampling stub 来源（哪块积木/API）与长度 |
| A5 | TRACE 可见 SAMPLE→NTT→GATE→INTT |
| A6 | 墙钟 ≤25min；SIM≤2 |

## 禁止

抄 Encrypt；5/7；Wait SyncAll；SoftSync；X14 空转；改 yaml；commit/push

## 反馈

```
T07: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
sample: <stub 描述>
trace_seen: …
notes: ≤5 行
```

---

## 本刀反馈（回填）

```
T07: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.13 (SIM kernel 7.8s；含编译首跑 ~15s)
sample: Host seed 32B + sha3_256 ref; AivSampleStub 16×int32 4-round Muls+Add → 64B/AIV
trace_seen: 108 211 311 212 312 + NTT/GATE/INTT + 199 (SIM 缺 401)
notes: T06 FSM 壳不变；SAMPLE 前置补结构缺口；SIM 不挂；禁 5/7/X14
```
