# EN08 — 贯通管线粘性多轮（CPU+SIM）

> DAG：`D-EXP-EN08`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN08-wired-sticky-rounds/`  
> 基线：复制 `EN07-pipeline-wired/`（勿改 EN01–07）  
> **仅 CPU+SIM**；主目标不挂。

---

## 1. 目标

在 EN07 贯通五段上，同进程同 session **整链重复 ≥2 轮**（默认 **4**，可用 env 覆盖并写入 STATUS）：

```text
for r in 1..R:
  Prep → NTT → Matvec → INTT → Pack
```

约束：

- **不** recreate stream / 不重 aclInit（粘性）；轮间可换 seed/输入，但须仍贯通  
- 禁 GATE 4/8、禁融胖 MIX  
- 主门禁：R 轮全部完成不挂  
- 正确性：至少第 1 轮 golden；后续轮可不对拍但须跑完  

对照旧 hang 线 ER05「粘性双 COMPUTE」：本刀是 **真积木贯通链** 的多轮，不是空壳。

---

## 2. 禁止

改 EN01–07；抄 Encrypt 核；NPU；commit/push；改 KB/DAG；否决 sync 红线。

## 3. 验收

```bash
cd graph-tests/enc_cann_ntt/EN08-wired-sticky-rounds
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

STATUS 写明 R、每轮是否完成；sync_audit；墙钟 ≤90min（SIM 可能较长）。

## 4. 反馈

```text
EN08: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
rounds: R=…
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```
