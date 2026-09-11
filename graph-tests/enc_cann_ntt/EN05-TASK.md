# EN05 — Prep 换真采样积木（CPU+SIM）

> DAG：`D-EXP-EN05`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN05-prep-sample-realbrick/`  
> 基线：复制 `EN04-matvec-realbrick/`（勿改 EN01–04）  
> **仅 CPU+SIM**；主目标不挂。

---

## 1. 目标

保持五段 Host：`Prep → NTT → Matvec → INTT → Pack`，将 **L1 Prep** 从桩升级为真采样能力（能力清单 S1/S2）：

| 优先 | 内容 |
|------|------|
| 必做 | Alg.8 **CBD η=2** 路径（为后续 NTT 提供 `y` 外形；可用 shared SHAKE / 探针契约**重写**） |
| 可选同刀 | Alg.7 SampleNTT 单 poly 或 Host 喂 Â 表 + 设备侧仅 CBD（须在 STATUS 写明范围） |

约束：

- Prep **独立 AIV launch**（X32）；**禁止**与 NTT MIX 融核、禁 GATE 4/8  
- NTT 输入须与 Prep 输出布局对齐（或 Host 显式转置/拷贝，写清）  
- Matvec/NTT/INTT/Pack：沿用 EN04；Pack 仍可桩  
- 主门禁不挂；采样正确性尽量对拍，失败可 soft-fail  

---

## 2. 允许

- 笔记：`F203-Alg7-SampleNTT-单poly技术总结.md`、`F203-CBD-eta2-性能优化技术总结.md`  
- shared：`shake_xof_kernel/`、`keccak_f1600_kernel/`、`fips203_se_sample/`（Host golden）  
- 单功能探针 STATUS（`pass-fix-f203-alg7-*`、`pass-fix-f203-alg8-cbd-*`）— **禁整文件抄核**  
- EN04 树复制  

## 3. 禁止

抄 Encrypt prep / alg14 / examples encrypt / ER 核；NPU；commit/push；改 KB/DAG；否决 sync 红线。

## 4. cannbot

sync_audit 全设备源 → STATUS。

## 5. 验收

```bash
cd graph-tests/enc_cann_ntt/EN05-prep-sample-realbrick
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

五段完成不挂；STATUS 写明 Prep 覆盖范围；墙钟 ≤70min。

## 6. 反馈

```text
EN05: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
prep: 覆盖范围一句
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```
