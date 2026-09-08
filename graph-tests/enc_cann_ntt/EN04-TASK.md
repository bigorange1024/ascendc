# EN04 — Matvec 换真 NTT 域积木（CPU+SIM）

> DAG：`D-EXP-EN04`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN04-matvec-realbrick/`  
> 基线：复制 `EN03-encrypt-host-skel/`（勿改 EN01–03）  
> **仅 CPU+SIM**；主目标不挂。

---

## 1. 目标

保持 EN03 **五段 Host 序**（Prep桩→NTT→**Matvec**→INTT→Pack桩），将 **L3 Matvec** 从轻桩升级为 **真 NTT 域乘法/内积**（能力清单 M1/M2 语义）：

- 形状对齐 ML-KEM-1024：\(k=4\)，系数 `int32`，`q=3329`  
- **独立 AIV launch**（沿用 X32：CPU `AIV_ONLY` / SIM 无握手 MIX 占位若需要）  
- **禁止** AIC 陪跑空等、GATE 4/8、与 NTT 融核  
- 主门禁不挂；Matvec 正确性尽量对拍，失败可 `CORRECTNESS_SOFT_FAIL`

Prep/Pack 仍可为桩；NTT/INTT 继续用迁入积木。

---

## 2. 允许阅读（契约 / 单功能）

| 允许 | 用法 |
|------|------|
| `docs/notes/F203-innerproduct-k4-技术总结.md`、`F203-2s1e-NTT内积UB融合技术总结.md`（仅契约段） | 布局/语义 |
| `pass-fix-f203-alg11-12-multiplyntts-k4`、`…-innerproduct-k4` 的 **STATUS / 笔记** | I/O 形状 |
| `library/shared/f203_mod_q/` | 模约减 |
| EN03 树 | 复制壳与 NTT/INTT |

## 3. 禁止

- **打开并抄** Encrypt/alg14 compute / examples encrypt / ER0* **核源码** 当 Matvec 模板  
- 抄 innerproduct 探针 **整文件粘贴**（允许按笔记 **重写** 进本刀目录）  
- GATE 4/8、NPU、commit/push、改 KB/DAG  

## 4. cannbot

sync_audit 全设备源；红线禁否决；读 crosscore 仅当改握手。

## 5. 验收

```bash
cd graph-tests/enc_cann_ntt/EN04-matvec-realbrick
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

五段均完成不挂；STATUS + sync；墙钟 ≤60min。

## 6. 反馈

```text
EN04: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
matvec: 实现要点一句
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```
