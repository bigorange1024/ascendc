# T06 — GATE 段真积木体量（禁空转加码）

**状态**：PASS  
**图谱**：`D-EXP-T06`（承接 X14 / T05）  
**目录**：`graph-tests/toys/T06-gate-real-brick/`  
**脚手架**：复制 T03/T04 全 FSM；**替换** GATE 段假循环为有界真计算

---

## 开刀前遍历

| 项 | 结论 |
|----|------|
| T01–T05 | 全 PASS；假循环×10、2×launch 均不挂 |
| X14 | 禁止再靠 UB 空转轮数「加压」 |
| X6 | 轻量多 launch 未挂 ≠ 生产已解 |
| 积木 | NTT/向量/轻 MM 已验路径**可引用拼装**（禁抄旧 Encrypt） |
| 缺口 | AIC 已 WAIT(4) 时，AIV 做**像生产**的有界计算是否仍稳 |

---

## 目标

1. 单 launch 全 FSM：NTT `1/3` → 生产 GATE（AIC **先** WAIT4）→ INTT 复用 `1/3`（禁 5/7）  
2. GATE 段 AIV：**真积木**有界负载，二选一或组合（须在 STATUS 写清参数）  
   - 有界 **Vec MAC / 乘加累加**（GM↔UB，固定元素数），或  
   - 极轻 **Cube/MMAD**（可参考 T01 级小矩阵，勿巨型）  
3. **禁止**用 `for` 空转 / 无意义 SetValue 循环冒充体量（X14）  
4. 单次 SIM kernel 目标 **<5 min**

## 验收

| # | 标准 |
|---|------|
| A1 | 仅本目录 |
| A2 | CPU exit 0 |
| A3 | `SIM_DIRECT=1` sim exit 0，无 Hang |
| A4 | STATUS 写清真积木类型与形状/元素数；对比「非假循环」 |
| A5 | TRACE 三段可分；INTT 仍 1/3 |
| A6 | 墙钟 ≤25 min；SIM≤2；超时→BLOCKED |

Hang 则记最后 TRACE；**勿**回退 5/7 / 对称 GATE / 空转加码来「修好」。

## 禁止

5/7；Wait SyncAll；SoftSync；抄 Encrypt；X14 空转加压；改 yaml；commit/push

## 参考（只读）

- T03/T04 壳；T01 轻 Cube 写法（可借鉴尺寸，勿抄 Encrypt）  
- KB X6/X14；`F-BRICKS-OK`

## 反馈

```
T06: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
brick: <类型与形状>
trace_seen: …
notes: ≤5 行
```

---

## 本刀反馈（回填）

```
T06: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.07 (SIM kernel 4.4s)
brick: Vec MAC int32 a/b/acc[64]×2 AIV, 8 rounds Mul+Add+Muls
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199 (SIM 缺 401)
notes: 真向量积木替代 T04 假循环；AIC 先 WAIT4 下 SIM 不挂；INTT 仍 1/3；tick 26094
```
