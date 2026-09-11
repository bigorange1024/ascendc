# T02 — 生产 GATE 时序（AIC 先 WAIT(4) + AIV 体量后双 SET(4)）

**状态**：下发中  
**图谱**：`D-EXP-T02` → 服务 `Q-REPRO-ON-SIM` / `Q-HANG-LOCUS` / `F-GATE-SEMANTICS-DIFF`  
**代码目录（本刀唯一）**：`graph-tests/toys/T02-prod-gate-timing/`  
**脚手架**：允许 **复制 T01 目录壳**（CMake/`run.sh`/TRACE 骨架）后改握手；**禁止**抄旧 Encrypt / 旧 `pass-toy-*` 生产核体

---

## 开刀前已遍历（主控）

| 项 | 结论 |
|----|------|
| T01 | PASS：1/3 握手 SIM 不挂；SIM 可缺 401（X13） |
| 禁令 | 5/7、Wait 中 SyncAll、自造 SoftSync、抄旧 Encrypt、加 launch 当修复 |
| 缺口 | 生产 GATE ≠ 对称 GATE；须验「AIC 已 WAIT(4) 时 AIV 仍在体量」 |
| 非目标 | INTT、真内积正确性、liboqs、多 launch |

---

## 目标

单 launch MIX（`KERNEL_TYPE_MIX_AIC_1_2`）顺序：

1. **NTT 同构**：`SET(1) → WAIT(1)+极轻 Cube → SET(3) → WAIT(3)`（沿用 T01 意图）
2. **生产 GATE 时序**（本刀新增）：
   - **AIC**：NTT 段结束后 **立刻** `WAIT(4)`（先占坑；Wait 期间 **禁止** SyncAll）
   - **AIV**：NTT 后做一段**轻体量**（例如 UB 上有界向量循环 / 少量 DataCopy；**勿**巨型 MMAD 把 SIM 拖死），然后 **双 AIV** `SET(4)`
   - **AIC**：`WAIT(4)` 返回 → 极轻活（可选）→ `SET(8)`
   - **AIV**：`WAIT(8)` → 写完成 mark
3. Host：`SynchronizeStream` 返回；打印 TRACE
4. **不做 INTT**

## 验收（全部满足才算过）

| # | 标准 |
|---|------|
| A1 | 仅改本目录；未改 frozen/stable/旧 toy/T01 |
| A2 | `bash run.sh -r cpu -v Ascend910B4` exit 0 |
| A3 | `SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4` exit 0，无 Hang/Timeout |
| A4 | `trace_map.md`：Host100s / AIV0 200s / AIV1 300s / AIC 400s；须能区分 NTT 段与 GATE 段 |
| A5 | 日志可见：NTT 完成相关号 + AIC 进入 WAIT4 前号 + AIV 体量后 SET4 + AIC SET8 + Host sync 后号 |
| A6 | `STATUS.md`：PASS/FAIL/BLOCKED + 命令 + wall + 日志路径 |
| A7 | **墙钟 ≤ 25 分钟**；SIM 尝试 ≤ 2；超时或同错 2 次 → **STOP → BLOCKED**（写清卡在编译/跑/挂哪一步）；**禁止**再开第三条错路 |

## 禁止

- INTT / flag 5/7  
- AIC 在 CrossCore Wait 中 `SyncAll`  
- SoftSync / softSyncGm / 乱 recreate stream  
- 照抄 Encrypt / `*l18*` 生产核；改图谱 yaml；commit/push  
- 「对称 GATE」捷径：即 AIV 未做体量、双方齐到再 SET4（那就不是本刀）  
- 巨型计算把单次 SIM 拖到 >5 min（体量要轻，时序要对）

## 允许参考（只读）

- `graph-tests/toys/T01-mix-ntt13-handshake/`（推荐 fork 壳）  
- KB §2 / §3 / X1–X13；`docs/rg-encrypt-hang-rewrite.yaml` 节点 `F-GATE-SEMANTICS-DIFF`  
- AscendC API 查阅索引（新 API 必查并写回）

## 反馈模板

```
T02: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
trace_seen: …
notes: ≤5 行（是否确认 AIC 先于 AIV SET4 进入 WAIT4）
```

---

## 本刀反馈（subagent 回填）

```
T02: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.25 (SIM kernel 7.8s；含编译首跑 ~14s)
trace_seen: 101 201 301 402 203 303 403 204 304 404 205 305 199 (SIM 缺 401)
notes: 生产 GATE 时序已实现；AIC SET(3) 后立即 TRACE(403)+WAIT(4)，AIV 在 WAIT(3) 后做轻体量再 SET(4)；非对称 GATE；SIM 不挂
```
