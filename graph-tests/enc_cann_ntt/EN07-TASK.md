# EN07 — 段间真数据贯通（CPU+SIM）

> DAG：`D-EXP-EN07`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN07-pipeline-wired/`  
> 基线：复制 `EN06-pack-compress-realbrick/`（勿改 EN01–06）  
> **仅 CPU+SIM**；主目标不挂。

---

## 1. 目标

在 EN06 五段真积木上，把 **GM 数据流贯通**（减少「每段独立造数、互不喂入」）：

```text
Prep(CBD→y) → NTT(y→ŷ) → Matvec(Â,ŷ→û/v̂ 形) → INTT → Pack→c
```

最低要求：

1. **Prep 输出 `y` 必须作为 NTT 输入**（同 bench/布局；Host 仅做指针/拷贝对齐，不算「重新随机造 y」）。  
2. **NTT 输出必须作为 Matvec 的 ŷ 输入**。  
3. Matvec→INTT→Pack：至少 **一条** 系数链贯通到 Pack 输入（可简化 Â/噪声仍 Host 喂，须 STATUS 写清）。  
4. 同进程五段 launch 顺序不变；禁融胖 MIX GATE。  
5. 主门禁不挂；贯通链尽量 golden，可 soft-fail。

非本刀：设备 SampleNTT；NPU；liboqs 全链。

---

## 2. 允许 / 禁止

允许：改 `main.cpp` / `gen_data` / 缓冲布局；微调各段 GM 偏移。  
禁止：抄 Encrypt 全核；改 EN01–06；GATE 4/8；NPU；commit/push；改 KB/DAG。

## 3. cannbot

sync_audit 全设备源 → STATUS。

## 4. 验收

```bash
cd graph-tests/enc_cann_ntt/EN07-pipeline-wired
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

STATUS 必须用表格写明：**每段输入来自哪段输出**；墙钟 ≤70min。

## 5. 反馈

```text
EN07: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
wired: Prep→NTT→… 一句
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```
