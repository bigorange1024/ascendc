# T05 — Host 多 launch 多轮（全 FSM）

**状态**：PASS  
**图谱**：`D-EXP-T05`（依据 X14：假循环加压不够）  
**目录**：`graph-tests/toys/T05-multi-launch-rounds/`  
**脚手架**：复制 T03（轻体量全 FSM）或 T04 壳；**Host 串行 launch ≥2 次**，每次完整 NTT→GATE→INTT

---

## 开刀前遍历

| 项 | 结论 |
|----|------|
| T01–T04 | 均 PASS；假循环体量×10 仍绿 → **X14** |
| 禁令 | 5/7、Wait SyncAll、SoftSync、抄 Encrypt、用加 launch「修挂」（本刀是**验证**多 launch，不是当修复叙事） |
| 缺口 | S3：2/3 launch 形态下多轮跑完不挂 |
| 非目标 | 数值正确、sampling、再加空转轮数到挂死 |

---

## 目标

1. 单核仍为全 FSM：NTT `1/3` + 生产 GATE（AIC 先 WAIT4）+ INTT 复用 `1/3`（禁 5/7）  
2. **Host**：`SynchronizeStream` 后 **再 launch 第 2 次**（建议共 **2** 轮；可选 3，但须控 SIM 总墙钟）  
3. TRACE：Host 用 `110/120`（或等价）区分 round；设备号可重复但 STATUS 须说明两轮都跑完  
4. 体量：默认用 **T03 轻量**（优先隔离「多 launch」变量）；若沿用 T04 体量须在 STATUS 写明，且总 SIM 目标 <5min  

## 验收

| # | 标准 |
|---|------|
| A1 | 仅本目录 |
| A2 | CPU 2 轮 launch 均 exit 0 |
| A3 | `SIM_DIRECT=1` sim：2 轮均完成，无 Hang |
| A4 | STATUS 写明 launch 次数、每轮 wall、trace（含 Host 轮次号） |
| A5 | INTT 仍 1/3；无 5/7 |
| A6 | 墙钟 ≤25min；SIM≤2；超时→BLOCKED |

## 禁止

- 5/7；Wait SyncAll；SoftSync；抄 Encrypt；改 yaml；commit/push  
- 为「制造挂」盲目把假循环加到 >5min（X14 已否决此路线）  
- 只 launch 1 次冒充多轮  

## 反馈

```
T05: PASS|FAIL|BLOCKED
cmd: …
exit: …
wall_min: …
launches: 2
trace_seen: …
notes: ≤5 行
```

---

## 本刀反馈（回填）

```
T05: PASS
cmd: SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
exit: 0
wall_min: ~0.33 (SIM kernel 19.6s；含编译首跑 ~26s)
launches: 2
trace_seen: 110 [r1 device 201-307] 120 [r2 device 201-307] 199 (SIM r1 缺 401/X13)
notes: Host 串行 2 launch；每轮全 FSM NTT→GATE→INTT(1/3)；轻量 T03 壳；无 5/7/SyncAll/SoftSync；多 launch 不挂
```
