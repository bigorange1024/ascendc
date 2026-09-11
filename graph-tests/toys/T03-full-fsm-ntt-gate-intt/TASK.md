# T03 — 单 launch 全 FSM：NTT + 生产 GATE + INTT(1/3)

**状态**：PASS  
**图谱**：`D-EXP-T03` → 服务 `Q-REPRO-ON-SIM` / `Q-HANG-LOCUS`  
**代码目录（本刀唯一）**：`graph-tests/toys/T03-full-fsm-ntt-gate-intt/`  
**脚手架**：允许复制 **T02** 目录壳后追加 INTT；禁止抄旧 Encrypt / 旧 pass-toy 生产核

---

## 开刀前已遍历（主控）

| 项 | 结论 |
|----|------|
| T01 | PASS：1/3 握手 |
| T02 | PASS：生产 GATE（AIC 先 WAIT4 + 轻体量后双 SET4 + SET8）；SIM 可缺 401（X13） |
| 禁令 | **INTT 禁用 5/7**（X1）；Wait 中 SyncAll（X2）；SoftSync（X3）；抄旧 Encrypt（X7）；加 launch 当修复（X4） |
| 缺口 | 单 launch 尚未闭合 INTT；生产路径是 GATE 后再用 **1/3** 做 INTT |
| 非目标 | 真 INTT 数值正确、liboqs、大体量、多 launch |

---

## 目标

单 launch MIX，顺序固定：

1. **NTT 同构**：`SET(1)→WAIT(1)+极轻 Cube→SET(3)→WAIT(3)`  
2. **生产 GATE**（保持 T02 时序）：AIC 立刻 `WAIT(4)`；AIV 轻体量后双 `SET(4)`；AIC `SET(8)`；AIV `WAIT(8)`  
3. **INTT 同构（本刀新增）**：再次 `SET(1)→WAIT(1)+极轻 Cube→SET(3)→WAIT(3)`  
   - **必须**复用 flag **1/3**  
   - **禁止** 5/7（已知挂死路线，不得「试一下」）  
4. Host SynchronizeStream 返回；打印 TRACE（NTT / GATE / INTT 三段可区分）  
5. 体量保持轻；不对正确性

## 验收

| # | 标准 |
|---|------|
| A1 | 仅本目录；未改 T01/T02/冻结树 |
| A2 | `bash run.sh -r cpu -v Ascend910B4` exit 0 |
| A3 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` exit 0，无 Hang/Timeout |
| A4 | `trace_map.md`：Host100s / AIV0 200s / AIV1 300s / AIC 400s；**三段**可区分 |
| A5 | 日志可见 GATE 段号 + INTT 段号（第二轮 1/3）+ Host sync 后号 |
| A6 | `STATUS.md`：PASS/FAIL/BLOCKED + 命令 + wall + 日志路径 |
| A7 | **墙钟 ≤ 25 分钟**；SIM ≤ 2 次；超时/同错 2 次 → STOP→BLOCKED |

## 禁止

- flag **5/7**（即使「只想快速试」也不行）  
- Wait 中 SyncAll；SoftSync；抄 Encrypt；改 yaml；commit/push  
- 对称 GATE 回退；巨型计算拖垮 SIM  

## 允许参考（只读）

- `graph-tests/toys/T02-prod-gate-timing/`（推荐 fork）  
- KB §2–§3 / X1–X13；图谱 `F-BAN-INTT-57`、`F-T02-SIM-PASS`

## 反馈模板

```
T03: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
trace_seen: …
notes: ≤5 行（INTT 是否复用 1/3；有无触碰 5/7）
```

---

## 本刀反馈（subagent 回填）

```
T03: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.3 (SIM kernel 9.5s；含编译首跑 ~17s)
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 206 306 405 406 207 307 199 (SIM 缺 401)
notes: INTT 复用 flag 1/3（第二轮 SET/WAIT 1/3）；绝对未用 5/7；三段 TRACE 可区分；单 launch 全 FSM 闭环
```
