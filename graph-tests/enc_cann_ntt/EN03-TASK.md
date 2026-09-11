# EN03 — Encrypt 形 Host 多段桩编排（CPU+SIM）

> DAG：`D-EXP-EN03`  
> 目录（新建）：`graph-tests/enc_cann_ntt/EN03-encrypt-host-skel/`  
> 基线：复用 EN02 的 NTT/INTT 积木与双 launch 经验；**勿改** EN01/EN02 目录。  
> **仅 CPU + SIM**；禁 NPU。主目标 **不挂**。

---

## 1. 目标

搭一条 **Encrypt 外形** 的 Host 编排骨架（对齐本线架构 A1–A3）：

| 段 | 角色 | 本刀实现 |
|----|------|----------|
| L1 Prep | 采样/解码外形 | **AIV_ONLY 桩**（轻量 Vec 或恒等拷贝级体量；可 Host 喂表；**禁止**抄旧 Encrypt prep 核） |
| L2 NTT | ŷ=NTT(y) | **真**迁入积木（自 EN02 拷贝核+矩阵；正向） |
| L3 Matvec | 域运算外形 | **AIV_ONLY 桩**（有界真 Vec MAC 可；**禁止** AIC 空等 GATE 4/8） |
| L4 INTT | INTT | **真**积木（换 M4_intt） |
| L5 Pack | 压缩编码外形 | **AIV_ONLY 桩** |

同进程、同 session **按序 launch 五段（或合并文档写明的最少段数，但 NTT 与 INTT 必须独立 launch）** 跑完不挂。

正确性：**非主门禁**；NTT/INTT 段可对拍；桩段可不对 golden。

---

## 2. 允许

- 复制 EN02 树中的 NTT/INTT 设备核、`scripts` 矩阵生成、main 双 launch 模式并扩展  
- 新建轻量 AIV_ONLY 桩核（中文注释）  
- 壳参考 graph-tests 既有 `cmake`/`run.sh`  
- cannbot sync_audit（**所有**设备侧源，含桩）

## 3. 禁止

- 抄 `examples/**encrypt|kem**`、`pass-*alg14*` 核、`frozen/**` 源、`enc_related/ER0*` 的 cpp/hpp  
- 自研 GATE **4/8**、INTT flag 5/7、Wait 中 SyncAll、自造 SoftSync  
- 把 Prep/Matvec/Pack 融进 NTT 的 MIX 胖核  
- `-r npu`、commit/push、改 KB/DAG  

## 4. cannbot

- CrossCore：仅保留 NTT/INTT 迁入核既有握手；桩核避免 CrossCore  
- `sync_audit.py` → STATUS；红线禁否决  
- 继承 X30/X31、B1–B7  

## 5. 验收

```bash
cd graph-tests/enc_cann_ntt/EN03-encrypt-host-skel
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

| 项 | 要求 |
|----|------|
| 不挂 | 五段（或任务书声明的段序）全部完成并正常退出 |
| STATUS | 段序、每段 kernel 名、墙钟/tick、sync 红线、一句教训 |
| 墙钟 | ≤60min；超时 ABORT |

## 6. 反馈

```text
EN03: PASS-NOHANG | FAIL | BLOCKED | ABORT
dir: ...
segments: Prep/NTT/Matvec/INTT/Pack 各一句
cpu/sim: ...
sync_audit: 红线=N path=…
lesson: 一句
```

## 7. 必读

`Encrypt-cann-ntt-kb.md`、`Encrypt-cann-ntt-workmode.md`、本文件、EN02 STATUS。
